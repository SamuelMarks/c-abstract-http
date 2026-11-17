/* clang-format off */
#if !defined(_WIN32)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libuv.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_libuv.h"
#else
#include <uv.h>
#endif
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
};

static int libuv_global_init_count = 0;

/**
 * @brief Initialize global libuv environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libuv_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  libuv_global_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global libuv environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libuv_global_cleanup(void) {
  if (libuv_global_init_count > 0) {
    libuv_global_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Create a new libuv-backed transport context. */
enum c_abstract_http_error
http_libuv_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_libuv_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libuv_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_context_init_fail) {
    *ctx = NULL;
  } else
#endif
  {
    *ctx = (struct HttpTransportContext *)malloc(
        sizeof(struct HttpTransportContext));
  }
  if (!*ctx) {
    LOG_DEBUG("http_libuv_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(*ctx, 0, sizeof(struct HttpTransportContext));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_config_init_fail) {
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libuv_context_init: Error http_config_init failed with %d",
              (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_libuv_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free the transport context. */
enum c_abstract_http_error
http_libuv_context_free(struct HttpTransportContext *ctx) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  LOG_DEBUG("http_libuv_context_free: Entering");
  if (ctx) {
    rc = http_config_free(&ctx->config);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return rc;
    }
    free(ctx);
  }
  LOG_DEBUG("http_libuv_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration to the transport context. */
enum c_abstract_http_error
http_libuv_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_libuv_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_libuv_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;

  free(ctx->config.user_agent);
  ctx->config.user_agent = NULL;
  if (config->user_agent) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_url);
  ctx->config.proxy_url = NULL;
  if (config->proxy_url) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_username);
  ctx->config.proxy_username = NULL;
  if (config->proxy_username) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->proxy_username,
                                &ctx->config.proxy_username);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_password);
  ctx->config.proxy_password = NULL;
  if (config->proxy_password) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->proxy_password,
                                &ctx->config.proxy_password);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  LOG_DEBUG("http_libuv_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HttpMethod enum to string.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Pointer to receive string constant.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error get_method_str(enum HttpMethod method,
                                                 const char **out_str) {
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
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Expose get_method_str for testing */
enum c_abstract_http_error
c_abstract_http_test_libuv_method_str(enum HttpMethod method,
                                      const char **out_str);
/** @brief Expose libuv helpers for testing */
enum c_abstract_http_error c_abstract_http_test_libuv_helpers(void);

/**
 * @brief Expose get_method_str for testing.
 *
 * @param[in] method Method enum.
 * @param[out] out_str Output string.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
c_abstract_http_test_libuv_method_str(enum HttpMethod method,
                                      const char **out_str) {
  return get_method_str(method, out_str);
}
#endif

/** @brief Internal struct libuv_state */
struct libuv_state {
  /** @brief Event loop */
  uv_loop_t loop;
  /** @brief TCP socket */
  uv_tcp_t socket;
  /** @brief Connect request */
  uv_connect_t connect_req;
  /** @brief Resolver request */
  uv_getaddrinfo_t resolver;
  /** @brief Timer handle */
  uv_timer_t timer;
  /** @brief Write request */
  uv_write_t write_req;

  /** @brief Transport context */
  struct HttpTransportContext *ctx;
  /** @brief Request pointer */
  const struct HttpRequest *req;
  /** @brief Response double pointer */
  struct HttpResponse **res;

  /** @brief Request buffer */
  char *req_buf;
  /** @brief Request buffer length */
  size_t req_len;
  /** @brief Request write position */
  size_t req_pos;

  /** @brief Response buffer */
  char *res_buf;
  /** @brief Response buffer length */
  size_t res_len;
  /** @brief Response buffer capacity */
  size_t res_cap;

  /** @brief Flag indicating headers have been parsed */
  int headers_parsed;
  /** @brief Error code */
  int error_code;
  /** @brief Done flag */
  int done;
  /** @brief Flag indicating socket is initialized */
  int has_socket;
  /** @brief Flag indicating timer is initialized */
  int has_timer;
};

/**
 * @brief Handle closing callback.
 *
 * @param[in] handle Handle.
 */
static void libuv_on_close(uv_handle_t *handle) {
  if (handle) {
  }
}

#define LIBUV_VOID_CB_DISPATCH(expr)                                           \
  do {                                                                         \
    enum c_abstract_http_error _uv_rc = (expr);                                \
    if (_uv_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("libuv_cb: impl returned %d", (int)_uv_rc);                    \
    }                                                                          \
  } while (0)

/**
 * @brief Finish libuv operation and clean up handles.
 *
 * @param[in,out] state State pointer.
 * @param[in] error Error code.
 * @return Error enum matching error.
 */
static enum c_abstract_http_error
libuv_finish(struct libuv_state *state, enum c_abstract_http_error error) {
  if (state->done) {
    return error;
  }
  state->done = 1;
  state->error_code = (int)error;
  uv_stop(&state->loop);

  LOG_DEBUG("libuv_finish: Finished with error=%d", (int)error);

  if (state->has_timer) {
    uv_timer_stop(&state->timer);
    uv_close((uv_handle_t *)&state->timer, libuv_on_close);
    state->has_timer = 0;
  }

  if (state->has_socket) {
    uv_close((uv_handle_t *)&state->socket, libuv_on_close);
    state->has_socket = 0;
  }
  return error;
}

/**
 * @brief Timeout handler for libuv request.
 *
 * @param[in] handle Timer handle.
 */
static void libuv_on_timeout(uv_timer_t *handle) {
  struct libuv_state *state = (struct libuv_state *)handle->data;
  LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_TIMEOUT));
}

/**
 * @brief Memory allocation callback for incoming libuv data.
 *
 * @param[in] handle Handle.
 * @param[in] suggested_size Suggested allocation size.
 * @param[out] buf Buffer structure to populate.
 */
static void libuv_alloc_cb(uv_handle_t *handle, size_t suggested_size,
                           uv_buf_t *buf) {
  struct libuv_state *state = (struct libuv_state *)handle->data;
  size_t avail;
  if (suggested_size) {
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_alloc_fail == 1 ||
      (g_mock_libuv_alloc_fail == 2 && state->res_len > 0)) {
    buf->base = NULL;
    buf->len = 0;
    return;
  }
#endif

  avail = state->res_cap - state->res_len;

  if (avail < 4096) {
    size_t new_cap = state->res_cap == 0 ? 8192 : state->res_cap * 2;
    char *new_buf;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_alloc_fail == 3) {
      new_buf = NULL;
    } else
#endif
    {
      new_buf = (char *)realloc(state->res_buf, new_cap);
    }
    if (!new_buf) {
      LOG_DEBUG("libuv_alloc_cb: Error ENOMEM reallocating buffer");
      buf->base = NULL;
      buf->len = 0;
      return;
    }
    state->res_buf = new_buf;
    state->res_cap = new_cap;
    avail = state->res_cap - state->res_len;
  }

  buf->base = state->res_buf + state->res_len;
  buf->len = (ULONG)avail;
}

/**
 * @brief Parse response headers from buffer.
 *
 * @param[in,out] state State pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error parse_headers(struct libuv_state *state) {
  struct HttpResponse *r;
  char *p;
  const char *end;
  enum c_abstract_http_error rc;

  if (!state) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (state->headers_parsed) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!state->res_buf) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  p = strstr(state->res_buf, "\r\n\r\n");
  if (!p) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  state->headers_parsed = 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_res_alloc_fail) {
    *state->res = NULL;
  } else
#endif
  {
    *state->res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!*state->res) {
    LOG_DEBUG("parse_headers: Error ENOMEM");
    return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  r = *state->res;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_res_init_fail) {
    free(*state->res);
    *state->res = NULL;
    return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#endif
  rc = http_response_init(r);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("parse_headers: Error http_response_init failed with %d",
              (int)rc);
    free(*state->res);
    *state->res = NULL;
    return libuv_finish(state, rc);
  }

  /* Parse status line */
  {
    const char *space = strchr(state->res_buf, ' ');
    if (space) {
      r->status_code = atoi(space + 1);
    }
  }

  /* Parse headers */
  p = strchr(state->res_buf, '\n') + 1;

  end = strstr(state->res_buf, "\r\n\r\n");
  while (p < end) {
    char *line_end = strchr(p, '\r');
    char *colon;
    *line_end = '\0';
    colon = strchr(p, ':');
    if (colon) {
      char *val = colon + 1;
      enum c_abstract_http_error add_rc;
      *colon = '\0';
      while (*val == ' ') {
        val++;
      }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_headers_realloc_fail) {
        LOG_DEBUG("parse_headers: Mock http_headers_add failure");
      } else
#endif
      {
        add_rc = http_headers_add(&r->headers, p, val);
        if (add_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("parse_headers: Error http_headers_add failed with %d",
                    (int)add_rc);
          return libuv_finish(state, add_rc);
        }
      }
    }
    p = line_end + 2;
  }

  {
    size_t hdr_len = (size_t)(end - state->res_buf) + 4;
    size_t body_len = state->res_len - hdr_len;

    if (state->req->on_chunk) {
      if (body_len > 0) {
        enum c_abstract_http_error chunk_rc;
        chunk_rc = state->req->on_chunk(state->req->on_chunk_user_data,
                                        state->res_buf + hdr_len, body_len);
        if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("parse_headers: Error on_chunk failed %d", (int)chunk_rc);
          LIBUV_VOID_CB_DISPATCH(
              libuv_finish(state, C_ABSTRACT_HTTP_ERR_CANCELED));
          return chunk_rc;
        }
      }
      state->res_len = hdr_len;
    } else {
      r->body_len = body_len;
      if (body_len > 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_libuv_body_alloc_fail) {
          r->body = NULL;
        } else
#endif
        {
          r->body = (char *)malloc(body_len + 1);
        }
        if (!r->body) {
          LOG_DEBUG("parse_headers: Error ENOMEM copying body");
          return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
        }
        memcpy(r->body, state->res_buf + hdr_len, body_len);
        ((char *)r->body)[body_len] = '\0';
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
static enum c_abstract_http_error parse_url(const char *url, char **host,
                                            int *port, char **path);
static void libuv_on_read(uv_stream_t *stream, ssize_t nread,
                          const uv_buf_t *buf);
static int uv_run(uv_loop_t *loop, int mode);

/**
 * @brief Test libuv helper functions and edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
static enum c_abstract_http_error
test_helper_chunk_cb(void *user_data, const void *chunk, size_t chunk_len) {
  if (user_data || chunk || chunk_len) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
test_helper_chunk_cb_err(void *user_data, const void *chunk, size_t chunk_len) {
  if (user_data || chunk || chunk_len) {
  }
  return (enum c_abstract_http_error)ECANCELED;
}

/** @brief Test libuv helper functions and edge cases */
enum c_abstract_http_error c_abstract_http_test_libuv_helpers(void) {
  enum c_abstract_http_error rc;
  uv_timer_t timer;
  struct libuv_state state;
  struct HttpRequest req;
  struct HttpResponse *test_res = NULL;
  char *h = NULL;
  int p = 0;
  char *path = NULL;

  memset(&timer, 0, sizeof(timer));
  memset(&state, 0, sizeof(state));
  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  timer.data = &state;
  libuv_on_timeout(&timer);

  libuv_on_close(NULL);
  uv_loop_close(NULL);
  rc = parse_url(NULL, &h, &p, &path);
  if (rc != C_ABSTRACT_HTTP_ERR_INVAL)
    return rc;
  rc = parse_url("http://127.0.0.1", NULL, &p, &path);
  if (rc != C_ABSTRACT_HTTP_ERR_INVAL)
    return rc;
  rc = parse_url("http://127.0.0.1", &h, NULL, &path);
  if (rc != C_ABSTRACT_HTTP_ERR_INVAL)
    return rc;
  rc = parse_url("http://127.0.0.1", &h, &p, NULL);
  if (rc != C_ABSTRACT_HTTP_ERR_INVAL)
    return rc;
  rc = parse_url("http://127.0.0.1/path:8080", &h, &p, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  free(h);
  h = NULL;
  free(path);
  path = NULL;

  /* parse_headers without end separator */
  {
    char *buf1 = NULL;
    if (c_abstract_http_strdup("HTTP/1.1 200 OK", &buf1) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.req = &req;
    state.res_buf = buf1;
    state.res_len = strlen(buf1);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf1);
      return rc;
    }
    /* Also test parse_headers when headers_parsed == 1 */
    state.headers_parsed = 1;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf1);
      return rc;
    }
    state.headers_parsed = 0;
    free(buf1);
  }

  /* parse_headers with valid headers and body */
  {
    char *buf2 = NULL;
    if (c_abstract_http_strdup(
            "HTTP/1.1 200 OK\r\nKey: Value\r\nNoColon\r\n\r\nBODY", &buf2) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf2;
    state.res_len = strlen(buf2);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf2);
      return rc;
    }
    rc = http_response_free(test_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(test_res);
    test_res = NULL;
    free(buf2);
  }

  /* parse_headers with status line without space */
  {
    char *buf3 = NULL;
    if (c_abstract_http_strdup("HTTP/1.1\r\n\r\n", &buf3) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf3;
    state.res_len = strlen(buf3);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf3);
      return rc;
    }
    rc = http_response_free(test_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(test_res);
    test_res = NULL;
    free(buf3);
  }

  /* parse_headers with on_chunk success and error */
  {
    char *buf4 = NULL;
    if (c_abstract_http_strdup("HTTP/1.1 200 OK\r\n\r\nCHUNK_DATA", &buf4) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    req.on_chunk = test_helper_chunk_cb;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf4;
    state.res_len = strlen(buf4);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf4);
      return rc;
    }
    rc = http_response_free(test_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(test_res);
    test_res = NULL;

    req.on_chunk = test_helper_chunk_cb_err;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_len = strlen(buf4);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc == C_ABSTRACT_HTTP_SUCCESS) {
      free(buf4);
      return rc;
    }
    rc = http_response_free(test_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(test_res);
    test_res = NULL;
    req.on_chunk = NULL;
    free(buf4);
  }

  /* test parse_headers with NULL res_buf */
  state.done = 0;
  state.error_code = 0;
  state.headers_parsed = 0;
  state.res_buf = NULL;
  rc = parse_headers(&state);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* test parse_headers with body alloc fail */
  {
    char *buf5 = NULL;
    if (c_abstract_http_strdup("HTTP/1.1 200 OK\r\n\r\nBODY_DATA", &buf5) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf5;
    state.res_len = strlen(buf5);
    test_res = NULL;
    state.res = &test_res;
    g_mock_libuv_body_alloc_fail = 1;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_ERR_NOMEM) {
      g_mock_libuv_body_alloc_fail = 0;
      free(buf5);
      return rc;
    }
    g_mock_libuv_body_alloc_fail = 0;
    rc = http_response_free(test_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(test_res);
    test_res = NULL;
    free(buf5);
  }

  /* test parse_headers with header add fail */
  {
    char *buf6 = NULL;
    if (c_abstract_http_strdup(
            "HTTP/1.1 200 OK\r\nKey: Val\r\nKey2: Val2\r\n\r\n", &buf6) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf6;
    state.res_len = strlen(buf6);
    test_res = NULL;
    state.res = &test_res;
    g_mock_libuv_headers_realloc_fail = 1;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      g_mock_libuv_headers_realloc_fail = 0;
      free(buf6);
      return rc;
    }
    g_mock_libuv_headers_realloc_fail = 0;
    rc = http_response_free(test_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(test_res);
    test_res = NULL;
    free(buf6);
  }

  /* test parse_headers NULL */
  rc = parse_headers(NULL);
  if (rc != C_ABSTRACT_HTTP_ERR_INVAL)
    return rc;

  /* test libuv_finish double call */
  state.done = 0;
  rc = libuv_finish(&state, C_ABSTRACT_HTTP_SUCCESS);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  rc = libuv_finish(&state, C_ABSTRACT_HTTP_SUCCESS);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* test libuv_on_read edge branches */
  {
    uv_buf_t b;
    memset(&b, 0, sizeof(b));
    state.socket.data = &state;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = NULL;
    libuv_on_read((uv_stream_t *)&state.socket, UV_EOF, &b);

    /* EOF with unparsed headers and alloc fail to trigger parse_headers error
     * in EOF branch */
    state.done = 0;
    if (c_abstract_http_strdup("HTTP/1.1 200 OK\r\n\r\n", &state.res_buf) !=
        C_ABSTRACT_HTTP_SUCCESS)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    test_res = NULL;
    state.res = &test_res;
    g_mock_libuv_res_alloc_fail = 1;
    libuv_on_read((uv_stream_t *)&state.socket, UV_EOF, &b);
    g_mock_libuv_res_alloc_fail = 0;
    state.res = NULL;
    free(state.res_buf);
    state.res_buf = NULL;

    state.done = 0;
    state.res_buf = NULL;
    libuv_on_read((uv_stream_t *)&state.socket, 10, &b);
  }

  /* test parse_url dup_rc fail */
  g_mock_libuv_req_buf_alloc_fail = 3;
  rc = parse_url("http://127.0.0.1/path", &h, &p, &path);
  if (rc != C_ABSTRACT_HTTP_ERR_NOMEM) {
    g_mock_libuv_req_buf_alloc_fail = 0;
    return rc;
  }
  g_mock_libuv_req_buf_alloc_fail = 4;
  rc = parse_url("http://127.0.0.1", &h, &p, &path);
  if (rc != C_ABSTRACT_HTTP_ERR_NOMEM) {
    g_mock_libuv_req_buf_alloc_fail = 0;
    return rc;
  }
  g_mock_libuv_req_buf_alloc_fail = 0;

  /* test uv_run NULL */
  uv_run(NULL, 0);
  {
    uv_loop_t l;
    memset(&l, 0, sizeof(l));
    uv_run(&l, 0);
  }

  /* test alloc_cb realloc branches */
  {
    uv_buf_t ab;
    uv_buf_t ab3;
    state.res_cap = 0;
    state.res_len = 0;
    state.res_buf = NULL;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab);
    state.res_len = 7000;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab);
    free(state.res_buf);
    state.res_buf = NULL;
    state.res_cap = 0;
    state.res_len = 0;
    g_mock_libuv_alloc_fail = 3;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab3);
    g_mock_libuv_alloc_fail = 0;
  }

  /* test uv_run alloc_cb failure branches */
  {
    struct libuv_state st_alloc;
    struct HttpResponse *res_obj = NULL;
    memset(&st_alloc, 0, sizeof(st_alloc));
    uv_loop_init(&st_alloc.loop);
    st_alloc.loop.data = &st_alloc;
    uv_tcp_init(&st_alloc.loop, &st_alloc.socket);
    uv_read_start((uv_stream_t *)&st_alloc.socket, libuv_alloc_cb,
                  libuv_on_read);
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = test_helper_chunk_cb;
    g_mock_libuv_alloc_fail = 1;
    uv_run(&st_alloc.loop, 0);

    memset(&st_alloc, 0, sizeof(st_alloc));
    uv_loop_init(&st_alloc.loop);
    st_alloc.loop.data = &st_alloc;
    uv_tcp_init(&st_alloc.loop, &st_alloc.socket);
    uv_read_start((uv_stream_t *)&st_alloc.socket, libuv_alloc_cb,
                  libuv_on_read);
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = test_helper_chunk_cb;
    g_mock_libuv_alloc_fail = 2;
    uv_run(&st_alloc.loop, 0);
    if (res_obj) {
      rc = http_response_free(res_obj);
      free(res_obj);
      res_obj = NULL;
      if (rc != C_ABSTRACT_HTTP_SUCCESS)
        return rc;
    }
    free(st_alloc.res_buf);

    memset(&st_alloc, 0, sizeof(st_alloc));
    uv_loop_init(&st_alloc.loop);
    st_alloc.loop.data = &st_alloc;
    uv_tcp_init(&st_alloc.loop, &st_alloc.socket);
    uv_read_start((uv_stream_t *)&st_alloc.socket, libuv_alloc_cb,
                  libuv_on_read);
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = NULL;
    g_mock_libuv_alloc_fail = 2;
    uv_run(&st_alloc.loop, 0);
    g_mock_libuv_alloc_fail = 0;
    if (res_obj) {
      rc = http_response_free(res_obj);
      free(res_obj);
      res_obj = NULL;
      if (rc != C_ABSTRACT_HTTP_SUCCESS)
        return rc;
    }
    free(st_alloc.res_buf);

    memset(&st_alloc, 0, sizeof(st_alloc));
    uv_loop_init(&st_alloc.loop);
    st_alloc.loop.data = &st_alloc;
    uv_tcp_init(&st_alloc.loop, &st_alloc.socket);
    uv_read_start((uv_stream_t *)&st_alloc.socket, libuv_alloc_cb,
                  libuv_on_read);
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = test_helper_chunk_cb_err;
    uv_run(&st_alloc.loop, 0);
    req.on_chunk = NULL;
    if (res_obj) {
      rc = http_response_free(res_obj);
      free(res_obj);
      res_obj = NULL;
      if (rc != C_ABSTRACT_HTTP_SUCCESS)
        return rc;
    }
    free(st_alloc.res_buf);

    memset(&st_alloc, 0, sizeof(st_alloc));
    uv_loop_init(&st_alloc.loop);
    st_alloc.loop.data = &st_alloc;
    uv_tcp_init(&st_alloc.loop, &st_alloc.socket);
    uv_read_start((uv_stream_t *)&st_alloc.socket, libuv_alloc_cb,
                  libuv_on_read);
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &test_res;
    st_alloc.req = &req;
    req.on_chunk = test_helper_chunk_cb;
    g_mock_libuv_res_alloc_fail = 1;
    uv_run(&st_alloc.loop, 0);
    g_mock_libuv_res_alloc_fail = 0;
    req.on_chunk = NULL;
    free(st_alloc.res_buf);
  }

  rc = http_request_free(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/* Internal read handler returning enum c_abstract_http_error. */
static enum c_abstract_http_error libuv_on_read_impl(struct libuv_state *state,
                                                     ssize_t nread) {
  if (nread < 0) {
    if (nread == UV_ENOBUFS) {
      return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
    } else if (nread != UV_EOF) {
      LOG_DEBUG("libuv_on_read: Connection error, nread=%ld", (long)nread);
      return libuv_finish(state, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
    } else {
      if (!state->headers_parsed) {
        enum c_abstract_http_error p_rc = parse_headers(state);
        if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("libuv_on_read: parse_headers failed %d", (int)p_rc);
          LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_SUCCESS));
          return p_rc;
        }
      }
      return libuv_finish(state, C_ABSTRACT_HTTP_SUCCESS);
    }
  }

  if (!state->res_buf) {
    return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
  }

  state->res_len += (size_t)nread;

  if (!state->headers_parsed) {
    enum c_abstract_http_error p_rc = parse_headers(state);
    if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("libuv_on_read: parse_headers failed %d", (int)p_rc);
      return p_rc;
    }
  } else if (state->req->on_chunk) {
    enum c_abstract_http_error chunk_rc;
    chunk_rc = state->req->on_chunk(
        state->req->on_chunk_user_data,
        state->res_buf + state->res_len - (size_t)nread, (size_t)nread);
    if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("libuv_on_read: Error on_chunk failed %d", (int)chunk_rc);
      LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_CANCELED));
      return chunk_rc;
    }
    state->res_len -= (size_t)nread;
  } else {
    struct HttpResponse *r = *state->res;
    char *new_body;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_body_alloc_fail || g_mock_libuv_body_realloc_fail) {
      new_body = NULL;
    } else
#endif
    {
      new_body = (char *)realloc(r->body, r->body_len + (size_t)nread + 1);
    }
    if (!new_body) {
      LOG_DEBUG("libuv_on_read: Error ENOMEM reallocating body");
      return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
    }
    memcpy(new_body + r->body_len,
           state->res_buf + state->res_len - (size_t)nread, (size_t)nread);
    r->body = new_body;
    r->body_len += (size_t)nread;
    ((char *)r->body)[r->body_len] = '\0';
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static void libuv_on_read(uv_stream_t *stream, ssize_t nread,
                          const uv_buf_t *buf) {
  struct libuv_state *state = (struct libuv_state *)stream->data;
  if (buf) {
  }
  LIBUV_VOID_CB_DISPATCH(libuv_on_read_impl(state, nread));
}

/**
 * @brief Write completion callback.
 *
 * @param[in] req Write request.
 * @param[in] status Write status.
 */
static void libuv_on_write(uv_write_t *req, int status) {
  struct libuv_state *state = (struct libuv_state *)req->data;
  if (status < 0) {
    LOG_DEBUG("libuv_on_write: Error status=%d", status);
    LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_IO));
    return;
  }

  if (uv_read_start((uv_stream_t *)&state->socket, libuv_alloc_cb,
                    libuv_on_read) < 0) {
    LOG_DEBUG("libuv_on_write: Error uv_read_start failed");
    LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_IO));
  }
}

/**
 * @brief Connection completion callback.
 *
 * @param[in] req Connect request.
 * @param[in] status Status.
 */
static void libuv_on_connect(uv_connect_t *req, int status) {
  struct libuv_state *state = (struct libuv_state *)req->data;
  uv_buf_t buf;

  if (status < 0) {
    LOG_DEBUG("libuv_on_connect: Error status=%d", status);
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_CONNREFUSED));
    return;
  }

  state->write_req.data = state;
  buf = uv_buf_init(state->req_buf, (unsigned int)state->req_len);

  if (uv_write(&state->write_req, (uv_stream_t *)&state->socket, &buf, 1,
               libuv_on_write) < 0) {
    LOG_DEBUG("libuv_on_connect: Error uv_write failed");
    LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_IO));
  }
}

/**
 * @brief Address resolution completion callback.
 *
 * @param[in] resolver Resolver request.
 * @param[in] status Status.
 * @param[in] res Addrinfo list.
 */
static void libuv_on_resolved(uv_getaddrinfo_t *resolver, int status,
                              struct addrinfo *res) {
  struct libuv_state *state = (struct libuv_state *)resolver->data;

  if (status < 0) {
    LOG_DEBUG("libuv_on_resolved: Error status=%d", status);
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_HOSTUNREACH));
    uv_freeaddrinfo(res);
    return;
  }

  state->connect_req.data = state;

  if (uv_tcp_connect(&state->connect_req, &state->socket, res->ai_addr,
                     libuv_on_connect) < 0) {
    LOG_DEBUG("libuv_on_resolved: Error uv_tcp_connect failed");
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_CONNREFUSED));
  }

  uv_freeaddrinfo(res);
}

/**
 * @brief Parse URL into host, port, and path.
 *
 * @param[in] url URL string.
 * @param[out] host Pointer to receive host.
 * @param[out] port Pointer to receive port.
 * @param[out] path Pointer to receive path.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error parse_url(const char *url, char **host,
                                            int *port, char **path) {
  const char *p;
  const char *host_start;
  const char *port_start;
  const char *path_start;
  size_t host_len;
  char *_ast_strdup = NULL;

  if (!url || !host || !port || !path) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (strncmp(url, "http://", 7) == 0) {
    p = url + 7;
    *port = 80;
  } else if (strncmp(url, "https://", 8) == 0) {
    p = url + 8;
    *port = 443;
  } else {
    p = url;
    *port = 80;
  }

  host_start = p;
  path_start = strchr(p, '/');
  port_start = strchr(p, ':');

  if (path_start) {
    if (port_start) {
      if (port_start > path_start) {
        port_start = NULL;
      }
    }
  }

  if (port_start) {
    host_len = (size_t)(port_start - host_start);
    *port = atoi(port_start + 1);
  } else if (path_start) {
    host_len = (size_t)(path_start - host_start);
  } else {
    host_len = strlen(host_start);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_req_buf_alloc_fail == 2) {
    *host = NULL;
  } else
#endif
  {
    *host = (char *)malloc(host_len + 1);
  }
  if (!*host) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strncpy_s(*host, host_len + 1, host_start, host_len);
#else
  strncpy(*host, host_start, host_len);
#endif
  (*host)[host_len] = '\0';

  if (path_start) {
    enum c_abstract_http_error dup_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_req_buf_alloc_fail == 3) {
      dup_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      dup_rc = c_abstract_http_strdup(path_start, &_ast_strdup);
    }
    if (dup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*host);
      *host = NULL;
      return dup_rc;
    }
    *path = _ast_strdup;
  } else {
    enum c_abstract_http_error dup_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_req_buf_alloc_fail == 4) {
      dup_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      dup_rc = c_abstract_http_strdup("/", &_ast_strdup);
    }
    if (dup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*host);
      *host = NULL;
      return dup_rc;
    }
    *path = _ast_strdup;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Send an HTTP request using libuv. */
enum c_abstract_http_error http_libuv_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **res) {
  struct libuv_state state;
  char *host = NULL;
  int port = 0;
  char *path = NULL;
  char port_str[16];
  const char *method_str;
  size_t i;
  size_t req_cap = 4096;
  struct addrinfo hints;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_libuv_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_libuv_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG(
        "http_libuv_send: Error EINVAL (multipart missing flattened body)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(&state, 0, sizeof(state));
  state.ctx = ctx;
  state.req = req;
  state.res = res;
  *res = NULL;

  rc = parse_url(req->url, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libuv_send: Error parse_url failed %d", (int)rc);
    return rc;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(port_str, sizeof(port_str), "%d", port);
#else
  sprintf(port_str, "%d", port);
#endif

  rc = get_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libuv_send: Error get_method_str failed %d", (int)rc);
    free(host);
    free(path);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_req_buf_alloc_fail) {
    state.req_buf = NULL;
  } else
#endif
  {
    state.req_buf = (char *)malloc(req_cap);
  }
  if (!state.req_buf) {
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  state.req_len = (size_t)sprintf_s(
      state.req_buf, req_cap,
      "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n", method_str,
      path, host, port);
#else
  state.req_len = (size_t)sprintf(
      state.req_buf, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n",
      method_str, path, host, port);
#endif

  for (i = 0; i < req->headers.count; i++) {
    size_t hdr_len = strlen(req->headers.headers[i].key) +
                     strlen(req->headers.headers[i].value) + 4;
    if (state.req_len + hdr_len > req_cap) {
      char *new_buf;
      req_cap *= 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_headers_realloc_fail) {
        new_buf = NULL;
      } else
#endif
      {
        new_buf = (char *)realloc(state.req_buf, req_cap);
      }
      if (!new_buf) {
        LOG_DEBUG("http_libuv_send: Error ENOMEM reallocating headers");
        free(host);
        free(path);
        free(state.req_buf);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      state.req_buf = new_buf;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state.req_len += (size_t)sprintf_s(
        state.req_buf + state.req_len, req_cap - state.req_len, "%s: %s\r\n",
        req->headers.headers[i].key, req->headers.headers[i].value);
#else
    state.req_len += (size_t)sprintf(state.req_buf + state.req_len,
                                     "%s: %s\r\n", req->headers.headers[i].key,
                                     req->headers.headers[i].value);
#endif
  }

  if (req->read_chunk && req->expected_body_len > 0) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state.req_len += (size_t)sprintf_s(
        state.req_buf + state.req_len, req_cap - state.req_len,
        "Content-Length: %lu\r\n", (unsigned long)req->expected_body_len);
#else
    state.req_len += (size_t)sprintf(state.req_buf + state.req_len,
                                     "Content-Length: %lu\r\n",
                                     (unsigned long)req->expected_body_len);
#endif
  } else if (req->body_len > 0) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state.req_len += (size_t)sprintf_s(
        state.req_buf + state.req_len, req_cap - state.req_len,
        "Content-Length: %lu\r\n", (unsigned long)req->body_len);
#else
    state.req_len += (size_t)sprintf(state.req_buf + state.req_len,
                                     "Content-Length: %lu\r\n",
                                     (unsigned long)req->body_len);
#endif
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  state.req_len += (size_t)sprintf_s(state.req_buf + state.req_len,
                                     req_cap - state.req_len, "\r\n");
#else
  state.req_len += (size_t)sprintf(state.req_buf + state.req_len, "\r\n");
#endif

  if (req->body && req->body_len > 0) {
    if (state.req_len + req->body_len > req_cap) {
      char *new_buf;
      req_cap = state.req_len + req->body_len + 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_body_alloc_fail) {
        new_buf = NULL;
      } else
#endif
      {
        new_buf = (char *)realloc(state.req_buf, req_cap);
      }
      if (!new_buf) {
        LOG_DEBUG("http_libuv_send: Error ENOMEM reallocating for body");
        free(host);
        free(path);
        free(state.req_buf);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      state.req_buf = new_buf;
    }
    memcpy(state.req_buf + state.req_len, req->body, req->body_len);
    state.req_len += req->body_len;
  } else if (req->read_chunk) {
    size_t out_read;
    char chunk_buf[4096];
    for (;;) {
      enum c_abstract_http_error chunk_rc = req->read_chunk(
          req->read_chunk_user_data, chunk_buf, sizeof(chunk_buf), &out_read);
      if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(host);
        free(path);
        free(state.req_buf);
        return chunk_rc;
      }
      if (out_read == 0) {
        break;
      }
      if (state.req_len + out_read > req_cap) {
        char *new_buf;
        req_cap = (state.req_len + out_read) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_libuv_body_realloc_fail) {
          new_buf = NULL;
        } else
#endif
        {
          new_buf = (char *)realloc(state.req_buf, req_cap);
        }
        if (!new_buf) {
          LOG_DEBUG("http_libuv_send: Error ENOMEM chunk accumulation");
          free(host);
          free(path);
          free(state.req_buf);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        state.req_buf = new_buf;
      }
      memcpy(state.req_buf + state.req_len, chunk_buf, out_read);
      state.req_len += out_read;
    }
  }

  if (uv_loop_init(&state.loop) < 0) {
    state.error_code = ENOMEM;
    goto cleanup;
  }
  state.loop.data = &state;
  if (uv_tcp_init(&state.loop, &state.socket) < 0) {
    state.error_code = ENOMEM;
    goto cleanup_loop;
  }
  state.has_socket = 1;
  state.socket.data = &state;

  if (ctx->config.timeout_ms > 0) {
    if (uv_timer_init(&state.loop, &state.timer) < 0) {
      state.error_code = ENOMEM;
      goto cleanup_loop;
    }
    state.has_timer = 1;
    state.timer.data = &state;
    if (uv_timer_start(&state.timer, libuv_on_timeout,
                       (uint64_t)ctx->config.timeout_ms, 0) < 0) {
      state.error_code = ENOMEM;
      goto cleanup_loop;
    }
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  state.resolver.data = &state;
  if (uv_getaddrinfo(&state.loop, &state.resolver, libuv_on_resolved, host,
                     port_str, &hints) < 0) {
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(&state, C_ABSTRACT_HTTP_ERR_HOSTUNREACH));
  }

  uv_run(&state.loop, UV_RUN_DEFAULT);

cleanup_loop:
  uv_loop_close(&state.loop);

cleanup:
  free(host);
  free(path);
  free(state.req_buf);
  free(state.res_buf);

  if (state.error_code != 0) {
    if (*res) {
      enum c_abstract_http_error free_rc = http_response_free(*res);
      free(*res);
      *res = NULL;
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return free_rc;
      }
    }
    LOG_DEBUG("http_libuv_send: Error returning %d", state.error_code);
    return (enum c_abstract_http_error)state.error_code;
  }

  LOG_DEBUG("http_libuv_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
