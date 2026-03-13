/**
 * @file test_http_wininet.h
 * @brief Unit tests for the WinInet Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, and parameter validation.
 * Note: Actual network tests are skipped on non-Windows platforms.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_WININET_H
#define TEST_HTTP_WININET_H

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wininet.h>
#include <c_abstract_http/str.h>

#include "cdd_test_helpers/mock_server.h"

/* clang-format on */
TEST test_wininet_lifecycle(void) {
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  int rc;

  /* Init */
  rc = http_wininet_global_init();
  ASSERT_EQ(0, rc);

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(0, rc);
  ASSERT(ctx != NULL);

  /* Cleanup */
  http_wininet_context_free(ctx);
  http_wininet_global_cleanup();
#else
  SKIPm("WinInet not supported on this platform");
#endif
  PASS();
}

TEST test_wininet_config_apply(void) {
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_proxy2 = NULL;
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  int rc;

  http_wininet_context_init(&ctx);
  http_config_init(&config);

  /* Customize */
  config.timeout_ms = 500;
  config.verify_peer = 0;      /* Insecure */
  config.follow_redirects = 0; /* Test redirect disable */

  /* Test with Proxy Auth */
  config.proxy_username =
      (c_cdd_strdup("admin", &_ast_strdup_proxy), _ast_strdup_proxy);
  config.proxy_password =
      (c_cdd_strdup("secret", &_ast_strdup_proxy2), _ast_strdup_proxy2);

  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(0, rc);

  /* Invalid args */
  ASSERT_EQ(EINVAL, http_wininet_config_apply(NULL, &config));
  ASSERT_EQ(EINVAL, http_wininet_config_apply(ctx, NULL));

  http_config_free(&config);
  http_wininet_context_free(ctx);
#else
  SKIPm("WinInet not supported on this platform");
#endif
  PASS();
}

TEST test_wininet_send_validation(void) {
  char *_ast_strdup_0 = NULL;
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  int rc;

  http_wininet_context_init(&ctx);
  http_request_init(&req);

  /* Null checks */
  rc = http_wininet_send(NULL, &req, &res);
  ASSERT_EQ(EINVAL, rc);

  rc = http_wininet_send(ctx, NULL, &res);
  ASSERT_EQ(EINVAL, rc);

  rc = http_wininet_send(ctx, &req, NULL);
  ASSERT_EQ(EINVAL, rc);

  /* Malformed URL handling in local testing (CrackUrl check) */
  req.url = (c_cdd_strdup("not-a-valid-url", &_ast_strdup_0), _ast_strdup_0);
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(EINVAL, rc);

  http_request_free(&req);
  http_wininet_context_free(ctx);
#else
  SKIPm("WinInet not supported on this platform");
#endif
  PASS();
}

TEST test_wininet_stubs(void) {
#ifndef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(0, http_wininet_global_init());
  http_wininet_global_cleanup();

  ASSERT_EQ(ENOTSUP, http_wininet_context_init(&ctx));
  http_wininet_context_free(NULL);

  ASSERT_EQ(ENOTSUP, http_wininet_config_apply(NULL, &cfg));
  ASSERT_EQ(ENOTSUP, http_wininet_send(NULL, &req, &res));
  PASS();
#else
  SKIPm("WinInet is supported on this platform");
#endif
}

struct wininet_TestChunkState {
  int call_count;
  size_t total_bytes;
  int abort_on_call;
};

static int wininet_mock_chunk_cb(void *user_data, const void *chunk,
                                 size_t chunk_len) {
  struct wininet_TestChunkState *state =
      (struct wininet_TestChunkState *)user_data;
  (void)chunk;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return ECANCELED; /* Abort */
  }
  return 0;
}

TEST test_wininet_send_chunked(void) {
#ifdef _WIN32
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct wininet_TestChunkState state;
  int rc;
  char url[128];
  char *_ast_strdup_2 = NULL;

  /* Start mock server */
  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_wininet_global_init();
  http_wininet_context_init(&ctx);
  http_config_init(&config);
  http_wininet_config_apply(ctx, &config);

  http_request_init(&req);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url, "http://127.0.0.1:%d/test", math_mock_server_get_port(server));
#endif
  req.url = (c_cdd_strdup(url, &_ast_strdup_2), _ast_strdup_2);

  /* Setup chunk callback */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = wininet_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_wininet_send(ctx, &req, &res);

  ASSERT_EQ(0, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  /* Body should not be populated when on_chunk is used */
  ASSERT_EQ(NULL, res->body);
  ASSERT_EQ(0, res->body_len);

  /* Verify chunk callback was called and read data ("OK" from mock server) */
  ASSERT(state.call_count > 0);
  ASSERT(state.total_bytes > 0);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_wininet_context_free(ctx);
  http_wininet_global_cleanup();
  mock_server_destroy(server);
#else
  SKIPm("WinInet not supported on this platform");
#endif
  PASS();
}

TEST test_wininet_send_chunked_abort(void) {
#ifdef _WIN32
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct wininet_TestChunkState state;
  int rc;
  char url[128];
  char *_ast_strdup_3 = NULL;

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_wininet_global_init();
  http_wininet_context_init(&ctx);
  http_config_init(&config);
  http_wininet_config_apply(ctx, &config);

  http_request_init(&req);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url, "http://127.0.0.1:%d/test", math_mock_server_get_port(server));
#endif
  req.url = (c_cdd_strdup(url, &_ast_strdup_3), _ast_strdup_3);

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1; /* Abort immediately on first chunk */
  req.on_chunk = wininet_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_wininet_send(ctx, &req, &res);

  /* Should return the error code returned by our callback (ECANCELED) */
  ASSERT_EQ(ECANCELED, rc);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_wininet_context_free(ctx);
  http_wininet_global_cleanup();
  mock_server_destroy(server);
#else
  SKIPm("WinInet not supported on this platform");
#endif
  PASS();
}

struct wininet_TestUploadState {
  const char *data;
  size_t len;
  size_t pos;
};

static int wininet_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                                  size_t *out_read) {
  struct wininet_TestUploadState *state =
      (struct wininet_TestUploadState *)user_data;
  size_t remaining = state->len - state->pos;
  size_t to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return 0;
}

TEST test_wininet_send_upload_chunked(void) {
#ifdef _WIN32
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct wininet_TestUploadState up_state;
  int rc;
  char url[128];
  char *_ast_strdup_4 = NULL;
  const char *payload = "UPLOAD_TEST_DATA";

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_wininet_global_init();
  http_wininet_context_init(&ctx);
  http_config_init(&config);
  http_wininet_config_apply(ctx, &config);

  http_request_init(&req);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url, "http://127.0.0.1:%d/test", math_mock_server_get_port(server));
#endif
  req.url = (c_cdd_strdup(url, &_ast_strdup_4), _ast_strdup_4);
  req.method = HTTP_POST;

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  req.read_chunk = wininet_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  rc = http_wininet_send(ctx, &req, &res);

  ASSERT_EQ(0, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);

  ASSERT_EQ(up_state.len, up_state.pos);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_wininet_context_free(ctx);
  http_wininet_global_cleanup();
  mock_server_destroy(server);
#else
  SKIPm("WinInet not supported on this platform");
#endif
  PASS();
}

SUITE(http_wininet_suite) {
  RUN_TEST(test_wininet_stubs);
  RUN_TEST(test_wininet_lifecycle);
  RUN_TEST(test_wininet_config_apply);
  RUN_TEST(test_wininet_send_validation);
  RUN_TEST(test_wininet_send_chunked);
  RUN_TEST(test_wininet_send_chunked_abort);
  RUN_TEST(test_wininet_send_upload_chunked);
}

#endif /* TEST_HTTP_WININET_H */
