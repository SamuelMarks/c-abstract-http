/**
 * @file http_libevent.c
 * @brief libevent2 evhttp transport backend implementation of the Abstract
 * Network Interface (ANI).
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libevent.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_libevent.h"
#else
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
};

/** @brief Reference count of global libevent initializations */
static int libevent_global_init_count = 0;

/**
 * @brief Initialize the global libevent environment safely.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libevent_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  libevent_global_init_count++;
#ifdef _WIN32
  if (libevent_global_init_count == 1) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up the global libevent environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libevent_global_cleanup(void) {
  if (libevent_global_init_count > 0) {
    libevent_global_init_count--;
#ifdef _WIN32
    if (libevent_global_init_count == 0) {
      WSACleanup();
    }
#endif
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Create a new libevent-backed transport context. */
enum c_abstract_http_error
http_libevent_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_libevent_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libevent_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_context_init_fail) {
    *ctx = NULL;
  } else
#endif
  {
    *ctx = (struct HttpTransportContext *)malloc(
        sizeof(struct HttpTransportContext));
  }
  if (!*ctx) {
    LOG_DEBUG("http_libevent_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(*ctx, 0, sizeof(struct HttpTransportContext));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_config_init_fail) {
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_libevent_context_init: Error http_config_init failed with %d",
        (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_libevent_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free the transport context. */
enum c_abstract_http_error
http_libevent_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libevent_context_free: Entering");
  if (ctx) {
    enum c_abstract_http_error rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    free(ctx);
  }
  LOG_DEBUG("http_libevent_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration settings to the transport context. */
enum c_abstract_http_error
http_libevent_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  enum c_abstract_http_error rc;
  if (!ctx || !config) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;

  if (config->user_agent) {
    if (ctx->config.user_agent) {
      free(ctx->config.user_agent);
      ctx->config.user_agent = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.user_agent) {
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
  }

  if (config->proxy_url) {
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_url) {
    free(ctx->config.proxy_url);
    ctx->config.proxy_url = NULL;
  }

  if (config->proxy_username) {
    if (ctx->config.proxy_username) {
      free(ctx->config.proxy_username);
      ctx->config.proxy_username = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_username,
                                  &ctx->config.proxy_username);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_username) {
    free(ctx->config.proxy_username);
    ctx->config.proxy_username = NULL;
  }

  if (config->proxy_password) {
    if (ctx->config.proxy_password) {
      free(ctx->config.proxy_password);
      ctx->config.proxy_password = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_password,
                                  &ctx->config.proxy_password);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_password) {
    free(ctx->config.proxy_password);
    ctx->config.proxy_password = NULL;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HttpMethod enum to libevent request command.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_cmd Pointer to receive libevent method command.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error get_method_cmd(enum HttpMethod method,
                                                 int *out_cmd) {
  if (!out_cmd) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (method) {
  case HTTP_GET:
    *out_cmd = EVHTTP_REQ_GET;
    break;
  case HTTP_POST:
    *out_cmd = EVHTTP_REQ_POST;
    break;
  case HTTP_PUT:
    *out_cmd = EVHTTP_REQ_PUT;
    break;
  case HTTP_DELETE:
    *out_cmd = EVHTTP_REQ_DELETE;
    break;
  case HTTP_PATCH:
    *out_cmd = EVHTTP_REQ_PATCH;
    break;
  case HTTP_HEAD:
    *out_cmd = EVHTTP_REQ_HEAD;
    break;
  case HTTP_OPTIONS:
    *out_cmd = EVHTTP_REQ_OPTIONS;
    break;
  case HTTP_TRACE:
    *out_cmd = EVHTTP_REQ_TRACE;
    break;
  case HTTP_CONNECT:
    *out_cmd = EVHTTP_REQ_CONNECT;
    break;
  default:
    *out_cmd = EVHTTP_REQ_GET;
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief No-op chunked callback used in libevent mock tests.
 * @param req Mock evhttp request.
 * @param arg User context pointer.
 */
static void dummy_libevent_chunk_cb(struct evhttp_request *req, void *arg) {
  (void)req;
  (void)arg;
}

/** @brief Test helper for libevent method command mapping */
enum c_abstract_http_error
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd);
/** @brief Test helper for libevent evbuffer operations */
enum c_abstract_http_error c_abstract_http_test_libevent_evbuffer(void);
/** @brief Test helper for libevent header edge cases */
enum c_abstract_http_error c_abstract_http_test_libevent_add_header_edge(void);

/**
 * @brief Expose get_method_cmd for testing.
 *
 * @param[in] method HTTP method.
 * @param[out] out_cmd Output command.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd) {
  return get_method_cmd(method, out_cmd);
}

/**
 * @brief Test evbuffer edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
enum c_abstract_http_error c_abstract_http_test_libevent_evbuffer(void) {
  struct evbuffer buf;
  struct evbuffer buf2;
  char out[16];
  memset(&buf, 0, sizeof(buf));
  memset(&buf2, 0, sizeof(buf2));
  evbuffer_add(NULL, "a", 1);
  evbuffer_add(&buf, NULL, 1);
  evbuffer_add(&buf, "a", 0);
  evbuffer_remove(NULL, out, 1);
  evbuffer_remove(&buf, NULL, 1);
  evbuffer_remove(&buf, out, 0);
  evbuffer_remove(&buf, out, 1);
  evbuffer_get_length(NULL);

  evbuffer_add(&buf, "hello", 5);
  evbuffer_add(&buf,
               "12345678901234567890123456789012345678901234567890123456789012"
               "34567890123456789012345678901234567890123456789012345678901234"
               "567890",
               130);
  evbuffer_remove(&buf, out, 2);
  evbuffer_remove(&buf, out, 10);
  evbuffer_free_internal(&buf);

  evbuffer_add(&buf2,
               "12345678901234567890123456789012345678901234567890123456789012"
               "34567890123456789012345678901234567890123456789012345678901234"
               "567890",
               130);
  evbuffer_free_internal(&buf2);

  evhttp_make_request(NULL, NULL, 0, NULL);
  event_base_dispatch(NULL);

  event_base_free(NULL);
  event_base_loopbreak(NULL);

  {
    struct event_base base_dummy;
    struct evhttp_connection *c;
    memset(&base_dummy, 0, sizeof(base_dummy));
    c = evhttp_connection_base_new(&base_dummy, NULL, NULL, 80);
    evhttp_connection_free(c);
  }

  evhttp_connection_set_timeout(NULL, 10);
  evhttp_connection_free(NULL);

  {
    struct evhttp_request *r = (struct evhttp_request *)calloc(1, sizeof(*r));
    r->output_headers.count = 1;
    r->output_headers.entries[0].key = NULL;
    r->output_headers.entries[0].val = NULL;
    evhttp_request_free(r);
  }
  evhttp_request_free(NULL);
  evhttp_request_set_chunked_cb(NULL, NULL);
  evhttp_request_get_response_code(NULL);

  {
    struct event_base base_test;
    struct evhttp_connection conn_test;
    struct evhttp_request req_test;

    memset(&base_test, 0, sizeof(base_test));
    memset(&conn_test, 0, sizeof(conn_test));
    memset(&req_test, 0, sizeof(req_test));

    conn_test.base = &base_test;
    evhttp_make_request(&conn_test, &req_test, 0, NULL);
    evhttp_make_request(&conn_test, NULL, 0, NULL);
    conn_test.base = NULL;
    evhttp_make_request(&conn_test, &req_test, 0, NULL);
    conn_test.base = &base_test;

    base_test.req = NULL;
    event_base_dispatch(&base_test);

    base_test.req = &req_test;
    base_test.conn = NULL;
    base_test.loop_break = 1;
    req_test.chunk_cb = dummy_libevent_chunk_cb;
    event_base_dispatch(&base_test);

    base_test.conn = &conn_test;
    conn_test.port = 80;
    base_test.loop_break = 0;
    req_test.chunk_cb = NULL;
    event_base_dispatch(&base_test);

    conn_test.port = 59999;
    event_base_dispatch(&base_test);
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Test evhttp_add_header edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
enum c_abstract_http_error c_abstract_http_test_libevent_add_header_edge(void) {
  struct evkeyvalq q;
  size_t i;
  memset(&q, 0, sizeof(q));
  evhttp_add_header(NULL, "k", "v");
  evhttp_add_header(&q, NULL, "v");
  evhttp_add_header(&q, "k", NULL);
  g_mock_libevent_res_init_fail = 1;
  evhttp_add_header(&q, "k", "v");
  g_mock_libevent_res_init_fail = 2;
  evhttp_add_header(&q, "k", "v");
  g_mock_libevent_res_init_fail = 0;
  for (i = 0; i < 32; i++) {
    evhttp_add_header(&q, "k", "v");
  }
  evhttp_add_header(&q, "k", "v");
  for (i = 0; i < q.count; i++) {
    free(q.entries[i].key);
    free(q.entries[i].val);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/** @brief Internal struct libevent_state */
struct libevent_state {
  /** @brief Event base */
  struct event_base *base;
  /** @brief evhttp connection */
  struct evhttp_connection *conn;
  /** @brief Request pointer */
  const struct HttpRequest *req;
  /** @brief Response double pointer */
  struct HttpResponse **res;
  /** @brief Error code captured during execution */
  int error_code;
  /** @brief Flag indicating completion */
  int done;
};

/**
 * @brief Initialize allocated HttpResponse inside libevent completion handler.
 * @param state Active libevent request state.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
static enum c_abstract_http_error
http_request_done_init_res(struct libevent_state *state) {
  enum c_abstract_http_error rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_res_init_fail) {
    free(*state->res);
    *state->res = NULL;
    state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(*state->res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_request_done: Error http_response_init failed with %d",
              (int)rc);
    free(*state->res);
    *state->res = NULL;
    state->error_code = (int)rc;
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Execute an enum-returning helper inside a void libevent callback.
 * @param expr Expression returning enum c_abstract_http_error.
 */
#define LIBEVENT_VOID_CB_DISPATCH(expr)                                        \
  do {                                                                         \
    enum c_abstract_http_error _ev_rc = (expr);                                \
    if (_ev_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("libevent callback returned %d", (int)_ev_rc);                 \
    }                                                                          \
  } while (0)

/**
 * @brief libevent completion callback invoked when an HTTP request finishes.
 * @param req_ev Completed evhttp_request pointer (or NULL on failure).
 * @param arg Pointer to struct libevent_state.
 */
static void http_request_done(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state;
  state = (struct libevent_state *)arg;
  state->done = 1;

  if (state->error_code != 0) {
    LOG_DEBUG("http_request_done: Aborted with error %d", state->error_code);
    return;
  }

  if (!req_ev) {
    LOG_DEBUG("http_request_done: Request failed (req_ev NULL)");
    state->error_code = ECONNREFUSED;
    return;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_res_alloc_fail) {
    *state->res = NULL;
  } else
#endif
  {
    *state->res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!*state->res) {
    LOG_DEBUG("http_request_done: Error ENOMEM allocating response");
    state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
    return;
  }

  LIBEVENT_VOID_CB_DISPATCH(http_request_done_init_res(state));
  if (state->error_code != 0) {
    return;
  }

  (*state->res)->status_code = evhttp_request_get_response_code(req_ev);

  {
    struct evbuffer *evb = evhttp_request_get_input_buffer(req_ev);
    size_t len = evbuffer_get_length(evb);
    if (len > 0) {
      char *body_char;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libevent_body_alloc_fail) {
        body_char = NULL;
      } else
#endif
      {
        body_char = (char *)malloc(len + 1);
      }
      if (body_char) {
        evbuffer_remove(evb, body_char, len);
        body_char[len] = '\0';
        (*state->res)->body = body_char;
        (*state->res)->body_len = len;
      } else {
        LOG_DEBUG("http_request_done: Error ENOMEM allocating body");
        state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
  }
}

/**
 * @brief Drain chunked evbuffer bytes and forward them to req->on_chunk.
 * @param req_ev Active evhttp_request pointer.
 * @param state Active libevent request state.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
static enum c_abstract_http_error
http_chunked_cb_impl(struct evhttp_request *req_ev,
                     struct libevent_state *state) {
  struct evbuffer *evb = evhttp_request_get_input_buffer(req_ev);
  size_t len = evbuffer_get_length(evb);

  if (len > 0 && state->req->on_chunk) {
    char *buf;
    enum c_abstract_http_error cb_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_body_alloc_fail) {
      buf = NULL;
    } else
#endif
    {
      buf = (char *)malloc(len);
    }
    if (!buf) {
      state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
      event_base_loopbreak(state->base);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    evbuffer_remove(evb, buf, len);
    cb_rc = state->req->on_chunk(state->req->on_chunk_user_data, buf, len);
    if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf);
      state->error_code = (int)cb_rc;
      event_base_loopbreak(state->base);
      return cb_rc;
    }
    free(buf);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief libevent chunked transfer callback trampoline.
 * @param req_ev Active evhttp_request pointer.
 * @param arg Pointer to struct libevent_state.
 */
static void http_chunked_cb(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state = (struct libevent_state *)arg;
  LIBEVENT_VOID_CB_DISPATCH(http_chunked_cb_impl(req_ev, state));
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Expose http_chunked_cb testing helper.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
enum c_abstract_http_error c_abstract_http_test_libevent_chunked_cb(void);

enum c_abstract_http_error c_abstract_http_test_libevent_chunked_cb(void) {
  struct libevent_state dummy_st;
  struct HttpRequest dummy_req;
  struct evhttp_request dummy_req_ev;

  memset(&dummy_st, 0, sizeof(dummy_st));
  memset(&dummy_req, 0, sizeof(dummy_req));
  memset(&dummy_req_ev, 0, sizeof(dummy_req_ev));

  dummy_st.req = &dummy_req;
  http_chunked_cb(&dummy_req_ev, &dummy_st);

  evbuffer_add(&dummy_req_ev.input_buffer, "x", 1);
  http_chunked_cb(&dummy_req_ev, &dummy_st);
  evbuffer_free_internal(&dummy_req_ev.input_buffer);

  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/* Send an HTTP request using libevent. */
enum c_abstract_http_error http_libevent_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  struct libevent_state state;
  struct evhttp_request *req_ev;
  char host[256];
  int port;
  const char *p;
  const char *path;
  struct evkeyvalq *output_headers;
  enum c_abstract_http_error rc;
  int method_cmd;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_libevent_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_libevent_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(&state, 0, sizeof(state));
  memset(host, 0, sizeof(host));
  port = 80;
  path = "/";
  state.req = req;
  state.res = res;
  state.error_code = 0;

  if (strncmp(req->url, "http://", 7) == 0) {
    p = req->url + 7;
  } else {
    p = req->url;
  }

  {
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
      size_t len = (size_t)(colon - p);
      if (len < sizeof(host)) {
        memcpy(host, p, len);
        host[len] = '\0';
      }
      port = atoi(colon + 1);
      if (slash) {
        path = slash;
      }
    } else if (slash) {
      size_t len = (size_t)(slash - p);
      if (len < sizeof(host)) {
        memcpy(host, p, len);
        host[len] = '\0';
      }
      path = slash;
    } else {
      size_t len = strlen(p);
      if (len < sizeof(host)) {
        memcpy(host, p, len);
        host[len] = '\0';
      }
    }
  }

  state.base = event_base_new();
  if (!state.base) {
    LOG_DEBUG("http_libevent_send: Error ENOMEM (event_base_new failed)");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  state.conn =
      evhttp_connection_base_new(state.base, NULL, host, (unsigned short)port);
  if (!state.conn) {
    LOG_DEBUG(
        "http_libevent_send: Error ENOMEM (evhttp_connection_base_new failed)");
    event_base_free(state.base);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (ctx->config.timeout_ms > 0) {
    evhttp_connection_set_timeout(state.conn,
                                  (int)(ctx->config.timeout_ms / 1000));
  }

  req_ev = evhttp_request_new(http_request_done, &state);
  if (!req_ev) {
    LOG_DEBUG("http_libevent_send: Error ENOMEM (evhttp_request_new failed)");
    evhttp_connection_free(state.conn);
    event_base_free(state.base);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (req->on_chunk) {
    evhttp_request_set_chunked_cb(req_ev, http_chunked_cb);
  }

  output_headers = evhttp_request_get_output_headers(req_ev);
  if (evhttp_add_header(output_headers, "Host", host) != 0) {
    LOG_DEBUG("http_libevent_send: Error evhttp_add_header failed");
  }
  if (evhttp_add_header(output_headers, "Connection", "close") != 0) {
    LOG_DEBUG("http_libevent_send: Error evhttp_add_header failed");
  }

  if (req->headers.count > 0) {
    size_t i;
    for (i = 0; i < req->headers.count; i++) {
      if (evhttp_add_header(output_headers, req->headers.headers[i].key,
                            req->headers.headers[i].value) != 0) {
        LOG_DEBUG("http_libevent_send: Error evhttp_add_header failed for "
                  "custom header");
      }
    }
  }

  if (req->body && req->body_len > 0) {
    struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
    if (evbuffer_add(evb, req->body, req->body_len) != 0) {
      LOG_DEBUG("http_libevent_send: Error evbuffer_add failed");
    }
  } else if (req->read_chunk) {
    struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
    char buf[4096];
    size_t read_bytes = 0;
    for (;;) {
      enum c_abstract_http_error r = req->read_chunk(
          req->read_chunk_user_data, buf, sizeof(buf), &read_bytes);
      if (r != C_ABSTRACT_HTTP_SUCCESS) {
        evhttp_request_free(req_ev);
        evhttp_connection_free(state.conn);
        event_base_free(state.base);
        return r;
      }
      if (read_bytes == 0) {
        break;
      }
      if (evbuffer_add(evb, buf, read_bytes) != 0) {
        LOG_DEBUG("http_libevent_send: Error evbuffer_add failed during "
                  "chunk read");
      }
    }
  }

  rc = get_method_cmd(req->method, &method_cmd);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    evhttp_connection_free(state.conn);
    event_base_free(state.base);
    return rc;
  }
  if (evhttp_make_request(state.conn, req_ev, method_cmd, path) != 0) {
    LOG_DEBUG("http_libevent_send: Error evhttp_make_request failed");
#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
    evhttp_request_free(req_ev);
#endif
    evhttp_connection_free(state.conn);
    event_base_free(state.base);
    return (enum c_abstract_http_error)ECONNREFUSED;
  }

  event_base_dispatch(state.base);

  if (state.error_code != 0) {
    rc = (enum c_abstract_http_error)state.error_code;
  } else if (!*res) {
    rc = (enum c_abstract_http_error)ECONNREFUSED;
  } else {
    rc = C_ABSTRACT_HTTP_SUCCESS;
  }

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
  evhttp_request_free(req_ev);
#endif
  evhttp_connection_free(state.conn);
  event_base_free(state.base);

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libevent_send: Success");
  } else {
    LOG_DEBUG("http_libevent_send: Error returning %d", (int)rc);
  }
  return rc;
}
