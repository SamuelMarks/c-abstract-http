/**
 * @file test_http_android.h
 * @brief Unit tests for the Android Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, parameter validation, and JNI interaction.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TEST_HTTP_ANDROID_H
#define C_ABSTRACT_HTTP_TEST_HTTP_ANDROID_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_android.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

/** @brief Test Android global and context lifecycle */
TEST test_android_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;
  enum c_abstract_http_error rc;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Init NULL ctx check */
  rc = http_android_context_init(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Init valid ctx */
  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  /* Set jvm on context */
  rc = http_android_context_set_jvm(NULL, (void *)0x1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_android_context_set_jvm(ctx, (void *)0x1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Global set jvm */
  rc = http_android_set_jvm((void *)0x2);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Free */
  http_android_context_free(ctx);
  http_android_context_free(NULL);

  rc = http_android_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  PASS();
}

/** @brief Test Android configuration application */
TEST test_android_config(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  enum c_abstract_http_error rc;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  rc = http_config_init(&cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  cfg.verify_peer = 0;
  cfg.verify_host = 0;

  rc = http_android_config_apply(NULL, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_android_config_apply(ctx, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_android_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  http_android_context_free(ctx);
  PASS();
}

/** @brief Test Android send parameter validation */
TEST test_android_send_invalid(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_send(NULL, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_android_send(ctx, NULL, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_android_send(ctx, &req, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL JVM returns NOTSUP */
  rc = http_android_context_set_jvm(ctx, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_android_context_free(ctx);
  PASS();
}

/** @brief Test Android send successful execution */
TEST test_android_send_success(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *url_dup = NULL;
  enum c_abstract_http_error rc;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = c_abstract_http_strdup("http://example.com/api", &url_dup);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  req.url = url_dup;
  req.method = HTTP_GET;

  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_android_context_free(ctx);
  PASS();
}

/** @brief Test Android send with all HTTP methods */
TEST test_android_send_methods(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;
  enum HttpMethod methods[8];
  size_t i;

  methods[0] = HTTP_POST;
  methods[1] = HTTP_PUT;
  methods[2] = HTTP_DELETE;
  methods[3] = HTTP_PATCH;
  methods[4] = HTTP_HEAD;
  methods[5] = HTTP_OPTIONS;
  methods[6] = (enum HttpMethod)999;
  methods[7] = HTTP_GET;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  for (i = 0; i < 8; ++i) {
    rc = http_request_init(&req);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    req.method = methods[i];
    /* On one iteration, leave req.url as NULL to test NULL url branch */
    if (i != 0) {
      char *u = NULL;
      rc = c_abstract_http_strdup("http://test.local/", &u);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
      req.url = u;
    }

    res = NULL;
    rc = http_android_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT(res != NULL);

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  http_android_context_free(ctx);
  PASS();
}

/** @brief Test Android send thread attachment and error states */
TEST test_android_send_thread_states(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_android_context_free(ctx);
  PASS();
}

/** @brief Test Android send JNI method and allocation failures */
TEST test_android_send_jni_failures(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  rc = http_android_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_android_context_free(ctx);
  PASS();
}

/** @brief Android test suite registration */
SUITE(http_android_suite) {
  RUN_TEST(test_android_lifecycle);
  RUN_TEST(test_android_config);
  RUN_TEST(test_android_send_invalid);
  RUN_TEST(test_android_send_success);
  RUN_TEST(test_android_send_methods);
  RUN_TEST(test_android_send_thread_states);
  RUN_TEST(test_android_send_jni_failures);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_HTTP_ANDROID_H */
