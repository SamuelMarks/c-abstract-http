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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Test allocation failure */
  ctx = NULL;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* Test config init failure */
  ctx = NULL;
  g_mock_android_config_init_fail = 1;
  rc = http_android_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_config_init_fail = 0;
#endif

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

  http_config_free(&cfg);
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

  http_request_free(&req);
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

  http_response_free(res);
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_android_status_code = 404;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(404, res->status_code);

  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_android_status_code = 0;
#endif

  http_request_free(&req);
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

    http_response_free(res);
    free(res);
    res = NULL;

    http_request_free(&req);
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
  (void)res;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Detached thread attach success */
  g_mock_android_getenv_detached = 1;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Detached thread attach fail */
  g_mock_android_getenv_detached = 1;
  g_mock_android_attach_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_getenv_detached = 0;
  g_mock_android_attach_fail = 0;

  /* GetEnv fail */
  g_mock_android_getenv_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_getenv_fail = 0;
#endif

  http_request_free(&req);
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
  (void)res;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* NewStringUTF failure */
  g_mock_android_new_string_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_new_string_fail = 0;

  /* FindClass failure */
  g_mock_android_find_class_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_find_class_fail = 0;

  /* GetMethodID failure */
  g_mock_android_get_method_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_get_method_fail = 0;

  /* GetMethodID failure on conn methods */
  g_mock_android_get_method_fail = 2;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_get_method_fail = 0;

  /* GetMethodID failure on openConnection */
  g_mock_android_get_method_fail = 3;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_get_method_fail = 0;

  /* GetMethodID failure on setRequestMethod */
  g_mock_android_get_method_fail = 4;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_get_method_fail = 0;

  /* GetMethodID failure on read */
  g_mock_android_get_method_fail = 5;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_get_method_fail = 0;

  /* GetMethodID failure on getInputStream */
  g_mock_android_get_method_fail = 6;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_get_method_fail = 0;

  /* GetMethodID failure on getErrorStream */
  g_mock_android_input_stream_fail = 1;
  g_mock_android_get_method_fail = 7;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_input_stream_fail = 0;
  g_mock_android_get_method_fail = 0;

  /* NewStringUTF failure on method string */
  g_mock_android_new_string_fail = 2;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_new_string_fail = 0;

  /* NewByteArray failure */
  g_mock_android_res_alloc_fail = 2;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_res_alloc_fail = 0;

  /* NewObject failure */
  g_mock_android_new_object_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_new_object_fail = 0;

  /* CallObjectMethod failure */
  g_mock_android_call_object_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);
  g_mock_android_call_object_fail = 0;

  /* Response allocation failure */
  g_mock_android_res_alloc_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_res_alloc_fail = 0;

  /* Response init failure */
  g_mock_android_res_init_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_res_init_fail = 0;

  /* Input stream failure falling back to error stream */
  g_mock_android_input_stream_fail = 1;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Both input and error stream failures */
  g_mock_android_input_stream_fail = 1;
  g_mock_android_error_stream_fail = 1;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Body alloc failure */
  g_mock_android_body_alloc_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_body_alloc_fail = 0;

  /* Body realloc failure during multi-chunk read */
  g_mock_android_read_chunks = 2;
  g_mock_android_body_realloc_fail = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_android_read_chunks = 0;
  g_mock_android_body_realloc_fail = 0;

  /* Multi-chunk read success with final body realloc fallback */
  g_mock_android_read_chunks = 2;
  g_mock_android_final_body_realloc_fail = 1;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(10000, res->body_len);
  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_android_read_chunks = 0;
  g_mock_android_final_body_realloc_fail = 0;

  /* Multi-chunk read success */
  g_mock_android_read_chunks = 2;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(10000, res->body_len);
  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_android_read_chunks = 0;

  /* Read exception */
  g_mock_android_read_exception = 1;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Read zero bytes */
  g_mock_android_read_chunks = 99;
  res = NULL;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_android_read_chunks = 0;

  /* Cleanup exception check */
  g_mock_android_cleanup_exception = 1;
  rc = http_android_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_android_cleanup_exception = 0;
#endif

  http_request_free(&req);
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
