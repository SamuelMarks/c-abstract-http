/**
 * @file test_http_winhttp.h
 * @brief Integration tests for WinHTTP Backend.
 *
 * Verifies initialization, handle lifecycle management, configuration
 * application, and error handling.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_WINHTTP_H
#define TEST_HTTP_WINHTTP_H

#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_winhttp.h>
#include <c_abstract_http/str.h>

TEST test_winhttp_lifecycle(void) {
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  int rc;

  /* Global init */
  rc = http_winhttp_global_init();
  if(rc!=0) printf("FAILING RC: %d\n", rc); ASSERT_EQ(0, rc);

  /* Context init */
  rc = http_winhttp_context_init(&ctx);
  if(rc!=0) printf("FAILING RC: %d\n", rc); ASSERT_EQ(0, rc);
  ASSERT(ctx != NULL);

  /* Cleanup */
  http_winhttp_context_free(ctx);
  http_winhttp_global_cleanup();
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

TEST test_winhttp_config_usage(void) {
  char *_ast_strdup_0 = NULL;
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  int rc;

  http_winhttp_context_init(&ctx);
  http_config_init(&cfg);

  /* Set some values */
  cfg.timeout_ms = 5000;
  /* Disable SSL verification to test flag caching logic */
  cfg.verify_peer = 0;
  cfg.verify_host = 0; /* Note: Implementation needs to map this */
  cfg.follow_redirects = 0;

  rc = http_winhttp_config_apply(ctx, &cfg);
  if(rc!=0) printf("FAILING RC: %d\n", rc); ASSERT_EQ(0, rc);

  /* Test proxy configuration */
  if (cfg.proxy_url)
    free(cfg.proxy_url);
  cfg.proxy_url =
      (c_cdd_strdup("http://127.0.0.1:8888", &_ast_strdup_0), _ast_strdup_0);
  
  if (cfg.proxy_username) free(cfg.proxy_username);
  cfg.proxy_username = (c_cdd_strdup("admin", &_ast_strdup_0), _ast_strdup_0);

  if (cfg.proxy_password) free(cfg.proxy_password);
  cfg.proxy_password = (c_cdd_strdup("secret", &_ast_strdup_0), _ast_strdup_0);

  rc = http_winhttp_config_apply(ctx, &cfg);
  if(rc!=0) printf("FAILING RC: %d\n", rc); ASSERT_EQ(0, rc);

  http_config_free(&cfg);
  http_winhttp_context_free(ctx);
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

TEST test_winhttp_send_fail(void) {
  char *_ast_strdup_1 = NULL;
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  int rc;

  rc = http_winhttp_context_init(&ctx);
  if(rc!=0) printf("FAILING RC: %d\n", rc); ASSERT_EQ(0, rc);

  /* Initialize request */
  http_request_init(&req);
  /* Invalid URL syntax to trigger immediate failure in CrackUrl */
  req.url = (c_cdd_strdup("not_a_url", &_ast_strdup_1), _ast_strdup_1);

  rc = http_winhttp_send(ctx, &req, &res);

  /* Should fail at CrackUrl stage with EINVAL */
  ASSERT(rc != 0);
  ASSERT(res == NULL);

  http_request_free(&req);
  http_winhttp_context_free(ctx);
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

TEST test_winhttp_send_null_checks(void) {
#ifdef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig cfg;
  int rc;

  http_winhttp_context_init(&ctx);
  http_config_init(&cfg);

  /* Send with NULLs */
  rc = http_winhttp_send(ctx, NULL, &res);
  ASSERT_EQ(EINVAL, rc);

  rc = http_winhttp_send(NULL, &req, &res);
  ASSERT_EQ(EINVAL, rc);

  rc = http_winhttp_send(ctx, &req, NULL);
  ASSERT_EQ(EINVAL, rc);

  /* Config Apply with NULLs */
  ASSERT_EQ(EINVAL, http_winhttp_config_apply(NULL, &cfg));
  ASSERT_EQ(EINVAL, http_winhttp_config_apply(ctx, NULL));

  http_config_free(&cfg);
  http_winhttp_context_free(ctx);
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

TEST test_winhttp_stubs(void) {
#ifndef _WIN32
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(0, http_winhttp_global_init());
  http_winhttp_global_cleanup();

  ASSERT_EQ(ENOTSUP, http_winhttp_context_init(&ctx));
  http_winhttp_context_free(NULL);

  ASSERT_EQ(ENOTSUP, http_winhttp_config_apply(NULL, &cfg));
  ASSERT_EQ(ENOTSUP, http_winhttp_send(NULL, &req, &res));
  PASS();
#else
  SKIPm("WinHTTP is supported on this platform");
#endif
}

#include "cdd_test_helpers/mock_server.h"

struct winhttp_TestChunkState {
  int call_count;
  size_t total_bytes;
  int abort_on_call;
};

static int winhttp_mock_chunk_cb(void *user_data, const void *chunk, size_t chunk_len) {
  struct winhttp_TestChunkState *state = (struct winhttp_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return ECANCELED; /* Abort */
  }
  return 0;
}

TEST test_winhttp_send_chunked(void) {
#ifdef _WIN32
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct winhttp_TestChunkState state;
  int rc;
  char url[128];
  char *_ast_strdup_2 = NULL;

  /* Start mock server */
  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_winhttp_global_init();
  http_winhttp_context_init(&ctx);
  http_config_init(&config);
  http_winhttp_config_apply(ctx, &config);

  http_request_init(&req);
  sprintf(url, "http://127.0.0.1:%d/test", mock_server_get_port(server));
  req.url = (c_cdd_strdup(url, &_ast_strdup_2), _ast_strdup_2);
  
  /* Setup chunk callback */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = winhttp_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_winhttp_send(ctx, &req, &res);

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
  http_winhttp_context_free(ctx);
  http_winhttp_global_cleanup();
  mock_server_destroy(server);
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

TEST test_winhttp_send_chunked_abort(void) {
#ifdef _WIN32
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct winhttp_TestChunkState state;
  int rc;
  char url[128];
  char *_ast_strdup_3 = NULL;

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_winhttp_global_init();
  http_winhttp_context_init(&ctx);
  http_config_init(&config);
  http_winhttp_config_apply(ctx, &config);

  http_request_init(&req);
  sprintf(url, "http://127.0.0.1:%d/test", mock_server_get_port(server));
  req.url = (c_cdd_strdup(url, &_ast_strdup_3), _ast_strdup_3);
  
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1; /* Abort immediately on first chunk */
  req.on_chunk = winhttp_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_winhttp_send(ctx, &req, &res);

  /* Should return the error code returned by our callback (ECANCELED) */
  ASSERT_EQ(ECANCELED, rc);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_winhttp_context_free(ctx);
  http_winhttp_global_cleanup();
  mock_server_destroy(server);
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

struct winhttp_TestUploadState {
  const char *data;
  size_t len;
  size_t pos;
};

static int winhttp_mock_upload_cb(void *user_data, void *buf, size_t buf_len, size_t *out_read) {
  struct winhttp_TestUploadState *state = (struct winhttp_TestUploadState *)user_data;
  size_t remaining = state->len - state->pos;
  size_t to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return 0;
}

TEST test_winhttp_send_upload_chunked(void) {
#ifdef _WIN32
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct winhttp_TestUploadState up_state;
  int rc;
  char url[128];
  char *_ast_strdup_4 = NULL;
  const char *payload = "UPLOAD_TEST_DATA";

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_winhttp_global_init();
  http_winhttp_context_init(&ctx);
  http_config_init(&config);
  http_winhttp_config_apply(ctx, &config);

  http_request_init(&req);
  sprintf(url, "http://127.0.0.1:%d/test", mock_server_get_port(server));
  req.url = (c_cdd_strdup(url, &_ast_strdup_4), _ast_strdup_4);
  req.method = HTTP_POST;

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  req.read_chunk = winhttp_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  rc = http_winhttp_send(ctx, &req, &res);

  ASSERT_EQ(0, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);

  ASSERT_EQ(up_state.len, up_state.pos);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_winhttp_context_free(ctx);
  http_winhttp_global_cleanup();
  mock_server_destroy(server);
#else
  SKIPm("WinHTTP not supported on this platform");
#endif
  PASS();
}

SUITE(http_winhttp_suite) {
  RUN_TEST(test_winhttp_stubs);
  RUN_TEST(test_winhttp_lifecycle);
  RUN_TEST(test_winhttp_config_usage);
  RUN_TEST(test_winhttp_send_fail);
  RUN_TEST(test_winhttp_send_null_checks);
  RUN_TEST(test_winhttp_send_chunked);
  RUN_TEST(test_winhttp_send_chunked_abort);
  RUN_TEST(test_winhttp_send_upload_chunked);
}

#endif /* TEST_HTTP_WINHTTP_H */
