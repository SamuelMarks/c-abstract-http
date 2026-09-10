/**
 * @file test_http_fetch.h
 * @brief Integration tests for Libfetch Backend.
 *
 * Verifies that the Fetch wrapper correctly initializes, handles configuration,
 * sends requests, and maps failures to error enums.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_FETCH_H
#define TEST_HTTP_FETCH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_fetch.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error
c_abstract_http_test_fetch_map_error(int err,
                                     enum c_abstract_http_error *out_err);
extern enum c_abstract_http_error
c_abstract_http_test_fetch_method_str(enum HttpMethod method,
                                      const char **out_str);
#endif

/**
 * @brief State structure for chunk callback tests.
 */
struct fetch_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes received */
  size_t total_bytes;
  /** @brief Abort on specific call */
  int abort_on_call;
};

/**
 * @brief Callback for on_chunk tests.
 *
 * @param[in,out] user_data Pointer to fetch_TestChunkState.
 * @param[in] chunk Pointer to received chunk.
 * @param[in] chunk_len Length of received chunk.
 * @return 0 on success, non-zero to abort.
 */
static int fetch_mock_chunk_cb(void *user_data, const void *chunk,
                               size_t chunk_len) {
  struct fetch_TestChunkState *state;
  (void)chunk;
  state = (struct fetch_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return (int)C_ABSTRACT_HTTP_ERR_IO;
  }
  return 0;
}

/**
 * @brief State structure for upload chunk callback tests.
 */
struct fetch_TestUploadState {
  /** @brief Payload data */
  const char *data;
  /** @brief Payload length */
  size_t len;
  /** @brief Current read position */
  size_t pos;
  /** @brief Flag to abort */
  int abort;
};

/**
 * @brief Callback for read_chunk tests.
 *
 * @param[in,out] user_data Pointer to fetch_TestUploadState.
 * @param[out] buf Destination buffer.
 * @param[in] buf_len Maximum buffer length.
 * @param[out] out_read Pointer to receive number of bytes read.
 * @return 0 on success, non-zero to abort.
 */
static int fetch_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                                size_t *out_read) {
  struct fetch_TestUploadState *state;
  size_t remaining;
  size_t to_copy;

  state = (struct fetch_TestUploadState *)user_data;
  if (state->abort) {
    return (int)C_ABSTRACT_HTTP_ERR_IO;
  }

  remaining = state->len - state->pos;
  to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return 0;
}

/**
 * @brief Test libfetch global lifecycle.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_fetch_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_global_init());
  g_mock_fetch_global_init_fail = 0;
#endif

  PASS();
}

/**
 * @brief Test libfetch context lifecycle.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_context_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_fetch_context_free(ctx);
  http_fetch_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_fetch_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_context_init(&ctx));
  g_mock_fetch_context_init_fail = 0;

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_context_init(&ctx));
  g_mock_alloc_fail = 0;

  g_mock_fetch_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_context_init(&ctx));
  g_mock_fetch_config_init_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch configuration application.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_config_application(void) {
  char *_ast_strdup_proxy;
  char *_ast_strdup_user;
  struct HttpTransportContext *ctx;
  struct HttpConfig config;

  _ast_strdup_proxy = NULL;
  _ast_strdup_user = NULL;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_config_apply(NULL, &config));

  config.timeout_ms = 500;
  config.verify_peer = 0;
  config.follow_redirects = 0;
  c_abstract_http_mock_strdup("http://proxy.local:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  c_abstract_http_mock_strdup("CustomAgent/1.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &config));

  /* Re-apply with new user-agent and proxy to cover freeing old strings */
  http_config_free(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  _ast_strdup_proxy = NULL;
  _ast_strdup_user = NULL;
  c_abstract_http_mock_strdup("http://proxy2.local:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  c_abstract_http_mock_strdup("CustomAgent2/1.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &config));

  /* Apply with NULL user_agent and NULL proxy_url to clear them when non-NULL
   */
  http_config_free(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  free(config.user_agent);
  config.user_agent = NULL;
  config.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &config));

  /* Apply again with NULL user_agent and NULL proxy_url when already NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &config));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* strdup fail for user_agent in config apply */
  _ast_strdup_user = NULL;
  c_abstract_http_mock_strdup("CustomAgent/1.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_config_apply(ctx, &config));
  g_mock_alloc_fail = 0;

  /* strdup fail for proxy_url only */
  free(config.user_agent);
  config.user_agent = NULL;
  _ast_strdup_proxy = NULL;
  c_abstract_http_mock_strdup("http://proxy.local:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_config_apply(ctx, &config));
  g_mock_alloc_fail = 0;
#endif

  http_config_free(&config);
  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch send invalid arguments.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig cfg;
  struct HttpResponse *res;
  struct HttpRequest req;
  char *url_str;

  url_str = NULL;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  free(cfg.user_agent);
  cfg.user_agent = NULL;
  cfg.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  http_config_free(&cfg);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, NULL));

  /* NULL url */
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));

  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_str);
  req.url = url_str;

  /* Multipart not flattened */
  req.parts.count = 1;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));

  /* Multipart flattened (so parts.count > 0 but req.body != NULL) */
  req.body = (void *)"flattened";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  req.parts.count = 0;
  req.body = NULL;

  /* Invalid method */
  req.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));
  req.method = HTTP_GET;

  /* Invalid URL */
  free((void *)req.url);
  url_str = NULL;
  c_abstract_http_mock_strdup("invalid://url", &url_str);
  req.url = url_str;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  free((void *)req.url);
  url_str = NULL;
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_str);
  req.url = url_str;
  g_mock_fetch_parse_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_parse_fail = 0;

  /* Successful send with NULL user_agent and proxy_url */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
#endif

  http_request_free(&req);
  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch send with all HTTP methods.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_send_methods(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  static const enum HttpMethod methods[] = {
      HTTP_GET,   HTTP_POST,    HTTP_PUT,   HTTP_DELETE,  HTTP_HEAD,
      HTTP_PATCH, HTTP_OPTIONS, HTTP_TRACE, HTTP_CONNECT, HTTP_QUERY};
  size_t i;

  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));

  for (i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
    char *url_str;
    url_str = NULL;
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_str);
    req.url = url_str;
    req.method = methods[i];

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
    ASSERT(res != NULL);
    ASSERT_EQ(200, res->status_code);

    http_response_free(res);
    free(res);
    http_request_free(&req);
  }

  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch send success and error paths.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_send_success_and_failures(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res;
  char *_ast_strdup_proxy;
  char *_ast_strdup_agent;
  char *_ast_strdup_body;
  char *_ast_strdup_url;

  _ast_strdup_proxy = NULL;
  _ast_strdup_agent = NULL;
  _ast_strdup_body = NULL;
  _ast_strdup_url = NULL;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  c_abstract_http_mock_strdup("http://proxy.test:8080", &_ast_strdup_proxy);
  cfg.proxy_url = _ast_strdup_proxy;
  c_abstract_http_mock_strdup("TestAgent/2.0", &_ast_strdup_agent);
  cfg.user_agent = _ast_strdup_agent;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &_ast_strdup_url);
  req.url = _ast_strdup_url;
  c_abstract_http_mock_strdup("request_body_payload", &_ast_strdup_body);
  req.body = _ast_strdup_body;
  req.body_len = strlen(_ast_strdup_body);

  /* Normal success */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(res->body != NULL);
  http_response_free(res);
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Empty body response */
  g_mock_fetch_empty_body = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(0, res->body_len);
  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_fetch_empty_body = 0;

  /* fetchReqHTTP fail with various error mappings */
  g_mock_fetch_req_fail = 1;

  g_mock_fetch_err_code = 1; /* FETCH_TIMEOUT */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_TIMEOUT, http_fetch_send(ctx, &req, &res));

  g_mock_fetch_err_code = 5; /* FETCH_MEMORY */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));

  g_mock_fetch_err_code = 2; /* FETCH_DOWN */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));

  g_mock_fetch_err_code = 3; /* FETCH_NETWORK */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));

  g_mock_fetch_err_code = 4; /* FETCH_RESOLV */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));

  g_mock_fetch_err_code = 999; /* default */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));

  g_mock_fetch_req_fail = 0;
  g_mock_fetch_err_code = 0;

  /* calloc fail for HttpResponse */
  g_mock_fetch_response_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_response_alloc_fail = 0;

  /* http_response_init fail */
  g_mock_fetch_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_response_init_fail = 0;

  /* realloc fail for response body */
  g_mock_fetch_body_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_body_realloc_fail = 0;
#endif

  http_request_free(&req);
  http_config_free(&cfg);
  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch streaming download (on_chunk).
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_send_chunked(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  struct fetch_TestChunkState state;
  char *_ast_strdup_url;

  _ast_strdup_url = NULL;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &_ast_strdup_url);
  req.url = _ast_strdup_url;

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = fetch_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(NULL, res->body);
  ASSERT_EQ(0, res->body_len);
  ASSERT(state.call_count > 0);
  ASSERT(state.total_bytes > 0);

  http_response_free(res);
  free(res);
  res = NULL;

  /* Abort on chunk callback */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
  ASSERT(res == NULL);

  http_request_free(&req);
  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch streaming upload (read_chunk).
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_send_upload_chunked(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  struct fetch_TestUploadState up_state;
  const char *payload;
  char large_payload[5000];
  char *_ast_strdup_url;

  _ast_strdup_url = NULL;
  payload = "TEST_UPLOAD_DATA";
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &_ast_strdup_url);
  req.url = _ast_strdup_url;
  req.method = HTTP_POST;

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;
  up_state.abort = 0;

  req.read_chunk = fetch_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(up_state.len, up_state.pos);

  http_response_free(res);
  free(res);
  res = NULL;

  /* expected_body_len == 0 fallback */
  up_state.pos = 0;
  req.expected_body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Both read_chunk and body set so !req->body is false */
  req.body = (void *)payload;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  req.body = NULL;

  /* Large payload causing realloc in upload buffer */
  memset(large_payload, 'X', sizeof(large_payload) - 1);
  large_payload[sizeof(large_payload) - 1] = '\0';
  up_state.data = large_payload;
  up_state.len = sizeof(large_payload) - 1;
  up_state.pos = 0;
  req.expected_body_len = 100; /* smaller than data, triggers growth */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(up_state.len, up_state.pos);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Abort during upload */
  up_state.pos = 0;
  up_state.abort = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
  ASSERT(res == NULL);
  up_state.abort = 0;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Failure paths where upload_body != NULL */
  up_state.pos = 0;
  g_mock_fetch_req_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_req_fail = 0;

  up_state.pos = 0;
  g_mock_fetch_response_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_response_alloc_fail = 0;

  up_state.pos = 0;
  g_mock_fetch_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_response_init_fail = 0;

  up_state.pos = 0;
  g_mock_fetch_body_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_body_realloc_fail = 0;

  /* Malloc fail for upload body */
  up_state.pos = 0;
  g_mock_fetch_upload_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_upload_alloc_fail = 0;

  /* Realloc fail for upload body */
  up_state.pos = 0;
  g_mock_fetch_upload_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_upload_realloc_fail = 0;
#endif

  http_request_free(&req);
  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch send multi.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_send_multi(void) {
  struct HttpTransportContext *ctx;
  struct ModalityEventLoop *loop;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpRequest *reqs[2];
  struct HttpMultiRequest multi;
  struct HttpFuture *futures[2];
  char *_ast_strdup_url1;
  char *_ast_strdup_url2;

  _ast_strdup_url1 = NULL;
  _ast_strdup_url2 = NULL;
  ctx = NULL;
  loop = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/multi1",
                              &_ast_strdup_url1);
  c_abstract_http_mock_strdup("http://127.0.0.1:8080/multi2",
                              &_ast_strdup_url2);
  req1.url = _ast_strdup_url1;
  req2.url = _ast_strdup_url2;

  reqs[0] = &req1;
  reqs[1] = &req2;
  multi.requests = reqs;
  multi.count = 2;

  futures[0] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[1] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  ASSERT(futures[0] != NULL && futures[1] != NULL);

  /* Invalid arguments */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_fetch_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_fetch_send_multi(ctx, NULL, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_fetch_send_multi(ctx, NULL, &multi, NULL));

  /* Success with loop == NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_fetch_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(1, futures[1]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[0]->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[1]->error_code);
  ASSERT(futures[0]->response != NULL);
  ASSERT(futures[1]->response != NULL);

  http_response_free(futures[0]->response);
  free(futures[0]->response);
  futures[0]->response = NULL;
  http_response_free(futures[1]->response);
  free(futures[1]->response);
  futures[1]->response = NULL;

  /* Success with loop != NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_fetch_send_multi(ctx, loop, &multi, futures));

  http_response_free(futures[0]->response);
  free(futures[0]->response);
  futures[0]->response = NULL;
  http_response_free(futures[1]->response);
  free(futures[1]->response);
  futures[1]->response = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Loop wakeup failure */
  g_mock_fetch_loop_wakeup_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_fetch_send_multi(ctx, loop, &multi, futures));
  g_mock_fetch_loop_wakeup_fail = 0;
  if (futures[0]->response) {
    http_response_free(futures[0]->response);
    free(futures[0]->response);
    futures[0]->response = NULL;
  }
  if (futures[1]->response) {
    http_response_free(futures[1]->response);
    free(futures[1]->response);
    futures[1]->response = NULL;
  }

  /* Request failure in multi */
  g_mock_fetch_req_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_fetch_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, futures[0]->error_code);
  g_mock_fetch_req_fail = 0;
#endif

  http_loop_free(loop);
  free(futures[0]);
  free(futures[1]);
  http_request_free(&req1);
  http_request_free(&req2);
  http_fetch_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

/**
 * @brief Test libfetch mock coverage and getters.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_mock_coverage(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  enum c_abstract_http_error err_out;
  const char *str_out;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_test_fetch_map_error(0, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_fetch_map_error(0, &err_out));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, err_out);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_test_fetch_method_str(HTTP_GET, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_fetch_method_str(HTTP_GET, &str_out));
  ASSERT_STR_EQ("GET", str_out);
#endif

  abstract_http_mock_get_g_mock_fetch_global_init_fail();
  abstract_http_mock_get_g_mock_fetch_context_init_fail();
  abstract_http_mock_get_g_mock_fetch_config_init_fail();
  abstract_http_mock_get_g_mock_fetch_parse_fail();
  abstract_http_mock_get_g_mock_fetch_req_fail();
  abstract_http_mock_get_g_mock_fetch_err_code();
  abstract_http_mock_get_g_mock_fetch_empty_body();
  abstract_http_mock_get_g_mock_fetch_response_init_fail();
  abstract_http_mock_get_g_mock_fetch_response_alloc_fail();
  abstract_http_mock_get_g_mock_fetch_upload_alloc_fail();
  abstract_http_mock_get_g_mock_fetch_upload_realloc_fail();
  abstract_http_mock_get_g_mock_fetch_body_realloc_fail();
  abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail();
  PASS();
}

/** @brief Documented suite for libfetch backend */
SUITE(http_fetch_suite) {
  RUN_TEST(test_fetch_global_lifecycle);
  RUN_TEST(test_fetch_context_lifecycle);
  RUN_TEST(test_fetch_config_application);
  RUN_TEST(test_fetch_send_invalid_arguments);
  RUN_TEST(test_fetch_send_methods);
  RUN_TEST(test_fetch_send_success_and_failures);
  RUN_TEST(test_fetch_send_chunked);
  RUN_TEST(test_fetch_send_upload_chunked);
  RUN_TEST(test_fetch_send_multi);
  RUN_TEST(test_fetch_mock_coverage);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_FETCH_H */
