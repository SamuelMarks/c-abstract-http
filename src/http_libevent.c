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
#include "crypto_utils.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_libevent.h"
#else
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>
#include <event2/bufferevent.h>
#if defined(C_ABSTRACT_HTTP_USE_OPENSSL) || defined(C_ABSTRACT_HTTP_USE_LIBRESSL) || defined(C_ABSTRACT_HTTP_USE_BORINGSSL)
#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>
/** @brief Indicator that libevent OpenSSL bufferevent TLS support is enabled */
#define CAH_LIBEVENT_HAVE_OPENSSL 1
#endif
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
NO_DISCARD c_abstract_http_error_t http_libevent_global_init(void) {
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
NO_DISCARD c_abstract_http_error_t http_libevent_global_cleanup(void) {
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
NO_DISCARD c_abstract_http_error_t
http_libevent_context_init(struct HttpTransportContext **ctx) {
  c_abstract_http_error_t rc;
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
NO_DISCARD c_abstract_http_error_t
http_libevent_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libevent_context_free: Entering");
  if (ctx) {
    c_abstract_http_error_t rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return rch;
    }
    free(ctx);
  }
  LOG_DEBUG("http_libevent_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration settings to the transport context. */
NO_DISCARD c_abstract_http_error_t http_libevent_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  c_abstract_http_error_t rc;
  if (!ctx || !config) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;
  ctx->config.tls_version_mask = config->tls_version_mask;

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
NO_DISCARD static c_abstract_http_error_t get_method_cmd(enum HttpMethod method,
                                                         int *out_cmd) {
  if (!out_cmd || (int)method < 0) {
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
/** @brief Test helper for libevent method command mapping */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd);
/** @brief Test helper for libevent evbuffer operations */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_libevent_evbuffer(void);
/** @brief Test helper for libevent header edge cases */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_add_header_edge(void);

/**
 * @brief Expose get_method_cmd for testing.
 *
 * @param[in] method HTTP method.
 * @param[out] out_cmd Output command.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd) {
  return get_method_cmd(method, out_cmd);
}

/**
 * @brief Test evbuffer edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_evbuffer(void) {
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
    if (r) {
      r->output_headers.count = 1;
      r->output_headers.entries[0].key = NULL;
      r->output_headers.entries[0].val = NULL;
      evhttp_request_free(r);
    }
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
    req_test.chunk_cb = NULL;
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
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_add_header_edge(void) {
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
NO_DISCARD static c_abstract_http_error_t
http_request_done_init_res(struct libevent_state *state) {
  c_abstract_http_error_t rc;
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
 * @param expr Expression returning c_abstract_http_error_t.
 */
#define LIBEVENT_VOID_CB_DISPATCH(expr)                                        \
  do {                                                                         \
    c_abstract_http_error_t _ev_rc = (expr);                                   \
    if (_ev_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("libevent callback returned %d", (int)_ev_rc);                 \
    }                                                                          \
  } while (0)

/**
 * @brief Populate response status, headers, and body on request completion.
 * @param req_ev Completed evhttp_request pointer (or NULL on failure).
 * @param state Active libevent request state.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD static c_abstract_http_error_t
http_request_done_impl(struct evhttp_request *req_ev,
                       struct libevent_state *state) {
  c_abstract_http_error_t rc;
  state->done = 1;

  if (state->error_code != 0) {
    LOG_DEBUG("http_request_done: Aborted with error %d", state->error_code);
    return (c_abstract_http_error_t)state->error_code;
  }

  if (!req_ev) {
    LOG_DEBUG("http_request_done: Request failed (req_ev NULL)");
    state->error_code = ECONNREFUSED;
    return (c_abstract_http_error_t)ECONNREFUSED;
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
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_request_done_init_res(state);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  (*state->res)->status_code = evhttp_request_get_response_code(req_ev);

  {
    struct evkeyvalq *in_hdrs = evhttp_request_get_input_headers(req_ev);
#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
    if (in_hdrs) {
      size_t hi;
      for (hi = 0; hi < in_hdrs->count; ++hi) {
        c_abstract_http_error_t hrc =
            http_headers_add(&(*state->res)->headers, in_hdrs->entries[hi].key,
                             in_hdrs->entries[hi].val);
        if (hrc != C_ABSTRACT_HTTP_SUCCESS) {
          state->error_code = (int)hrc;
          return hrc;
        }
      }
    }
#else
    if (in_hdrs) {
      struct evkeyval *kv;
      for (kv = in_hdrs->tqh_first; kv; kv = kv->next.tqe_next) {
        c_abstract_http_error_t hrc =
            http_headers_add(&(*state->res)->headers, kv->key, kv->value);
        if (hrc != C_ABSTRACT_HTTP_SUCCESS) {
          state->error_code = (int)hrc;
          return hrc;
        }
      }
    }
#endif
  }

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
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief libevent completion callback invoked when an HTTP request finishes.
 * @param req_ev Completed evhttp_request pointer (or NULL on failure).
 * @param arg Pointer to struct libevent_state.
 */
static void http_request_done(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state = (struct libevent_state *)arg;
  LIBEVENT_VOID_CB_DISPATCH(http_request_done_impl(req_ev, state));
}

/**
 * @brief Drain chunked evbuffer bytes and forward them to req->on_chunk.
 * @param req_ev Active evhttp_request pointer.
 * @param state Active libevent request state.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD static c_abstract_http_error_t
http_chunked_cb_impl(struct evhttp_request *req_ev,
                     struct libevent_state *state) {
  struct evbuffer *evb = evhttp_request_get_input_buffer(req_ev);
  size_t len = evbuffer_get_length(evb);

  if (len > 0 && state->req->on_chunk) {
    char *buf;
    c_abstract_http_error_t cb_rc;
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
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_chunked_cb(void);

NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_chunked_cb(void) {
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

/** @brief Maximum automatic redirect hops in libevent backend */
#define LIBEVENT_MAX_REDIRECTS 10

/**
 * @brief Case-insensitive ASCII string comparison for header keys.
 * @param a First string.
 * @param b Second string.
 * @return 0 when equal ignoring ASCII case, non-zero otherwise.
 */
static int math_ev_strcasecmp(const char *a, const char *b) {
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
 * @brief Parse URL string into host buffer, port, path pointer, and HTTPS flag.
 * @param url Input URL string.
 * @param host Output host buffer (`host_cap` bytes).
 * @param host_cap Capacity of @p host.
 * @param out_port Output port number.
 * @param out_path Output path string pointer inside @p url or `"/"`.
 * @param out_https Set to 1 if `https://`, 0 otherwise.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
ev_parse_url_parts(const char *url, char *host, size_t host_cap, int *out_port,
                   const char **out_path, int *out_https) {
  const char *p;
  const char *colon;
  const char *slash;

  if (!url || url[0] == '\0') {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  memset(host, 0, host_cap);
  *out_port = 80;
  *out_path = "/";
  *out_https = 0;

  if (strncmp(url, "https://", 8) == 0) {
    p = url + 8;
    *out_port = 443;
    *out_https = 1;
  } else if (strncmp(url, "http://", 7) == 0) {
    p = url + 7;
  } else {
    p = url;
  }

  colon = strchr(p, ':');
  slash = strchr(p, '/');
  if (colon && (!slash || colon < slash)) {
    size_t len = (size_t)(colon - p);
    if (len < host_cap) {
      memcpy(host, p, len);
      host[len] = '\0';
    }
    *out_port = atoi(colon + 1);
    if (slash) {
      *out_path = slash;
    }
  } else if (slash) {
    size_t len = (size_t)(slash - p);
    if (len < host_cap) {
      memcpy(host, p, len);
      host[len] = '\0';
    }
    *out_path = slash;
  } else {
    size_t len = strlen(p);
    if (len < host_cap) {
      memcpy(host, p, len);
      host[len] = '\0';
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Build Basic Proxy-Authorization header value ("Basic <base64>").
 * @param user Proxy username.
 * @param pass Optional proxy password.
 * @param out_val Newly allocated header string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
ev_build_proxy_auth(const char *user, const char *pass, char **out_val) {
  const char *pw = pass ? pass : "";
  size_t ulen = strlen(user);
  size_t plen = strlen(pw);
  char *raw;
  char *b64 = NULL;
  size_t b64_len = 0;
  char *hdr;
  c_abstract_http_error_t rc;

  raw = (char *)malloc(ulen + 1 + plen + 1);
  if (!raw) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(raw, user, ulen);
  raw[ulen] = ':';
  memcpy(raw + ulen + 1, pw, plen);
  raw[ulen + 1 + plen] = '\0';
  rc = base64_encode((const unsigned char *)raw, ulen + 1 + plen, &b64,
                     &b64_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(raw);
    return rc;
  }
  free(raw);
  hdr = (char *)malloc(6 + b64_len + 1);
  if (!hdr) {
    free(b64);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(hdr, "Basic ", 6);
  memcpy(hdr + 6, b64, b64_len + 1);
  free(b64);
  *out_val = hdr;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Create an `evhttp_connection` (plaintext or TLS bufferevent).
 * @param ctx Transport context.
 * @param base Active `event_base`.
 * @param dial_host Target host or proxy host.
 * @param dial_port Target port or proxy port.
 * @param is_https Non-zero when TLS `bufferevent_openssl_socket_new` is needed.
 * @param sni_host Origin SNI hostname.
 * @param out_conn Output connection handle.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ev_open_connection(const struct HttpTransportContext *ctx,
                   struct event_base *base, const char *dial_host,
                   int dial_port, int is_https, const char *sni_host,
                   struct evhttp_connection **out_conn) {
  struct evhttp_connection *conn = NULL;
  (void)sni_host;

  if (is_https) {
    if (ctx->config.tls_version_mask != 0 &&
        (ctx->config.tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) ==
            0) {
      return C_ABSTRACT_HTTP_ERR_NOTSUP;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
    {
      struct bufferevent *bev = bufferevent_openssl_socket_new(
          base, -1, NULL, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE);
      if (!bev) {
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      conn = evhttp_connection_base_bufferevent_new(base, NULL, bev, dial_host,
                                                    (unsigned short)dial_port);
      if (!conn) {
        bufferevent_free(bev);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
#elif defined(CAH_LIBEVENT_HAVE_OPENSSL)
    {
      SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_client_method());
      SSL *ssl;
      struct bufferevent *bev;
      if (!ssl_ctx) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      SSL_CTX_set_verify(
          ssl_ctx, ctx->config.verify_peer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
          NULL);
      ssl = SSL_new(ssl_ctx);
      SSL_CTX_free(ssl_ctx);
      if (!ssl) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      if (sni_host && sni_host[0] != '\0') {
        if (SSL_set_tlsext_host_name(ssl, sni_host) != 1) {
          LOG_DEBUG("ev_open_connection: SSL_set_tlsext_host_name failed");
        }
      }
      bev = bufferevent_openssl_socket_new(
          base, -1, ssl, BUFFEREVENT_SSL_CONNECTING, BEV_OPT_CLOSE_ON_FREE);
      if (!bev) {
        SSL_free(ssl);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      conn = evhttp_connection_base_bufferevent_new(base, NULL, bev, dial_host,
                                                    (unsigned short)dial_port);
      if (!conn) {
        bufferevent_free(bev);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
#else
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
#endif
  } else {
    conn = evhttp_connection_base_new(base, NULL, dial_host,
                                      (unsigned short)dial_port);
    if (!conn) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }

  if (ctx->config.timeout_ms > 0) {
    int tsec = (int)((ctx->config.timeout_ms + 999L) / 1000L);
    evhttp_connection_set_timeout(conn, tsec);
  }
  *out_conn = conn;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Submit an `HttpRequest` onto an `event_base` (`state->conn` +
 * `req_ev`).
 * @param ctx Transport context.
 * @param base Active `event_base`.
 * @param req Effective HTTP request.
 * @param state Initialized `libevent_state` (`state->req` and `state->res`
 * set).
 * @param out_req_ev Output `evhttp_request` pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t ev_submit_request_on_base(
    const struct HttpTransportContext *ctx, struct event_base *base,
    const struct HttpRequest *req, struct libevent_state *state,
    struct evhttp_request **out_req_ev) {
  char host[256];
  char proxy_host[256];
  int port = 80;
  int proxy_port = 80;
  const char *path = "/";
  const char *proxy_path = "/";
  int is_https = 0;
  int proxy_https = 0;
  const char *dial_host;
  int dial_port;
  const char *req_uri;
  struct evhttp_request *req_ev = NULL;
  struct evkeyvalq *output_headers;
  int method_cmd = 0;
  int has_ua = 0;
  c_abstract_http_error_t rc;

  rc =
      ev_parse_url_parts(req->url, host, sizeof(host), &port, &path, &is_https);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (ctx->config.proxy_url && !is_https) {
    rc = ev_parse_url_parts(ctx->config.proxy_url, proxy_host,
                            sizeof(proxy_host), &proxy_port, &proxy_path,
                            &proxy_https);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    if (proxy_https && proxy_path) {
      LOG_DEBUG("ev_submit_request_on_base: proxy %s", proxy_host);
    }
    dial_host = proxy_host;
    dial_port = proxy_port;
    req_uri = req->url;
  } else {
    dial_host = host;
    dial_port = port;
    req_uri = path;
  }

  state->base = base;
  rc = ev_open_connection(ctx, base, dial_host, dial_port, is_https, host,
                          &state->conn);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  req_ev = evhttp_request_new(http_request_done, state);
  if (!req_ev) {
    evhttp_connection_free(state->conn);
    state->conn = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (req->on_chunk) {
    evhttp_request_set_chunked_cb(req_ev, http_chunked_cb);
  }

  output_headers = evhttp_request_get_output_headers(req_ev);
  if (evhttp_add_header(output_headers, "Host", host) != 0) {
    LOG_DEBUG("ev_submit_request_on_base: Host header add failed");
  }
  if (evhttp_add_header(output_headers, "Connection", "close") != 0) {
    LOG_DEBUG("ev_submit_request_on_base: Connection header add failed");
  }

  if (req->headers.count > 0) {
    size_t i;
    for (i = 0; i < req->headers.count; i++) {
      if (math_ev_strcasecmp(req->headers.headers[i].key, "User-Agent") == 0) {
        has_ua = 1;
      }
      if (evhttp_add_header(output_headers, req->headers.headers[i].key,
                            req->headers.headers[i].value) != 0) {
        LOG_DEBUG("ev_submit_request_on_base: custom header add failed");
      }
    }
  }
  if (!has_ua && ctx->config.user_agent) {
    if (evhttp_add_header(output_headers, "User-Agent",
                          ctx->config.user_agent) != 0) {
      LOG_DEBUG("ev_submit_request_on_base: User-Agent add failed");
    }
  }
  if (ctx->config.proxy_url && ctx->config.proxy_username) {
    char *auth_val = NULL;
    rc = ev_build_proxy_auth(ctx->config.proxy_username,
                             ctx->config.proxy_password, &auth_val);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      evhttp_request_free(req_ev);
      evhttp_connection_free(state->conn);
      state->conn = NULL;
      return rc;
    }
    if (evhttp_add_header(output_headers, "Proxy-Authorization", auth_val) !=
        0) {
      LOG_DEBUG("ev_submit_request_on_base: Proxy-Authorization add failed");
    }
    free(auth_val);
  }

  if (req->body && req->body_len > 0) {
    struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
    if (evbuffer_add(evb, req->body, req->body_len) != 0) {
      LOG_DEBUG("ev_submit_request_on_base: evbuffer_add failed");
    }
  } else if (req->read_chunk) {
    struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
    char buf[4096];
    size_t read_bytes = 0;
    for (;;) {
      c_abstract_http_error_t r = req->read_chunk(
          req->read_chunk_user_data, buf, sizeof(buf), &read_bytes);
      if (r != C_ABSTRACT_HTTP_SUCCESS) {
        evhttp_request_free(req_ev);
        evhttp_connection_free(state->conn);
        state->conn = NULL;
        return r;
      }
      if (read_bytes == 0) {
        break;
      }
      if (evbuffer_add(evb, buf, read_bytes) != 0) {
        LOG_DEBUG("ev_submit_request_on_base: chunk evbuffer_add failed");
      }
    }
  }

  rc = get_method_cmd(req->method, &method_cmd);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    evhttp_request_free(req_ev);
    evhttp_connection_free(state->conn);
    state->conn = NULL;
    return rc;
  }
  if (evhttp_make_request(state->conn, req_ev, method_cmd, req_uri) != 0) {
#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
    evhttp_request_free(req_ev);
#endif
    evhttp_connection_free(state->conn);
    state->conn = NULL;
    return (c_abstract_http_error_t)ECONNREFUSED;
  }

  *out_req_ev = req_ev;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Flatten multipart `req->parts` into a temporary request copy if
 * present.
 * @param req Input request.
 * @param flat_req Scratch request structure.
 * @param out_flattened Set to 1 if @p flat_req owns memory.
 * @param out_eff Output pointer to effective request.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ev_prepare_effective_req(const struct HttpRequest *req,
                         struct HttpRequest *flat_req, int *out_flattened,
                         const struct HttpRequest **out_eff) {
  size_t i;
  c_abstract_http_error_t rc;

  *out_flattened = 0;
  *out_eff = req;
  if (req->parts.count == 0 || req->body || req->read_chunk) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!req->parts.parts) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(flat_req, 0, sizeof(*flat_req));
  flat_req->method = req->method;
  flat_req->url = req->url;
  flat_req->parts = req->parts;
  flat_req->on_chunk = req->on_chunk;
  flat_req->on_chunk_user_data = req->on_chunk_user_data;

  rc = http_headers_init(&flat_req->headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  for (i = 0; i < req->headers.count; ++i) {
    rc = http_headers_add(&flat_req->headers, req->headers.headers[i].key,
                          req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t frc = http_headers_free(&flat_req->headers);
      return frc != C_ABSTRACT_HTTP_SUCCESS ? frc : rc;
    }
  }
  rc = http_request_flatten_parts(flat_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t frc;
    free(flat_req->body);
    flat_req->body = NULL;
    frc = http_headers_free(&flat_req->headers);
    return frc != C_ABSTRACT_HTTP_SUCCESS ? frc : rc;
  }
  *out_flattened = 1;
  *out_eff = flat_req;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Resolve redirect `Location` header relative to current request URL.
 * @param cur_url Current request URL.
 * @param loc Redirect `Location` header value.
 * @param out_url Newly allocated target URL.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
ev_resolve_redirect_url(const char *cur_url, const char *loc, char **out_url) {
  char host[256];
  int port = 80;
  const char *path = "/";
  int is_https = 0;
  size_t need;
  char *buf;
  c_abstract_http_error_t rc;

  if (strncmp(loc, "http://", 7) == 0 || strncmp(loc, "https://", 8) == 0) {
    return c_abstract_http_strdup(loc, out_url);
  }
  rc = ev_parse_url_parts(cur_url, host, sizeof(host), &port, &path, &is_https);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  need = strlen(host) + strlen(loc) + 32;
  buf = (char *)malloc(need);
  if (!buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(buf, need, "%s://%s:%d%s%s", is_https ? "https" : "http", host,
            port, loc[0] == '/' ? "" : "/", loc);
#else
  sprintf(buf, "%s://%s:%d%s%s", is_https ? "https" : "http", host, port,
          loc[0] == '/' ? "" : "/", loc);
#endif
  *out_url = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Test helper invoking `ev_resolve_redirect_url` with NULL current URL.
 * @return C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_resolve_null(void);

NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_libevent_resolve_null(void) {
  char *out = NULL;
  return ev_resolve_redirect_url(NULL, "/rel", &out);
}
#endif

/**
 * @brief Execute one single-hop libevent HTTP transaction.
 * @param ctx Transport context.
 * @param req Effective HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ev_send_single_hop(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  struct libevent_state state;
  struct event_base *base;
  struct evhttp_request *req_ev = NULL;
  c_abstract_http_error_t rc;

  memset(&state, 0, sizeof(state));
  state.req = req;
  state.res = res;

  base = event_base_new();
  if (!base) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = ev_submit_request_on_base(ctx, base, req, &state, &req_ev);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    event_base_free(base);
    return rc;
  }

  event_base_dispatch(base);

  if (state.error_code != 0) {
    rc = (c_abstract_http_error_t)state.error_code;
    if (*res) {
      c_abstract_http_error_t frc = http_response_free(*res);
      free(*res);
      *res = NULL;
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        rc = frc;
      }
    }
  } else if (!*res) {
    rc = (c_abstract_http_error_t)ECONNREFUSED;
  } else {
    rc = C_ABSTRACT_HTTP_SUCCESS;
  }

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
  evhttp_request_free(req_ev);
#endif
  evhttp_connection_free(state.conn);
  event_base_free(base);
  return rc;
}

/**
 * @brief Check whether a response triggers an HTTP redirect hop.
 * @param ctx Transport context.
 * @param res Received response.
 * @param out_loc Output pointer receiving Location value when redirecting.
 * @return 1 if redirect should be followed, 0 otherwise.
 */
static int math_ev_should_redirect(const struct HttpTransportContext *ctx,
                                   const struct HttpResponse *res,
                                   const char **out_loc) {
  size_t i;
  const char *loc = NULL;
  if (!ctx || !ctx->config.follow_redirects || !res) {
    return 0;
  }
  if (res->status_code != 301 && res->status_code != 302 &&
      res->status_code != 303 && res->status_code != 307 &&
      res->status_code != 308) {
    return 0;
  }
  for (i = 0; i < res->headers.count; ++i) {
    if (res->headers.headers[i].key &&
        math_ev_strcasecmp(res->headers.headers[i].key, "Location") == 0) {
      loc = res->headers.headers[i].value;
      break;
    }
  }
  if (!loc || loc[0] == '\0') {
    return 0;
  }
  *out_loc = loc;
  return 1;
}

/**
 * @brief Clean up redirect and flattened request buffers at end of
 * `http_libevent_send`.
 * @param redir_url Heap redirect URL or NULL.
 * @param flat_req Flattened scratch request.
 * @param flattened Non-zero when `flat_req` owns heap resources.
 * @param primary_rc Primary send status to return when cleanup succeeds.
 * @return Cleanup error if cleanup fails and `primary_rc` succeeded, else
 * `primary_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
ev_finish_send_cleanup(char *redir_url, struct HttpRequest *flat_req,
                       int flattened, c_abstract_http_error_t primary_rc) {
  free(redir_url);
  if (flattened) {
    c_abstract_http_error_t cl_rc;
    free(flat_req->body);
    cl_rc = http_headers_free(&flat_req->headers);
    if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return primary_rc != C_ABSTRACT_HTTP_SUCCESS ? primary_rc : cl_rc;
    }
  }
  return primary_rc;
}

/* Send an HTTP request using libevent (with parts flattening & redirects). */
NO_DISCARD c_abstract_http_error_t
http_libevent_send(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  struct HttpRequest flat_req;
  const struct HttpRequest *eff_req = NULL;
  struct HttpRequest hop_req;
  char *redir_url = NULL;
  int flattened = 0;
  int hop;
  c_abstract_http_error_t rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = NULL;

  rc = ev_prepare_effective_req(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  hop_req = *eff_req;
  for (hop = 0; hop <= LIBEVENT_MAX_REDIRECTS; ++hop) {
    const char *loc = NULL;
    char *next_url = NULL;
    c_abstract_http_error_t frc;
    int status;

    rc = ev_send_single_hop(ctx, &hop_req, res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ev_finish_send_cleanup(redir_url, &flat_req, flattened, rc);
    }
    if (!math_ev_should_redirect(ctx, *res, &loc)) {
      break;
    }
    status = (*res)->status_code;
    if (hop == LIBEVENT_MAX_REDIRECTS) {
      frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        return ev_finish_send_cleanup(redir_url, &flat_req, flattened, frc);
      }
      free(*res);
      *res = NULL;
      return ev_finish_send_cleanup(redir_url, &flat_req, flattened,
                                    C_ABSTRACT_HTTP_ERR_LOOP);
    }
    rc = ev_resolve_redirect_url(hop_req.url, loc, &next_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        return ev_finish_send_cleanup(redir_url, &flat_req, flattened, frc);
      }
      free(*res);
      *res = NULL;
      return ev_finish_send_cleanup(redir_url, &flat_req, flattened, rc);
    }
    frc = http_response_free(*res);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      free(next_url);
      return ev_finish_send_cleanup(redir_url, &flat_req, flattened, frc);
    }
    free(*res);
    *res = NULL;
    free(redir_url);
    redir_url = next_url;
    hop_req.url = redir_url;
    if (status == 303 ||
        ((status == 301 || status == 302) && hop_req.method == HTTP_POST)) {
      hop_req.method = HTTP_GET;
      hop_req.body = NULL;
      hop_req.body_len = 0;
      hop_req.read_chunk = NULL;
    }
  }

  return ev_finish_send_cleanup(redir_url, &flat_req, flattened,
                                C_ABSTRACT_HTTP_SUCCESS);
}

/** @brief Per-slot state for concurrent `http_libevent_send_multi` */
struct ev_multi_slot {
  struct libevent_state st;      /**< Per-request libevent callback state */
  struct HttpRequest flat_req;   /**< Flattened scratch multipart request */
  int flattened;                 /**< Non-zero if flat_req owns heap memory */
  struct evhttp_request *req_ev; /**< Active evhttp_request handle */
  struct HttpResponse *res;      /**< Populated HTTP response */
  struct HttpFuture *future;     /**< Target future slot */
  int settled;                   /**< Non-zero once future is resolved */
};

/** @brief Batch coordinator bridging `event_base_loop(EVLOOP_NONBLOCK)` */
struct ev_multi_batch {
  struct event_base *base;           /**< Shared libevent base */
  struct ev_multi_slot *slots;       /**< Array of per-request slots */
  size_t count;                      /**< Total slot count */
  int caller_loop;                   /**< Non-zero when caller owns loop */
  c_abstract_http_error_t first_err; /**< First error observed across batch */
};

/**
 * @brief Settle one completed or failed libevent batch slot.
 * @param batch Parent batch state.
 * @param sl Slot to settle.
 * @return Slot error code (`C_ABSTRACT_HTTP_SUCCESS` on success).
 */
NO_DISCARD static c_abstract_http_error_t
ev_settle_multi_slot(struct ev_multi_batch *batch, struct ev_multi_slot *sl) {
  c_abstract_http_error_t rc;
  sl->settled = 1;
  if (sl->st.error_code != 0) {
    rc = (c_abstract_http_error_t)sl->st.error_code;
    if (sl->res) {
      c_abstract_http_error_t frc = http_response_free(sl->res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(sl->res);
        sl->res = NULL;
        sl->future->response = NULL;
        sl->future->error_code = frc;
        sl->future->is_ready = 1;
        if (batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
          batch->first_err = frc;
        }
        return frc;
      }
      free(sl->res);
      sl->res = NULL;
    }
  } else if (!sl->res) {
    rc = (c_abstract_http_error_t)ECONNREFUSED;
  } else {
    rc = C_ABSTRACT_HTTP_SUCCESS;
  }
  sl->future->response = sl->res;
  sl->future->error_code = rc;
  sl->future->is_ready = 1;
  if (rc != C_ABSTRACT_HTTP_SUCCESS &&
      batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
    batch->first_err = rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief `ModalityEventLoop` callback driving
 * `event_base_loop(EVLOOP_NONBLOCK)`.
 * @param loop Active `ModalityEventLoop`.
 * @param timer_id Fired timer ID.
 * @param user_data Pointer to `struct ev_multi_batch`.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop control error.
 */
NO_DISCARD static c_abstract_http_error_t
ev_pump_nonblock_cb(struct ModalityEventLoop *loop, int timer_id,
                    void *user_data) {
  struct ev_multi_batch *batch = (struct ev_multi_batch *)user_data;
  size_t i;
  size_t pending = 0;
  int lrc;
  (void)timer_id;

  lrc = event_base_loop(batch->base, EVLOOP_NONBLOCK);

  for (i = 0; i < batch->count; ++i) {
    struct ev_multi_slot *sl = &batch->slots[i];
    if (sl->settled) {
      continue;
    }
    if (sl->st.done || lrc != 0) {
      c_abstract_http_error_t src_rc = ev_settle_multi_slot(batch, sl);
      if (src_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return src_rc;
      }
    } else {
      pending++;
    }
  }

  if (pending > 0) {
    int next_id = 0;
    return http_loop_add_timer(loop, 1, ev_pump_nonblock_cb, batch, &next_id);
  }
  if (batch->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Prepare and submit a single slot onto `base` for `send_multi`.
 * @param ctx Transport context.
 * @param batch Batch coordinator.
 * @param req Input HTTP request.
 * @param slot Target slot structure.
 * @param out_active Incremented when the slot is armed asynchronously.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
ev_arm_multi_slot(struct HttpTransportContext *ctx,
                  struct ev_multi_batch *batch, const struct HttpRequest *req,
                  struct ev_multi_slot *slot, size_t *out_active) {
  const struct HttpRequest *eff_req = NULL;
  c_abstract_http_error_t rc;

  if (!slot->future) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc = ev_prepare_effective_req(req, &slot->flat_req, &slot->flattened,
                                &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    slot->settled = 1;
    slot->future->response = NULL;
    slot->future->error_code = rc;
    slot->future->is_ready = 1;
    if (batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch->first_err = rc;
    }
    return !slot->future ? rc : C_ABSTRACT_HTTP_SUCCESS;
  }
  slot->st.req = eff_req;
  slot->st.res = &slot->res;
  rc = ev_submit_request_on_base(ctx, batch->base, eff_req, &slot->st,
                                 &slot->req_ev);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    slot->settled = 1;
    slot->future->response = NULL;
    slot->future->error_code = rc;
    slot->future->is_ready = 1;
    if (batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch->first_err = rc;
    }
    return !slot->future ? rc : C_ABSTRACT_HTTP_SUCCESS;
  }
  (*out_active)++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Arm timer and run `active_loop` for `http_libevent_send_multi`.
 * @param active_loop Event loop to drive.
 * @param batch Active batch state.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error code.
 */
NO_DISCARD static c_abstract_http_error_t
ev_drive_multi_loop(struct ModalityEventLoop *active_loop,
                    struct ev_multi_batch *batch) {
  int timer_id = 0;
  c_abstract_http_error_t rc;

  rc = http_loop_add_timer(active_loop, 0, ev_pump_nonblock_cb, batch,
                           &timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return http_loop_run(active_loop);
}

/**
 * @brief Tear down `http_libevent_send_multi` resources and finalize loop.
 * @param slots Slot array to release.
 * @param count Number of slots.
 * @param base Shared `event_base`.
 * @param owned_loop Internally owned loop or NULL.
 * @param caller_loop Caller-supplied loop or NULL.
 * @param primary_rc Primary batch status.
 * @return Final `c_abstract_http_error_t`.
 */
NO_DISCARD static c_abstract_http_error_t ev_finish_multi_cleanup(
    struct ev_multi_slot *slots, size_t count, struct event_base *base,
    struct ModalityEventLoop *owned_loop, struct ModalityEventLoop *caller_loop,
    c_abstract_http_error_t primary_rc) {
  size_t i;
  c_abstract_http_error_t cleanup_err = C_ABSTRACT_HTTP_SUCCESS;

  for (i = 0; i < count; ++i) {
#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
    evhttp_request_free(slots[i].req_ev);
#endif
    evhttp_connection_free(slots[i].st.conn);
    if (slots[i].flattened) {
      c_abstract_http_error_t hrc;
      free(slots[i].flat_req.body);
      hrc = http_headers_free(&slots[i].flat_req.headers);
      if (hrc != C_ABSTRACT_HTTP_SUCCESS) {
        free(slots);
        event_base_free(base);
        if (owned_loop) {
          c_abstract_http_error_t lrc = http_loop_free(owned_loop);
          if (lrc != C_ABSTRACT_HTTP_SUCCESS) {
            return lrc;
          }
        }
        return hrc;
      }
    }
  }
  free(slots);
  event_base_free(base);

  if (owned_loop) {
    cleanup_err = http_loop_free(owned_loop);
    if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_err;
    }
  } else if (caller_loop) {
    cleanup_err = abstract_http_event_loop_test_unstop(caller_loop);
    if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_err;
    }
    cleanup_err = http_loop_wakeup(caller_loop);
    if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_err;
    }
  }
  return primary_rc;
}

/* Multiplex multiple requests via event_base_loop(EVLOOP_NONBLOCK) on
 * ModalityEventLoop. */
NO_DISCARD c_abstract_http_error_t http_libevent_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop;
  struct event_base *base = NULL;
  struct ev_multi_batch batch;
  struct ev_multi_slot *slots;
  size_t i;
  size_t active_slots = 0;
  c_abstract_http_error_t rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !multi || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  base = event_base_new();
  if (!base) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  slots = (struct ev_multi_slot *)calloc(multi->count, sizeof(*slots));
  if (!slots) {
    event_base_free(base);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (loop) {
    active_loop = loop;
  } else {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slots);
      event_base_free(base);
      return rc;
    }
    active_loop = owned_loop;
  }

  memset(&batch, 0, sizeof(batch));
  batch.base = base;
  batch.slots = slots;
  batch.count = multi->count;
  batch.caller_loop = (loop != NULL) ? 1 : 0;
  batch.first_err = C_ABSTRACT_HTTP_SUCCESS;

  for (i = 0; i < multi->count; ++i) {
    slots[i].future = futures[i];
    rc = ev_arm_multi_slot(ctx, &batch, multi->requests[i], &slots[i],
                           &active_slots);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ev_finish_multi_cleanup(slots, multi->count, base, owned_loop,
                                     loop, rc);
    }
  }

  if (active_slots > 0) {
    rc = ev_drive_multi_loop(active_loop, &batch);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ev_finish_multi_cleanup(
          slots, multi->count, base, owned_loop, loop,
          batch.first_err != C_ABSTRACT_HTTP_SUCCESS ? batch.first_err : rc);
    }
  }

  return ev_finish_multi_cleanup(slots, multi->count, base, owned_loop, loop,
                                 batch.first_err);
}
