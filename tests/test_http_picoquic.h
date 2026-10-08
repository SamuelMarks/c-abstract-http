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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_picoquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/**
 * @brief Test successful sending and error paths for http_picoquic_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
