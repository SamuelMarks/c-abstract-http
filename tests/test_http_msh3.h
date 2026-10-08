/**
 * @file test_http_msh3.h
 * @brief Unit tests for the MsH3 Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, request dispatching, error simulation, and multi-send.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_MSH3_H
#define TEST_HTTP_MSH3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_msh3.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

/**
 * @brief Setup request helper returning error enum.
 *
 * @param[out] req Pointer to HttpRequest to initialize.
 * @param[in] url_str URL string to copy into request.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error
test_msh3_setup_request(struct HttpRequest *req, const char *url_str) {
  char *u = NULL;
  enum c_abstract_http_error rc;

  rc = http_request_init(req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = c_abstract_http_strdup(url_str, &u);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(req));
    return rc;
  }
  req->url = u;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Test MsH3 helper error returns */
TEST test_msh3_setup_request_coverage(void) {
  struct HttpRequest req;
  enum c_abstract_http_error rc;
  (void)req;

  /* rc != SUCCESS on init with NULL */
  rc = test_msh3_setup_request(NULL, "http://localhost/");
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* strdup failure */
  g_mock_alloc_fail = 1;
  rc = test_msh3_setup_request(&req, "http://localhost/");
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/** @brief Test MsH3 global lifecycle and reference counting */
TEST test_msh3_global_lifecycle(void) {
  enum c_abstract_http_error rc;

  /* Calling cleanup before any init when g_msh3_mutex is NULL */
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Reference counting */
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Final cleanup frees mutex */
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Now mutex is NULL, cleanup returns ERR_INVAL */
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Test mutex init failure (g_msh3_mutex is NULL now) */
  g_mock_msh3_mutex_init_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_mutex_init_fail = 0;

  /* Test global lock failure in global_init */
  g_mock_msh3_global_lock_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_msh3_global_lock_fail = 0;

  /* Successful init to test cleanup lock failure */
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  g_mock_msh3_global_lock_fail = 1;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_msh3_global_lock_fail = 0;

  /* Clean up */
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Test API open failure */
  g_mock_msh3_api_open_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_api_open_fail = 0;

  /* Test unlock failure in global_init */
  g_mock_mutex_fail = 2;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_mutex_fail = 0;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Test unlock failure in global_init when rc is already an error */
  g_mock_msh3_api_open_fail = 1;
  g_mock_mutex_fail = 2;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_api_open_fail = 0;
  g_mock_mutex_fail = 0;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#endif

  PASS();
}

/** @brief Test MsH3 context creation and destruction */
TEST test_msh3_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  enum c_abstract_http_error rc;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  /* Test context_free with ctx->config != NULL */
  rc = http_config_init(&cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_msh3_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

  http_msh3_context_free(ctx);
  http_msh3_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Alloc fail */
  ctx = NULL;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* Config init fail */
  ctx = NULL;
  g_mock_msh3_config_init_fail = 1;
  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_config_init_fail = 0;
#endif

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 configuration application */
TEST test_msh3_config_application(void) {
  struct HttpConfig config;
  struct HttpTransportContext *ctx = NULL;
  enum c_abstract_http_error rc;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_config_apply(NULL, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_config_apply(ctx, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  config.verify_peer = 0;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Re-apply config when ctx->config is already non-NULL */
  config.verify_peer = 1;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_msh3_config_open_fail = 1;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_config_open_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  http_msh3_context_free(ctx);
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 send parameter validation */
TEST test_msh3_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  enum c_abstract_http_error rc;
  size_t i;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  (void)i;

  /* NULL ctx */
  rc = http_msh3_send(NULL, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL req */
  rc = http_msh3_send(ctx, NULL, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL res pointer */
  rc = http_msh3_send(ctx, &req, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL req->url */
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Invalid URL */
  {
    char *u = NULL;
    rc = c_abstract_http_strdup("invalid_url_without_scheme", &u);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    req.url = u;
    rc = http_msh3_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Test parse_url allocation failures for each step */
  for (i = 1; i <= 7; ++i) {
    g_mock_msh3_parse_url_alloc_fail = (int)i;
    res = NULL;
    if (i == 4) {
      rc = test_msh3_setup_request(&req, "https://127.0.0.1");
    } else if (i == 3) {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080");
    } else if (i == 5) {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1");
    } else if (i == 7) {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080");
    } else {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1/path");
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    rc = http_msh3_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
    ASSERT(res == NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    g_mock_msh3_parse_url_alloc_fail = 0;
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_msh3_context_free(ctx);
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 send success and method dispatch */
