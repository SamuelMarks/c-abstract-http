/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wasm.h>
#include "c_abstract_http/log.h"
#include "str.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_wasm_fetch_fail;
extern int g_mock_wasm_fetch_timeout;
extern int g_mock_wasm_config_init_fail;
extern int g_mock_wasm_response_init_fail;
extern int g_mock_wasm_header_add_fail;
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#else
#define EMSCRIPTEN_FETCH_SYNCHRONOUS 1
typedef struct emscripten_fetch_attr_t {
  char requestMethod[32];
  unsigned long timeoutMSecs;
  unsigned int attributes;
  const char **requestHeaders;
  const char *requestData;
  size_t requestDataSize;
} emscripten_fetch_attr_t;

typedef struct emscripten_fetch_t {
  unsigned short status;
  uint64_t numBytes;
  unsigned short readyState;
  const char *data;
} emscripten_fetch_t;

static void emscripten_fetch_attr_init(emscripten_fetch_attr_t *attr) {
  memset(attr, 0, sizeof(*attr));
}

static void emscripten_fetch_close(emscripten_fetch_t *fetch) {
  if (fetch) {
    if (fetch->data) {
      free((void *)fetch->data);
    }
    free(fetch);
  }
}

static size_t
emscripten_fetch_get_response_headers_length(emscripten_fetch_t *fetch) {
  static const char hdr[] =
      "Content-Type: text/plain\nNoColonHeader\r\nLast: header";
  (void)fetch;
  return sizeof(hdr) - 1;
}

static void
emscripten_fetch_get_response_headers(emscripten_fetch_t *fetch, char *dst,
                                      size_t dst_size) {
  static const char hdr[] =
      "Content-Type: text/plain\nNoColonHeader\r\nLast: header";
  (void)fetch;
  if (dst && dst_size >= sizeof(hdr)) {
    memcpy(dst, hdr, sizeof(hdr));
  }
}

static emscripten_fetch_t *emscripten_fetch(const emscripten_fetch_attr_t *attr,
                                            const char *url) {
  emscripten_fetch_t *f;
  char *d;
  (void)attr;
  (void)url;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wasm_fetch_fail) {
    return NULL;
  }
  if (g_mock_wasm_fetch_timeout) {
    f = (emscripten_fetch_t *)calloc(1, sizeof(*f));
    if (f) {
      f->status = 0;
      f->numBytes = 0;
      f->readyState = 1;
    }
    return f;
  }
#endif
  f = (emscripten_fetch_t *)calloc(1, sizeof(*f));
  if (!f) {
    return NULL;
  }
  if (strcmp(attr->requestMethod, "HEAD") == 0) {
    f->status = 200;
    f->numBytes = 0;
    f->readyState = 4;
    f->data = NULL;
    return f;
  }
  f->status = 200;
  f->numBytes = 12;
  f->readyState = 4;
  d = (char *)malloc(13);
  if (d) {
    memcpy(d, "Hello WebAsm\0", 13);
  }
  f->data = d;
  return f;
}
#endif
/* clang-format on */

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
void http_wasm_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wasm_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_wasm_context_free: Exiting");
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

  emscripten_fetch_attr_init(&attr);
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
      int chunk_rc;
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
      if (chunk_rc != 0) {
        LOG_DEBUG("http_wasm_send: Error read_chunk failed %d", chunk_rc);
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

  fetch = emscripten_fetch(&attr, req->url);

  if (!fetch) {
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
      int chunk_rc = req->on_chunk(req->on_chunk_user_data, (*res)->body,
                                   (*res)->body_len);
      if (chunk_rc != 0) {
        LOG_DEBUG("http_wasm_send: Error ECANCELED (chunk callback failed %d)",
                  chunk_rc);
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
    size_t hdrs_len = emscripten_fetch_get_response_headers_length(fetch);
    if (hdrs_len > 0) {
      char *hdrs_buf = (char *)malloc(hdrs_len + 1);
      if (hdrs_buf) {
        emscripten_fetch_get_response_headers(fetch, hdrs_buf, hdrs_len + 1);
        {
          char *p = hdrs_buf;
          const char *end = hdrs_buf + hdrs_len;
          while (p < end && *p) {
            char *line_end = strchr(p, 13);
            if (!line_end) {
              line_end = strchr(p, 10);
            }
            if (line_end) {
              *line_end = '\0';
            }
            {
              char *colon = strchr(p, ':');
              if (colon) {
                char *val = colon + 1;
                *colon = '\0';
                while (*val == ' ')
                  val++;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
                if (g_mock_wasm_header_add_fail ||
                    http_headers_add(&(*res)->headers, p, val) != 0)
#else
                if (http_headers_add(&(*res)->headers, p, val) != 0)
#endif
                {
                  LOG_DEBUG("http_wasm_send: Error http_headers_add failed");
                  rc = C_ABSTRACT_HTTP_ERR_NOMEM;
                  free(hdrs_buf);
                  goto cleanup;
                }
              }
            }
            if (line_end) {
              p = line_end + 1;
              if (*p == 10)
                p++;
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
    emscripten_fetch_close(fetch);
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
      http_response_free(*res);
      free(*res);
      *res = NULL;
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
  (void)loop;

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
