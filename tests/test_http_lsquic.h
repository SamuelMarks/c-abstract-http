/**
 * @file test_http_lsquic.h
 * @brief Integration tests for lsquic Backend.
 *
 * Verifies that the lsquic wrapper correctly initializes and handles
 * configuration.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_LSQUIC_H
#define TEST_HTTP_LSQUIC_H

#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <string.h>

#include <c_abstract_http/http_lsquic.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/str.h>

TEST test_lsquic_global_lifecycle(void) {
  ASSERT_EQ(0, http_lsquic_global_init());
  ASSERT_EQ(0, http_lsquic_global_init()); /* Test ref counting */
  http_lsquic_global_cleanup();
  http_lsquic_global_cleanup();
  PASS();
}

TEST test_lsquic_context_lifecycle(void) {
  ASSERT_EQ(0, http_lsquic_global_init());

  struct HttpTransportContext *ctx = NULL;
  ASSERT_EQ(0, http_lsquic_context_init(&ctx));
  ASSERT_NEQ(NULL, ctx);

  http_lsquic_context_free(ctx);
  http_lsquic_context_free(NULL);

  http_lsquic_global_cleanup();
  PASS();
}

TEST test_lsquic_config_application(void) {
  ASSERT_EQ(0, http_lsquic_global_init());

  struct HttpConfig config;
  ASSERT_EQ(0, http_config_init(&config));
  config.version_mask = HTTP_VERSION_3;

  struct HttpTransportContext *ctx = NULL;
  ASSERT_EQ(0, http_lsquic_context_init(&ctx));

  ASSERT_EQ(0, http_lsquic_config_apply(ctx, &config));

  http_config_free(&config);
  http_lsquic_context_free(ctx);
  http_lsquic_global_cleanup();
  PASS();
}

TEST test_lsquic_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  http_lsquic_global_init();
  http_lsquic_context_init(&ctx);
  http_request_init(&req);

  /* NULL ctx */
  ASSERT_EQ(EINVAL, http_lsquic_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(EINVAL, http_lsquic_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(EINVAL, http_lsquic_send(ctx, &req, NULL));

  /* NULL internal config application */
  ASSERT_EQ(EINVAL, http_lsquic_config_apply(NULL, NULL));
  ASSERT_EQ(EINVAL, http_lsquic_config_apply(ctx, NULL));

  http_request_free(&req);
  http_lsquic_context_free(ctx);
  http_lsquic_global_cleanup();
  PASS();
}

SUITE(http_lsquic_suite) {
#ifdef C_ABSTRACT_HTTP_USE_LSQUIC
  RUN_TEST(test_lsquic_global_lifecycle);
  RUN_TEST(test_lsquic_context_lifecycle);
  RUN_TEST(test_lsquic_config_application);
  RUN_TEST(test_lsquic_send_invalid_arguments);
#endif
}

#endif /* TEST_HTTP_LSQUIC_H */
