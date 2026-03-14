/**
 * @file test_http_msh3.h
 * @brief Integration tests for MsH3 Backend.
 *
 * Verifies that the MsH3 wrapper correctly initializes and handles
 * configuration.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_MSH3_H
#define TEST_HTTP_MSH3_H

#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <string.h>

#include <c_abstract_http/http_msh3.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/str.h>

static int setup_request(struct HttpRequest *req, int port) {
  char *_ast_strdup_0 = NULL;
  int rc;
  char url[64];

  rc = http_request_init(req);
  if (rc != 0)
    return rc;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif

  req->url = (c_cdd_strdup(url, &_ast_strdup_0), _ast_strdup_0);
  return (enum greatest_test_res)0;
}

TEST test_msh3_global_lifecycle(void) {
  ASSERT_EQ(0, http_msh3_global_init());
  ASSERT_EQ(0, http_msh3_global_init()); /* Test ref counting */
  http_msh3_global_cleanup();
  http_msh3_global_cleanup();
  PASS();
}

TEST test_msh3_context_lifecycle(void) {
  ASSERT_EQ(0, http_msh3_global_init());

  struct HttpTransportContext *ctx = NULL;
  ASSERT_EQ(0, http_msh3_context_init(&ctx));
  ASSERT_NEQ(NULL, ctx);

  http_msh3_context_free(ctx);
  http_msh3_context_free(NULL);

  http_msh3_global_cleanup();
  PASS();
}

TEST test_msh3_config_application(void) {
  ASSERT_EQ(0, http_msh3_global_init());

  struct HttpConfig config;
  ASSERT_EQ(0, http_config_init(&config));
  config.version_mask = HTTP_VERSION_3;

  struct HttpTransportContext *ctx = NULL;
  ASSERT_EQ(0, http_msh3_context_init(&ctx));

  ASSERT_EQ(0, http_msh3_config_apply(ctx, &config));

  http_config_free(&config);
  http_msh3_context_free(ctx);
  http_msh3_global_cleanup();
  PASS();
}

TEST test_msh3_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  http_msh3_global_init();
  http_msh3_context_init(&ctx);
  http_request_init(&req);

  /* NULL ctx */
  ASSERT_EQ(EINVAL, http_msh3_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(EINVAL, http_msh3_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(EINVAL, http_msh3_send(ctx, &req, NULL));

  /* NULL internal config application */
  ASSERT_EQ(EINVAL, http_msh3_config_apply(NULL, NULL));
  ASSERT_EQ(EINVAL, http_msh3_config_apply(ctx, NULL));

  http_request_free(&req);
  http_msh3_context_free(ctx);
  http_msh3_global_cleanup();
  PASS();
}

TEST test_msh3_send_connection_failure(void) {
  /* Expect mapped error (ECONNREFUSED or ETIMEDOUT or EHOSTUNREACH) */
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  int rc;

  http_msh3_global_init();
  http_msh3_context_init(&ctx);
  http_config_init(&config);

  config.timeout_ms = 50;
  http_msh3_config_apply(ctx, &config);

  /* Use an invalid port */
  setup_request(&req, 59999);

  rc = http_msh3_send(ctx, &req, &res);

  if (rc != ECONNREFUSED && rc != ETIMEDOUT && rc != EHOSTUNREACH &&
      rc != EIO) {
    fprintf(stderr, "Unexpected return code: %d (%s)\n", rc, strerror(rc));
    FAIL();
  }

  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_msh3_context_free(ctx);
  http_msh3_global_cleanup();
  PASS();
}

SUITE(http_msh3_suite) {
#ifdef C_ABSTRACT_HTTP_USE_MSH3
  RUN_TEST(test_msh3_global_lifecycle);
  RUN_TEST(test_msh3_context_lifecycle);
  RUN_TEST(test_msh3_config_application);
  RUN_TEST(test_msh3_send_invalid_arguments);
  RUN_TEST(test_msh3_send_connection_failure);
#endif
}

#endif /* TEST_HTTP_MSH3_H */
