/**
 * @file http_wasm.c
 * @brief Emscripten Fetch API (WebAssembly) transport backend implementation of
 * the Abstract Network Interface (ANI).
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wasm.h>
#include "c_abstract_http/log.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection flag for wasm fetch failure */
extern int g_mock_wasm_fetch_fail;
/** @brief Fault-injection mode for wasm fetch timeout/state simulation */
extern int g_mock_wasm_fetch_timeout;
/** @brief Fault-injection flag for wasm config init failure */
extern int g_mock_wasm_config_init_fail;
/** @brief Fault-injection flag for wasm response init failure */
extern int g_mock_wasm_response_init_fail;
/** @brief Fault-injection flag for wasm response header addition failure */
extern int g_mock_wasm_header_add_fail;
/** @brief Fault-injection mode for wasm response headers length edge cases */
extern int g_mock_wasm_headers_len_zero;
/** @brief Fault-injection flag for mock allocator failure */
extern int g_mock_alloc_fail;
/** @brief Counter for mock allocator calls */
extern int g_mock_alloc_count;
#endif

#ifndef __EMSCRIPTEN__
/** @brief Fallback flag value for synchronous Emscripten fetch */
#define EMSCRIPTEN_FETCH_SYNCHRONOUS 1
/** @brief Fallback flag value for in-memory Emscripten fetch */
#define EMSCRIPTEN_FETCH_LOAD_TO_MEMORY 2
struct emscripten_fetch_t;
/** @brief Fallback struct emscripten_fetch_attr_t */
typedef struct emscripten_fetch_attr_t {
  /** @brief requestMethod */
  char requestMethod[32];
  /** @brief userData */
  void *userData;
  /** @brief onsuccess async callback */
  void (*onsuccess)(struct emscripten_fetch_t *fetch);
  /** @brief onerror async callback */
  void (*onerror)(struct emscripten_fetch_t *fetch);
  /** @brief onprogress async callback */
  void (*onprogress)(struct emscripten_fetch_t *fetch);
  /** @brief timeoutMSecs */
  unsigned long timeoutMSecs;
  /** @brief withCredentials */
  int withCredentials;
  /** @brief attributes */
  unsigned int attributes;
  /** @brief requestHeaders */
  const char **requestHeaders;
  /** @brief requestData */
  const char *requestData;
  /** @brief requestDataSize */
  size_t requestDataSize;
} emscripten_fetch_attr_t;

/** @brief Fallback struct emscripten_fetch_t */
typedef struct emscripten_fetch_t {
  /** @brief userData */
  void *userData;
  /** @brief status */
  unsigned short status;
  /** @brief numBytes */
  uint64_t numBytes;
  /** @brief readyState */
  unsigned short readyState;
  /** @brief data */
  const char *data;
} emscripten_fetch_t;
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Internal fault-injection selector for static wasm wrapper tests */
static int g_mock_wasm_op_fail = 0;
/** @brief Countdown before `g_mock_wasm_op_fail` fires */
static int g_mock_wasm_op_countdown = 0;

/**
 * @brief Check whether a test wasm static wrapper fault should fire.
 * @param op Operation code to match.
 * @return 1 if fault fires on this call, 0 otherwise.
 */
static int math_wasm_should_fail(int op) {
  if (g_mock_wasm_op_fail != op) {
    return 0;
  }
  if (g_mock_wasm_op_countdown > 0) {
    g_mock_wasm_op_countdown--;
    return 0;
  }
  g_mock_wasm_op_fail = 0;
  return 1;
}
#endif

/**
 * @brief Initialize an emscripten_fetch_attr_t structure.
 *
 * @param[out] attr Pointer to attribute structure.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_fetch_attr_init(emscripten_fetch_attr_t *attr) {
  if (!attr) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_wasm_should_fail(1)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
  emscripten_fetch_attr_init(attr);
#else
  memset(attr, 0, sizeof(*attr));
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Close and free an emscripten_fetch_t structure.
 *
 * @param[in] fetch Fetch structure to close.
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_fetch_close(emscripten_fetch_t *fetch) {
  if (fetch) {
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
    emscripten_fetch_close(fetch);
#else
    if (fetch->data) {
      free((void *)(size_t)fetch->data);
    }
    free(fetch);
#endif
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_wasm_should_fail(2)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Get length of response headers from an emscripten_fetch_t structure.
 *
 * @param[in] fetch Fetch structure.
 * @param[out] out_len Pointer to receive header length.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_fetch_get_response_headers_length(emscripten_fetch_t *fetch,
                                       size_t *out_len) {
  if (!out_len) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_wasm_should_fail(3)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
  *out_len = fetch ? emscripten_fetch_get_response_headers_length(fetch) : 0;
#else
  if (!fetch) {
    *out_len = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wasm_headers_len_zero == 1) {
    *out_len = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (g_mock_wasm_headers_len_zero == 2) {
    *out_len = 10;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (g_mock_wasm_headers_len_zero == 3) {
    *out_len = strlen(
        "Content-Type: text/plain\r\nSet-Cookie: wasm_sid=xyz123; Path=/");
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif
  *out_len = strlen("Content-Type: text/plain\r\nX-Custom-Header: value");
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Copy response headers from an emscripten_fetch_t structure.
 *
 * @param[in] fetch Fetch structure.
 * @param[out] dst Destination buffer.
 * @param[in] dst_size Size of destination buffer.
 * @param[out] out_copied Pointer to receive number of bytes copied.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_fetch_get_response_headers(emscripten_fetch_t *fetch, char *dst,
                                size_t dst_size, size_t *out_copied) {
  if (!out_copied) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_wasm_should_fail(4)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (!fetch || !dst || dst_size == 0) {
    *out_copied = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_copied = emscripten_fetch_get_response_headers(fetch, dst, dst_size);
#else
  {
    const char *hdrs = "Content-Type: text/plain\r\nX-Custom-Header: value";
    size_t len;
    if (!fetch || !dst || dst_size == 0) {
      *out_copied = 0;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wasm_headers_len_zero == 2) {
      dst[0] = '\0';
      *out_copied = 0;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (g_mock_wasm_headers_len_zero == 3) {
      hdrs = "Content-Type: text/plain\r\nSet-Cookie: wasm_sid=xyz123; Path=/";
    }
#endif
    len = strlen(hdrs);
    if (len >= dst_size) {
      len = dst_size - 1;
    }
    memcpy(dst, hdrs, len);
    dst[len] = '\0';
    *out_copied = len;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Execute an emscripten_fetch request.
 *
 * @param[in] attr Fetch attributes.
 * @param[in] url Target URL.
 * @param[out] out_fetch Pointer to receive fetch result.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_fetch_exec(const emscripten_fetch_attr_t *attr, const char *url,
                emscripten_fetch_t **out_fetch) {
  if (!attr || !out_fetch) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_fetch = NULL;
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    emscripten_fetch_attr_t local_attr = *attr;
    *out_fetch = emscripten_fetch(&local_attr, url);
  }
  return *out_fetch ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_IO;
#else
  {
    emscripten_fetch_t *f;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wasm_fetch_fail) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (math_wasm_should_fail(5)) {
      *out_fetch = NULL;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
#endif
    f = (emscripten_fetch_t *)calloc(1, sizeof(emscripten_fetch_t));
    if (!f) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    f->userData = attr->userData;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wasm_fetch_timeout == 1) {
      f->status = 0;
      f->numBytes = 0;
      f->readyState = 2;
      if ((attr->attributes & EMSCRIPTEN_FETCH_SYNCHRONOUS) == 0 &&
          attr->onerror) {
        attr->onerror(f);
        return C_ABSTRACT_HTTP_SUCCESS;
      }
      *out_fetch = f;
      return C_ABSTRACT_HTTP_SUCCESS;
    } else if (g_mock_wasm_fetch_timeout == 2) {
      char *buf = (char *)malloc(5);
      if (!buf) {
        free(f);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      f->status = 0;
      f->numBytes = 5;
      f->readyState = 2;
      memcpy(buf, "1234", 5);
      f->data = buf;
      *out_fetch = f;
      return C_ABSTRACT_HTTP_SUCCESS;
    } else if (g_mock_wasm_fetch_timeout == 3) {
      f->status = 0;
      f->numBytes = 0;
      f->readyState = 4;
      *out_fetch = f;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
#endif
    f->status = 200;
    f->readyState = 4;
    if (url && strstr(url, "empty") != NULL) {
      f->numBytes = 0;
      f->data = NULL;
    } else {
      const char *resp_body = "mock response body";
      char *buf;
      f->numBytes = strlen(resp_body);
      buf = (char *)malloc((size_t)f->numBytes + 1);
      if (!buf) {
        free(f);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(buf, (size_t)f->numBytes + 1, resp_body);
#else
      strcpy(buf, resp_body);
#endif
      f->data = buf;
    }
    if ((attr->attributes & EMSCRIPTEN_FETCH_SYNCHRONOUS) == 0) {
      if (attr->onprogress && f->numBytes > 0) {
        attr->onprogress(f);
      }
      if (attr->onsuccess) {
        attr->onsuccess(f);
        return C_ABSTRACT_HTTP_SUCCESS;
      }
    }
    *out_fetch = f;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif
}

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
  /** @brief Non-zero when user_agent was explicitly customized */
  int has_custom_ua;
};

/** @brief Reference count of global wasm transport initializations */
static int wasm_global_init_count = 0;

/**
 * @brief Initialize global wasm transport environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD c_abstract_http_error_t http_wasm_global_init(void) {
  wasm_global_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global wasm transport environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD c_abstract_http_error_t http_wasm_global_cleanup(void) {
  if (wasm_global_init_count > 0) {
    wasm_global_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Initialize a new wasm transport context. */
NO_DISCARD c_abstract_http_error_t
http_wasm_context_init(struct HttpTransportContext **ctx) {
  c_abstract_http_error_t rc;
  LOG_DEBUG("http_wasm_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_wasm_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *ctx = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_wasm_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wasm_config_init_fail) {
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wasm_context_init: Error http_config_init failed with %d",
              rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }
  LOG_DEBUG("http_wasm_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free a wasm transport context. */
NO_DISCARD c_abstract_http_error_t
http_wasm_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wasm_context_free: Entering");
  if (ctx) {
    c_abstract_http_error_t free_rc;
    free_rc = http_config_free(&ctx->config);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return free_rc;
    }
    free(ctx);
  }
  LOG_DEBUG("http_wasm_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration to wasm transport context. */
NO_DISCARD c_abstract_http_error_t http_wasm_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  LOG_DEBUG("http_wasm_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_wasm_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->proxy_url || config->cert_pin_sha256 || config->ca_path ||
      (config->ca_buffer && config->ca_buffer_len > 0) ||
      config->client_cert_path || config->client_key_path) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  if (!config->user_agent) {
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
    ctx->has_custom_ua = 0;
  } else if (!ctx->config.user_agent ||
             strcmp(ctx->config.user_agent, config->user_agent) != 0) {
    c_abstract_http_error_t rc;
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
    rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    ctx->has_custom_ua =
        strcmp(config->user_agent, "c_abstract_http/0.1.0") != 0 ? 1 : 0;
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
  LOG_DEBUG("http_wasm_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HTTP method enum to string representation.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Pointer to receive string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t get_method_str(enum HttpMethod method,
                                                         const char **out_str) {
  switch (method) {
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
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Case-insensitive ASCII string comparison for header keys.
 * @param a First string.
 * @param b Second string.
 * @return 0 if equal ignoring case, non-zero otherwise.
 */
static int math_wasm_strcasecmp(const char *a, const char *b) {
  unsigned char ca;
  unsigned char cb;
  while (*a && *b) {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (unsigned char)(ca + ('a' - 'A'));
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (unsigned char)(cb + ('a' - 'A'));
    }
    if (ca != cb) {
      return (int)ca - (int)cb;
    }
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/**
 * @brief Compute effective fetch timeout in milliseconds from HttpConfig.
 * @param cfg Transport configuration.
 * @return Effective timeout in milliseconds (0 if none configured).
 */
static unsigned long math_wasm_timeout_ms(const struct HttpConfig *cfg) {
  long best = cfg->timeout_ms;
  if (best > 0) {
    return (unsigned long)best;
  }
  if (cfg->connect_timeout_ms > best) {
    best = cfg->connect_timeout_ms;
  }
  if (cfg->read_timeout_ms > best) {
    best = cfg->read_timeout_ms;
  }
  if (cfg->write_timeout_ms > best) {
    best = cfg->write_timeout_ms;
  }
  return best > 0 ? (unsigned long)best : 0UL;
}

/**
 * @brief Prepare Emscripten Fetch attributes, headers array, and upload body.
 * @param ctx Transport context.
 * @param req Input HTTP request.
 * @param sync_mode Non-zero to enable EMSCRIPTEN_FETCH_SYNCHRONOUS.
 * @param out_attr Populated emscripten_fetch_attr_t.
 * @param out_headers Allocated requestHeaders array (caller frees).
 * @param out_cookie_hdr Allocated Cookie header value (caller frees).
 * @param out_body_buf Allocated streamed body buffer (caller frees).
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t wasm_prepare_request(
    const struct HttpTransportContext *ctx, const struct HttpRequest *req,
    int sync_mode, emscripten_fetch_attr_t *out_attr, const char ***out_headers,
    char **out_cookie_hdr, char **out_body_buf) {
  const char *method_str = NULL;
  const char **headers = NULL;
  char *cookie_hdr = NULL;
  char *body_buffer = NULL;
  size_t body_len = 0;
  size_t headers_count = 0;
  size_t i;
  int has_ua = 0;
  int has_cookie = 0;
  int inject_ua = 0;
  unsigned long to_ms;
  c_abstract_http_error_t rc;

  *out_headers = NULL;
  *out_cookie_hdr = NULL;
  *out_body_buf = NULL;

  if (req->parts.count > 0 && !req->body) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc = wasm_fetch_attr_init(out_attr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = get_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(out_attr->requestMethod, sizeof(out_attr->requestMethod),
           method_str);
#else
  strcpy(out_attr->requestMethod, method_str);
#endif
  out_attr->attributes =
      sync_mode
          ? (EMSCRIPTEN_FETCH_SYNCHRONOUS | EMSCRIPTEN_FETCH_LOAD_TO_MEMORY)
          : EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  out_attr->withCredentials = ctx->config.cookie_jar ? 1 : 0;

  to_ms = math_wasm_timeout_ms(&ctx->config);
  if (to_ms > 0) {
    out_attr->timeoutMSecs = to_ms;
  }

  for (i = 0; i < req->headers.count; ++i) {
    if (math_wasm_strcasecmp(req->headers.headers[i].key, "User-Agent") == 0) {
      has_ua = 1;
    } else if (math_wasm_strcasecmp(req->headers.headers[i].key, "Cookie") ==
               0) {
      has_cookie = 1;
    }
  }
  inject_ua = (!has_ua && ctx->has_custom_ua && ctx->config.user_agent != NULL);

  if (ctx->config.cookie_jar && !has_cookie) {
    rc = http_cookie_jar_build_header(ctx->config.cookie_jar, req->url,
                                      &cookie_hdr);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  if (req->headers.count > 0 || inject_ua || cookie_hdr) {
    size_t pair_cnt =
        req->headers.count + (inject_ua ? 1U : 0U) + (cookie_hdr ? 1U : 0U);
    headers = (const char **)malloc((pair_cnt * 2 + 1) * sizeof(const char *));
    if (!headers) {
      free(cookie_hdr);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    for (i = 0; i < req->headers.count; ++i) {
      headers[headers_count++] = req->headers.headers[i].key;
      headers[headers_count++] = req->headers.headers[i].value;
    }
    if (inject_ua) {
      headers[headers_count++] = "User-Agent";
      headers[headers_count++] = ctx->config.user_agent;
    }
    if (cookie_hdr) {
      headers[headers_count++] = "Cookie";
      headers[headers_count++] = cookie_hdr;
    }
    headers[headers_count] = NULL;
    out_attr->requestHeaders = headers;
  }

  if (req->read_chunk) {
    size_t cap = req->expected_body_len > 0 ? req->expected_body_len : 4096;
    size_t out_read = 0;

    body_buffer = (char *)malloc(cap);
    if (!body_buffer) {
      free((void *)headers);
      free(cookie_hdr);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    for (;;) {
      c_abstract_http_error_t read_rc;
      if (body_len == cap) {
        char *new_buf;
        cap *= 2;
        new_buf = (char *)realloc(body_buffer, cap);
        if (!new_buf) {
          free((void *)headers);
          free(cookie_hdr);
          free(body_buffer);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        body_buffer = new_buf;
      }
      read_rc =
          req->read_chunk(req->read_chunk_user_data, body_buffer + body_len,
                          cap - body_len, &out_read);
      if (read_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free((void *)headers);
        free(cookie_hdr);
        free(body_buffer);
        return (read_rc == C_ABSTRACT_HTTP_ERR_NOMEM)
                   ? read_rc
                   : C_ABSTRACT_HTTP_ERR_CANCELED;
      }
      if (out_read == 0) {
        break;
      }
      body_len += out_read;
    }
    out_attr->requestData = body_buffer;
    out_attr->requestDataSize = body_len;
  } else if (req->body) {
    out_attr->requestData = (const char *)req->body;
    out_attr->requestDataSize = req->body_len;
  }

  *out_headers = headers;
  *out_cookie_hdr = cookie_hdr;
  *out_body_buf = body_buffer;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert a completed `emscripten_fetch_t` into an `HttpResponse`.
 * @param ctx Transport context (optional cookie jar owner).
 * @param fetch Completed fetch handle.
 * @param req Originating HTTP request.
 * @param streamed_offset Bytes already delivered incrementally via
 * `onprogress`.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_parse_fetch_response(const struct HttpTransportContext *ctx,
                          emscripten_fetch_t *fetch,
                          const struct HttpRequest *req, size_t streamed_offset,
                          struct HttpResponse **res) {
  c_abstract_http_error_t rc;

  if (fetch->status == 0 && fetch->numBytes == 0 && fetch->readyState != 4) {
    return C_ABSTRACT_HTTP_ERR_TIMEOUT;
  }

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wasm_response_init_fail) {
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    return rc;
  }

  (*res)->status_code = fetch->status;

  if (fetch->numBytes > 0) {
    if (req->on_chunk) {
      size_t total_len = (size_t)fetch->numBytes;
      if (total_len > streamed_offset) {
        c_abstract_http_error_t cb_rc = req->on_chunk(
            req->on_chunk_user_data, fetch->data + streamed_offset,
            total_len - streamed_offset);
        if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
          struct HttpResponse *tmp_cb = *res;
          c_abstract_http_error_t free_cb_rc;
          *res = NULL;
          free_cb_rc = http_response_free(tmp_cb);
          if (free_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
            free(tmp_cb);
            return free_cb_rc;
          }
          free(tmp_cb);
          return (cb_rc == C_ABSTRACT_HTTP_ERR_NOMEM)
                     ? cb_rc
                     : C_ABSTRACT_HTTP_ERR_CANCELED;
        }
      }
    } else {
      (*res)->body = malloc((size_t)fetch->numBytes);
      if (!(*res)->body) {
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        goto err_res;
      }
      memcpy((*res)->body, fetch->data, (size_t)fetch->numBytes);
      (*res)->body_len = (size_t)fetch->numBytes;
    }
  }

  {
    size_t hdrs_len = 0;
    rc = wasm_fetch_get_response_headers_length(fetch, &hdrs_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto err_res;
    }
    if (hdrs_len > 0) {
      char *hdrs_buf = (char *)malloc(hdrs_len + 1);
      if (hdrs_buf) {
        size_t r = 0;
        char *p;
        const char *end;
        rc = wasm_fetch_get_response_headers(fetch, hdrs_buf, hdrs_len + 1, &r);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(hdrs_buf);
          goto err_res;
        }
        p = hdrs_buf;
        end = hdrs_buf + (r > 0 && r < hdrs_len ? r : hdrs_len);
        while (p < end) {
          char *line_end = strchr(p, 10);
          char *colon;
          if (line_end) {
            *line_end = '\0';
            if (line_end > p && *(line_end - 1) == 13) {
              *(line_end - 1) = '\0';
            }
          }
          colon = strchr(p, ':');
          if (colon) {
            char *val = colon + 1;
            *colon = '\0';
            while (*val == ' ') {
              val++;
            }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
            if (g_mock_wasm_header_add_fail) {
              free(hdrs_buf);
              rc = C_ABSTRACT_HTTP_ERR_NOMEM;
              goto err_res;
            }
#endif
            rc = http_headers_add(&(*res)->headers, p, val);
            if (rc != C_ABSTRACT_HTTP_SUCCESS) {
              free(hdrs_buf);
              goto err_res;
            }
            if (ctx->config.cookie_jar &&
                math_wasm_strcasecmp(p, "Set-Cookie") == 0) {
              rc =
                  http_cookie_jar_parse_set_cookie(ctx->config.cookie_jar, val);
              if (rc != C_ABSTRACT_HTTP_SUCCESS) {
                free(hdrs_buf);
                goto err_res;
              }
            }
          }
          if (line_end) {
            p = line_end + 1;
          } else {
            break;
          }
        }
        free(hdrs_buf);
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;

err_res: {
  struct HttpResponse *tmp_r = *res;
  c_abstract_http_error_t free_rc;
  *res = NULL;
  free_rc = http_response_free(tmp_r);
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(tmp_r);
    return free_rc;
  }
  free(tmp_r);
}
  return rc;
}

/**
 * @brief Close fetch handle, free temporary buffers, and percolate status.
 * @param fetch Optional emscripten_fetch_t handle to close.
 * @param headers Allocated header pointer array.
 * @param cookie_hdr Allocated Cookie header string.
 * @param body_buffer Allocated upload body buffer.
 * @param primary_rc Primary status to return when close succeeds.
 * @return Primary error or close error enum.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_cleanup_send_and_return(emscripten_fetch_t *fetch, const char **headers,
                             char *cookie_hdr, char *body_buffer,
                             c_abstract_http_error_t primary_rc) {
  free((void *)headers);
  free(cookie_hdr);
  free(body_buffer);
  if (fetch) {
    c_abstract_http_error_t close_rc = wasm_fetch_close(fetch);
    if (close_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return (primary_rc != C_ABSTRACT_HTTP_SUCCESS) ? primary_rc : close_rc;
    }
  }
  return primary_rc;
}

/* Perform a single HTTP request using wasm fetch. */
NO_DISCARD c_abstract_http_error_t
http_wasm_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
               struct HttpResponse **res) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_t *fetch = NULL;
  const char **headers = NULL;
  char *cookie_hdr = NULL;
  char *body_buffer = NULL;
  c_abstract_http_error_t rc;

  LOG_DEBUG("http_wasm_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_wasm_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = NULL;

  rc = wasm_prepare_request(ctx, req, 1, &attr, &headers, &cookie_hdr,
                            &body_buffer);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = wasm_fetch_exec(&attr, req->url, &fetch);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_cleanup_send_and_return(fetch, headers, cookie_hdr, body_buffer,
                                        rc);
  }
  if (!fetch) {
    return wasm_cleanup_send_and_return(NULL, headers, cookie_hdr, body_buffer,
                                        C_ABSTRACT_HTTP_ERR_IO);
  }
  rc = wasm_parse_fetch_response(ctx, fetch, req, 0, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_cleanup_send_and_return(fetch, headers, cookie_hdr, body_buffer,
                                        rc);
  }

  return wasm_cleanup_send_and_return(fetch, headers, cookie_hdr, body_buffer,
                                      C_ABSTRACT_HTTP_SUCCESS);
}

/** @brief Shared coordinator for non-blocking `http_wasm_send_multi` */
struct wasm_multi_batch {
  /** @brief Owning transport context */
  const struct HttpTransportContext *ctx;
  /** @brief Optional event loop to notify upon batch completion */
  struct ModalityEventLoop *loop;
  /** @brief Number of still-pending async fetches */
  size_t remaining;
  /** @brief First error observed across requests */
  c_abstract_http_error_t first_err;
  /** @brief Wakeup error observed if loop wakeup failed */
  c_abstract_http_error_t wakeup_err;
};

/** @brief Per-request state for non-blocking `emscripten_fetch` callbacks */
struct wasm_multi_slot {
  /** @brief Parent batch coordinator */
  struct wasm_multi_batch *batch;
  /** @brief Originating request */
  const struct HttpRequest *req;
  /** @brief Target future handle */
  struct HttpFuture *future;
  /** @brief Allocated header array */
  const char **headers;
  /** @brief Allocated Cookie header value */
  char *cookie_hdr;
  /** @brief Allocated upload body buffer */
  char *body_buffer;
  /** @brief Number of progress notifications received */
  size_t progress_events;
  /** @brief Byte offset already delivered to `req->on_chunk` via `onprogress`
   */
  size_t streamed_offset;
  /** @brief Non-zero error recorded if `req->on_chunk` aborted in `onprogress`
   */
  c_abstract_http_error_t chunk_abort_err;
};

/**
 * @brief Settle future, decrement batch counter, close fetch, and return
 * status.
 * @param slot Active async slot.
 * @param fetch Fetch handle to close.
 * @param res Parsed response or NULL on error.
 * @param slot_rc Status enum for this slot.
 * @return Effective slot/close/wakeup error enum.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_settle_async_slot(struct wasm_multi_slot *slot, emscripten_fetch_t *fetch,
                       struct HttpResponse *res,
                       c_abstract_http_error_t slot_rc) {
  c_abstract_http_error_t out_rc = slot_rc;
  c_abstract_http_error_t close_rc;

  free((void *)slot->headers);
  slot->headers = NULL;
  free(slot->cookie_hdr);
  slot->cookie_hdr = NULL;
  free(slot->body_buffer);
  slot->body_buffer = NULL;

  close_rc = wasm_fetch_close(fetch);
  if (close_rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (out_rc == C_ABSTRACT_HTTP_SUCCESS) {
      out_rc = close_rc;
    }
    slot->future->response = res;
    slot->future->error_code = out_rc;
    slot->future->is_ready = 1;
    if (slot->batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
      slot->batch->first_err = out_rc;
    }
    if (slot->batch->remaining > 0) {
      slot->batch->remaining--;
      if (slot->batch->remaining == 0 && slot->batch->loop) {
        c_abstract_http_error_t wrc = http_loop_wakeup(slot->batch->loop);
        if (wrc != C_ABSTRACT_HTTP_SUCCESS) {
          slot->batch->wakeup_err = wrc;
          return wrc;
        }
      }
    }
    return close_rc;
  }

  slot->future->response = res;
  slot->future->error_code = out_rc;
  slot->future->is_ready = 1;
  if (out_rc != C_ABSTRACT_HTTP_SUCCESS &&
      slot->batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
    slot->batch->first_err = out_rc;
  }

  if (slot->batch->remaining > 0) {
    slot->batch->remaining--;
    if (slot->batch->remaining == 0 && slot->batch->loop) {
      c_abstract_http_error_t wrc = http_loop_wakeup(slot->batch->loop);
      if (wrc != C_ABSTRACT_HTTP_SUCCESS) {
        slot->batch->wakeup_err = wrc;
        return wrc;
      }
    }
  }
  return out_rc;
}

/**
 * @brief Complete an asynchronous Emscripten fetch slot
 * (`onsuccess`/`onerror`).
 * @param fetch Completed or failed `emscripten_fetch_t` handle.
 * @param is_error Non-zero when invoked from `onerror`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_async_complete_impl(emscripten_fetch_t *fetch, int is_error) {
  struct wasm_multi_slot *slot = (struct wasm_multi_slot *)fetch->userData;
  struct HttpResponse *res = NULL;
  c_abstract_http_error_t rc;

  if (slot->chunk_abort_err != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_settle_async_slot(slot, fetch, NULL, slot->chunk_abort_err);
  }
  if (is_error && fetch->status == 0 && fetch->readyState != 4) {
    return wasm_settle_async_slot(slot, fetch, NULL,
                                  C_ABSTRACT_HTTP_ERR_TIMEOUT);
  }
  if (is_error && fetch->status == 0) {
    return wasm_settle_async_slot(slot, fetch, NULL, C_ABSTRACT_HTTP_ERR_IO);
  }
  rc = wasm_parse_fetch_response(slot->batch->ctx, fetch, slot->req,
                                 slot->streamed_offset, &res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_settle_async_slot(slot, fetch, res, rc);
  }
  return wasm_settle_async_slot(slot, fetch, res, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Dispatch an enum-returning wasm helper inside a void C-ABI callback.
 * @param expr Expression returning `c_abstract_http_error_t`.
 */
#define WASM_VOID_CB_DISPATCH(expr)                                            \
  do {                                                                         \
    c_abstract_http_error_t _cb_rc = (expr);                                   \
    if (_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("wasm_async_cb: error %d", (int)_cb_rc);                       \
    }                                                                          \
  } while (0)

/**
 * @brief Incremental `onprogress` handler forwarding newly arrived bytes to
 * `req->on_chunk`.
 * @param fetch Active Emscripten fetch handle.
 * @return C_ABSTRACT_HTTP_SUCCESS or chunk callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_async_progress_impl(emscripten_fetch_t *fetch) {
  struct wasm_multi_slot *slot = (struct wasm_multi_slot *)fetch->userData;
  if (!slot) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  slot->progress_events++;
  if (slot->chunk_abort_err == C_ABSTRACT_HTTP_SUCCESS && slot->req->on_chunk &&
      fetch->data && (size_t)fetch->numBytes > slot->streamed_offset) {
    size_t prev_off = slot->streamed_offset;
    size_t delta = (size_t)fetch->numBytes - prev_off;
    c_abstract_http_error_t cb_rc;
    slot->streamed_offset = (size_t)fetch->numBytes;
    cb_rc = slot->req->on_chunk(slot->req->on_chunk_user_data,
                                fetch->data + prev_off, delta);
    if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
      slot->chunk_abort_err = (cb_rc == C_ABSTRACT_HTTP_ERR_NOMEM)
                                  ? cb_rc
                                  : C_ABSTRACT_HTTP_ERR_CANCELED;
      return (cb_rc == C_ABSTRACT_HTTP_ERR_NOMEM)
                 ? cb_rc
                 : C_ABSTRACT_HTTP_ERR_CANCELED;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Emscripten Fetch C-API `onprogress` trampoline.
 * @param fetch Active fetch handle.
 */
static void wasm_async_onprogress_cb(emscripten_fetch_t *fetch) {
  WASM_VOID_CB_DISPATCH(wasm_async_progress_impl(fetch));
}

/**
 * @brief Emscripten Fetch C-API `onsuccess` trampoline.
 * @param fetch Completed fetch handle.
 */
static void wasm_async_onsuccess_cb(emscripten_fetch_t *fetch) {
  WASM_VOID_CB_DISPATCH(wasm_async_complete_impl(fetch, 0));
}

/**
 * @brief Emscripten Fetch C-API `onerror` trampoline.
 * @param fetch Failed fetch handle.
 */
static void wasm_async_onerror_cb(emscripten_fetch_t *fetch) {
  WASM_VOID_CB_DISPATCH(wasm_async_complete_impl(fetch, 1));
}

/* Perform multiple HTTP requests concurrently via non-blocking wasm fetch. */
NO_DISCARD c_abstract_http_error_t http_wasm_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct wasm_multi_batch batch;
  struct wasm_multi_slot *slots;
  size_t i;
  c_abstract_http_error_t rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  LOG_DEBUG("http_wasm_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_wasm_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  slots = (struct wasm_multi_slot *)calloc(multi->count, sizeof(*slots));
  if (!slots) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  memset(&batch, 0, sizeof(batch));
  batch.ctx = ctx;
  batch.loop = loop;
  batch.remaining = multi->count;
  batch.first_err = C_ABSTRACT_HTTP_SUCCESS;
  batch.wakeup_err = C_ABSTRACT_HTTP_SUCCESS;

  for (i = 0; i < multi->count; i++) {
    emscripten_fetch_attr_t attr;
    emscripten_fetch_t *async_handle = NULL;

    slots[i].batch = &batch;
    slots[i].req = multi->requests[i];
    slots[i].future = futures[i];

    rc = wasm_prepare_request(ctx, slots[i].req, 0, &attr, &slots[i].headers,
                              &slots[i].cookie_hdr, &slots[i].body_buffer);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      futures[i]->response = NULL;
      futures[i]->error_code = rc;
      futures[i]->is_ready = 1;
      free(slots);
      return rc;
    }

    attr.userData = &slots[i];
    attr.onsuccess = wasm_async_onsuccess_cb;
    attr.onerror = wasm_async_onerror_cb;
    attr.onprogress = wasm_async_onprogress_cb;

    rc = wasm_fetch_exec(&attr, slots[i].req->url, &async_handle);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free((void *)slots[i].headers);
      free(slots[i].cookie_hdr);
      free(slots[i].body_buffer);
      futures[i]->response = NULL;
      futures[i]->error_code = rc;
      futures[i]->is_ready = 1;
      free(slots);
      return rc;
    }
    if (!async_handle) {
      LOG_DEBUG("http_wasm_send_multi: async_handle settled synchronously");
    }
  }

#if !defined(__EMSCRIPTEN__) || defined(C_ABSTRACT_HTTP_TEST_OOM)
  free(slots);
#endif

  if (batch.first_err != C_ABSTRACT_HTTP_SUCCESS) {
    return batch.first_err;
  }
  return batch.wakeup_err;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Set internal wasm fault-injection mode for unit tests.
 * @param op Mode selector (0=off, 1=attr_init, 2=close, 3=hdrs_len, 4=hdrs_get,
 * 5=null_fetch, 10..14=direct helpers).
 * @param countdown Number of calls to skip before triggering.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_set_wasm_op_fail(int op, int countdown);

NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_set_wasm_op_fail(int op, int countdown) {
  g_mock_wasm_op_fail = op;
  g_mock_wasm_op_countdown = countdown;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Expose internal wasm helpers for test coverage.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_wasm_helpers(void);

/**
 * @brief Close fetch handle and preserve primary test helper error.
 * @param f Fetch handle.
 * @param primary_rc Primary error code.
 * @return Cleanup error or `primary_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
wasm_helper_close_ret(emscripten_fetch_t *f,
                      c_abstract_http_error_t primary_rc) {
  if (f && f->data) {
    free((void *)(size_t)f->data);
  }
  free(f);
  return primary_rc;
}

NO_DISCARD c_abstract_http_error_t c_abstract_http_test_wasm_helpers(void) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_t *f = NULL;
  struct wasm_multi_batch mb;
  struct wasm_multi_slot ms;
  struct HttpRequest req;
  struct HttpFuture fut;
  char buf[64];
  size_t len = 0;
  c_abstract_http_error_t rc;

  if (g_mock_wasm_op_fail == 10) {
    return wasm_fetch_attr_init(NULL);
  }
  if (g_mock_wasm_op_fail == 11) {
    return wasm_fetch_get_response_headers_length(NULL, NULL);
  }
  if (g_mock_wasm_op_fail == 12) {
    return wasm_fetch_get_response_headers(NULL, NULL, 0, NULL);
  }
  if (g_mock_wasm_op_fail == 13) {
    return wasm_fetch_exec(NULL, NULL, NULL);
  }
  if (g_mock_wasm_op_fail == 14) {
    emscripten_fetch_t *ef =
        (emscripten_fetch_t *)calloc(1, sizeof(emscripten_fetch_t));
    if (!ef) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memset(&mb, 0, sizeof(mb));
    memset(&ms, 0, sizeof(ms));
    memset(&req, 0, sizeof(req));
    memset(&fut, 0, sizeof(fut));
    mb.remaining = 1;
    ms.batch = &mb;
    ms.req = &req;
    ms.future = &fut;
    ef->userData = NULL;
    wasm_async_onprogress_cb(ef);
    ef->userData = &ms;
    ef->status = 0;
    ef->readyState = 4;
    wasm_async_onerror_cb(ef);
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  rc = wasm_fetch_close(NULL);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = wasm_fetch_get_response_headers_length(NULL, &len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = wasm_fetch_get_response_headers(NULL, NULL, 0, &len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = wasm_fetch_get_response_headers(NULL, buf, 0, &len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = wasm_fetch_attr_init(&attr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = wasm_fetch_exec(&attr, "http://example.com", &f);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = wasm_fetch_get_response_headers(f, NULL, 10, &len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_helper_close_ret(f, rc);
  }
  rc = wasm_fetch_get_response_headers(f, buf, 0, &len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_helper_close_ret(f, rc);
  }
  rc = wasm_fetch_get_response_headers(f, buf, 5, &len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return wasm_helper_close_ret(f, rc);
  }
  rc = wasm_fetch_close(f);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  f = (emscripten_fetch_t *)calloc(1, sizeof(emscripten_fetch_t));
  if (f) {
    f->data = NULL;
    rc = wasm_fetch_close(f);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif
