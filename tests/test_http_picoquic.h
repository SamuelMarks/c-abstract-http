/**
 * @file test_http_picoquic.h
 * @brief Unit and integration tests for the picoquic backend.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_PICOQUIC_H
#define TEST_HTTP_PICOQUIC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_picoquic.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

/**
 * @brief Test picoquic global lifecycle.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_picoquic_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Test picoquic context lifecycle and allocation failures.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_picoquic_context_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_picoquic_context_free(ctx);
  http_picoquic_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Test context free with NULL quic */
  g_mock_picoquic_quic_null_on_free = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT(ctx != NULL);
  http_picoquic_context_free(ctx);
  g_mock_picoquic_quic_null_on_free = 0;

  g_mock_picoquic_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_context_init(&ctx));
  g_mock_picoquic_config_init_fail = 0;

  g_mock_picoquic_create_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_context_init(&ctx));
  g_mock_picoquic_create_fail = 0;

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_context_init(&ctx));
  g_mock_alloc_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Test picoquic configuration application.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_picoquic_config_application(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_picoquic_config_apply(NULL, &config));

  config.version_mask = HTTP_VERSION_3;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));

  config.version_mask = HTTP_VERSION_1_1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));

  http_config_free(&config);
  http_picoquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Test input validation for http_picoquic_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_picoquic_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx;
  struct HttpResponse *res;
  struct HttpRequest req;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, &req, NULL));

  /* Not configured yet */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, &req, &res));

  http_request_free(&req);
  http_picoquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Test successful sending and error paths for http_picoquic_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_picoquic_send_success_and_failures(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  struct HttpRequest req;
  struct HttpResponse *res;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  http_response_free(res);
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_picoquic_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_picoquic_response_init_fail = 0;

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_alloc_fail = 0;
#endif

  http_request_free(&req);
  http_config_free(&config);
  http_picoquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Test sending multiple requests via picoquic.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_picoquic_send_multi(void) {
  struct HttpTransportContext *ctx;
  struct HttpTransportContext *unconfigured_ctx;
  struct HttpConfig config;
  struct HttpMultiRequest multi;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpRequest *requests[2];
  struct HttpFuture *futures[2];
  ctx = NULL;
  unconfigured_ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_picoquic_context_init(&unconfigured_ctx));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));

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
            http_picoquic_send_multi(NULL, NULL, &multi, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_picoquic_send_multi(ctx, NULL, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_picoquic_send_multi(ctx, NULL, &multi, NULL));

  /* Successful multi send */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_picoquic_send_multi(ctx, NULL, &multi, futures));
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

  /* Error path when request fails (unconfigured context) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_picoquic_send_multi(unconfigured_ctx, NULL, &multi, futures));
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, futures[0]->error_code);

  free(futures[0]);
  free(futures[1]);
  http_request_free(&req1);
  http_request_free(&req2);
  http_config_free(&config);
  http_picoquic_context_free(ctx);
  http_picoquic_context_free(unconfigured_ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Suite definition for picoquic backend.
 */
SUITE(http_picoquic_suite) {
  RUN_TEST(test_picoquic_global_lifecycle);
  RUN_TEST(test_picoquic_context_lifecycle);
  RUN_TEST(test_picoquic_config_application);
  RUN_TEST(test_picoquic_send_invalid_arguments);
  RUN_TEST(test_picoquic_send_success_and_failures);
  RUN_TEST(test_picoquic_send_multi);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_PICOQUIC_H */
