/**
 * @file test_http_picoquic.h
 * @brief Integration tests for Picoquic Backend.
 *
 * Verifies that the Picoquic wrapper correctly initializes and handles
 * configuration.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_PICOQUIC_H
#define TEST_HTTP_PICOQUIC_H

#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <string.h>

#include <c_abstract_http/http_picoquic.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/str.h>

TEST test_picoquic_global_lifecycle(void) {
  ASSERT_EQ(0, http_picoquic_global_init());
  ASSERT_EQ(0, http_picoquic_global_init()); /* Test ref counting */
  http_picoquic_global_cleanup();
  http_picoquic_global_cleanup();
  PASS();
}

TEST test_picoquic_context_lifecycle(void) {
  ASSERT_EQ(0, http_picoquic_global_init());

  struct HttpTransportContext *ctx = NULL;
  ASSERT_EQ(0, http_picoquic_context_init(&ctx));
  ASSERT_NEQ(NULL, ctx);

  http_picoquic_context_free(ctx);
  http_picoquic_context_free(NULL);

  http_picoquic_global_cleanup();
  PASS();
}

TEST test_picoquic_config_application(void) {
  ASSERT_EQ(0, http_picoquic_global_init());

  struct HttpConfig config;
  ASSERT_EQ(0, http_config_init(&config));
  config.version_mask = HTTP_VERSION_3;

  struct HttpTransportContext *ctx = NULL;
  ASSERT_EQ(0, http_picoquic_context_init(&ctx));

  ASSERT_EQ(0, http_picoquic_config_apply(ctx, &config));

  http_config_free(&config);
  http_picoquic_context_free(ctx);
  http_picoquic_global_cleanup();
  PASS();
}

TEST test_picoquic_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  http_picoquic_global_init();
  http_picoquic_context_init(&ctx);
  http_request_init(&req);

  /* NULL ctx */
  ASSERT_EQ(EINVAL, http_picoquic_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(EINVAL, http_picoquic_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(EINVAL, http_picoquic_send(ctx, &req, NULL));

  /* NULL internal config application */
  ASSERT_EQ(EINVAL, http_picoquic_config_apply(NULL, NULL));
  ASSERT_EQ(EINVAL, http_picoquic_config_apply(ctx, NULL));

  http_request_free(&req);
  http_picoquic_context_free(ctx);
  http_picoquic_global_cleanup();
  PASS();
}

SUITE(http_picoquic_suite) {
#ifdef C_ABSTRACT_HTTP_USE_PICOQUIC
  RUN_TEST(test_picoquic_global_lifecycle);
  RUN_TEST(test_picoquic_context_lifecycle);
  RUN_TEST(test_picoquic_config_application);
  RUN_TEST(test_picoquic_send_invalid_arguments);
#endif
}

#endif /* TEST_HTTP_PICOQUIC_H */
