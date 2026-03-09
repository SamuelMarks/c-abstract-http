/**
 * @file test_http_curl.h
 * @brief Integration tests for Libcurl Backend.
 *
 * Verifies that the Curl wrapper correctly initializes, handles configuration,
 * sends requests, and maps failures to errno.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_CURL_H
#define TEST_HTTP_CURL_H

#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <string.h>

#include <c_abstract_http/http_curl.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/str.h>

/* Helper: Build a request to localhost on a port likely to be closed */
static int setup_request(struct HttpRequest *req, int port) {
  char *_ast_strdup_0 = NULL;
  int rc;
  char url[64];

  rc = http_request_init(req);
  if (rc != 0)
    return rc;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif

  req->url = (c_cdd_strdup(url, &_ast_strdup_0), _ast_strdup_0);
  return (enum greatest_test_res)0;
}

TEST test_curl_global_lifecycle(void) {
  /* Should succeed and track ref count internally */
  ASSERT_EQ(0, http_curl_global_init());
  ASSERT_EQ(0, http_curl_global_init()); /* Re-entrant check */

  http_curl_global_cleanup();
  http_curl_global_cleanup();
  PASS();
}

TEST test_curl_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;
  int rc;

  http_curl_global_init();

  rc = http_curl_context_init(&ctx);
  ASSERT_EQ(0, rc);
  ASSERT(ctx != NULL);

  http_curl_context_free(ctx);
  /* Double free safety check (should be safe with NULL) */
  http_curl_context_free(NULL);

  http_curl_global_cleanup();
  PASS();
}

TEST test_curl_config_application(void) {
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_user = NULL;
  char *_ast_strdup_pass = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  int rc;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);

  /* Set some values */
  config.timeout_ms = 500;
  config.verify_peer = 0; /* Insecure for testing logic */
  config.follow_redirects = 0;
  config.proxy_url =
      (c_cdd_strdup("http://proxy.local:8080", &_ast_strdup_proxy),
       _ast_strdup_proxy);
  config.proxy_username =
      (c_cdd_strdup("admin", &_ast_strdup_user), _ast_strdup_user);
  config.proxy_password =
      (c_cdd_strdup("secret", &_ast_strdup_pass), _ast_strdup_pass);

  rc = http_curl_config_apply(ctx, &config);
  ASSERT_EQ(0, rc);

  http_config_free(&config);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

TEST test_curl_send_connection_failure(void) {
  /* Expect mapped error (ECONNREFUSED or ETIMEDOUT or EHOSTUNREACH) */
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  int rc;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);

  /* Fast timeout for test speed */
  config.timeout_ms = 50;
  http_curl_config_apply(ctx, &config);

  /* Use an invalid port */
  setup_request(&req, 59999);

  rc = http_curl_send(ctx, &req, &res);

  /* Verify mapping logic.
     Note: On some systems connection refused happens instanly (ECONNREFUSED),
     on others it times out (ETIMEDOUT). Both are valid error mappings
     for this test. */
  if (rc != ECONNREFUSED && rc != ETIMEDOUT && rc != EHOSTUNREACH &&
      rc != EIO) {
    fprintf(stderr, "Unexpected return code: %d (%s)\n", rc, strerror(rc));
    FAIL();
  }

  ASSERT(res == NULL); /* Should not be allocated on failure */

  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

TEST test_curl_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_request_init(&req);

  /* NULL ctx */
  ASSERT_EQ(EINVAL, http_curl_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(EINVAL, http_curl_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(EINVAL, http_curl_send(ctx, &req, NULL));

  /* NULL internal config application */
  ASSERT_EQ(EINVAL, http_curl_config_apply(NULL, NULL));
  ASSERT_EQ(EINVAL, http_curl_config_apply(ctx, NULL));

  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

/*
 * NOTE: Testing a successful request requires a running server.
 * We skip strictly specific success tests here (mocking/stubbing libcurl
 * internals requires complex LD_PRELOAD or weak symbols which is beyond
 * standard C89 unit test scope without heavy frameworks).
 * The failure cases prove the logic integration.
 */

#include "cdd_test_helpers/mock_server.h"

struct curl_TestChunkState {
  int call_count;
  size_t total_bytes;
  int abort_on_call;
};

static int curl_mock_chunk_cb(void *user_data, const void *chunk,
                              size_t chunk_len) {
  struct curl_TestChunkState *state = (struct curl_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return ECANCELED; /* Abort */
  }
  return 0;
}

TEST test_curl_send_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct curl_TestChunkState state;
  int rc;

  /* Start mock server */
  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  setup_request(&req, mock_server_get_port(server));

  /* Setup chunk callback */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = curl_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_curl_send(ctx, &req, &res);

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
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

TEST test_curl_send_chunked_abort(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct curl_TestChunkState state;
  int rc;

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  setup_request(&req, mock_server_get_port(server));

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1; /* Abort immediately on first chunk */
  req.on_chunk = curl_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_curl_send(ctx, &req, &res);

  /* Should return the error code returned by our callback (ECANCELED) */
  ASSERT_EQ(ECANCELED, rc);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

struct curl_TestUploadState {
  const char *data;
  size_t len;
  size_t pos;
};

static int curl_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                               size_t *out_read) {
  struct curl_TestUploadState *state = (struct curl_TestUploadState *)user_data;
  size_t remaining = state->len - state->pos;
  size_t to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return 0;
}

TEST test_curl_send_upload_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct curl_TestUploadState up_state;
  int rc;
  const char *payload = "UPLOAD_TEST_DATA";

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  setup_request(&req, mock_server_get_port(server));
  req.method = HTTP_POST;

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  rc = http_curl_send(ctx, &req, &res);

  ASSERT_EQ(0, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);

  /* Read the payload back to verify? Our mock server doesn't echo it, but it
     reads it. We just check that it completed successfully and our pos
     advanced. */
  ASSERT_EQ(up_state.len, up_state.pos);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

SUITE(http_curl_suite) {
  RUN_TEST(test_curl_global_lifecycle);
  RUN_TEST(test_curl_context_lifecycle);
  RUN_TEST(test_curl_config_application);
  RUN_TEST(test_curl_send_connection_failure);
  RUN_TEST(test_curl_send_invalid_arguments);
  RUN_TEST(test_curl_send_chunked);
  RUN_TEST(test_curl_send_chunked_abort);
  RUN_TEST(test_curl_send_upload_chunked);
}

#endif /* TEST_HTTP_CURL_H */
