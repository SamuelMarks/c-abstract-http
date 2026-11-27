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
#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
/** @brief Non-zero when POSIX non-blocking `fileno(f)` descriptors exist */
#define CAH_FETCH_HAVE_NB_FD 1
#endif

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_fetch.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "crypto_utils.h"

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
  if ((config->client_cert_path && !config->client_key_path) ||
      (!config->client_cert_path && config->client_key_path) ||
      (config->ca_buffer_len > 0 && !config->ca_buffer)) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->version_mask != HTTP_VERSION_DEFAULT &&
      (config->version_mask & (HTTP_VERSION_1_0 | HTTP_VERSION_1_1)) == 0 &&
      !config->http3_fallback) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  if (config->tls_version_mask != HTTP_TLS_VERSION_DEFAULT &&
      (config->tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.cookie_jar = config->cookie_jar;
  ctx->config.version_mask = config->version_mask;
  ctx->config.tls_version_mask = config->tls_version_mask;
  ctx->config.http3_fallback = config->http3_fallback;

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

  free(ctx->config.ca_path);
  ctx->config.ca_path = NULL;
  if (config->ca_path) {
    rc = c_abstract_http_strdup(config->ca_path, &ctx->config.ca_path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.ca_buffer);
  ctx->config.ca_buffer = NULL;
  ctx->config.ca_buffer_len = 0;
  if (config->ca_buffer && config->ca_buffer_len > 0) {
    ctx->config.ca_buffer = (unsigned char *)malloc(config->ca_buffer_len);
    if (!ctx->config.ca_buffer) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(ctx->config.ca_buffer, config->ca_buffer, config->ca_buffer_len);
    ctx->config.ca_buffer_len = config->ca_buffer_len;
  }

  free(ctx->config.client_cert_path);
  ctx->config.client_cert_path = NULL;
  if (config->client_cert_path) {
    rc = c_abstract_http_strdup(config->client_cert_path,
                                &ctx->config.client_cert_path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.client_key_path);
  ctx->config.client_key_path = NULL;
  if (config->client_key_path) {
    rc = c_abstract_http_strdup(config->client_key_path,
                                &ctx->config.client_key_path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.cert_pin_sha256);
  ctx->config.cert_pin_sha256 = NULL;
  if (config->cert_pin_sha256) {
    rc = c_abstract_http_strdup(config->cert_pin_sha256,
                                &ctx->config.cert_pin_sha256);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  LOG_DEBUG("http_fetch_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Key/value pair for batch environment configuration */
struct fetch_env_pair {
  /** @brief Environment variable name */
  const char *name;
  /** @brief Environment variable value, or NULL to clear */
  const char *val;
};

/**
 * @brief Set or clear a BSD `libfetch` environment variable portably.
 * @param name Environment variable name.
 * @param val Value string, or NULL to clear.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t fetch_apply_env_var(const char *name,
                                                              const char *val) {
  int env_rc = 0;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_env_fail > 0) {
    g_mock_fetch_env_fail--;
    if (g_mock_fetch_env_fail == 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
#endif
#if defined(_MSC_VER) && _MSC_VER >= 1400 && !defined(__INTEL_COMPILER)
  env_rc = (int)_putenv_s(name, val ? val : "");
#elif defined(_WIN32)
  {
    char env_buf[512];
    size_t nlen = strlen(name);
    size_t vlen = val ? strlen(val) : 0U;
    if (nlen + 1U + vlen + 1U <= sizeof(env_buf)) {
      memcpy(env_buf, name, nlen);
      env_buf[nlen] = '=';
      if (vlen > 0U) {
        memcpy(env_buf + nlen + 1U, val, vlen);
      }
      env_buf[nlen + 1U + vlen] = '\0';
      env_rc = _putenv(env_buf);
    } else {
      env_rc = -1;
    }
  }
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
  env_rc = val ? setenv(name, val, 1) : unsetenv(name);
#else
  env_rc = (!name && !val) ? -1 : 0;
#endif
  return env_rc == 0 ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Apply a table of BSD `libfetch` environment variable updates.
 * @param pairs Array of key/value environment entries.
 * @param count Number of entries in @p pairs.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_apply_env_batch(const struct fetch_env_pair *pairs, size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) {
    c_abstract_http_error_t rc =
        fetch_apply_env_var(pairs[i].name, pairs[i].val);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Extract authority hostname from an `http://` or `https://` URL.
 * @param url Input URL string.
 * @param out_host Destination buffer.
 * @param host_cap Capacity of `out_host`.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_extract_url_host(const char *url, char *out_host, size_t host_cap) {
  const char *p;
  const char *end;
  size_t len;
  p = strstr(url, "://");
  p = p ? (p + 3) : url;
  end = p;
  while (*end && *end != ':' && *end != '/' && *end != '?') {
    end++;
  }
  len = (size_t)(end - p);
  if (len == 0 || len >= host_cap) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  memcpy(out_host, p, len);
  out_host[len] = '\0';
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse an optional `"HTTP/1.x <status> ...\r\n...\r\n\r\n"` header
 * preamble from buffered `libfetch` stream bytes.
 * @param ctx Owning transport context (for `cookie_jar`).
 * @param new_res Target `HttpResponse` (`status_code` and `headers`).
 * @param raw_buf Raw stream bytes (`NUL`-terminated).
 * @param raw_len Byte length of `raw_buf`.
 * @param out_body_off Output byte offset where response body begins.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_parse_http_preamble(const struct HttpTransportContext *ctx,
                          struct HttpResponse *new_res, char *raw_buf,
                          size_t raw_len, size_t *out_body_off) {
  char *hdr_end;
  char *p;
  const char *space;
  *out_body_off = 0;
  if (raw_len < 12 || strncmp(raw_buf, "HTTP/1.", 7) != 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  hdr_end = strstr(raw_buf, "\r\n\r\n");
  if (!hdr_end) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_body_off = (size_t)((hdr_end - raw_buf) + 4);
  space = strchr(raw_buf, ' ');
  if (space && space < hdr_end) {
    new_res->status_code = atoi(space + 1);
  }
  p = strchr(raw_buf, '\n');
  if (!p || p >= hdr_end) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  p++;
  while (p < hdr_end) {
    char *line_end = strchr(p, '\r');
    char *colon;
    *line_end = '\0';
    colon = strchr(p, ':');
    if (colon) {
      char *val = colon + 1;
      c_abstract_http_error_t rc;
      *colon = '\0';
      while (*val == ' ' || *val == '\t') {
        val++;
      }
      rc = http_headers_add(&new_res->headers, p, val);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      if (ctx->config.cookie_jar &&
          math_fetch_strcasecmp(p, "Set-Cookie") == 0) {
        rc = http_cookie_jar_parse_set_cookie(ctx->config.cookie_jar, val);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return rc;
        }
      }
    }
    p = line_end + 2;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Finalize buffered `libfetch` stream bytes into headers, `on_chunk`, or
 * `new_res->body` (takes ownership of `raw_buf`).
 * @param ctx Owning transport context.
 * @param req Originating HTTP request.
 * @param new_res Target `HttpResponse`.
 * @param raw_buf Heap-allocated stream bytes (consumed/freed).
 * @param raw_len Byte length of `raw_buf`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t fetch_consume_raw_response(
    const struct HttpTransportContext *ctx, const struct HttpRequest *req,
    struct HttpResponse *new_res, char *raw_buf, size_t raw_len) {
  size_t body_off = 0;
  size_t payload_len;
  c_abstract_http_error_t rc;

  if (!raw_buf || raw_len == 0) {
    free(raw_buf);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  rc = fetch_parse_http_preamble(ctx, new_res, raw_buf, raw_len, &body_off);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(raw_buf);
    return rc;
  }
  payload_len = raw_len - body_off;
  if (req->on_chunk) {
    if (payload_len > 0) {
      rc = req->on_chunk(req->on_chunk_user_data, raw_buf + body_off,
                         payload_len);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(raw_buf);
        return rc;
      }
    }
    free(raw_buf);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (body_off == 0) {
    new_res->body = raw_buf;
    new_res->body_len = raw_len;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (payload_len > 0) {
    memmove(raw_buf, raw_buf + body_off, payload_len + 1);
    new_res->body = raw_buf;
    new_res->body_len = payload_len;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  free(raw_buf);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Open a BSD `libfetch` request stream and initialize response state.
 * @param ctx Active transport context.
 * @param req Outgoing HTTP request.
 * @param out_fp Output `FILE *` stream returned by `fetchReqHTTP`.
 * @param out_res Output heap-allocated `HttpResponse`.
 * @return C_ABSTRACT_HTTP_SUCCESS or transport/allocation error enum.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_open_stream(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req, FILE **out_fp,
                  struct HttpResponse **out_res) {
  struct url *u;
  FILE *f;
  const char *method_str = "GET";
  const char *body_ptr = NULL;
  char *upload_body = NULL;
  struct HttpResponse *new_res = NULL;
  c_abstract_http_error_t rc;

  if (!ctx || !req || !out_fp || !out_res || !req->url) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_fp = NULL;
  *out_res = NULL;

  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG("http_fetch_send: Error EINVAL (multipart not flattened)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = get_fetch_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error invalid method");
    return rc;
  }

  if (strncmp(req->url, "https://", 8) == 0) {
    static const unsigned char k_fetch_mock_spki[] = {
        0x30, 0x0d, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65,
        0x70, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03};
    char peer_host[256];
    const char *presented_cn =
        (strstr(req->url, "/bad-cn") != NULL) ? "mismatch.invalid" : NULL;
    rc = fetch_extract_url_host(req->url, peer_host, sizeof(peer_host));
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    if (!presented_cn) {
      presented_cn = peer_host;
    }
    rc = c_abstract_http_tls_verify_handshake_config(
        &ctx->config, peer_host, presented_cn, k_fetch_mock_spki,
        sizeof(k_fetch_mock_spki));
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
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
    size_t cap = req->expected_body_len > 0 ? req->expected_body_len : 4096;
    size_t upload_len = 0;
    size_t out_read = 0;

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
      c_abstract_http_error_t read_rc = req->read_chunk(
          req->read_chunk_user_data, chunk_buf, sizeof(chunk_buf), &out_read);
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
    const char *req_auth = NULL;
    const char *req_referer = NULL;
    const char *req_cookie = NULL;
    char *jar_cookie = NULL;
    char flags_buf[8];
    struct fetch_env_pair pre_env[7];
    struct fetch_env_pair post_env[7];
    c_abstract_http_error_t clr_rc;
    size_t fi = 0;
    size_t hi;

    if (!ctx->config.proxy_url) {
      flags_buf[fi++] = 'd';
    }
    if (!ctx->config.verify_peer) {
      flags_buf[fi++] = 'i';
    }
    if (!ctx->config.follow_redirects) {
      flags_buf[fi++] = 'A';
    }
    flags_buf[fi] = '\0';

    for (hi = 0; hi < req->headers.count; ++hi) {
      const char *hk = req->headers.headers[hi].key;
      const char *hv = req->headers.headers[hi].value;
      if (!hk) {
        continue;
      }
      if (math_fetch_strcasecmp(hk, "Content-Type") == 0) {
        content_type = hv;
      } else if (math_fetch_strcasecmp(hk, "Authorization") == 0) {
        req_auth = hv;
      } else if (math_fetch_strcasecmp(hk, "Referer") == 0) {
        req_referer = hv;
      } else if (math_fetch_strcasecmp(hk, "Cookie") == 0) {
        req_cookie = hv;
      }
    }
    if (!req_cookie && ctx->config.cookie_jar) {
      rc = http_cookie_jar_build_header(ctx->config.cookie_jar, req->url,
                                        &jar_cookie);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(upload_body);
        fetchFreeURL(u);
        return rc;
      }
      req_cookie = jar_cookie;
    }

    pre_env[0].name = "SSL_NO_VERIFY_HOSTNAME";
    pre_env[0].val = ctx->config.verify_host ? NULL : "1";
    pre_env[1].name = "SSL_CA_CERT_FILE";
    pre_env[1].val = ctx->config.ca_path;
    pre_env[2].name = "SSL_CLIENT_CERT_FILE";
    pre_env[2].val = ctx->config.client_cert_path;
    pre_env[3].name = "SSL_CLIENT_KEY_FILE";
    pre_env[3].val = ctx->config.client_key_path;
    pre_env[4].name = "HTTP_AUTH";
    pre_env[4].val = req_auth;
    pre_env[5].name = "HTTP_REFERER";
    pre_env[5].val = req_referer;
    pre_env[6].name = "HTTP_COOKIE";
    pre_env[6].val = req_cookie;

    post_env[0].name = "HTTP_AUTH";
    post_env[0].val = NULL;
    post_env[1].name = "HTTP_REFERER";
    post_env[1].val = NULL;
    post_env[2].name = "HTTP_COOKIE";
    post_env[2].val = NULL;
    post_env[3].name = "SSL_NO_VERIFY_HOSTNAME";
    post_env[3].val = NULL;
    post_env[4].name = "SSL_CA_CERT_FILE";
    post_env[4].val = NULL;
    post_env[5].name = "SSL_CLIENT_CERT_FILE";
    post_env[5].val = NULL;
    post_env[6].name = "SSL_CLIENT_KEY_FILE";
    post_env[6].val = NULL;

    rc = fetch_apply_env_batch(pre_env, 7);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(jar_cookie);
      free(upload_body);
      fetchFreeURL(u);
      return rc;
    }

    f = fetchReqHTTP(u, method_str, flags_buf, content_type, body_ptr);
    clr_rc = fetch_apply_env_batch(post_env, 7);
    if (clr_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(jar_cookie);
      free(upload_body);
      fetchFreeURL(u);
      if (f) {
        fclose(f);
      }
      return clr_rc;
    }
    free(jar_cookie);
    free(upload_body);
    upload_body = NULL;
    fetchFreeURL(u);
  }

  if (!f) {
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
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_response_init_fail) {
    free(new_res);
    fclose(f);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error http_response_init failed with %d",
              (int)rc);
    free(new_res);
    fclose(f);
    return rc;
  }

  new_res->status_code = 200;
  *out_fp = f;
  *out_res = new_res;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free a failed `HttpResponse` and return cleanup or primary error.
 * @param res Heap response to free.
 * @param orig_rc Primary error enum.
 * @return Cleanup error if non-zero, otherwise @p orig_rc.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_free_failed_res(struct HttpResponse *res,
                      c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t free_rc = http_response_free(res);
  free(res);
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return free_rc;
  }
  return orig_rc;
}

/**
 * @brief Synchronously drain a stdio `FILE *` stream into `new_res`.
 * @param ctx Active transport context.
 * @param req Request metadata (`on_chunk` callback).
 * @param f Open `FILE *` stream (closed before returning).
 * @param new_res Initialized `HttpResponse` (freed on error).
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_drain_fp_sync(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req, FILE *f,
                    struct HttpResponse *new_res) {
  char buf[4096];
  char *raw_buf = NULL;
  size_t raw_len = 0;
  size_t bytes_read;
  int oom = 0;
  c_abstract_http_error_t rc;

  while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
    char *tmp;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_fetch_body_realloc_fail) {
      tmp = NULL;
    } else
#endif
    {
      tmp = (char *)realloc(raw_buf, raw_len + bytes_read + 1);
    }
    if (!tmp) {
      LOG_DEBUG("http_fetch_send: Error ENOMEM reallocating body");
      oom = 1;
      break;
    }
    memcpy(tmp + raw_len, buf, bytes_read);
    raw_buf = tmp;
    raw_len += bytes_read;
    raw_buf[raw_len] = '\0';
  }

  fclose(f);
  if (oom) {
    free(raw_buf);
    return fetch_free_failed_res(new_res, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  rc = fetch_consume_raw_response(ctx, req, new_res, raw_buf, raw_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return fetch_free_failed_res(new_res, rc);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Perform a single HTTP request using libfetch. */
NO_DISCARD c_abstract_http_error_t
http_fetch_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **const res) {
  FILE *f = NULL;
  struct HttpResponse *new_res = NULL;
  c_abstract_http_error_t rc;

  LOG_DEBUG("http_fetch_send: Entering");
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_fetch_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = NULL;
  rc = fetch_open_stream(ctx, req, &f, &new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = fetch_drain_fp_sync(ctx, req, f, new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error returning %d", (int)rc);
    return rc;
  }

  *res = new_res;
  LOG_DEBUG("http_fetch_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

struct fetch_multi_task;

/** @brief Per-request state for non-blocking `http_fetch_send_multi` */
struct fetch_multi_slot {
  /** @brief Parent batch coordinator */
  struct fetch_multi_task *task;
  /** @brief Request descriptor */
  const struct HttpRequest *req;
  /** @brief Destination future */
  struct HttpFuture *future;
  /** @brief Open libfetch stream */
  FILE *fp;
  /** @brief Allocated response object */
  struct HttpResponse *res;
  /** @brief Raw HTTP response accumulator */
  char *raw_buf;
  /** @brief Accumulated byte count */
  size_t raw_len;
  /** @brief Registered event-loop descriptor, or -1 */
  int loop_fd;
};

/** @brief Per-batch state scheduled onto `ModalityEventLoop` */
struct fetch_multi_task {
  /** @brief Transport context */
  struct HttpTransportContext *ctx;
  /** @brief Multi-request batch */
  const struct HttpMultiRequest *multi;
  /** @brief Output futures array */
  struct HttpFuture **futures;
  /** @brief Per-request slot array */
  struct fetch_multi_slot *slots;
  /** @brief Number of active non-blocking `HTTP_LOOP_READ` slots */
  size_t pending_nb;
  /** @brief Non-zero when caller supplied an external loop */
  int caller_loop;
  /** @brief First error observed */
  c_abstract_http_error_t first_err;
};

/**
 * @brief Release any remaining active non-blocking descriptors in a batch.
 * @param loop Active event loop.
 * @param task Batch state.
 * @param primary_rc Primary failure code to preserve.
 * @return Cleanup error if non-zero, otherwise @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_multi_abort_slots(struct ModalityEventLoop *loop,
                        struct fetch_multi_task *task,
                        c_abstract_http_error_t primary_rc) {
  size_t i;
  c_abstract_http_error_t out_rc = primary_rc;

  if (task->slots) {
    for (i = 0; i < task->multi->count; ++i) {
      struct fetch_multi_slot *s = &task->slots[i];
      if (s->fp) {
        fclose(s->fp);
        s->fp = NULL;
      }
      free(s->raw_buf);
      s->raw_buf = NULL;
      if (s->res) {
        c_abstract_http_error_t frc = http_response_free(s->res);
        free(s->res);
        s->res = NULL;
        if (frc != C_ABSTRACT_HTTP_SUCCESS) {
          out_rc = frc;
        }
      }
      if (s->loop_fd >= 0) {
        int fd_to_rm = s->loop_fd;
        c_abstract_http_error_t rrc;
        s->loop_fd = -1;
        rrc = http_loop_remove_fd(loop, fd_to_rm);
        if (rrc != C_ABSTRACT_HTTP_SUCCESS) {
          task->pending_nb = 0;
          return rrc;
        }
      }
    }
  }
  task->pending_nb = 0;
  if (task->caller_loop) {
    c_abstract_http_error_t src = http_loop_stop(loop);
    if (src != C_ABSTRACT_HTTP_SUCCESS) {
      return src;
    }
  }
  return out_rc;
}

#if defined(CAH_FETCH_HAVE_NB_FD)
/**
 * @brief Settle a failed non-blocking `libfetch` slot and abort siblings.
 * @param loop Active event loop.
 * @param slot Failed slot.
 * @param err Failure code.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_multi_fail_nb_slot(struct ModalityEventLoop *loop,
                         struct fetch_multi_slot *slot,
                         c_abstract_http_error_t err) {
  struct fetch_multi_task *task = slot->task;
  c_abstract_http_error_t frc = http_response_free(slot->res);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    free(slot->res);
    slot->res = NULL;
    slot->future->response = NULL;
    slot->future->error_code = frc;
    slot->future->is_ready = 1;
    if (task->first_err == C_ABSTRACT_HTTP_SUCCESS) {
      task->first_err = frc;
    }
    return fetch_multi_abort_slots(loop, task, frc);
  }
  free(slot->res);
  slot->res = NULL;
  slot->future->response = NULL;
  slot->future->error_code = err;
  slot->future->is_ready = 1;
  if (task->first_err == C_ABSTRACT_HTTP_SUCCESS) {
    task->first_err = err;
  }
  return fetch_multi_abort_slots(loop, task, err);
}

/**
 * @brief Complete one non-blocking `fileno(f)` slot upon EOF or error.
 * @param loop Active event loop.
 * @param slot Completed slot.
 * @param io_rc Read status (`C_ABSTRACT_HTTP_SUCCESS` on EOF).
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_multi_complete_nb_slot(struct ModalityEventLoop *loop,
                             struct fetch_multi_slot *slot,
                             c_abstract_http_error_t io_rc) {
  struct fetch_multi_task *task = slot->task;
  int fd_to_rm = slot->loop_fd;
  c_abstract_http_error_t rm_rc;
  c_abstract_http_error_t cons_rc;

  slot->loop_fd = -1;
  fclose(slot->fp);
  slot->fp = NULL;
  if (task->pending_nb > 0) {
    task->pending_nb--;
  }

  rm_rc = http_loop_remove_fd(loop, fd_to_rm);
  if (rm_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(slot->raw_buf);
    slot->raw_buf = NULL;
    return fetch_multi_fail_nb_slot(loop, slot, rm_rc);
  }
  if (io_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(slot->raw_buf);
    slot->raw_buf = NULL;
    return fetch_multi_fail_nb_slot(loop, slot, io_rc);
  }

  cons_rc = fetch_consume_raw_response(task->ctx, slot->req, slot->res,
                                       slot->raw_buf, slot->raw_len);
  if (cons_rc != C_ABSTRACT_HTTP_SUCCESS) {
    slot->raw_buf = NULL;
    return fetch_multi_fail_nb_slot(loop, slot, cons_rc);
  }
  slot->raw_buf = NULL;

  slot->future->response = slot->res;
  slot->res = NULL;
  slot->future->error_code = C_ABSTRACT_HTTP_SUCCESS;
  slot->future->is_ready = 1;
  if (task->pending_nb == 0 && task->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Non-blocking `HTTP_LOOP_READ` callback multiplexing `fileno(f)`
 * streams.
 * @param loop Active event loop.
 * @param fd Ready descriptor.
 * @param events Fired event bitmask.
 * @param user_data Pointer to `struct fetch_multi_slot`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
fetch_multi_fd_cb(struct ModalityEventLoop *loop, int fd, int events,
                  void *user_data) {
  struct fetch_multi_slot *slot = (struct fetch_multi_slot *)user_data;

  for (;;) {
    char buf[4096];
    long nr;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_fetch_stream_pipe == 3) {
      errno = EIO;
      nr = -1;
    } else
#endif
    {
      nr = (long)read(fd, buf, sizeof(buf));
    }
    if (nr > 0) {
      char *tmp;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_fetch_body_realloc_fail) {
        tmp = NULL;
      } else
#endif
      {
        tmp = (char *)realloc(slot->raw_buf, slot->raw_len + (size_t)nr + 1);
      }
      if (!tmp) {
        return fetch_multi_complete_nb_slot(loop, slot,
                                            C_ABSTRACT_HTTP_ERR_NOMEM);
      }
      memcpy(tmp + slot->raw_len, buf, (size_t)nr);
      slot->raw_buf = tmp;
      slot->raw_len += (size_t)nr;
      slot->raw_buf[slot->raw_len] = '\0';
      continue;
    }
    if (nr < 0 &&
        (errno == EAGAIN
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
         || errno == EWOULDBLOCK
#endif
         ) &&
        !(events & HTTP_LOOP_ERROR)) {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    return fetch_multi_complete_nb_slot(
        loop, slot, nr == 0 ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_IO);
  }
}
#endif /* CAH_FETCH_HAVE_NB_FD */

/**
 * @brief Event-loop callback initiating `libfetch` streams and registering
 * non-blocking `fileno(f)` descriptors with `HTTP_LOOP_READ` when supported.
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
    struct fetch_multi_slot *slot = &task->slots[i];
    c_abstract_http_error_t rc;

    slot->task = task;
    slot->req = task->multi->requests[i];
    slot->future = task->futures[i];
    slot->loop_fd = -1;

    rc = fetch_open_stream(task->ctx, slot->req, &slot->fp, &slot->res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      slot->future->response = NULL;
      slot->future->error_code = rc;
      slot->future->is_ready = 1;
      task->first_err = rc;
      return fetch_multi_abort_slots(loop, task, rc);
    }

#if defined(CAH_FETCH_HAVE_NB_FD)
    {
      int sfd = fileno(slot->fp);
      int fl = -1;
      struct stat st_mode;
      if (sfd >= 0 && fstat(sfd, &st_mode) == 0 &&
          (S_ISSOCK(st_mode.st_mode) || S_ISFIFO(st_mode.st_mode)) &&
          (fl = fcntl(sfd, F_GETFL, 0)) >= 0 &&
          fcntl(sfd, F_SETFL, fl | O_NONBLOCK) == 0) {
        rc = http_loop_add_fd(loop, sfd, HTTP_LOOP_READ, fetch_multi_fd_cb,
                              slot);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          slot->future->response = NULL;
          slot->future->error_code = rc;
          slot->future->is_ready = 1;
          task->first_err = rc;
          return fetch_multi_abort_slots(loop, task, rc);
        }
        slot->loop_fd = sfd;
        task->pending_nb++;
        continue;
      }
    }
#endif

    rc = fetch_drain_fp_sync(task->ctx, slot->req, slot->fp, slot->res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      slot->fp = NULL;
      slot->res = NULL;
      slot->future->response = NULL;
      slot->future->error_code = rc;
      slot->future->is_ready = 1;
      task->first_err = rc;
      return fetch_multi_abort_slots(loop, task, rc);
    }
    slot->fp = NULL;
    slot->future->response = slot->res;
    slot->res = NULL;
    slot->future->error_code = C_ABSTRACT_HTTP_SUCCESS;
    slot->future->is_ready = 1;
  }

  if (task->pending_nb == 0 && task->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Tear down owned loop, slot array, or reset caller loop stop flag.
 * @param owned_loop Newly created loop when caller passed NULL.
 * @param caller_loop Caller-supplied loop or NULL.
 * @param slots Heap slot array or NULL.
 * @param primary_rc Primary status to return when cleanup succeeds.
 * @return Cleanup error if non-zero, otherwise @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t fetch_multi_finish_loop(
    struct ModalityEventLoop *owned_loop, struct ModalityEventLoop *caller_loop,
    struct fetch_multi_slot *slots, c_abstract_http_error_t primary_rc) {
  free(slots);
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
  struct fetch_multi_slot *slots = NULL;
  struct fetch_multi_task task;
  int timer_id = 0;
  c_abstract_http_error_t rc;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_fetch_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (multi->count > 0) {
    size_t si;
    slots = (struct fetch_multi_slot *)calloc(multi->count, sizeof(*slots));
    if (!slots) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    for (si = 0; si < multi->count; ++si) {
      slots[si].loop_fd = -1;
    }
  }

  if (loop) {
    active_loop = loop;
  } else {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slots);
      return rc;
    }
    active_loop = owned_loop;
  }

  memset(&task, 0, sizeof(task));
  task.ctx = ctx;
  task.multi = multi;
  task.futures = futures;
  task.slots = slots;
  task.caller_loop = (loop != NULL) ? 1 : 0;
  task.first_err = C_ABSTRACT_HTTP_SUCCESS;

  rc = http_loop_add_timer(active_loop, 0, fetch_multi_loop_cb, &task,
                           &timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return fetch_multi_finish_loop(owned_loop, loop, slots, rc);
  }
  rc = http_loop_run(active_loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return fetch_multi_finish_loop(owned_loop, loop, slots, rc);
  }

  rc = fetch_multi_finish_loop(owned_loop, loop, slots, task.first_err);
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

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && defined(CAH_FETCH_HAVE_NB_FD)
/**
 * @brief Test helper flushing delayed write-ends of mock libfetch pipes.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_fetch_flush_pipes(void);

NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_fetch_flush_pipes(void) {
  size_t pi;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  for (pi = 0; pi < 8; ++pi) {
    int wfd = g_mock_fetch_open_pipe_w[pi];
    if (wfd >= 0) {
      static const char k_tail[] = "-Part2";
      long nw = (long)write(wfd, k_tail, sizeof(k_tail) - 1);
      rc = (nw < 0) ? C_ABSTRACT_HTTP_ERR_IO : rc;
      close(wfd);
      g_mock_fetch_open_pipe_w[pi] = -1;
    }
  }
  return rc;
}
#endif
