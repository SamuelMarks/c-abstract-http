/**
 * @file test_http_xquic.h
 * @brief Unit and integration tests for the xquic backend.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_XQUIC_H
#define TEST_HTTP_XQUIC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <greatest.h>
#include <c_abstract_http/http_xquic.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* clang-format on */

/**
 * @brief Mock callback for loop wakeup that fails.
 *
 * @param[in] ctx User context.
 * @return C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error xquic_test_mock_loop_wakeup_fail(void *ctx) {
  (void)ctx;
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Test lifecycle of xquic context and global initialization.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_xquic_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_global_cleanup());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_xquic_context_free(NULL);
  http_xquic_context_free(ctx);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Test free with engine present */
  g_mock_xquic_engine_present = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_context_init(&ctx));
  ASSERT(ctx != NULL);
  http_xquic_context_free(ctx);
  g_mock_xquic_engine_present = 0;

  /* Test config init failure */
  g_mock_xquic_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_xquic_context_init(&ctx));
  g_mock_xquic_config_init_fail = 0;

  /* Test calloc failure */
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_xquic_context_init(&ctx));
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/**
 * @brief Test applying configuration to xquic context.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_xquic_config(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig cfg;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_config_apply(NULL, &cfg));

  cfg.timeout_ms = 3000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_config_apply(ctx, &cfg));

  http_config_free(&cfg);
  http_xquic_context_free(ctx);
  PASS();
}

/**
 * @brief Test input validation and dispatch for http_xquic_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_xquic_send(void) {
  struct HttpTransportContext *ctx;
  struct HttpRequest req;
  struct HttpResponse *res;
  char *_ast_strdup_fail;
  char *_ast_strdup_ok;
  ctx = NULL;
  res = NULL;
  _ast_strdup_fail = NULL;
  _ast_strdup_ok = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_send(NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_send(ctx, &req, NULL));

  /* Test with req.url == NULL */
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_xquic_send(ctx, &req, &res));

  /* Test simulated fail url */
  req.url = (c_abstract_http_mock_strdup("http://fail_url", &_ast_strdup_fail),
             _ast_strdup_fail);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_xquic_send(ctx, &req, &res));
  free(req.url);

  /* Test normal url returning ENOTSUP */
  req.url =
      (c_abstract_http_mock_strdup("https://example.com/test", &_ast_strdup_ok),
       _ast_strdup_ok);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_xquic_send(ctx, &req, &res));

  http_request_free(&req);
  http_xquic_context_free(ctx);
  PASS();
}

/**
 * @brief Test sending multiple requests via xquic.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_http_xquic_send_multi(void) {
  struct HttpTransportContext *ctx;
  struct HttpMultiRequest multi;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpRequest *requests[2];
  struct HttpFuture *futures[2];
  struct ModalityEventLoop *loop;
  char *_ast_strdup_1;
  char *_ast_strdup_2;
  ctx = NULL;
  loop = NULL;
  _ast_strdup_1 = NULL;
  _ast_strdup_2 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_xquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));

  req1.url =
      (c_abstract_http_mock_strdup("https://example.com/1", &_ast_strdup_1),
       _ast_strdup_1);
  req2.url =
      (c_abstract_http_mock_strdup("https://example.com/2", &_ast_strdup_2),
       _ast_strdup_2);

  requests[0] = &req1;
  requests[1] = &req2;
  multi.requests = requests;
  multi.count = 2;

  futures[0] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[1] = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  ASSERT(futures[0] != NULL);
  ASSERT(futures[1] != NULL);

  /* Validation checks */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_xquic_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_xquic_send_multi(ctx, NULL, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_xquic_send_multi(ctx, NULL, &multi, NULL));

  /* Multi send without loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_xquic_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, futures[0]->is_ready);
  ASSERT_EQ(1, futures[1]->is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, futures[0]->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, futures[1]->error_code);

  /* Multi send with loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_xquic_send_multi(ctx, loop, &multi, futures));
  http_loop_free(loop);

  {
    struct HttpLoopHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.wakeup = (int (*)(void *))xquic_test_mock_loop_wakeup_fail;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init_external(&loop, &hooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_xquic_send_multi(ctx, loop, &multi, futures));
    http_loop_free(loop);
  }

  free(futures[0]);
  free(futures[1]);
  http_request_free(&req1);
  http_request_free(&req2);
  http_xquic_context_free(ctx);
  PASS();
}

/**
 * @brief Suite definition for xquic backend.
 */
SUITE(http_xquic_suite) {
  RUN_TEST(test_http_xquic_lifecycle);
  RUN_TEST(test_http_xquic_config);
  RUN_TEST(test_http_xquic_send);
  RUN_TEST(test_http_xquic_send_multi);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_XQUIC_H */
