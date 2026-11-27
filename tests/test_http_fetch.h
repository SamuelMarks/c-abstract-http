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
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_fetch_map_error(int err, c_abstract_http_error_t *out_err);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_fetch_method_str(enum HttpMethod method,
                                      const char **out_str);
#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_fetch_flush_pipes(void);
#endif
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
NO_DISCARD static c_abstract_http_error_t
fetch_mock_chunk_cb(void *user_data, const void *chunk, size_t chunk_len) {
  struct fetch_TestChunkState *state;
  if (chunk) {
    /* pass */
  }
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
NO_DISCARD static c_abstract_http_error_t
fetch_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                     size_t *out_read) {
  struct fetch_TestUploadState *state;
  size_t remaining;
  size_t to_copy;

  state = (struct fetch_TestUploadState *)user_data;
  if (state->abort) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  remaining = state->len - state->pos;
  to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return C_ABSTRACT_HTTP_SUCCESS;
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(NULL));

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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://proxy.local:8080",
                                        &_ast_strdup_proxy));
  config.proxy_url = _ast_strdup_proxy;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("CustomAgent/1.0", &_ast_strdup_user));
  config.user_agent = _ast_strdup_user;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &config));

  /* Re-apply with new user-agent and proxy to cover freeing old strings */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  _ast_strdup_proxy = NULL;
  _ast_strdup_user = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://proxy2.local:8080",
                                        &_ast_strdup_proxy));
  config.proxy_url = _ast_strdup_proxy;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("CustomAgent2/1.0", &_ast_strdup_user));
  config.user_agent = _ast_strdup_user;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &config));

  /* Apply with NULL user_agent and NULL proxy_url to clear them when non-NULL
   */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("CustomAgent/1.0", &_ast_strdup_user));
  config.user_agent = _ast_strdup_user;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_config_apply(ctx, &config));
  g_mock_alloc_fail = 0;

  /* strdup fail for proxy_url only */
  free(config.user_agent);
  config.user_agent = NULL;
  _ast_strdup_proxy = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://proxy.local:8080",
                                        &_ast_strdup_proxy));
  config.proxy_url = _ast_strdup_proxy;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_config_apply(ctx, &config));
  g_mock_alloc_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, NULL));

  /* NULL url */
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_str));
  req.url = url_str;

  /* Multipart not flattened */
  req.parts.count = 1;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));

  /* Multipart flattened (so parts.count > 0 but req.body != NULL) */
  req.body = (void *)"flattened";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("invalid://url", &url_str));
  req.url = url_str;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  free((void *)req.url);
  url_str = NULL;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_str));
  req.url = url_str;
  g_mock_fetch_parse_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));
  g_mock_fetch_parse_fail = 0;

  /* Successful send with NULL user_agent and proxy_url */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_str));
    req.url = url_str;
    req.method = methods[i];

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
    ASSERT(res != NULL);
    ASSERT_EQ(200, res->status_code);

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://proxy.test:8080",
                                        &_ast_strdup_proxy));
  cfg.proxy_url = _ast_strdup_proxy;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("TestAgent/2.0", &_ast_strdup_agent));
  cfg.user_agent = _ast_strdup_agent;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080/test",
                                        &_ast_strdup_url));
  req.url = _ast_strdup_url;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("request_body_payload", &_ast_strdup_body));
  req.body = _ast_strdup_body;
  req.body_len = strlen(_ast_strdup_body);

  /* Normal success */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(res->body != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Empty body response */
  g_mock_fetch_empty_body = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(0, res->body_len);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080/test",
                                        &_ast_strdup_url));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Abort on chunk callback */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
  ASSERT(res == NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080/test",
                                        &_ast_strdup_url));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* expected_body_len == 0 fallback */
  up_state.pos = 0;
  req.expected_body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Both read_chunk and body set so !req->body is false */
  req.body = (void *)payload;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080/multi1",
                                        &_ast_strdup_url1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080/multi2",
                                        &_ast_strdup_url2));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(futures[0]->response));
  free(futures[0]->response);
  futures[0]->response = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(futures[1]->response));
  free(futures[1]->response);
  futures[1]->response = NULL;

  /* Success with loop != NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_fetch_send_multi(ctx, loop, &multi, futures));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(futures[0]->response));
  free(futures[0]->response);
  futures[0]->response = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(futures[1]->response));
  free(futures[1]->response);
  futures[1]->response = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Loop wakeup failure */
  g_mock_fetch_loop_wakeup_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_fetch_send_multi(ctx, loop, &multi, futures));
  g_mock_fetch_loop_wakeup_fail = 0;
  if (futures[0]->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_response_free(futures[0]->response));
    free(futures[0]->response);
    futures[0]->response = NULL;
  }
  if (futures[1]->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_response_free(futures[1]->response));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  free(futures[0]);
  free(futures[1]);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
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
  c_abstract_http_error_t err_out;
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

  {
    int *mock_ptr = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_global_init_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_context_init_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_config_init_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_parse_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_req_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_err_code(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_empty_body(&mock_ptr));
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        abstract_http_mock_get_g_mock_fetch_response_init_fail(&mock_ptr));
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        abstract_http_mock_get_g_mock_fetch_response_alloc_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_upload_alloc_fail(&mock_ptr));
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        abstract_http_mock_get_g_mock_fetch_upload_realloc_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_body_realloc_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_stream_pipe(&mock_ptr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_get_g_mock_fetch_env_fail(&mock_ptr));
  }
  PASS();
}

TEST test_fetch_proxy_auth_timeouts_and_loop(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  int step;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  cfg.timeout_ms = 0;
  cfg.connect_timeout_ms = 1200;
  cfg.read_timeout_ms = 2500;
  cfg.write_timeout_ms = 3500;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:8080", &cfg.proxy_url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("puser", &cfg.proxy_username));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("ppass", &cfg.proxy_password));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  for (step = 0; step < 12; ++step) {
    c_abstract_http_error_t arc;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = step;
    arc = http_fetch_config_apply(ctx, &cfg);
    g_mock_alloc_fail = 0;
    if (arc == C_ABSTRACT_HTTP_SUCCESS) {
      break;
    }
  }
#else
  (void)step;
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"http://127.0.0.1:8080/auth";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Also test proxy_username with NULL proxy_password & read/write timeouts */
  free(cfg.proxy_password);
  cfg.proxy_password = NULL;
  cfg.connect_timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  cfg.read_timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Custom", "1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  cfg.verify_peer = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Content-Type", "text/plain"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  free(cfg.proxy_url);
  cfg.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int k;
    struct HttpMultiRequest m2;
    struct HttpFuture mf;
    struct HttpFuture *mfs[1];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&m2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&m2, &req));
    mfs[0] = &mf;
    for (k = 0; k < 20; ++k) {
      c_abstract_http_error_t orc;
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_fetch_config_apply(ctx, &cfg);
      g_mock_alloc_fail = 0;
      ASSERT((int)orc >= -2000 && (int)orc <= 2000);

      res = NULL;
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_fetch_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }

      memset(&mf, 0, sizeof(mf));
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_fetch_send_multi(ctx, NULL, &m2, mfs);
      g_mock_alloc_fail = 0;
      ASSERT((int)orc >= -2000 && (int)orc <= 2000);
      if (mf.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf.response));
        free(mf.response);
      }
    }
    g_mock_alloc_count = 0;
    {
      struct HttpTransportContext *c2 = NULL;
      struct ModalityEventLoop *floop = NULL;
      struct fetch_TestUploadState ust;
      struct fetch_TestChunkState cst;

      g_mock_alloc_count = 1;
      g_mock_alloc_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_fetch_context_init(&c2));
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&c2));
      g_mock_config_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_context_free(c2));
      g_mock_config_free_fail = 0;
      c2 = NULL;

      memset(&ust, 0, sizeof(ust));
      ust.data = "abc";
      ust.len = 3;
      req.read_chunk = fetch_mock_upload_cb;
      req.read_chunk_user_data = &ust;
      g_mock_headers_init_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
      g_mock_headers_init_fail = 0;

      ust.pos = 0;
      memset(&cst, 0, sizeof(cst));
      cst.abort_on_call = 1;
      req.on_chunk = fetch_mock_chunk_cb;
      req.on_chunk_user_data = &cst;
      g_mock_response_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
      g_mock_response_free_fail = 0;
      req.on_chunk = NULL;
      req.read_chunk = NULL;

      g_mock_fetch_body_realloc_fail = 1;
      g_mock_response_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req, &res));
      g_mock_fetch_body_realloc_fail = 0;
      g_mock_response_free_fail = 0;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&floop));
      {
        c_abstract_http_error_t me = C_ABSTRACT_HTTP_ERR_IO;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_fetch_map_error(0, &me));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, me);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_fetch_map_error(1, &me));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_TIMEOUT, me);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_fetch_map_error(5, &me));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, me);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_fetch_map_error(2, &me));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, me);
      }
      g_mock_fetch_req_fail = 1;
      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, floop, &m2, mfs));

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, floop, &m2, mfs));
      g_mock_fetch_req_fail = 0;

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(1, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, NULL, &m2, mfs));
      if (mf.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf.response));
        free(mf.response);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(2, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, floop, &m2, mfs));
      if (mf.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf.response));
        free(mf.response);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(4, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, NULL, &m2, mfs));

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(5, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, NULL, &m2, mfs));

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, floop, &m2, mfs));
      if (mf.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf.response));
        free(mf.response);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(floop));
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&m2));
  }
#endif

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

TEST test_fetch_cookies_tls_headers_and_status(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const char *cval = NULL;
  const char *hval = NULL;
  unsigned char spki_hash[32];
  char *b64_pin = NULL;
  size_t b64_len = 0;
  char pin_hdr[96];
  static const unsigned char k_fetch_spki[] = {0x30, 0x0d, 0x30, 0x05, 0x06,
                                               0x03, 0x2b, 0x65, 0x70, 0x03,
                                               0x04, 0x00, 0x01, 0x02, 0x03};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_cookie_jar_set(&jar, "f_init", "abc"));

  /* Validation errors for mTLS, version_mask, tls_version_mask */
  cfg.client_cert_path = (char *)"c.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_config_apply(ctx, &cfg));
  cfg.client_cert_path = NULL;
  cfg.client_key_path = (char *)"k.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_config_apply(ctx, &cfg));
  cfg.client_key_path = NULL;
  cfg.ca_buffer_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_config_apply(ctx, &cfg));
  cfg.ca_buffer_len = 0;

  cfg.version_mask = HTTP_VERSION_3;
  cfg.http3_fallback = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_fetch_config_apply(ctx, &cfg));
  cfg.http3_fallback = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  cfg.version_mask = HTTP_VERSION_1_1;

  cfg.tls_version_mask = 0x8000U;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_fetch_config_apply(ctx, &cfg));
  cfg.tls_version_mask = HTTP_TLS_VERSION_1_2;

  /* Valid SPKI SHA-256 pin + CA/mTLS config */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_sha256_oneshot(k_fetch_spki, sizeof(k_fetch_spki),
                                           spki_hash));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_base64_encode(spki_hash, 32, &b64_pin, &b64_len));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(pin_hdr, sizeof(pin_hdr), "sha256//%s", b64_pin);
#else
  sprintf(pin_hdr, "sha256//%s", b64_pin);
#endif
  free(b64_pin);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(pin_hdr, &cfg.cert_pin_sha256));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("ca.pem", &cfg.ca_path));
  cfg.ca_buffer = (unsigned char *)malloc(4);
  memcpy(cfg.ca_buffer, "CERT", 4);
  cfg.ca_buffer_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("cc.pem", &cfg.client_cert_path));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("ck.pem", &cfg.client_key_path));
  cfg.cookie_jar = &jar;
  cfg.follow_redirects = 0;
  cfg.verify_host = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));

  /* HTTPS 201 Created + Set-Cookie + response header extraction */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com:443/set-cookie";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Authorization", "Bearer tok"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Referer", "https://example.com/"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Content-Type", "text/plain"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_FETCH)
  ASSERT_EQ(201, res->status_code);
  ASSERT_STR_EQ("CreatedBody", (const char *)res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&res->headers, "X-Fetch", &hval));
  ASSERT_STR_EQ("1", hval);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_cookie_jar_get(&jar, "fetch_sid", &cval));
  ASSERT_STR_EQ("f88", cval);
#else
  (void)cval;
  (void)hval;
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* HTTPS hostname mismatch & invalid URL & SPKI mismatch */
  req.url = (char *)"https://example.com/bad-cn";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_fetch_send(ctx, &req, &res));
  req.url = (char *)"https://";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_fetch_send(ctx, &req, &res));
  req.url = (char *)"example.com/no-scheme";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* verify_host == 0 + raw body stream (no HTTP/1.x preamble) + explicit Cookie
   */
  cfg.verify_host = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Cookie", "override=1"));
  req.url = (char *)"http://127.0.0.1:8080/raw-body";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127.0.0.1:8080/partial-http";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127.0.0.1:8080/204-empty";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int step;
    struct HttpRequest r_oom;
    for (step = 0; step < 12; ++step) {
      c_abstract_http_error_t arc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      arc = http_fetch_config_apply(ctx, &cfg);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (arc == C_ABSTRACT_HTTP_SUCCESS) {
        break;
      }
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_config_apply(ctx, &cfg));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r_oom));
    r_oom.url = (char *)"http://127.0.0.1:8080/set-cookie";
    /* NULL header key edge */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&r_oom.headers, "X-Test", "1"));
    free(r_oom.headers.headers[0].key);
    r_oom.headers.headers[0].key = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_send(ctx, &r_oom, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    for (step = 0; step < 26; ++step) {
      c_abstract_http_error_t orc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_fetch_send(ctx, &r_oom, &res);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
        break;
      }
    }
    r_oom.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r_oom));
  }
#endif

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(_WIN32) &&                   \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
NO_DISCARD static c_abstract_http_error_t
fetch_test_flush_pipes_timer_cb(struct ModalityEventLoop *loop, int timer_id,
                                void *user_data) {
  (void)loop;
  (void)timer_id;
  (void)user_data;
  return c_abstract_http_test_fetch_flush_pipes();
}
#endif

/**
 * @brief Test non-blocking `fileno(f)` `ModalityEventLoop` `HTTP_LOOP_READ`
 * multiplexing and environment batch error percolation.
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_fetch_nonblocking_pipe_multiplexing(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpMultiRequest empty_multi;
  struct HttpFuture *empty_futs[1];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
  req1.url = (char *)"http://127.0.0.1:8080/nb1";
  req2.url = (char *)"http://127.0.0.1:8080/nb2";

  memset(&empty_multi, 0, sizeof(empty_multi));
  empty_futs[0] = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_fetch_send_multi(ctx, NULL, &empty_multi, empty_futs));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpResponse *res = NULL;
    struct HttpMultiRequest multi;
    struct HttpFuture f1;
    struct HttpFuture f2;
    struct HttpFuture *futs[2];

    /* Environment batch pre/post failures */
    g_mock_fetch_env_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req1, &res));
    g_mock_fetch_env_fail = 8;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req1, &res));
    g_mock_fetch_env_fail = 8;
    g_mock_fetch_req_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_fetch_send(ctx, &req1, &res));
    g_mock_fetch_req_fail = 0;
    g_mock_fetch_env_fail = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req2));
    futs[0] = &f1;
    futs[1] = &f2;

    /* Sync drain failure inside multi (regular file mode) */
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    req2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_fetch_send_multi(ctx, NULL, &multi, futs));
    req2.url = (char *)"http://127.0.0.1:8080/nb2";

    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    g_mock_fetch_body_realloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              http_fetch_send_multi(ctx, NULL, &multi, futs));
    g_mock_fetch_body_realloc_fail = 0;

#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
    {
      struct ModalityEventLoop *loop = NULL;
      int tid = 0;
      const char *hval = NULL;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));

      /* 1. Immediate pipe streams multiplexed via HTTP_LOOP_READ */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      ASSERT_EQ(1, f1.is_ready);
      ASSERT_EQ(1, f2.is_ready);
      ASSERT_EQ(200, f1.response->status_code);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_get(&f1.response->headers, "X-Pipe", &hval));
      ASSERT_STR_EQ("1", hval);
      ASSERT_STR_EQ("PipeStreamPart1", (const char *)f1.response->body);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
      free(f2.response);

      /* 2. Delayed pipe tail exercising EAGAIN backpressure + second wakeup */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 2;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_loop_add_timer(loop, 5, fetch_test_flush_pipes_timer_cb,
                                    NULL, &tid));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      ASSERT_EQ(1, f1.is_ready);
      ASSERT_EQ(1, f2.is_ready);
      ASSERT_STR_EQ("PipeStreamPart1-Part2", (const char *)f1.response->body);
      ASSERT_STR_EQ("PipeStreamPart1-Part2", (const char *)f2.response->body);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
      free(f2.response);

      /* 3. Non-blocking pipe with owned loop (loop == NULL) */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_fetch_send_multi(ctx, NULL, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
      free(f2.response);

      /* 4. Non-blocking fd read EIO failure + on_chunk abort */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 3;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, loop, &multi, futs));

      {
        struct fetch_TestChunkState abt_st;
        memset(&abt_st, 0, sizeof(abt_st));
        abt_st.abort_on_call = 1;
        req1.on_chunk = fetch_mock_chunk_cb;
        req1.on_chunk_user_data = &abt_st;
        memset(&f1, 0, sizeof(f1));
        memset(&f2, 0, sizeof(f2));
        g_mock_fetch_stream_pipe = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                  http_fetch_send_multi(ctx, loop, &multi, futs));
        req1.on_chunk = NULL;
        req1.on_chunk_user_data = NULL;
      }

      /* 5. Non-blocking fd realloc OOM + response_free failure */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 1;
      g_mock_fetch_body_realloc_fail = 1;
      g_mock_response_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      g_mock_fetch_body_realloc_fail = 0;
      g_mock_response_free_fail = 0;

      /* 6. http_loop_add_fd failure on 2nd slot (aborts 1st registered slot) */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(8, 1));
      g_mock_response_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      g_mock_response_free_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(0, 0));

      /* 7. http_loop_remove_fd failure on completion & during sibling abort */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(7, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(0, 0));

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 3;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(7, 1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(0, 0));

      /* 8. http_loop_stop failure on last non-blocking slot completion */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_fetch_stream_pipe = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_fetch_send_multi(ctx, loop, &multi, futs));
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(0, 0));

      g_mock_fetch_stream_pipe = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
    }
#endif

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  }
#endif

  req1.url = NULL;
  req2.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_fetch_global_cleanup());
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
  RUN_TEST(test_fetch_proxy_auth_timeouts_and_loop);
  RUN_TEST(test_fetch_cookies_tls_headers_and_status);
  RUN_TEST(test_fetch_nonblocking_pipe_multiplexing);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_FETCH_H */
