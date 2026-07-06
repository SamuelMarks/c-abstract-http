/**
 * @file test_http_wasm.h
 * @brief Integration tests for Wasm Backend.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_WASM_H
#define TEST_HTTP_WASM_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wasm.h>
#include "functions/parse/str.h"
/* clang-format on */

static int setup_request(struct HttpRequest *req, int port) {
  char *_ast_strdup_0 = NULL;
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

/** @brief Documented */
TEST test_wasm_global_lifecycle(void) {
  ASSERT_EQ(0, http_wasm_global_init());
  ASSERT_EQ(0, http_wasm_global_init());

  http_wasm_global_cleanup();
  http_wasm_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_wasm_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  http_wasm_global_init();

  rc = http_wasm_context_init(&ctx);
  ASSERT_EQ(0, rc);
  ASSERT(ctx != NULL);

  http_wasm_context_free(ctx);
  http_wasm_context_free(NULL);

  http_wasm_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_wasm_config_application(void) {
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_user = NULL;
  char *_ast_strdup_pass = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;

  http_wasm_global_init();
  http_wasm_context_init(&ctx);
  http_config_init(&config);

  config.timeout_ms = 500;
  config.verify_peer = 0;
  config.follow_redirects = 0;
  config.proxy_url =
      (c_cdd_strdup("http://proxy.local:8080", &_ast_strdup_proxy),
       _ast_strdup_proxy);
  config.proxy_username =
      (c_cdd_strdup("admin", &_ast_strdup_user), _ast_strdup_user);
  config.proxy_password =
      (c_cdd_strdup("secret", &_ast_strdup_pass), _ast_strdup_pass);

  rc = http_wasm_config_apply(ctx, &config);
  ASSERT_EQ(0, rc);

  http_config_free(&config);
  http_wasm_context_free(ctx);
  http_wasm_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_wasm_send_connection_failure(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;

  http_wasm_global_init();
  http_wasm_context_init(&ctx);
  http_config_init(&config);

  config.timeout_ms = 50;
  http_wasm_config_apply(ctx, &config);

  setup_request(&req, 59999);

  rc = http_wasm_send(ctx, &req, &res);

  if (rc != C_ABSTRACT_HTTP_ERR_TIMEOUT && rc != ECONNREFUSED &&
      rc != EHOSTUNREACH && rc != C_ABSTRACT_HTTP_ERR_IO) {
    fprintf(stderr, "Unexpected return code: %d (%s)\n", rc, strerror(rc));
    FAIL();
  }

  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_wasm_context_free(ctx);
  http_wasm_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_wasm_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  http_wasm_global_init();
  http_wasm_context_init(&ctx);
  http_request_init(&req);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(ctx, &req, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_config_apply(ctx, NULL));

  http_request_free(&req);
  http_wasm_context_free(ctx);
  http_wasm_global_cleanup();
  PASS();
}

/** @brief Documented */
SUITE(http_wasm_suite) {
  RUN_TEST(test_wasm_global_lifecycle);
  RUN_TEST(test_wasm_context_lifecycle);
  RUN_TEST(test_wasm_config_application);
  RUN_TEST(test_wasm_send_connection_failure);
  RUN_TEST(test_wasm_send_invalid_arguments);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_WASM_H */
