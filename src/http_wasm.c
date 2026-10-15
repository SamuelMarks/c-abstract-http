/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wasm.h>
#include "c_abstract_http/log.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_wasm_fetch_fail;
extern int g_mock_wasm_fetch_timeout;
extern int g_mock_wasm_config_init_fail;
extern int g_mock_wasm_response_init_fail;
extern int g_mock_wasm_header_add_fail;
extern int g_mock_wasm_headers_len_zero;
extern int g_mock_alloc_fail;
extern int g_mock_alloc_count;
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#define EMSCRIPTEN_FETCH_SYNCHRONOUS 1
/** @brief Fallback struct emscripten_fetch_attr_t */
typedef struct emscripten_fetch_attr_t {
  /** @brief requestMethod */
  char requestMethod[32];
  /** @brief timeoutMSecs */
  unsigned long timeoutMSecs;
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
/* clang-format on */

/**
 * @brief Initialize an emscripten_fetch_attr_t structure.
 *
 * @param[out] attr Pointer to attribute structure.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
static enum c_abstract_http_error
wasm_fetch_attr_init(emscripten_fetch_attr_t *attr) {
  if (!attr) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
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
static enum c_abstract_http_error wasm_fetch_close(emscripten_fetch_t *fetch) {
  if (fetch) {
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
    emscripten_fetch_close(fetch);
#else
    if (fetch->data) {
      free((char *)fetch->data);
    }
    free(fetch);
#endif
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Get length of response headers from an emscripten_fetch_t structure.
 *
 * @param[in] fetch Fetch structure.
 * @param[out] out_len Pointer to receive header length.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
static enum c_abstract_http_error
wasm_fetch_get_response_headers_length(emscripten_fetch_t *fetch,
                                       size_t *out_len) {
  if (!out_len) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
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
static enum c_abstract_http_error
wasm_fetch_get_response_headers(emscripten_fetch_t *fetch, char *dst,
                                size_t dst_size, size_t *out_copied) {
  if (!out_copied) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
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
static enum c_abstract_http_error
wasm_fetch_exec(const emscripten_fetch_attr_t *attr, const char *url,
                emscripten_fetch_t **out_fetch) {
  if (!attr || !out_fetch) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_fetch = NULL;
#if defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_TEST_OOM)
  *out_fetch = emscripten_fetch((emscripten_fetch_attr_t *)attr, url);
  return *out_fetch ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_IO;
#else
  {
    emscripten_fetch_t *f;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wasm_fetch_fail) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#endif
    f = (emscripten_fetch_t *)calloc(1, sizeof(emscripten_fetch_t));
    if (!f) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wasm_fetch_timeout == 1) {
      f->status = 0;
      f->numBytes = 0;
      f->readyState = 2;
      *out_fetch = f;
      return C_ABSTRACT_HTTP_SUCCESS;
    } else if (g_mock_wasm_fetch_timeout == 2) {
      f->status = 0;
      f->numBytes = 5;
      f->readyState = 2;
      {
        char *buf = (char *)malloc(5);
        if (buf) {
          memcpy(buf, "1234", 5);
        }
        f->data = buf;
      }
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
      if (buf) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
        strcpy_s(buf, (size_t)f->numBytes + 1, resp_body);
#else
        strcpy(buf, resp_body);
#endif
      }
      f->data = buf;
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
};

static int wasm_global_init_count = 0;

/**
 * @brief Initialize global wasm transport environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_wasm_global_init(void) {
  wasm_global_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global wasm transport environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_wasm_global_cleanup(void) {
  if (wasm_global_init_count > 0) {
    wasm_global_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a new wasm transport context.
 *
 * @param[out] ctx Pointer to receive context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_wasm_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
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
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&(*ctx)->config);
  }
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

/**
 * @brief Free a wasm transport context.
 *
 * @param[in] ctx The context to free.
 */
enum c_abstract_http_error
http_wasm_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wasm_context_free: Entering");
  if (ctx) {
    enum c_abstract_http_error free_rc = http_config_free(&ctx->config);
    free(ctx);
    return free_rc;
  }
  LOG_DEBUG("http_wasm_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Apply configuration to wasm transport context.
 *
 * @param[in] ctx The context.
 * @param[in] config The configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_wasm_config_apply(struct HttpTransportContext *ctx,
                       const struct HttpConfig *config) {
  LOG_DEBUG("http_wasm_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_wasm_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
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
static enum c_abstract_http_error get_method_str(enum HttpMethod method,
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
 * @brief Perform a single HTTP request using wasm fetch.
 *
 * @param[in] ctx The context.
 * @param[in] req The request to send.
 * @param[out] res Pointer to receive response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_wasm_send(struct HttpTransportContext *ctx,
                                          const struct HttpRequest *req,
                                          struct HttpResponse **res) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_t *fetch;
  const char **headers;
  size_t headers_count;
  size_t i;
  char *body_buffer;
  size_t body_len;
  const char *method_str;
  enum c_abstract_http_error rc;

  fetch = NULL;
  headers = NULL;
  headers_count = 0;
  body_buffer = NULL;
  body_len = 0;
  rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_wasm_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_wasm_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = NULL;

  /* Check if parts are flattened properly before attempting to send them */
  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG("http_wasm_send: Error EINVAL (multipart not flattened)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = wasm_fetch_attr_init(&attr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = get_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wasm_send: Error get_method_str failed %d", (int)rc);
    return rc;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(attr.requestMethod, sizeof(attr.requestMethod), method_str);
#else
  strcpy(attr.requestMethod, method_str);
#endif
  attr.attributes = EMSCRIPTEN_FETCH_SYNCHRONOUS;

  if (ctx->config.timeout_ms > 0) {
    attr.timeoutMSecs = (unsigned long)ctx->config.timeout_ms;
  }

  if (req->headers.count > 0) {
    headers = (const char **)malloc((req->headers.count * 2 + 1) *
                                    sizeof(const char *));
    if (!headers) {
      LOG_DEBUG("http_wasm_send: Error ENOMEM (headers)");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    for (i = 0; i < req->headers.count; i++) {
      headers[headers_count++] = req->headers.headers[i].key;
      headers[headers_count++] = req->headers.headers[i].value;
    }
    headers[headers_count] = NULL;
    attr.requestHeaders = headers;
  }

  if (req->read_chunk) {
    size_t cap = req->expected_body_len > 0 ? req->expected_body_len : 4096;
    size_t out_read;

    body_buffer = (char *)malloc(cap);
    if (!body_buffer) {
      LOG_DEBUG("http_wasm_send: Error ENOMEM (body_buffer)");
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      goto cleanup;
    }

    for (;;) {
      enum c_abstract_http_error chunk_rc;
      if (body_len == cap) {
        char *new_buf;
        cap *= 2;
        new_buf = (char *)realloc(body_buffer, cap);
        if (!new_buf) {
          LOG_DEBUG("http_wasm_send: Error ENOMEM (body_buffer realloc)");
          rc = C_ABSTRACT_HTTP_ERR_NOMEM;
          goto cleanup;
        }
        body_buffer = new_buf;
      }

      chunk_rc =
          req->read_chunk(req->read_chunk_user_data, body_buffer + body_len,
                          cap - body_len, &out_read);
      if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_wasm_send: Error read_chunk failed %d", (int)chunk_rc);
        rc = ECANCELED;
        goto cleanup;
      }
      if (out_read == 0) {
        break;
      }
      body_len += out_read;
    }

    attr.requestData = body_buffer;
    attr.requestDataSize = body_len;
  } else if (req->body) {
    attr.requestData = (const char *)req->body;
    attr.requestDataSize = req->body_len;
  }

  rc = wasm_fetch_exec(&attr, req->url, &fetch);
  if (rc != C_ABSTRACT_HTTP_SUCCESS || !fetch) {
    LOG_DEBUG("http_wasm_send: Error EIO (emscripten_fetch failed)");
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  if (fetch->status == 0 && fetch->numBytes == 0 && fetch->readyState != 4) {
    LOG_DEBUG("http_wasm_send: Error ETIMEDOUT");
    rc = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    goto cleanup;
  }

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res) {
    LOG_DEBUG("http_wasm_send: Error ENOMEM (*res)");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wasm_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(*res);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wasm_send: Error http_response_init failed with %d", rc);
    goto cleanup;
  }

  (*res)->status_code = fetch->status;

  if (fetch->numBytes > 0) {
    (*res)->body = malloc((size_t)fetch->numBytes);
    if (!(*res)->body) {
      LOG_DEBUG("http_wasm_send: Error ENOMEM ((*res)->body)");
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      goto cleanup;
    }
    memcpy((*res)->body, fetch->data, (size_t)fetch->numBytes);
    (*res)->body_len = (size_t)fetch->numBytes;

    if (req->on_chunk) {
      enum c_abstract_http_error chunk_rc;
      chunk_rc = req->on_chunk(req->on_chunk_user_data, (*res)->body,
                               (*res)->body_len);
      if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_wasm_send: Error ECANCELED (chunk callback failed %d)",
                  (int)chunk_rc);
        rc = ECANCELED;
        goto cleanup;
      }
      /* If chunk callback consumed data, we free the response body */
      free((*res)->body);
      (*res)->body = NULL;
      (*res)->body_len = 0;
    }
  }

  {
    size_t hdrs_len = 0;
    rc = wasm_fetch_get_response_headers_length(fetch, &hdrs_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup;
    }
    if (hdrs_len > 0) {
      char *hdrs_buf = (char *)malloc(hdrs_len + 1);
      if (hdrs_buf) {
        size_t r = 0;
        rc = wasm_fetch_get_response_headers(fetch, hdrs_buf, hdrs_len + 1, &r);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(hdrs_buf);
          goto cleanup;
        }
        if (r > 0) {
        }
        {
          char *p = hdrs_buf;
          const char *end = hdrs_buf + hdrs_len;
          while (p < end) {
            char *line_end = strchr(p, 10);
            if (line_end) {
              *line_end = '\0';
              if (line_end > p && *(line_end - 1) == 13) {
                *(line_end - 1) = '\0';
              }
            }
            {
              char *colon = strchr(p, ':');
              if (colon) {
                enum c_abstract_http_error add_rc;
                char *val = colon + 1;
                *colon = '\0';
                while (*val == ' ')
                  val++;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
                if (g_mock_wasm_header_add_fail) {
                  add_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
                } else {
                  add_rc = http_headers_add(&(*res)->headers, p, val);
                }
#else
                add_rc = http_headers_add(&(*res)->headers, p, val);
#endif
                if (add_rc != C_ABSTRACT_HTTP_SUCCESS) {
                  LOG_DEBUG("http_wasm_send: Error http_headers_add failed");
                  rc = C_ABSTRACT_HTTP_ERR_NOMEM;
                  free(hdrs_buf);
                  goto cleanup;
                }
              }
            }
            if (line_end) {
              p = line_end + 1;
            } else {
              break;
            }
          }
        }
        free(hdrs_buf);
      }
    }
  }

cleanup:
  if (fetch) {
    enum c_abstract_http_error close_rc = wasm_fetch_close(fetch);
    if (close_rc != C_ABSTRACT_HTTP_SUCCESS && rc == C_ABSTRACT_HTTP_SUCCESS) {
      rc = close_rc;
    }
  }
  if (headers) {
    free((void *)headers);
  }
  if (body_buffer) {
    free(body_buffer);
  }

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wasm_send: Success");
  } else {
    if (*res) {
      enum c_abstract_http_error free_rc = http_response_free(*res);
      free(*res);
      *res = NULL;
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return free_rc;
      }
    }
    LOG_DEBUG("http_wasm_send: Error returning %d", rc);
  }
  return rc;
}

/**
 * @brief Perform multiple HTTP requests concurrently via wasm fetch.
 *
 * @param[in] ctx The context.
 * @param[in] loop The event loop context.
 * @param[in] multi The multi request definition.
 * @param[out] futures Array of futures to populate.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_wasm_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  cah_cppcheck_mut_ptr((void *)ctx);
  if (loop) {
  }

  LOG_DEBUG("http_wasm_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_wasm_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    rc = http_wasm_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  if (loop) {
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Expose internal wasm helpers for test coverage.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_wasm_helpers(void);

enum c_abstract_http_error c_abstract_http_test_wasm_helpers(void) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_t *f = NULL;
  char buf[64];
  size_t len = 0;
  enum c_abstract_http_error rc;

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
  if (f) {
    rc = wasm_fetch_get_response_headers(f, NULL, 10, &len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = wasm_fetch_get_response_headers(f, buf, 0, &len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = wasm_fetch_get_response_headers(f, buf, 5, &len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = wasm_fetch_close(f);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  /* Cover f->data == NULL in wasm_fetch_close */
  f = (emscripten_fetch_t *)calloc(1, sizeof(emscripten_fetch_t));
  if (f) {
    f->data = NULL;
    rc = wasm_fetch_close(f);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  /* Cover malloc failure for resp_body inside wasm_fetch_exec */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = wasm_fetch_exec(&attr, "http://example.com", &f);
  if (rc == C_ABSTRACT_HTTP_SUCCESS && f) {
    rc = wasm_fetch_close(f);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      g_mock_alloc_fail = 0;
      return rc;
    }
  }
  g_mock_alloc_fail = 0;

  /* Cover malloc failure for timeout == 2 inside wasm_fetch_exec */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  g_mock_wasm_fetch_timeout = 2;
  rc = wasm_fetch_exec(&attr, "http://example.com", &f);
  if (rc == C_ABSTRACT_HTTP_SUCCESS && f) {
    rc = wasm_fetch_close(f);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      g_mock_wasm_fetch_timeout = 0;
      g_mock_alloc_fail = 0;
      return rc;
    }
  }
  g_mock_wasm_fetch_timeout = 0;
  g_mock_alloc_fail = 0;

  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif
