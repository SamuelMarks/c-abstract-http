
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
 * @brief Suite definition for xquic backend.
 */
SUITE(http_xquic_suite) {
  RUN_TEST(test_http_xquic_lifecycle);
  RUN_TEST(test_http_xquic_config);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_XQUIC_H */
