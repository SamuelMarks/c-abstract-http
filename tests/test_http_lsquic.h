/**
 * @file test_http_lsquic.h
 * @brief Integration tests for lsquic Backend.
 *
 * Verifies that the lsquic wrapper correctly initializes, handles
 * configuration, sends requests, and handles multi requests.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_LSQUIC_H
#define TEST_HTTP_LSQUIC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_lsquic.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error c_abstract_http_test_lsquic_helpers(void);
#endif

/** @brief Documented */
TEST test_lsquic_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_lsquic_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_lsquic_global_init());
  g_mock_lsquic_global_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_lsquic_helpers());
#endif

  PASS();
}

/** @brief Documented */
TEST test_lsquic_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_context_init(NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_context_init(&ctx));
  ASSERT_NEQ(NULL, ctx);

  http_lsquic_context_free(ctx);
  http_lsquic_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_lsquic_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_context_init(&ctx));
  g_mock_lsquic_context_init_fail = 0;

  g_mock_lsquic_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_context_init(&ctx));
  g_mock_lsquic_config_init_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_lsquic_config_application(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  char *_ast_agent = NULL;
  char *_ast_proxy = NULL;
  char *_ast_user = NULL;
  char *_ast_pwd = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_config_apply(NULL, &config));

  config.version_mask = HTTP_VERSION_3;
  config.timeout_ms = 5000;
  c_abstract_http_strdup("agent", &_ast_agent);
  config.user_agent = _ast_agent;
  c_abstract_http_strdup("http://proxy:8080", &_ast_proxy);
  config.proxy_url = _ast_proxy;
  c_abstract_http_strdup("user", &_ast_user);
  config.proxy_username = _ast_user;
  c_abstract_http_strdup("pwd", &_ast_pwd);
  config.proxy_password = _ast_pwd;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_config_apply(ctx, &config));

  /* Re-apply with same to test replacement */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_config_apply(ctx, &config));

  /* Re-apply with NULL to test clearing */
  config.user_agent = NULL;
  config.proxy_url = NULL;
  config.proxy_username = NULL;
  config.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_config_apply(ctx, &config));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.user_agent = (char *)"agent";
    g_mock_lsquic_config_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_config_apply(ctx, &cfg));
    cfg.user_agent = NULL;

    cfg.proxy_url = (char *)"proxy";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_config_apply(ctx, &cfg));
    cfg.proxy_url = NULL;

    cfg.proxy_username = (char *)"user";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_config_apply(ctx, &cfg));
    cfg.proxy_username = NULL;

    cfg.proxy_password = (char *)"pwd";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_config_apply(ctx, &cfg));
    g_mock_lsquic_config_init_fail = 0;
  }
#endif

  free(_ast_agent);
  free(_ast_proxy);
  free(_ast_user);
  free(_ast_pwd);
  http_config_free(&config);
  http_lsquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_lsquic_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* NULL ctx */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_send(ctx, &req, NULL));

  /* Not configured */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_lsquic_send(ctx, &req, &res));

  http_request_free(&req);
  http_lsquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_lsquic_send_success(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  struct HttpConfig config;
  char *_ast_url = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;

  /* First send creates engine */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_send(ctx, &req, &res));
  ASSERT_NEQ(NULL, res);
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("OK", (char *)res->body);
  http_response_free(res);
  free(res);
  res = NULL;

  /* Second send reuses engine */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_send(ctx, &req, &res));
  ASSERT_NEQ(NULL, res);
  http_response_free(res);
  free(res);
  res = NULL;

  http_request_free(&req);
  http_config_free(&config);
  http_lsquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_lsquic_send_faults(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  struct HttpConfig config;
  char *_ast_url = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;

  /* Res alloc fail */
  g_mock_lsquic_res_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_send(ctx, &req, &res));
  g_mock_lsquic_res_alloc_fail = 0;

  /* Res init fail */
  g_mock_lsquic_res_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_send(ctx, &req, &res));
  g_mock_lsquic_res_init_fail = 0;

  /* Engine new fail */
  g_mock_lsquic_engine_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_send(ctx, &req, &res));
  g_mock_lsquic_engine_new_fail = 0;

  /* Body alloc fail in read */
  g_mock_lsquic_body_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_lsquic_send(ctx, &req, &res));
  g_mock_lsquic_body_alloc_fail = 0;

  /* Read fail returning -1 */
  g_mock_lsquic_read_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_lsquic_send(ctx, &req, &res));
  g_mock_lsquic_read_fail = 0;

  /* Read fail returning 0 */
  g_mock_lsquic_read_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  g_mock_lsquic_read_fail = 0;

  http_request_free(&req);
  http_config_free(&config);
  http_lsquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
#endif
  PASS();
}

/** @brief Documented */
TEST test_lsquic_send_multi(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpMultiRequest multi;
  struct HttpRequest req1, req2;
  struct HttpRequest *reqs[2];
  struct HttpFuture *futures[2];
  struct HttpFuture f1, f2;
  struct HttpConfig config;
  char *_ast_url1 = NULL;
  char *_ast_url2 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_lsquic_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_lsquic_send_multi(ctx, NULL, NULL, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
  c_abstract_http_strdup("http://127.0.0.1:8080/1", &_ast_url1);
  c_abstract_http_strdup("http://127.0.0.1:8080/2", &_ast_url2);
  req1.url = _ast_url1;
  req2.url = _ast_url2;
  reqs[0] = &req1;
  reqs[1] = &req2;

  memset(&multi, 0, sizeof(multi));
  multi.requests = reqs;
  multi.count = 2;

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  futures[0] = &f1;
  futures[1] = &f2;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_lsquic_send_multi(ctx, NULL, &multi, futures));
  ASSERT(f1.is_ready);
  ASSERT(f2.is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, (enum c_abstract_http_error)f1.error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, (enum c_abstract_http_error)f2.error_code);

  if (f1.response) {
    http_response_free(f1.response);
    free(f1.response);
  }
  if (f2.response) {
    http_response_free(f2.response);
    free(f2.response);
  }

  http_request_free(&req1);
  http_request_free(&req2);
  http_config_free(&config);
  http_lsquic_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_lsquic_global_cleanup());
  PASS();
}

/** @brief Documented */
SUITE(http_lsquic_suite) {
  RUN_TEST(test_lsquic_global_lifecycle);
  RUN_TEST(test_lsquic_context_lifecycle);
  RUN_TEST(test_lsquic_config_application);
  RUN_TEST(test_lsquic_send_invalid_arguments);
  RUN_TEST(test_lsquic_send_success);
  RUN_TEST(test_lsquic_send_faults);
  RUN_TEST(test_lsquic_send_multi);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_LSQUIC_H */
