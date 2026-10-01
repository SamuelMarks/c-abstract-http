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
  config.timeout_ms = 0;
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
