
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wasm.h>
#include "c_abstract_http/log.h"
#include "str.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  struct HttpConfig config;
};

static int wasm_global_init_count = 0;

int http_wasm_global_init(void) {
  wasm_global_init_count++;
  return 0;
}

void http_wasm_global_cleanup(void) {
  if (wasm_global_init_count > 0) {
    wasm_global_init_count--;
  }
}

int http_wasm_context_init(struct HttpTransportContext **ctx) {
  LOG_DEBUG("http_wasm_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_wasm_context_init: Error EINVAL");
    return EINVAL;
  }
  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_wasm_context_init: Error ENOMEM");
    return ENOMEM;
  }
  memset(*ctx, 0, sizeof(struct HttpTransportContext));

  rc = http_config_init(&(*ctx)->config);
  if (rc != 0) {
    LOG_DEBUG("http_wasm_context_init: Error http_config_init failed with %d",
              rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }
  LOG_DEBUG("http_wasm_context_init: Success");
  return 0;
}

void http_wasm_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wasm_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_wasm_context_free: Exiting");
}

int http_wasm_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  LOG_DEBUG("http_wasm_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_wasm_config_apply: Error EINVAL");
    return EINVAL;
  }
  ctx->config = *config;
  LOG_DEBUG("http_wasm_config_apply: Success");
  return 0;
}

static int get_method_str(enum HttpMethod method, const char **out_str) {
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
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  case HTTP_HEAD:
    *out_str = "HEAD";
    break;
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out_str = "TRACE";
    break;
  case HTTP_QUERY:
    *out_str = "QUERY";
    break;
  case HTTP_CONNECT:
    *out_str = "CONNECT";
    break;
  default:
    *out_str = "GET";
    break;
  }
  return 0;
}

int http_wasm_send(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
#ifdef __EMSCRIPTEN__
  emscripten_fetch_attr_t attr;
  emscripten_fetch_t *fetch;
  const char **headers = NULL;
  size_t headers_count = 0;
  size_t i;
  char *body_buffer = NULL;
  size_t body_len = 0;
  const char *method_str;

  LOG_DEBUG("http_wasm_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_wasm_send: Error EINVAL");
    return EINVAL;
  }

  *res = NULL;

  /* Check if parts are flattened properly before attempting to send them */
  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG("http_wasm_send: Error EINVAL (multipart not flattened)");
    return EINVAL;
  }

  emscripten_fetch_attr_init(&attr);
  get_method_str(req->method, &method_str);
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
      return ENOMEM;
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
      rc = ENOMEM;
      goto cleanup;
    }

    while (1) {
      int chunk_rc;
      if (body_len == cap) {
        char *new_buf;
        cap *= 2;
        new_buf = (char *)realloc(body_buffer, cap);
        if (!new_buf) {
          LOG_DEBUG("http_wasm_send: Error ENOMEM (body_buffer realloc)");
          rc = ENOMEM;
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
    rc = EIO;
    goto cleanup;
  }

  if (fetch->status == 0 && fetch->numBytes == 0 && fetch->readyState != 4) {
    LOG_DEBUG("http_wasm_send: Error ETIMEDOUT");
    emscripten_fetch_close(fetch);
    rc = ETIMEDOUT;
    goto cleanup;
  }

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res) {
    LOG_DEBUG("http_wasm_send: Error ENOMEM (*res)");
    emscripten_fetch_close(fetch);
    rc = ENOMEM;
    goto cleanup;
  }

  rc = http_response_init(*res);
  if (rc != 0) {
    LOG_DEBUG("http_wasm_send: Error http_response_init failed with %d", rc);
    emscripten_fetch_close(fetch);
    free(*res);
    *res = NULL;
    goto cleanup;
  }

  (*res)->status_code = fetch->status;

  if (fetch->numBytes > 0) {
    (*res)->body = malloc(fetch->numBytes);
    if (!(*res)->body) {
      LOG_DEBUG("http_wasm_send: Error ENOMEM ((*res)->body)");
      emscripten_fetch_close(fetch);
      http_response_free(*res);
      free(*res);
      *res = NULL;
      rc = ENOMEM;
      goto cleanup;
    }
    memcpy((*res)->body, fetch->data, fetch->numBytes);
    (*res)->body_len = fetch->numBytes;

    if (req->on_chunk) {
      int chunk_rc = req->on_chunk(req->on_chunk_user_data, (*res)->body,
                                   (*res)->body_len);
      if (chunk_rc != 0) {
        LOG_DEBUG("http_wasm_send: Error ECANCELED (chunk callback failed %d)",
                  chunk_rc);
        http_response_free(*res);
        free(*res);
        *res = NULL;
        emscripten_fetch_close(fetch);
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
        /* http_response_init already initializes headers! No need to call
           http_headers_init here, but if we do it's a no-op / reset. */
        {
          char *p = hdrs_buf;
          const char *end = hdrs_buf + hdrs_len;
          while (p < end && *p) {
            char *line_end = strchr(p, '\r');
            if (!line_end) {
              line_end = strchr(p, '\n');
            }
            if (line_end) {
              *line_end = '\0';
            }
            if (p) {
              char *colon = strchr(p, ':');
              if (colon) {
                int add_rc;
                *colon = '\0';
                char *val = colon + 1;
                while (*val == ' ')
                  val++;
                add_rc = http_headers_add(&(*res)->headers, p, val);
                if (add_rc != 0) {
                  LOG_DEBUG(
                      "http_wasm_send: Error http_headers_add failed with %d",
                      add_rc);
                }
              }
            }
            if (line_end) {
              p = line_end + 1;
              if (*p == '\n')
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

  emscripten_fetch_close(fetch);

cleanup:
  if (headers) {
    free((void *)headers);
  }
  if (body_buffer) {
    free(body_buffer);
  }

  if (rc == 0) {
    LOG_DEBUG("http_wasm_send: Success");
  } else {
    LOG_DEBUG("http_wasm_send: Error returning %d", rc);
  }
  return rc;
#endif
  return 0;
}
