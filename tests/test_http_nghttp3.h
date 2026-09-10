/**
 * @file test_http_nghttp3.h
 * @brief Integration tests for nghttp3 Backend.
 *
 * Verifies that the nghttp3 wrapper correctly initializes, handles
 * configuration, dispatches requests, and handles errors.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_NGHTTP3_H
#define TEST_HTTP_NGHTTP3_H

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
#include <c_abstract_http/http_nghttp3.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error c_abstract_http_test_nghttp3_callbacks(void);
#endif

/**
 * @brief Chunk callback state for testing nghttp3 streaming.
 */
struct nghttp3_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes received */
  size_t total_bytes;
  /** @brief Flag to abort */
  int abort;
};

/**
 * @brief Callback for testing nghttp3 chunked receiving.
 *
 * @param[in,out] user_data Pointer to nghttp3_TestChunkState.
 * @param[in] chunk Pointer to received chunk data.
 * @param[in] chunk_len Number of bytes in chunk.
 * @return 0 on success, non-zero to abort.
 */
static int nghttp3_mock_chunk_cb(void *user_data, const void *chunk,
                                 size_t chunk_len) {
  struct nghttp3_TestChunkState *state;
  (void)chunk;
  state = (struct nghttp3_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort) {
    return (int)C_ABSTRACT_HTTP_ERR_IO;
  }
  return 0;
}

/**
 * @brief Test global lifecycle for nghttp3 backend.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_nghttp3_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_global_init());
  g_mock_nghttp3_global_init_fail = 0;
#endif

  PASS();
}

/**
 * @brief Test context creation and teardown for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_context_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_nghttp3_context_free(ctx);
  http_nghttp3_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_nghttp3_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_context_init(&ctx));
  g_mock_nghttp3_context_init_fail = 0;

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_context_init(&ctx));
  g_mock_alloc_fail = 0;

  g_mock_nghttp3_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_context_init(&ctx));
  g_mock_nghttp3_config_init_fail = 0;

  g_mock_nghttp3_conn_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_context_init(&ctx));
  g_mock_nghttp3_conn_new_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test configuration apply for nghttp3 backend.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_config_application(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  char *_ast_strdup_user;
  char *_ast_strdup_proxy;

  ctx = NULL;
  _ast_strdup_user = NULL;
  _ast_strdup_proxy = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_nghttp3_config_apply(NULL, &config));

  config.timeout_ms = 1000;
  config.verify_peer = 1;
  config.verify_host = 1;
  config.follow_redirects = 1;
  config.version_mask = HTTP_VERSION_3;

  c_abstract_http_mock_strdup("Nghttp3Client/1.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  c_abstract_http_mock_strdup("http://proxy.internal:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Re-apply with new strings to cover freeing previous strings */
  http_config_free(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  _ast_strdup_user = NULL;
  _ast_strdup_proxy = NULL;
  c_abstract_http_mock_strdup("Nghttp3Client/2.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  c_abstract_http_mock_strdup("http://proxy2.internal:8080",
                              &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Apply with NULL user_agent and proxy_url to clear them */
  http_config_free(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  free(config.user_agent);
  config.user_agent = NULL;
  config.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Apply again with NULL user_agent and proxy_url when already NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* strdup fail for user_agent in config apply */
  _ast_strdup_user = NULL;
  c_abstract_http_mock_strdup("Nghttp3Client/1.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_config_apply(ctx, &config));
  g_mock_alloc_fail = 0;

  /* strdup fail for proxy_url only */
  free(config.user_agent);
  config.user_agent = NULL;
  _ast_strdup_proxy = NULL;
  c_abstract_http_mock_strdup("http://proxy.internal:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_config_apply(ctx, &config));
  g_mock_alloc_fail = 0;
#endif

  http_config_free(&config);
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test send validation for nghttp3 backend.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx;
  struct HttpResponse *res;
  struct HttpRequest req;

  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* NULL ctx */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, NULL));

  /* Not configured yet */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, &res));

  http_request_free(&req);
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test sending single request and error paths for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_send_success_and_failures(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  struct HttpRequest req;
  struct HttpResponse *res;
  char *url_str;

  ctx = NULL;
  res = NULL;
  url_str = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_mock_strdup("https://example.com/h3", &url_str);
  req.url = url_str;

  /* Normal success */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(res->body != NULL);
  ASSERT(res->body_len > 0);
  http_response_free(res);
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Response alloc fail */
  g_mock_nghttp3_response_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_response_alloc_fail = 0;

  /* Response init fail */
  g_mock_nghttp3_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_response_init_fail = 0;

  /* Send fail */
  g_mock_nghttp3_send_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_send_fail = 0;

  /* Body strdup fail */
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;
#endif

  http_request_free(&req);
  http_config_free(&config);
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test sending with chunk streaming for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_send_chunked(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  struct HttpRequest req;
  struct HttpResponse *res;
  struct nghttp3_TestChunkState state;
  char *url_str;

  ctx = NULL;
  res = NULL;
  url_str = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_mock_strdup("https://example.com/h3", &url_str);
  req.url = url_str;

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort = 0;
  req.on_chunk = nghttp3_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(NULL, res->body);
  ASSERT_EQ(0, res->body_len);
  ASSERT_EQ(1, state.call_count);
  ASSERT(state.total_bytes > 0);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Abort on chunk callback */
  state.abort = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  ASSERT(res == NULL);

  http_request_free(&req);
  http_config_free(&config);
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test multi request sending for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_send_multi(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  struct ModalityEventLoop *loop;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpRequest *reqs[2];
  struct HttpMultiRequest multi;
  struct HttpFuture *futures[2];
  char *url1;
  char *url2;

  ctx = NULL;
  loop = NULL;
  url1 = NULL;
  url2 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
  c_abstract_http_mock_strdup("https://example.com/h3_1", &url1);
  c_abstract_http_mock_strdup("https://example.com/h3_2", &url2);
  req1.url = url1;
  req2.url = url2;

  reqs[0] = &req1;
  reqs[1] = &req2;
  multi.requests = reqs;
  multi.count = 2;

  futures[0] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[1] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  ASSERT(futures[0] != NULL && futures[1] != NULL);

  /* Invalid arguments */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_nghttp3_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_nghttp3_send_multi(ctx, NULL, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_nghttp3_send_multi(ctx, NULL, &multi, NULL));

  /* Multi send with loop == NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_nghttp3_send_multi(ctx, NULL, &multi, futures));
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

  /* Multi send with loop != NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_nghttp3_send_multi(ctx, loop, &multi, futures));
  http_response_free(futures[0]->response);
  free(futures[0]->response);
  futures[0]->response = NULL;
  http_response_free(futures[1]->response);
  free(futures[1]->response);
  futures[1]->response = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Loop wakeup failure */
  g_mock_nghttp3_loop_wakeup_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_nghttp3_send_multi(ctx, loop, &multi, futures));
  g_mock_nghttp3_loop_wakeup_fail = 0;
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

  /* Single request failure during multi */
  g_mock_nghttp3_send_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_nghttp3_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, futures[0]->error_code);
  g_mock_nghttp3_send_fail = 0;
#endif

  http_loop_free(loop);
  free(futures[0]);
  free(futures[1]);
  http_request_free(&req1);
  http_request_free(&req2);
  http_config_free(&config);
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test callback execution for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_callbacks(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_nghttp3_callbacks());
#endif
  PASS();
}

/**
 * @brief Test mock getters for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_mock_coverage(void) {
  abstract_http_mock_get_g_mock_nghttp3_global_init_fail();
  abstract_http_mock_get_g_mock_nghttp3_context_init_fail();
  abstract_http_mock_get_g_mock_nghttp3_config_init_fail();
  abstract_http_mock_get_g_mock_nghttp3_conn_new_fail();
  abstract_http_mock_get_g_mock_nghttp3_response_alloc_fail();
  abstract_http_mock_get_g_mock_nghttp3_response_init_fail();
  abstract_http_mock_get_g_mock_nghttp3_send_fail();
  abstract_http_mock_get_g_mock_nghttp3_loop_wakeup_fail();
  PASS();
}

/** @brief Suite definition for nghttp3 backend */
SUITE(http_nghttp3_suite) {
  RUN_TEST(test_nghttp3_global_lifecycle);
  RUN_TEST(test_nghttp3_context_lifecycle);
  RUN_TEST(test_nghttp3_config_application);
  RUN_TEST(test_nghttp3_send_invalid_arguments);
  RUN_TEST(test_nghttp3_send_success_and_failures);
  RUN_TEST(test_nghttp3_send_chunked);
  RUN_TEST(test_nghttp3_send_multi);
  RUN_TEST(test_nghttp3_callbacks);
  RUN_TEST(test_nghttp3_mock_coverage);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_NGHTTP3_H */
