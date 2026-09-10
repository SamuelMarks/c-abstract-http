/**
 * @file test_http_winhttp.h
 * @brief Integration and unit tests for WinHTTP Backend.
 *
 * Verifies initialization, handle lifecycle management, configuration
 * application, synchronous/asynchronous sending, OOM conditions, and error
 * handling.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_WINHTTP_H
#define TEST_HTTP_WINHTTP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_winhttp.h>
#include "mock_alloc.h"
#include "abstract_http_test_helpers/mock_server.h"
/* clang-format on */

#if defined(_WIN32)
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#endif

/** @brief Helper structure for chunk testing */
struct winhttp_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes */
  size_t total_bytes;
  /** @brief Abort on call */
  int abort_on_call;
};

static int winhttp_chunk_cb(void *user_data, const void *data, size_t length) {
  struct winhttp_TestChunkState *state =
      (struct winhttp_TestChunkState *)user_data;
  (void)data;
  state->call_count++;
  state->total_bytes += length;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return 1;
  }
  return 0;
}

static int winhttp_read_cb(void *user_data, void *buffer, size_t max_len,
                           size_t *out_len) {
  struct winhttp_TestChunkState *state =
      (struct winhttp_TestChunkState *)user_data;
  state->call_count++;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return 1;
  }
  if (state->call_count == 1) {
    size_t to_copy = (max_len < 5) ? max_len : 5;
    memcpy(buffer, "CHUNK", to_copy);
    *out_len = to_copy;
    return 0;
  }
  *out_len = 0; /* EOF */
  return 0;
}

/** @brief Test WinHTTP lifecycle */
TEST test_winhttp_lifecycle(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpTransportContext *ctx = NULL;

  /* Global init/cleanup */
  rc = http_winhttp_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_winhttp_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Context init NULL check */
  rc = http_winhttp_context_init(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Context init success */
  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  /* Free NULL and ctx */
  http_winhttp_context_free(NULL);
  http_winhttp_context_free(ctx);

#if !defined(_WIN32)
  g_mock_winhttp_context_init_fail = 1;
  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_winhttp_context_init_fail = 0;

  g_mock_winhttp_open_fail = 1;
  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_open_fail = 0;

  g_mock_alloc_fail = 1;
  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/** @brief Test WinHTTP configuration */
TEST test_winhttp_config_usage(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *_ast_strdup_0 = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* NULL checks */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_winhttp_config_apply(NULL, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_winhttp_config_apply(ctx, NULL));

  /* Various timeouts */
  cfg.timeout_ms = 5000;
  cfg.connect_timeout_ms = 1000;
  cfg.read_timeout_ms = 2000;
  cfg.write_timeout_ms = 3000;
  cfg.verify_peer = 0;
  cfg.verify_host = 0;
  cfg.follow_redirects = 0;

  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  cfg.follow_redirects = 1;
  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Proxy config */
  c_abstract_http_mock_strdup("http://127.0.0.1:8888", &_ast_strdup_0);
  cfg.proxy_url = _ast_strdup_0;

  c_abstract_http_mock_strdup("admin", &_ast_strdup_0);
  cfg.proxy_username = _ast_strdup_0;

  c_abstract_http_mock_strdup("secret", &_ast_strdup_0);
  cfg.proxy_password = _ast_strdup_0;

  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if !defined(_WIN32)
  g_mock_winhttp_set_timeouts_fail = 1;
  rc = http_winhttp_config_apply(ctx, &cfg);
  g_mock_winhttp_set_timeouts_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_alloc_fail = 1;
  rc = http_winhttp_config_apply(ctx, &cfg);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_winhttp_set_option_fail = 1;
  rc = http_winhttp_config_apply(ctx, &cfg);
  g_mock_winhttp_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
#endif

  /* Timeout ms only */
  cfg.timeout_ms = 1000;
  cfg.connect_timeout_ms = 0;
  cfg.read_timeout_ms = 0;
  cfg.write_timeout_ms = 0;
  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Read and write timeout only */
  cfg.timeout_ms = 0;
  cfg.connect_timeout_ms = 0;
  cfg.read_timeout_ms = 100;
  cfg.write_timeout_ms = 100;
  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Proxy without credentials */
  free((void *)cfg.proxy_username);
  cfg.proxy_username = NULL;
  free((void *)cfg.proxy_password);
  cfg.proxy_password = NULL;
  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* No proxy (default proxy) */
  free((void *)cfg.proxy_url);
  cfg.proxy_url = NULL;
  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  http_config_free(&cfg);
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP send validation */
TEST test_winhttp_send_null_checks(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Null checks */
  rc = http_winhttp_send(NULL, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  rc = http_winhttp_send(ctx, NULL, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  rc = http_winhttp_send(ctx, &req, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  http_request_free(&req);
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP send failure paths */
TEST test_winhttp_send_fail(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *_ast_strdup_1 = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Invalid URL syntax */
  c_abstract_http_mock_strdup("not_a_url", &_ast_strdup_1);
  req.url = _ast_strdup_1;

  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  http_request_free(&req);
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP methods and headers */
TEST test_winhttp_send_methods_and_headers(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum HttpMethod methods[11];
  size_t i;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif

  methods[0] = HTTP_GET;
  methods[1] = HTTP_POST;
  methods[2] = HTTP_PUT;
  methods[3] = HTTP_DELETE;
  methods[4] = HTTP_HEAD;
  methods[5] = HTTP_OPTIONS;
  methods[6] = HTTP_TRACE;
  methods[7] = HTTP_QUERY;
  methods[8] = HTTP_CONNECT;
  methods[9] = HTTP_PATCH;
  methods[10] = (enum HttpMethod)99;

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  for (i = 0; i < 11; ++i) {
    rc = http_request_init(&req);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
    c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
    c_abstract_http_mock_strdup("https://127.0.0.1:8443/test", &url_dup);
#endif
    req.url = url_dup;
    req.method = methods[i];

    rc = http_headers_add(&req.headers, "X-Custom", "Value");
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

    res = NULL;
    rc = http_winhttp_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT(res != NULL);
    ASSERT_EQ(200, res->status_code);

    http_response_free(res);
    free(res);
    http_request_free(&req);
  }

#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP cookies handling */
TEST test_winhttp_send_cookies(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpCookieJar jar;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_cookie_jar_init(&jar);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_cookie_jar_set(&jar, "session", "xyz123");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  {
    struct HttpConfig cfg;
    rc = http_config_init(&cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    cfg.cookie_jar = &jar;
    rc = http_winhttp_config_apply(ctx, &cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    http_config_free(&cfg);
  }

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/cookies", &url_dup);
#endif
  req.url = url_dup;

#if !defined(_WIN32)
  g_mock_winhttp_cookie_count = 1;
#endif

  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  http_response_free(res);
  free(res);
  http_request_free(&req);
  http_cookie_jar_free(&jar);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP chunked streaming response */
TEST test_winhttp_send_chunked(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct winhttp_TestChunkState state;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif
  memset(&state, 0, sizeof(state));

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/stream", &url_dup);
#endif
  req.url = url_dup;
  req.on_chunk = winhttp_chunk_cb;
  req.on_chunk_user_data = &state;

#if !defined(_WIN32)
  g_mock_winhttp_read_chunks = 2;
#endif

  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  http_response_free(res);
  free(res);
  http_request_free(&req);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP chunked response abort */
TEST test_winhttp_send_chunked_abort(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct winhttp_TestChunkState state;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif
  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/abort", &url_dup);
#endif
  req.url = url_dup;
  req.on_chunk = winhttp_chunk_cb;
  req.on_chunk_user_data = &state;

#if !defined(_WIN32)
  g_mock_winhttp_read_chunks = 2;
#endif

  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);

  http_request_free(&req);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP upload chunked */
TEST test_winhttp_send_upload_chunked(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct winhttp_TestChunkState state;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif
  memset(&state, 0, sizeof(state));

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/upload", &url_dup);
#endif
  req.url = url_dup;
  req.method = HTTP_POST;
  req.read_chunk = winhttp_read_cb;
  req.read_chunk_user_data = &state;
  req.expected_body_len = 5;

  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  http_response_free(res);
  free(res);
  http_request_free(&req);

  /* Upload chunk abort */
  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/upload_abort", &url_dup);
#endif
  req.url = url_dup;
  req.method = HTTP_POST;
  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;
  req.read_chunk = winhttp_read_cb;
  req.read_chunk_user_data = &state;
  req.expected_body_len = 5;

  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  http_request_free(&req);

#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP send multi */
TEST test_winhttp_send_multi(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct ModalityEventLoop *loop = NULL;
  struct HttpRequest req;
  struct HttpMultiRequest multi;
  struct HttpFuture *futures[1];
  struct HttpRequest *reqs[1];
  struct HttpFuture f1;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_loop_init(&loop);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/multi", &url_dup);
#endif
  req.url = url_dup;

  reqs[0] = &req;
  multi.requests = reqs;
  multi.count = 1;

  memset(&f1, 0, sizeof(f1));
  futures[0] = &f1;

  /* Null checks */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_winhttp_send_multi(NULL, loop, &multi, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_winhttp_send_multi(ctx, loop, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_winhttp_send_multi(ctx, loop, &multi, NULL));

  rc = http_winhttp_send_multi(ctx, loop, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  {
    int wait_cycles = 0;
    while (!f1.is_ready && wait_cycles < 500) {
      Sleep(10);
      wait_cycles++;
    }
  }
#endif

#if !defined(_WIN32)
  g_mock_winhttp_queue_work_item_fail = 1;
  rc = http_winhttp_send_multi(ctx, loop, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_queue_work_item_fail = 0;

  g_mock_alloc_fail = 1;
  rc = http_winhttp_send_multi(ctx, loop, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  http_future_free(&f1);
  http_request_free(&req);
  http_loop_free(loop);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_winhttp_context_free(ctx);
  PASS();
}

#if !defined(_WIN32)
static int mock_winhttp_fail_wakeup(void *user_ctx) {
  (void)user_ctx;
  return 1;
}

/** @brief Test WinHTTP send mock failure branches */
TEST test_winhttp_send_mock_failures(void) {
  enum c_abstract_http_error rc;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_dup);
  req.url = url_dup;

  /* wUrl alloc fail */
  g_mock_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* crack url fail */
  g_mock_winhttp_crack_url_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  g_mock_winhttp_crack_url_fail = 0;

  /* URL without path */
  c_abstract_http_mock_strdup("http://127.0.0.1:8080", &url_dup);
  req.url = url_dup;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_dup);
  req.url = url_dup;

  /* hostName / urlPath alloc fail */
  g_mock_alloc_fail = 2;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* WinHttpConnect fail */
  g_mock_winhttp_connect_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_connect_fail = 0;

  /* WinHttpOpenRequest fail */
  g_mock_winhttp_open_request_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_open_request_fail = 0;

  /* WinHttpSendRequest fail */
  g_mock_winhttp_send_request_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_send_request_fail = 0;

  /* WinHttpReceiveResponse fail */
  g_mock_winhttp_receive_response_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_receive_response_fail = 0;

  /* WinHttpQueryHeaders fail */
  g_mock_winhttp_query_headers_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_query_headers_fail = 0;

  /* readBuf alloc fail */
  g_mock_winhttp_read_buf_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_winhttp_read_buf_alloc_fail = 0;

  /* QueryDataAvailable fail */
  g_mock_winhttp_query_data_available_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_query_data_available_fail = 0;

  /* ReadData fail */
  g_mock_winhttp_read_chunks = 1;
  g_mock_winhttp_read_data_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_winhttp_read_data_fail = 0;
  g_mock_winhttp_read_chunks = 0;

  /* totalBody realloc fail */
  g_mock_winhttp_read_chunks = 1;
  g_mock_winhttp_total_body_realloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_winhttp_total_body_realloc_fail = 0;
  g_mock_winhttp_read_chunks = 0;

  /* res alloc fail */
  g_mock_winhttp_res_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_winhttp_res_alloc_fail = 0;

  /* response_init fail */
  g_mock_winhttp_response_init_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_winhttp_response_init_fail = 0;

  /* POST body without read_chunk */
  req.method = HTTP_POST;
  req.body = "payload";
  req.body_len = 7;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  req.body = NULL;
  req.body_len = 0;
  req.method = HTTP_GET;

  /* read_chunk with send_request fail */
  {
    struct winhttp_TestChunkState state;
    memset(&state, 0, sizeof(state));
    req.read_chunk = winhttp_read_cb;
    req.read_chunk_user_data = &state;
    req.expected_body_len = 5;
    g_mock_winhttp_send_request_fail = 1;
    rc = http_winhttp_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_winhttp_send_request_fail = 0;

    /* read_chunk with write_data fail */
    memset(&state, 0, sizeof(state));
    g_mock_winhttp_write_data_fail = 1;
    rc = http_winhttp_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_winhttp_write_data_fail = 0;
    req.read_chunk = NULL;
  }

  /* headers_to_wide_block fail */
  rc = http_headers_add(&req.headers, "X-Fail", "Val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  g_mock_alloc_count = 3;
  g_mock_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  http_request_free(&req);
  http_winhttp_context_free(ctx);
  PASS();
}

/** @brief Test WinHTTP coverage branches */
TEST test_winhttp_coverage_branches(void) {
  enum c_abstract_http_error rc;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct winhttp_TestChunkState state;

  /* 1. Context init config failure */
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_winhttp_context_init(&ctx);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 2. Context init */
  rc = http_winhttp_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_config_init(&cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* 3. Config apply proxy user/pass alloc fail */
  c_abstract_http_mock_strdup("http://127.0.0.1:8888", &cfg.proxy_url);
  c_abstract_http_mock_strdup("admin", &cfg.proxy_username);
  c_abstract_http_mock_strdup("secret", &cfg.proxy_password);
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_winhttp_config_apply(ctx, &cfg);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_count = 2;
  g_mock_alloc_fail = 1;
  rc = http_winhttp_config_apply(ctx, &cfg);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  http_config_free(&cfg);

  /* 4. Send with hostName alloc fail (alloc 1) and urlPath alloc fail (alloc 2)
   */
  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/path", &url_dup);
  req.url = url_dup;

  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_count = 2;
  g_mock_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 5. Send with connect fail and open_request fail */
  g_mock_winhttp_connect_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_connect_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_winhttp_open_request_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_open_request_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 6. Send with security_flags, disable_redirects, cookies in jar, and headers
   * add fail */
  rc = http_cookie_jar_init(&jar);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_cookie_jar_set(&jar, "cookie1", "val1");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  cfg.verify_peer = 0;
  cfg.verify_host = 0;
  cfg.follow_redirects = 0;
  cfg.cookie_jar = &jar;
  rc = http_winhttp_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  http_config_free(&cfg);

  rc = http_headers_add(&req.headers, "X-Header", "Value");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  g_mock_winhttp_add_request_headers_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_add_request_headers_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 7. Send request fail without read_chunk */
  g_mock_winhttp_send_request_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_send_request_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 8. Receive response fail, query headers fail, read buf alloc fail */
  g_mock_winhttp_receive_response_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_receive_response_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_winhttp_query_headers_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_query_headers_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_winhttp_read_buf_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_read_buf_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 9. Query data available fail, read data fail, on_chunk abort */
  g_mock_winhttp_query_data_available_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_query_data_available_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_winhttp_read_chunks = 1;
  g_mock_winhttp_read_data_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_read_data_fail = 0;
  g_mock_winhttp_read_chunks = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;
  req.on_chunk = winhttp_chunk_cb;
  req.on_chunk_user_data = &state;
  g_mock_winhttp_read_chunks = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_read_chunks = 0;
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  req.on_chunk = NULL;

  /* 10. totalBody realloc fail, res alloc fail, response_init fail */
  g_mock_winhttp_read_chunks = 1;
  g_mock_winhttp_total_body_realloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_total_body_realloc_fail = 0;
  g_mock_winhttp_read_chunks = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_winhttp_res_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_res_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_winhttp_response_init_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_response_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* totalBody freed on cleanup */
  g_mock_winhttp_read_chunks = 1;
  g_mock_winhttp_res_alloc_fail = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_read_chunks = 0;
  g_mock_winhttp_res_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* Large chunk > 8192 and status code 404 */
  g_mock_winhttp_read_chunks = 3;
  g_mock_winhttp_status_code = 404;
  rc = http_winhttp_send(ctx, &req, &res);
  g_mock_winhttp_read_chunks = 0;
  g_mock_winhttp_status_code = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  /* 11. Cookie parsing in response with semicolon */
  g_mock_winhttp_cookie_count = 1;
  rc = http_winhttp_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  /* 12. Send multi with loop wakeup failure and queue fail */
  {
    struct ModalityEventLoop *loop = NULL;
    struct HttpLoopHooks hooks;
    struct HttpMultiRequest multi;
    struct HttpFuture *futures[1];
    struct HttpRequest *reqs[1];
    struct HttpFuture f1;

    memset(&hooks, 0, sizeof(hooks));
    hooks.wakeup = mock_winhttp_fail_wakeup;
    rc = http_loop_init_external(&loop, &hooks);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

    reqs[0] = &req;
    multi.requests = reqs;
    multi.count = 1;
    memset(&f1, 0, sizeof(f1));
    futures[0] = &f1;

    rc = http_winhttp_send_multi(ctx, loop, &multi, futures);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

    g_mock_winhttp_queue_work_item_fail = 1;
    rc = http_winhttp_send_multi(ctx, loop, &multi, futures);
    g_mock_winhttp_queue_work_item_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

    http_future_free(&f1);
    http_loop_free(loop);
  }

  http_cookie_jar_free(&jar);
  http_request_free(&req);
  http_winhttp_context_free(ctx);
  PASS();
}
#endif

/** @brief WinHTTP test suite */
SUITE(http_winhttp_suite) {
  RUN_TEST(test_winhttp_lifecycle);
  RUN_TEST(test_winhttp_config_usage);
  RUN_TEST(test_winhttp_send_null_checks);
  RUN_TEST(test_winhttp_send_fail);
  RUN_TEST(test_winhttp_send_methods_and_headers);
  RUN_TEST(test_winhttp_send_cookies);
  RUN_TEST(test_winhttp_send_chunked);
  RUN_TEST(test_winhttp_send_chunked_abort);
  RUN_TEST(test_winhttp_send_upload_chunked);
  RUN_TEST(test_winhttp_send_multi);
#if !defined(_WIN32)
  RUN_TEST(test_winhttp_send_mock_failures);
  RUN_TEST(test_winhttp_coverage_branches);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_WINHTTP_H */
