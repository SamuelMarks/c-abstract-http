/**
 * @file test_http_wininet.h
 * @brief Unit tests for the WinInet Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, parameter validation, chunking, and error handling.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_WININET_H
#define TEST_HTTP_WININET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wininet.h>
#include "mock_alloc.h"
#include "abstract_http_test_helpers/mock_server.h"
/* clang-format on */

/** @brief Helper structure for chunk testing */
struct wininet_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes */
  size_t total_bytes;
  /** @brief Abort on call */
  int abort_on_call;
};

static int wininet_chunk_cb(void *user_data, const void *data, size_t length) {
  struct wininet_TestChunkState *state =
      (struct wininet_TestChunkState *)user_data;
  (void)data;
  state->call_count++;
  state->total_bytes += length;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return 1;
  }
  return 0;
}

static int wininet_read_cb(void *user_data, void *buffer, size_t max_len,
                           size_t *out_len) {
  struct wininet_TestChunkState *state =
      (struct wininet_TestChunkState *)user_data;
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

/** @brief Test WinInet lifecycle */
TEST test_wininet_lifecycle(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpTransportContext *ctx = NULL;

  /* Global init/cleanup */
  rc = http_wininet_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_wininet_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Context init NULL check */
  rc = http_wininet_context_init(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Context init success */
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  /* Free NULL and ctx */
  http_wininet_context_free(NULL);
  http_wininet_context_free(ctx);

#if !defined(_WIN32)
  g_mock_wininet_context_init_fail = 1;
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_context_init_fail = 0;

  g_mock_wininet_open_fail = 1;
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_open_fail = 0;

  g_mock_alloc_fail = 1;
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/** @brief Test WinInet configuration */
TEST test_wininet_config_apply(void) {
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_proxy2 = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Customize */
  config.timeout_ms = 500;
  config.connect_timeout_ms = 600;
  config.read_timeout_ms = 700;
  config.write_timeout_ms = 800;
  config.verify_peer = 0;
  config.verify_host = 0;
  config.follow_redirects = 0;

  /* Test with Proxy Auth */
  c_abstract_http_mock_strdup("admin", &_ast_strdup_proxy);
  config.proxy_username = _ast_strdup_proxy;

  c_abstract_http_mock_strdup("secret", &_ast_strdup_proxy2);
  config.proxy_password = _ast_strdup_proxy2;

  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Timeout ms only */
  config.timeout_ms = 500;
  config.connect_timeout_ms = 0;
  config.read_timeout_ms = 0;
  config.write_timeout_ms = 0;
  config.verify_peer = 1;
  config.verify_host = 0;
  config.follow_redirects = 1;
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Verify peer 0, verify host 1 */
  config.verify_peer = 0;
  config.verify_host = 1;
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if !defined(_WIN32)
  /* send timeout fail */
  config.timeout_ms = 0;
  config.connect_timeout_ms = 0;
  config.write_timeout_ms = 500;
  config.read_timeout_ms = 0;
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* recv timeout fail */
  config.write_timeout_ms = 0;
  config.read_timeout_ms = 500;
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* proxy username strdup fail */
  config.read_timeout_ms = 0;
  c_abstract_http_mock_strdup("admin", &config.proxy_username);
  g_mock_wininet_context_init_fail = 2;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_context_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* proxy password strdup fail */
  c_abstract_http_mock_strdup("secret", &config.proxy_password);
  g_mock_wininet_context_init_fail = 3;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_context_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  config.timeout_ms = 500;
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
#endif

  /* Invalid args */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_wininet_config_apply(NULL, &config));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wininet_config_apply(ctx, NULL));

  http_config_free(&config);
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet send validation */
TEST test_wininet_send_validation(void) {
  char *_ast_strdup_0 = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Null checks */
  rc = http_wininet_send(NULL, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_wininet_send(ctx, NULL, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_wininet_send(ctx, &req, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL URL */
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Malformed URL handling in local testing (CrackUrl check) */
  c_abstract_http_mock_strdup("not-a-valid-url", &_ast_strdup_0);
  req.url = _ast_strdup_0;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Multipart validation */
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &_ast_strdup_0);
  req.url = _ast_strdup_0;
  req.parts.count = 1;
  req.body = NULL;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  req.parts.count = 0;

  http_request_free(&req);
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet methods and headers */
TEST test_wininet_send_methods_and_headers(void) {
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
  methods[5] = HTTP_PATCH;
  methods[6] = HTTP_OPTIONS;
  methods[7] = HTTP_TRACE;
  methods[8] = HTTP_QUERY;
  methods[9] = HTTP_CONNECT;
  methods[10] = (enum HttpMethod)99;

  rc = http_wininet_context_init(&ctx);
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
    rc = http_wininet_send(ctx, &req, &res);
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
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet cookies handling */
TEST test_wininet_send_cookies(void) {
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

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_cookie_jar_init(&jar);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_cookie_jar_set(&jar, "session", "xyz123");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  {
    struct HttpConfig config;
    rc = http_config_init(&config);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    config.cookie_jar = &jar;
    rc = http_wininet_config_apply(ctx, &config);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    http_config_free(&config);
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
  g_mock_wininet_cookie_count = 1;
#endif

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  http_response_free(res);
  free(res);
  http_request_free(&req);
  http_cookie_jar_free(&jar);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet chunked streaming response */
TEST test_wininet_send_chunked(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct wininet_TestChunkState state;
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

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/stream", &url_dup);
#endif
  req.url = url_dup;
  req.on_chunk = wininet_chunk_cb;
  req.on_chunk_user_data = &state;

#if !defined(_WIN32)
  g_mock_wininet_read_chunks = 2;
#endif

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  http_response_free(res);
  free(res);
  http_request_free(&req);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet chunked response abort */
TEST test_wininet_send_chunked_abort(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct wininet_TestChunkState state;
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

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  c_abstract_http_mock_strdup(url_buf, &url_dup);
#else
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/abort", &url_dup);
#endif
  req.url = url_dup;
  req.on_chunk = wininet_chunk_cb;
  req.on_chunk_user_data = &state;

#if !defined(_WIN32)
  g_mock_wininet_read_chunks = 2;
#endif

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);

  http_request_free(&req);
#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet upload chunked */
TEST test_wininet_send_upload_chunked(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct wininet_TestChunkState state;
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

  rc = http_wininet_context_init(&ctx);
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
  req.read_chunk = wininet_read_cb;
  req.read_chunk_user_data = &state;
  req.expected_body_len = 5;

  rc = http_wininet_send(ctx, &req, &res);
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
  req.read_chunk = wininet_read_cb;
  req.read_chunk_user_data = &state;
  req.expected_body_len = 5;

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  http_request_free(&req);

#if defined(_WIN32)
  mock_server_destroy(server);
#endif
  http_wininet_context_free(ctx);
  PASS();
}

#if !defined(_WIN32)
/** @brief Test WinInet send mock failure branches */
TEST test_wininet_send_mock_failures(void) {
  enum c_abstract_http_error rc;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_dup);
  req.url = url_dup;

  /* wUrl alloc fail */
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* crack url fail */
  g_mock_wininet_crack_url_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  g_mock_wininet_crack_url_fail = 0;

  /* URL without path */
  c_abstract_http_mock_strdup("http://127.0.0.1:8080", &url_dup);
  req.url = url_dup;
  rc = http_wininet_send(ctx, &req, &res);
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
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* InternetConnectW fail */
  g_mock_wininet_connect_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_connect_fail = 0;

  /* HttpOpenRequestW fail */
  g_mock_wininet_open_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_open_request_fail = 0;

  /* HttpSendRequestW fail */
  g_mock_wininet_send_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_send_request_fail = 0;

  /* HttpQueryInfoW fail */
  g_mock_wininet_query_info_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_query_info_fail = 0;

  /* readChunk alloc fail */
  g_mock_wininet_read_chunk_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_read_chunk_alloc_fail = 0;

  /* InternetReadFile fail */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_read_file_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_read_file_fail = 0;
  g_mock_wininet_read_chunks = 0;

  /* body realloc fail */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_body_realloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_body_realloc_fail = 0;
  g_mock_wininet_read_chunks = 0;

  /* res alloc fail */
  g_mock_wininet_res_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_res_alloc_fail = 0;

  /* response_init fail */
  g_mock_wininet_response_init_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_response_init_fail = 0;

  /* POST body without read_chunk */
  req.method = HTTP_POST;
  req.body = "payload";
  req.body_len = 7;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  req.body = NULL;
  req.body_len = 0;
  req.method = HTTP_GET;

  /* read_chunk with send_request_ex fail */
  {
    struct wininet_TestChunkState state;
    memset(&state, 0, sizeof(state));
    req.read_chunk = wininet_read_cb;
    req.read_chunk_user_data = &state;
    req.expected_body_len = 5;

    g_mock_wininet_send_request_ex_fail = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_wininet_send_request_ex_fail = 0;

    /* read_chunk with write_file fail */
    memset(&state, 0, sizeof(state));
    g_mock_wininet_write_file_fail = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_wininet_write_file_fail = 0;

    /* read_chunk with end_request fail */
    memset(&state, 0, sizeof(state));
    g_mock_wininet_end_request_fail = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_wininet_end_request_fail = 0;

    req.read_chunk = NULL;
  }

  /* headers_to_wide_block fail */
  rc = http_headers_add(&req.headers, "X-Fail", "Val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  g_mock_alloc_count = 3;
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* HttpAddRequestHeaders fail */
  g_mock_wininet_add_headers_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_add_headers_fail = 0;

  http_request_free(&req);
  http_wininet_context_free(ctx);
  PASS();
}

/** @brief Test WinInet coverage branches */
TEST test_wininet_coverage_branches(void) {
  enum c_abstract_http_error rc;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct HttpCookieJar jar;
  struct wininet_TestChunkState state;

  /* 1. Context init config failure */
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_wininet_context_init(&ctx);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 2. Context init and config apply */
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  config.verify_host = 0;
  config.verify_peer = 1;
  config.follow_redirects = 1;
  c_abstract_http_mock_strdup("admin", &config.proxy_username);
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  config.proxy_username = NULL;
  c_abstract_http_mock_strdup("secret", &config.proxy_password);
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  http_config_free(&config);

  /* 3. Send host/path alloc fail */
  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/path", &url_dup);
  req.url = url_dup;

  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_count = 2;
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 4. Connect fail, proxy options, https flag, open request fail */
  g_mock_wininet_connect_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_connect_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  config.verify_peer = 0;
  config.verify_host = 0;
  config.follow_redirects = 0;
  c_abstract_http_mock_strdup("user", &config.proxy_username);
  c_abstract_http_mock_strdup("pass", &config.proxy_password);
  rc = http_cookie_jar_init(&jar);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_cookie_jar_set(&jar, "ck", "val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  config.cookie_jar = &jar;
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  http_config_free(&config);

  /* InternetSetOptionA failure on proxy */
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* InternetSetOptionA failure on proxy password */
  g_mock_wininet_set_option_fail = 2;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  c_abstract_http_mock_strdup("https://127.0.0.1:8443/secure", &url_dup);
  req.url = url_dup;

  g_mock_wininet_open_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_open_request_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 5. Headers add fail */
  rc = http_headers_add(&req.headers, "X-Test", "Val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* 6. Send request fail without read_chunk */
  g_mock_wininet_send_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_send_request_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 7. Query info fail, readChunk alloc fail, readFile fail */
  g_mock_wininet_query_info_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_query_info_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_wininet_read_chunk_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_chunk_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_wininet_read_file_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_file_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 8. on_chunk abort */
  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;
  req.on_chunk = wininet_chunk_cb;
  req.on_chunk_user_data = &state;
  g_mock_wininet_read_chunks = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_chunks = 0;
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  req.on_chunk = NULL;

  /* 9. body realloc fail, res alloc fail, response_init fail */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_body_realloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_body_realloc_fail = 0;
  g_mock_wininet_read_chunks = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_wininet_res_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_res_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_wininet_response_init_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_response_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* bodyBuf freed on cleanup */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_res_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_chunks = 0;
  g_mock_wininet_res_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* status code 404 */
  g_mock_wininet_status_code = 404;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_status_code = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  /* 10. Cookie parsing in response with semicolon */
  g_mock_wininet_cookie_count = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  http_cookie_jar_free(&jar);
  http_request_free(&req);
  http_wininet_context_free(ctx);
  PASS();
}
#endif

/** @brief WinInet test suite */
SUITE(http_wininet_suite) {
  RUN_TEST(test_wininet_lifecycle);
  RUN_TEST(test_wininet_config_apply);
  RUN_TEST(test_wininet_send_validation);
  RUN_TEST(test_wininet_send_methods_and_headers);
  RUN_TEST(test_wininet_send_cookies);
  RUN_TEST(test_wininet_send_chunked);
  RUN_TEST(test_wininet_send_chunked_abort);
  RUN_TEST(test_wininet_send_upload_chunked);
#if !defined(_WIN32)
  RUN_TEST(test_wininet_send_mock_failures);
  RUN_TEST(test_wininet_coverage_branches);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_WININET_H */
