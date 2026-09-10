/**
 * @file test_http_aria2.h
 * @brief Unit and integration tests for the aria2 backend.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_ARIA2_H
#define TEST_HTTP_ARIA2_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <greatest.h>
#include <c_abstract_http/http_aria2.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* clang-format on */

/**
 * @brief Test lifecycle of aria2 context and global initialization.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_aria2_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_global_cleanup());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_aria2_context_free(NULL);
  http_aria2_context_free(ctx);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_aria2_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_aria2_context_init(&ctx));
  g_mock_aria2_config_init_fail = 0;

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_aria2_context_init(&ctx));
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/**
 * @brief Test applying configuration to aria2 context.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_aria2_config(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig cfg;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_config_apply(NULL, &cfg));

  cfg.timeout_ms = 5000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_config_apply(ctx, &cfg));

  http_config_free(&cfg);
  http_aria2_context_free(ctx);
  PASS();
}

/**
 * @brief Test input validation for http_aria2_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_aria2_send_validation(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_send(NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_send(ctx, &req, NULL));

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_aria2_send(ctx, &req, &res));

  http_request_free(&req);
  http_aria2_context_free(ctx);
  PASS();
}

/**
 * @brief Test successful sending of request via aria2.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_aria2_send_success(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  char *_ast_strdup_0;
  enum c_abstract_http_error rc;
  ctx = NULL;
  res = NULL;
  _ast_strdup_0 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  req.url = (c_abstract_http_mock_strdup("http://127.0.0.1:8080/test",
                                         &_ast_strdup_0),
             _ast_strdup_0);

  rc = http_aria2_send(ctx, &req, &res);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(res->body != NULL);
  ASSERT(res->body_len > 0);

  http_response_free(res);
  free(res);
#else
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    ASSERT(res != NULL);
    http_response_free(res);
    free(res);
  } else {
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  }
#endif
  http_request_free(&req);
  http_aria2_context_free(ctx);
  PASS();
}

/**
 * @brief Test failure paths for http_aria2_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_aria2_send_failures(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  char *_ast_strdup_0;
  ctx = NULL;
  res = NULL;
  (void)res;
  _ast_strdup_0 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  req.url = (c_abstract_http_mock_strdup("http://127.0.0.1:8080/test",
                                         &_ast_strdup_0),
             _ast_strdup_0);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* system() failure */
  g_mock_aria2_system_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_aria2_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_aria2_system_fail = 0;

  /* Empty file returned (file_size == 0) */
  g_mock_aria2_system_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(0, (int)res->body_len);
  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_aria2_system_fail = 0;

  /* fopen failure */
  g_mock_aria2_fopen_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_aria2_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_aria2_fopen_fail = 0;

  /* http_response_init failure */
  g_mock_aria2_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_aria2_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_aria2_response_init_fail = 0;

  /* calloc failure for HttpResponse */
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_aria2_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_alloc_fail = 0;

  /* malloc failure for body */
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_aria2_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_alloc_fail = 0;
#endif

  http_request_free(&req);
  http_aria2_context_free(ctx);
  PASS();
}

/**
 * @brief Test sending multiple requests concurrently via aria2.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_aria2_send_multi(void) {
  struct HttpTransportContext *ctx;
  struct HttpMultiRequest multi;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpRequest *requests[2];
  struct HttpFuture *futures[2];
  char *_ast_strdup_1;
  char *_ast_strdup_2;
  enum c_abstract_http_error rc;
  ctx = NULL;
  _ast_strdup_1 = NULL;
  _ast_strdup_2 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_aria2_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));

  req1.url =
      (c_abstract_http_mock_strdup("http://127.0.0.1:8080/1", &_ast_strdup_1),
       _ast_strdup_1);
  req2.url =
      (c_abstract_http_mock_strdup("http://127.0.0.1:8080/2", &_ast_strdup_2),
       _ast_strdup_2);

  requests[0] = &req1;
  requests[1] = &req2;
  multi.requests = requests;
  multi.count = 2;

  futures[0] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[1] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  ASSERT(futures[0] != NULL);
  ASSERT(futures[1] != NULL);

  /* Validation checks */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_aria2_send_multi(NULL, NULL, &multi, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_aria2_send_multi(ctx, NULL, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_aria2_send_multi(ctx, NULL, &multi, NULL));

  /* Successful multi send */
  rc = http_aria2_send_multi(ctx, NULL, &multi, futures);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(1, futures[1]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[0]->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[1]->error_code);
  ASSERT(futures[0]->response != NULL);
  ASSERT(futures[1]->response != NULL);

  http_response_free(futures[0]->response);
  free(futures[0]->response);
  http_response_free(futures[1]->response);
  free(futures[1]->response);
#else
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    if (futures[0]->response) {
      http_response_free(futures[0]->response);
      free(futures[0]->response);
    }
    if (futures[1]->response) {
      http_response_free(futures[1]->response);
      free(futures[1]->response);
    }
  } else {
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  }
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Error in multi send */
  futures[0]->response = NULL;
  futures[1]->response = NULL;
  g_mock_aria2_system_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_aria2_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, futures[0]->error_code);
  g_mock_aria2_system_fail = 0;
#endif

  free(futures[0]);
  free(futures[1]);
  http_request_free(&req1);
  http_request_free(&req2);
  http_aria2_context_free(ctx);
  PASS();
}

/**
 * @brief Suite definition for aria2 backend.
 */
SUITE(http_aria2_suite) {
  RUN_TEST(test_http_aria2_lifecycle);
  RUN_TEST(test_http_aria2_config);
  RUN_TEST(test_http_aria2_send_validation);
  RUN_TEST(test_http_aria2_send_success);
  RUN_TEST(test_http_aria2_send_failures);
  RUN_TEST(test_http_aria2_send_multi);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_ARIA2_H */
