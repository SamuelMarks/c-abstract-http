/**
 * @file test_http_nghttp3.h
 * @brief Integration tests for nghttp3 Backend.
 *
 * Verifies that the nghttp3 wrapper correctly initializes, handles
 * configuration, dispatches requests, and handles errors.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_NGHTTP3_H
#define TEST_HTTP_NGHTTP3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_nghttp3.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

/**
 * @brief Chunk callback state for testing nghttp3 streaming.
 */
struct nghttp3_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes received */
  size_t total_bytes;
  /** @brief Flag to abort */
  int abort;
};

/**
 * @brief Callback for testing nghttp3 chunked receiving.
 *
 * @param[in,out] user_data Pointer to nghttp3_TestChunkState.
 * @param[in] chunk Pointer to received chunk data.
 * @param[in] chunk_len Number of bytes in chunk.
 * @return 0 on success, non-zero to abort.
 */
static int nghttp3_mock_chunk_cb(void *user_data, const void *chunk,
                                 size_t chunk_len) {
  struct nghttp3_TestChunkState *state;
  (void)chunk;
  state = (struct nghttp3_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort) {
    return (int)C_ABSTRACT_HTTP_ERR_IO;
  }
  return 0;
}

/**
 * @brief Test global lifecycle for nghttp3 backend.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());

  PASS();
}

/**
 * @brief Test context creation and teardown for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_context_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_nghttp3_context_free(ctx);
  http_nghttp3_context_free(NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test configuration apply for nghttp3 backend.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_config_application(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  char *_ast_strdup_user;
  char *_ast_strdup_proxy;

  ctx = NULL;
  _ast_strdup_user = NULL;
  _ast_strdup_proxy = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_nghttp3_config_apply(NULL, &config));

  config.timeout_ms = 1000;
  config.verify_peer = 1;
  config.verify_host = 1;
  config.follow_redirects = 1;
  config.version_mask = HTTP_VERSION_3;

  c_abstract_http_mock_strdup("Nghttp3Client/1.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  c_abstract_http_mock_strdup("http://proxy.internal:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Re-apply with new strings to cover freeing previous strings */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  _ast_strdup_user = NULL;
  _ast_strdup_proxy = NULL;
  c_abstract_http_mock_strdup("Nghttp3Client/2.0", &_ast_strdup_user);
  config.user_agent = _ast_strdup_user;
  c_abstract_http_mock_strdup("http://proxy2.internal:8080",
                              &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Apply with NULL user_agent and proxy_url to clear them */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  free(config.user_agent);
  config.user_agent = NULL;
  config.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Apply again with NULL user_agent and proxy_url when already NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test send validation for nghttp3 backend.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_nghttp3_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx;
  struct HttpResponse *res;
  struct HttpRequest req;

  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* NULL ctx */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, NULL));

  /* Not configured yet */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  http_nghttp3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

/**
 * @brief Test sending single request and error paths for nghttp3.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
