/**
 * @file http_fetch.c
 * @brief FreeBSD libfetch transport backend implementation of the Abstract
 * Network Interface (ANI).
 */

/* clang-format off */
#if !defined(_WIN32)
#ifndef _DEFAULT_SOURCE
/** @brief Enable POSIX/BSD system extensions */
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
/** @brief Target POSIX.1-2008 API level */
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_fetch.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_fetch.h"
#else
#include <fetch.h>
#endif
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Transport configuration */
  struct HttpConfig config;
  /** @brief Preformatted `basic:*:user:pass` value for `HTTP_PROXY_AUTH` */
  char *proxy_auth_val;
#if !defined(_MSC_VER) && !defined(_POSIX_C_SOURCE) && !defined(__unix__) &&   \
    !defined(__APPLE__)
  /** @brief Owned `putenv` string for `HTTP_USER_AGENT` on legacy C89 */
  char *putenv_ua;
  /** @brief Owned `putenv` string for `HTTP_PROXY` on legacy C89 */
  char *putenv_proxy;
  /** @brief Owned `putenv` string for `HTTP_PROXY_AUTH` on legacy C89 */
  char *putenv_proxy_auth;
#endif
};

/** @brief Reference count of global libfetch initializations */
static int g_fetch_init_count = 0;

/**
 * @brief Pure case-insensitive ASCII string comparison for header lookup.
 * @param a First NUL-terminated string.
 * @param b Second NUL-terminated string.
 * @return Difference between first non-matching lowercased characters, or 0.
 */
static int math_fetch_strcasecmp(const char *a, const char *b) {
  while (*a && *b) {
    unsigned char ca = (unsigned char)*a;
    unsigned char cb = (unsigned char)*b;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (unsigned char)(ca + ('a' - 'A'));
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (unsigned char)(cb + ('a' - 'A'));
    }
    if (ca != cb) {
      return (int)ca - (int)cb;
    }
    a++;
    b++;
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/**
 * @brief Map a libfetch error code to `c_abstract_http_error_t`.
 * @param err Libfetch error code.
 * @return Corresponding `c_abstract_http_error_t` value.
 */
NO_DISCARD static c_abstract_http_error_t fetch_map_err_code(int err) {
  switch (err) {
  case FETCH_TIMEOUT:
    return C_ABSTRACT_HTTP_ERR_TIMEOUT;
  case FETCH_MEMORY:
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  case FETCH_DOWN:
  case FETCH_NETWORK:
  case FETCH_RESOLV:
  default:
    return C_ABSTRACT_HTTP_ERR_IO;
  }
}

/**
 * @brief Map libfetch error code to abstract HTTP error enum.
 *
 * @param[in] err Libfetch error code.
 * @param[out] out_err Pointer to receive abstract HTTP error enum.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
map_fetch_error(int err, c_abstract_http_error_t *out_err) {
  if (!out_err) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (err) {
  case FETCH_OK:
    *out_err = C_ABSTRACT_HTTP_SUCCESS;
    break;
  case FETCH_TIMEOUT:
    *out_err = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    break;
  case FETCH_MEMORY:
    *out_err = C_ABSTRACT_HTTP_ERR_NOMEM;
    break;
  case FETCH_DOWN:
  case FETCH_NETWORK:
  case FETCH_RESOLV:
  default:
    *out_err = C_ABSTRACT_HTTP_ERR_IO;
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HTTP method enum to libfetch method string.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Pointer to receive method string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
get_fetch_method_str(enum HttpMethod method, const char **out_str) {
  if (!out_str) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch ((int)method) {
  case HTTP_GET:
    *out_str = "GET";
    break;
  case HTTP_POST:
    *out_str = "POST";
    break;
  case HTTP_PUT:
    *out_str = "PUT";
    break;
  case HTTP_DELETE:
    *out_str = "DELETE";
    break;
  case HTTP_HEAD:
    *out_str = "HEAD";
    break;
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out_str = "TRACE";
    break;
  case HTTP_CONNECT:
    *out_str = "CONNECT";
    break;
  case HTTP_QUERY:
    *out_str = "QUERY";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_fetch_map_error(int err, c_abstract_http_error_t *out_err);

NO_DISCARD c_abstract_http_error_t c_abstract_http_test_fetch_method_str(
    enum HttpMethod method, const char **out_str);

/**
 * @brief Test hook to invoke map_fetch_error directly.
 *
 * @param[in] err Libfetch error code.
 * @param[out] out_err Mapped error enum pointer.
 * @return Error enum.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_fetch_map_error(
    int err, c_abstract_http_error_t *out_err) {
  return map_fetch_error(err, out_err);
}

/**
 * @brief Test hook to invoke get_fetch_method_str directly.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Method string pointer.
 * @return Error enum.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_fetch_method_str(
    enum HttpMethod method, const char **out_str) {
  return get_fetch_method_str(method, out_str);
}
#endif

/* Initialize global libfetch resources. */
NO_DISCARD c_abstract_http_error_t http_fetch_global_init(void) {
  LOG_DEBUG("http_fetch_global_init: Entering");
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  if (g_fetch_init_count++ == 0) {
    /* Initialize global libfetch state */
  }
  LOG_DEBUG("http_fetch_global_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Clean up global libfetch resources. */
NO_DISCARD c_abstract_http_error_t http_fetch_global_cleanup(void) {
  LOG_DEBUG("http_fetch_global_cleanup: Entering");
  if (g_fetch_init_count > 0 && --g_fetch_init_count == 0) {
    /* Clean up global libfetch state */
  }
  LOG_DEBUG("http_fetch_global_cleanup: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Initialize a new libfetch transport context. */
NO_DISCARD c_abstract_http_error_t
http_fetch_context_init(struct HttpTransportContext **const ctx) {
  c_abstract_http_error_t rc;
  struct HttpTransportContext *c;

  LOG_DEBUG("http_fetch_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_fetch_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  c = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!c) {
    LOG_DEBUG("http_fetch_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_config_init_fail) {
    free(c);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_context_init: Error http_config_init failed with %d",
              (int)rc);
    free(c);
    *ctx = NULL;
    return rc;
  }

  *ctx = c;
  LOG_DEBUG("http_fetch_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free a libfetch transport context. */
NO_DISCARD c_abstract_http_error_t
http_fetch_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_fetch_context_free: Entering");
  if (ctx) {
    c_abstract_http_error_t rch;
    free(ctx->proxy_auth_val);
    ctx->proxy_auth_val = NULL;
#if !defined(_MSC_VER) && !defined(_POSIX_C_SOURCE) && !defined(__unix__) &&   \
    !defined(__APPLE__)
    free(ctx->putenv_ua);
    free(ctx->putenv_proxy);
    free(ctx->putenv_proxy_auth);
#endif
    rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return rch;
    }
    free(ctx);
  }
  LOG_DEBUG("http_fetch_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration to libfetch transport context. */
NO_DISCARD c_abstract_http_error_t http_fetch_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  c_abstract_http_error_t rc;

  LOG_DEBUG("http_fetch_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_fetch_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;

  free(ctx->config.user_agent);
  ctx->config.user_agent = NULL;
  if (config->user_agent) {
    rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_url);
  ctx->config.proxy_url = NULL;
  if (config->proxy_url) {
    rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_username);
  ctx->config.proxy_username = NULL;
  if (config->proxy_username) {
    rc = c_abstract_http_strdup(config->proxy_username,
                                &ctx->config.proxy_username);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_password);
  ctx->config.proxy_password = NULL;
  if (config->proxy_password) {
    rc = c_abstract_http_strdup(config->proxy_password,
                                &ctx->config.proxy_password);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->proxy_auth_val);
  ctx->proxy_auth_val = NULL;
  if (ctx->config.proxy_username) {
    const char *pw =
        ctx->config.proxy_password ? ctx->config.proxy_password : "";
    size_t ulen = strlen(ctx->config.proxy_username);
    size_t plen = strlen(pw);
    char *auth = (char *)malloc(8U + ulen + 1U + plen + 1U);
    if (!auth) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(auth, "basic:*:", 8U);
    memcpy(auth + 8U, ctx->config.proxy_username, ulen);
    auth[8U + ulen] = ':';
    memcpy(auth + 8U + ulen + 1U, pw, plen);
    auth[8U + ulen + 1U + plen] = '\0';
    ctx->proxy_auth_val = auth;
  }

#if !defined(_MSC_VER) && !defined(_POSIX_C_SOURCE) && !defined(__unix__) &&   \
    !defined(__APPLE__)
  free(ctx->putenv_ua);
  ctx->putenv_ua = NULL;
  if (ctx->config.user_agent) {
    size_t n = strlen(ctx->config.user_agent);
    ctx->putenv_ua = (char *)malloc(16U + n + 1U);
    if (!ctx->putenv_ua) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(ctx->putenv_ua, "HTTP_USER_AGENT=", 16U);
    memcpy(ctx->putenv_ua + 16U, ctx->config.user_agent, n + 1U);
  }
  free(ctx->putenv_proxy);
  ctx->putenv_proxy = NULL;
  if (ctx->config.proxy_url) {
    size_t n = strlen(ctx->config.proxy_url);
    ctx->putenv_proxy = (char *)malloc(11U + n + 1U);
    if (!ctx->putenv_proxy) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(ctx->putenv_proxy, "HTTP_PROXY=", 11U);
    memcpy(ctx->putenv_proxy + 11U, ctx->config.proxy_url, n + 1U);
  }
  free(ctx->putenv_proxy_auth);
  ctx->putenv_proxy_auth = NULL;
  if (ctx->proxy_auth_val) {
    size_t n = strlen(ctx->proxy_auth_val);
    ctx->putenv_proxy_auth = (char *)malloc(16U + n + 1U);
    if (!ctx->putenv_proxy_auth) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(ctx->putenv_proxy_auth, "HTTP_PROXY_AUTH=", 16U);
    memcpy(ctx->putenv_proxy_auth + 16U, ctx->proxy_auth_val, n + 1U);
  }
#endif

  LOG_DEBUG("http_fetch_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Perform a single HTTP request using libfetch. */
NO_DISCARD c_abstract_http_error_t
http_fetch_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **const res) {
  struct url *u;
  FILE *f;
  const char *method_str;
  const char *body_ptr;
  char *upload_body;
  char buf[4096];
  size_t bytes_read;
  struct HttpResponse *new_res;
  char *tmp;
  c_abstract_http_error_t rc;

  method_str = "GET";
  body_ptr = NULL;
  upload_body = NULL;
  new_res = NULL;
  tmp = NULL;
  rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_fetch_send: Entering");
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_fetch_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = NULL;

  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG("http_fetch_send: Error EINVAL (multipart not flattened)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = get_fetch_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error invalid method");
    return rc;
  }

  u = fetchParseURL(req->url);
  if (!u) {
    LOG_DEBUG("http_fetch_send: Error fetchParseURL failed");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  /* Configure BSD libfetch timeout (in seconds, rounded up) */
  {
    long eff_ms = ctx->config.timeout_ms;
    if (eff_ms <= 0 && ctx->config.connect_timeout_ms > eff_ms) {
      eff_ms = ctx->config.connect_timeout_ms;
    }
    if (eff_ms <= 0 && ctx->config.read_timeout_ms > eff_ms) {
      eff_ms = ctx->config.read_timeout_ms;
    }
    if (eff_ms <= 0 && ctx->config.write_timeout_ms > eff_ms) {
      eff_ms = ctx->config.write_timeout_ms;
    }
    fetchTimeout = eff_ms > 0 ? (int)((eff_ms + 999L) / 1000L) : 0;
  }

  /* Environment variables for proxy, proxy auth, and user-agent */
  if (ctx->config.user_agent) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_USER_AGENT", ctx->config.user_agent);
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    setenv("HTTP_USER_AGENT", ctx->config.user_agent, 1);
#else
    if (ctx->putenv_ua) {
      putenv(ctx->putenv_ua);
    }
#endif
  }

  if (ctx->config.proxy_url) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_PROXY", ctx->config.proxy_url);
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    setenv("HTTP_PROXY", ctx->config.proxy_url, 1);
#else
    if (ctx->putenv_proxy) {
      putenv(ctx->putenv_proxy);
    }
#endif
  } else {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_PROXY", "");
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    unsetenv("HTTP_PROXY");
#endif
  }

  if (ctx->proxy_auth_val) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_PROXY_AUTH", ctx->proxy_auth_val);
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    setenv("HTTP_PROXY_AUTH", ctx->proxy_auth_val, 1);
#else
    if (ctx->putenv_proxy_auth) {
      putenv(ctx->putenv_proxy_auth);
    }
#endif
  } else {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_PROXY_AUTH", "");
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    unsetenv("HTTP_PROXY_AUTH");
#endif
  }

  if (req->read_chunk && !req->body) {
    size_t cap;
    size_t upload_len;
    size_t out_read;

    if (req->expected_body_len > 0) {
      cap = req->expected_body_len;
    } else {
      cap = 4096;
    }
    upload_len = 0;
    out_read = 0;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_fetch_upload_alloc_fail) {
      upload_body = NULL;
    } else
#endif
    {
      upload_body = (char *)malloc(cap + 1);
    }
    if (!upload_body) {
      LOG_DEBUG("http_fetch_send: Error ENOMEM (upload_body)");
      fetchFreeURL(u);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    while (1) {
      char chunk_buf[1024];
      c_abstract_http_error_t read_rc;

      read_rc = req->read_chunk(req->read_chunk_user_data, chunk_buf,
                                sizeof(chunk_buf), &out_read);
      if (read_rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_fetch_send: read_chunk aborted");
        free(upload_body);
        fetchFreeURL(u);
        return read_rc;
      }
      if (out_read == 0) {
        break;
      }

      if (upload_len + out_read > cap) {
        char *new_buf;
        cap = (upload_len + out_read) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_fetch_upload_realloc_fail) {
          new_buf = NULL;
        } else
#endif
        {
          new_buf = (char *)realloc(upload_body, cap + 1);
        }
        if (!new_buf) {
          LOG_DEBUG("http_fetch_send: Error ENOMEM reallocating upload_body");
          free(upload_body);
          fetchFreeURL(u);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        upload_body = new_buf;
      }
      memcpy(upload_body + upload_len, chunk_buf, out_read);
      upload_len += out_read;
    }
    upload_body[upload_len] = '\0';
    body_ptr = upload_body;
  } else if (req->body) {
    body_ptr = (const char *)req->body;
  }

  {
    const char *content_type = NULL;
    const char *flags_str = ctx->config.verify_peer
                                ? (ctx->config.proxy_url ? "" : "d")
                                : (ctx->config.proxy_url ? "i" : "di");
    size_t hi;
    for (hi = 0; hi < req->headers.count; ++hi) {
      if (req->headers.headers[hi].key &&
          math_fetch_strcasecmp(req->headers.headers[hi].key, "Content-Type") ==
              0) {
        content_type = req->headers.headers[hi].value;
        break;
      }
    }
    f = fetchReqHTTP(u, method_str, flags_str, content_type, body_ptr);
  }
  fetchFreeURL(u);

  if (!f) {
    if (upload_body) {
      free(upload_body);
    }
    return fetch_map_err_code(fetchLastErrCode);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_response_alloc_fail) {
    new_res = NULL;
  } else
#endif
  {
    new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!new_res) {
    LOG_DEBUG("http_fetch_send: Error ENOMEM allocating new_res");
    fclose(f);
    if (upload_body) {
      free(upload_body);
    }
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_response_init_fail) {
    free(new_res);
    fclose(f);
    if (upload_body) {
      free(upload_body);
    }
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error http_response_init failed with %d",
              (int)rc);
    free(new_res);
    fclose(f);
    if (upload_body) {
      free(upload_body);
    }
    return rc;
  }

  new_res->status_code = 200;

  while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (req->on_chunk) {
      c_abstract_http_error_t chunk_rc =
          req->on_chunk(req->on_chunk_user_data, buf, bytes_read);
      if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
        c_abstract_http_error_t free_rc;
        LOG_DEBUG("http_fetch_send: Error on_chunk failed with %d",
                  (int)chunk_rc);
        fclose(f);
        if (upload_body) {
          free(upload_body);
        }
        *res = NULL;
        free_rc = http_response_free(new_res);
        if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(new_res);
          return free_rc;
        }
        free(new_res);
        return chunk_rc;
      }
    } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_fetch_body_realloc_fail) {
        tmp = NULL;
      } else
#endif
      {
        tmp = (char *)realloc((void *)new_res->body,
                              new_res->body_len + bytes_read + 1);
      }
      if (!tmp) {
        LOG_DEBUG("http_fetch_send: Error ENOMEM reallocating body");
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        break;
      }
      memcpy(tmp + new_res->body_len, buf, bytes_read);
      new_res->body = tmp;
      new_res->body_len += bytes_read;
      ((char *)new_res->body)[new_res->body_len] = '\0';
    }
  }

  fclose(f);

  if (upload_body) {
    free(upload_body);
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t free_rc;
    LOG_DEBUG("http_fetch_send: Error returning %d", (int)rc);
    *res = NULL;
    free_rc = http_response_free(new_res);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(new_res);
      return free_rc;
    }
    free(new_res);
    return rc;
  }

  *res = new_res;
  LOG_DEBUG("http_fetch_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Per-batch state scheduled onto `ModalityEventLoop` */
struct fetch_multi_task {
  /** @brief Transport context */
  struct HttpTransportContext *ctx;
  /** @brief Multi-request batch */
  const struct HttpMultiRequest *multi;
  /** @brief Output futures array */
  struct HttpFuture **futures;
  /** @brief Non-zero when caller supplied an external loop */
  int caller_loop;
  /** @brief First error observed */
  c_abstract_http_error_t first_err;
};

/**
 * @brief Event-loop callback driving `libfetch` batch execution and wakeup.
 *
 * Note: BSD `libfetch` (`fetchReqHTTP`) performs blocking socket/TLS connection
 * and HTTP header negotiation before returning each `FILE *` stream; scheduling
 * execution inside `ModalityEventLoop` integrates completion and loop wakeup
 * cleanly with `MODALITY_ASYNC` callers.
 *
 * @param loop Active event loop.
 * @param timer_id Fired timer identifier.
 * @param user_data Pointer to `struct fetch_multi_task`.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop control status.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_multi_loop_cb(struct ModalityEventLoop *loop, int timer_id,
                    void *user_data) {
  struct fetch_multi_task *task = (struct fetch_multi_task *)user_data;
  size_t i;
  (void)timer_id;

  for (i = 0; i < task->multi->count; i++) {
    struct HttpResponse *res = NULL;
    c_abstract_http_error_t rc =
        http_fetch_send(task->ctx, task->multi->requests[i], &res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      task->futures[i]->response = res;
      task->futures[i]->error_code = rc;
      task->futures[i]->is_ready = 1;
      task->first_err = rc;
      if (task->caller_loop) {
        c_abstract_http_error_t src = http_loop_stop(loop);
        if (src != C_ABSTRACT_HTTP_SUCCESS) {
          return src;
        }
      }
      return rc;
    }
    task->futures[i]->response = res;
    task->futures[i]->error_code = C_ABSTRACT_HTTP_SUCCESS;
    task->futures[i]->is_ready = 1;
  }
  if (task->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Tear down owned loop or reset caller loop stop flag and percolate
 * error.
 * @param owned_loop Newly created loop when caller passed NULL.
 * @param caller_loop Caller-supplied loop or NULL.
 * @param primary_rc Primary status to return when cleanup succeeds.
 * @return Cleanup error if non-zero, otherwise @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_multi_finish_loop(struct ModalityEventLoop *owned_loop,
                        struct ModalityEventLoop *caller_loop,
                        c_abstract_http_error_t primary_rc) {
  if (owned_loop) {
    c_abstract_http_error_t frc = http_loop_free(owned_loop);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return frc;
    }
  } else if (caller_loop) {
    c_abstract_http_error_t urc =
        abstract_http_event_loop_test_unstop(caller_loop);
    if (urc != C_ABSTRACT_HTTP_SUCCESS) {
      return urc;
    }
  }
  return primary_rc;
}

/* Dispatch multiple HTTP requests using libfetch integrated with
 * ModalityEventLoop. */
NO_DISCARD c_abstract_http_error_t http_fetch_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop;
  struct fetch_multi_task task;
  int timer_id = 0;
  c_abstract_http_error_t rc;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_fetch_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop) {
    active_loop = loop;
  } else {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    active_loop = owned_loop;
  }

  memset(&task, 0, sizeof(task));
  task.ctx = ctx;
  task.multi = multi;
  task.futures = futures;
  task.caller_loop = (loop != NULL) ? 1 : 0;
  task.first_err = C_ABSTRACT_HTTP_SUCCESS;

  rc = http_loop_add_timer(active_loop, 0, fetch_multi_loop_cb, &task,
                           &timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return fetch_multi_finish_loop(owned_loop, loop, rc);
  }
  rc = http_loop_run(active_loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return fetch_multi_finish_loop(owned_loop, loop, rc);
  }

  rc = fetch_multi_finish_loop(owned_loop, loop, task.first_err);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (loop) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_fetch_loop_wakeup_fail) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#endif
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}
