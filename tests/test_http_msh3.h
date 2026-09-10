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
    http_request_free(req);
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
  http_config_free(&cfg);

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

  http_config_free(&config);
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
    http_request_free(&req);
    g_mock_msh3_parse_url_alloc_fail = 0;
  }
#endif

  http_request_free(&req);
  http_msh3_context_free(ctx);
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 send success and method dispatch */
TEST test_msh3_send_success_and_methods(void) {
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

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* 1. Normal GET request */
  rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080/test");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* 2. HTTPS request with default 443 port and path */
  rc = test_msh3_setup_request(&req, "https://127.0.0.1/test");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* 3. HTTP request with default 80 port and no path */
  rc = test_msh3_setup_request(&req, "http://127.0.0.1");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* 4. HTTPS request with default 443 port and no path */
  rc = test_msh3_setup_request(&req, "https://127.0.0.1");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* 5. HTTP request with explicit port and no path */
  rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* 5b. HTTP request with colon in path */
  rc = test_msh3_setup_request(&req, "http://127.0.0.1/path:withcolon");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* 6. Test all methods */
  for (i = 0; i < 8; ++i) {
    rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080/methods");
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    req.method = methods[i];
    res = NULL;
    rc = http_msh3_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT(res != NULL);
    http_response_free(res);
    free(res);
    res = NULL;
    http_request_free(&req);
  }

  http_msh3_context_free(ctx);
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 simulated failures */
TEST test_msh3_send_failures(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080/fail");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  (void)res;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* res alloc failure */
  g_mock_msh3_res_alloc_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_res_alloc_fail = 0;

  /* res init failure */
  g_mock_msh3_res_init_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_res_init_fail = 0;

  /* getaddrinfo failure */
  g_mock_msh3_getaddrinfo_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_getaddrinfo_fail = 0;

  /* getaddrinfo null result */
  g_mock_msh3_getaddrinfo_null_result = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  g_mock_msh3_getaddrinfo_null_result = 0;

  /* connection open failure */
  g_mock_msh3_conn_open_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_conn_open_fail = 0;

  /* mutex init failure in send */
  g_mock_msh3_mutex_init_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_mutex_init_fail = 0;

  /* cond init failure in send */
  g_mock_msh3_cond_init_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_cond_init_fail = 0;

  /* request open failure */
  g_mock_msh3_request_open_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_request_open_fail = 0;

  /* request send failure */
  g_mock_msh3_send_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_send_fail = 0;

  /* send mutex lock failure */
  g_mock_msh3_send_lock_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_send_lock_fail = 0;

  /* cond wait failure */
  g_mock_msh3_cond_wait_fail = 1;
  g_mock_cond_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_cond_wait_fail = 0;
  g_mock_cond_fail = 0;

  /* shutdown error */
  g_mock_msh3_shutdown_error = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_shutdown_error = 0;

  /* shutdown error 2 (ConnectionErrorCode) */
  g_mock_msh3_shutdown_error = 2;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_shutdown_error = 0;

  /* header alloc failure */
  g_mock_msh3_header_alloc_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_header_alloc_fail = 0;

  /* header alloc failure 2 (vstr fail) */
  g_mock_msh3_header_alloc_fail = 2;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  g_mock_msh3_header_alloc_fail = 0;

  /* header add failure */
  g_mock_msh3_header_add_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  g_mock_msh3_header_add_fail = 0;

  /* body realloc failure */
  g_mock_msh3_body_realloc_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  g_mock_msh3_body_realloc_fail = 0;

  /* cb mutex lock failure paired with cond wait failure to prevent hang */
  g_mock_msh3_cb_mutex_lock_fail = 1;
  g_mock_cond_fail = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);
  g_mock_msh3_cb_mutex_lock_fail = 0;
  g_mock_cond_fail = 0;

  /* extra events (NULL header, unknown event type 999, NULL context) */
  g_mock_msh3_extra_events = 1;
  res = NULL;
  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  g_mock_msh3_extra_events = 0;
#endif

  http_request_free(&req);
  http_msh3_context_free(ctx);
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 multi send */
TEST test_msh3_multi(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpMultiRequest multi;
  struct HttpRequest req1, req2;
  struct HttpRequest *reqs[2];
  struct HttpFuture *futures[2];
  struct HttpFuture f1, f2;
  enum c_abstract_http_error rc;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = test_msh3_setup_request(&req1, "http://127.0.0.1:8080/m1");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = test_msh3_setup_request(&req2, "http://127.0.0.1:8080/m2");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  reqs[0] = &req1;
  reqs[1] = &req2;
  multi.requests = reqs;
  multi.count = 2;

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  futures[0] = &f1;
  futures[1] = &f2;

  /* Invalid args */
  rc = http_msh3_send_multi(NULL, NULL, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_send_multi(ctx, NULL, NULL, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_send_multi(ctx, NULL, &multi, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Multi send with one invalid request to test error propagation */
  free(req2.url);
  req2.url = NULL;
  rc = http_msh3_send_multi(ctx, NULL, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(1, futures[1]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[0]->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, futures[1]->error_code);

  if (futures[0]->response) {
    http_response_free(futures[0]->response);
    free(futures[0]->response);
  }

  /* Valid multi send */
  rc = test_msh3_setup_request(&req2, "http://127.0.0.1:8080/m2");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_msh3_send_multi(ctx, NULL, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(1, futures[1]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[0]->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, futures[1]->error_code);

  if (futures[0]->response) {
    http_response_free(futures[0]->response);
    free(futures[0]->response);
  }
  if (futures[1]->response) {
    http_response_free(futures[1]->response);
    free(futures[1]->response);
  }

  http_request_free(&req1);
  http_request_free(&req2);
  http_msh3_context_free(ctx);
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief MsH3 test suite registration */
SUITE(http_msh3_suite) {
  RUN_TEST(test_msh3_setup_request_coverage);
  RUN_TEST(test_msh3_global_lifecycle);
  RUN_TEST(test_msh3_context_lifecycle);
  RUN_TEST(test_msh3_config_application);
  RUN_TEST(test_msh3_send_invalid_arguments);
  RUN_TEST(test_msh3_send_success_and_methods);
  RUN_TEST(test_msh3_send_failures);
  RUN_TEST(test_msh3_multi);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_MSH3_H */
