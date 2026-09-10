/**
 * @file test_http_wasm.h
 * @brief Unit and integration tests for the wasm backend.
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
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wasm.h>
#include "mock_alloc.h"
/* clang-format on */

/**
 * @brief Helper callback for streaming request read_chunk.
 *
 * @param[in] user_data User context pointer (cast to int *).
 * @param[out] buf Destination buffer.
 * @param[in] max_len Maximum bytes to copy.
 * @param[out] out_read Number of bytes copied.
 * @return 0 on success.
 */
static int wasm_mock_read_chunk(void *user_data, void *buf, size_t max_len,
                                size_t *out_read) {
  int *called = (int *)user_data;
  if (*called == 0) {
    size_t to_copy = max_len < 10 ? max_len : 10;
    memcpy(buf, "0123456789", to_copy);
    *out_read = to_copy;
    *called = 1;
  } else {
    *out_read = 0;
  }
  return 0;
}

/**
 * @brief Helper callback for streaming response on_chunk success.
 *
 * @param[in] user_data User context pointer.
 * @param[in] data Received chunk bytes.
 * @param[in] len Length of received chunk.
 * @return 0 on success.
 */
static int wasm_mock_on_chunk_success(void *user_data, const void *data,
                                      size_t len) {
  (void)user_data;
  (void)data;
  (void)len;
  return 0;
}

/**
 * @brief Helper callback for streaming response on_chunk failure.
 *
 * @param[in] user_data User context pointer.
 * @param[in] data Received chunk bytes.
 * @param[in] len Length of received chunk.
 * @return Non-zero error code.
 */
static int wasm_mock_on_chunk_fail(void *user_data, const void *data,
                                   size_t len) {
  (void)user_data;
  (void)data;
  (void)len;
  return -1;
}

/**
 * @brief Helper callback for streaming request read_chunk failure.
 *
 * @param[in] user_data User context pointer.
 * @param[out] buf Destination buffer.
 * @param[in] max_len Maximum bytes to copy.
 * @param[out] out_read Number of bytes copied.
 * @return Non-zero error code.
 */
static int wasm_mock_read_chunk_fail(void *user_data, void *buf, size_t max_len,
                                     size_t *out_read) {
  (void)user_data;
  (void)buf;
  (void)max_len;
  (void)out_read;
  return -1;
}

/**
 * @brief Helper callback for streaming request read_chunk that triggers buffer
 * reallocation.
 *
 * @param[in] user_data User context pointer (cast to int *).
 * @param[out] buf Destination buffer.
 * @param[in] max_len Maximum bytes to copy.
 * @param[out] out_read Number of bytes copied.
 * @return 0 on success.
 */
static int wasm_mock_read_chunk_large(void *user_data, void *buf,
                                      size_t max_len, size_t *out_read) {
  int *called = (int *)user_data;
  if (*called < 3) {
    size_t to_copy = max_len < 10 ? max_len : 10;
    memset(buf, 'A', to_copy);
    *out_read = to_copy;
    (*called)++;
  } else {
    *out_read = 0;
  }
  return 0;
}

/**
 * @brief Test wasm global environment lifecycle.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test wasm context lifecycle and allocation failures.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_context_lifecycle(void) {
  struct HttpTransportContext *ctx;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_wasm_context_free(ctx);
  http_wasm_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_wasm_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_wasm_context_init(&ctx));
  g_mock_wasm_config_init_fail = 0;

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_wasm_context_init(&ctx));
  g_mock_alloc_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test wasm configuration application.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_config_application(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_config_apply(NULL, &config));

  config.timeout_ms = 500;
  config.verify_peer = 0;
  config.follow_redirects = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_config_apply(ctx, &config));

  http_config_free(&config);
  http_wasm_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test input validation for http_wasm_send.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_send_validation(void) {
  struct HttpTransportContext *ctx;
  struct HttpResponse *res;
  struct HttpRequest req;
  ctx = NULL;
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(ctx, &req, NULL));

  /* Multipart not flattened check */
  req.parts.count = 1;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(ctx, &req, &res));

  /* Multipart flattened check */
  {
    char *b = (char *)malloc(8);
    ASSERT(b != NULL);
    memcpy(b, "content", 8);
    req.body = b;
    req.body_len = 7;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_send(ctx, &req, &res));
    if (res) {
      http_response_free(res);
      free(res);
      res = NULL;
    }
  }
  req.parts.count = 0;

  /* Invalid method */
  req.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wasm_send(ctx, &req, &res));

  http_request_free(&req);
  http_wasm_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test sending various HTTP methods using wasm.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_send_methods(void) {
  struct HttpTransportContext *ctx;
  struct HttpResponse *res;
  struct HttpRequest req;
  enum HttpMethod methods[7];
  size_t i;
  char *_ast_strdup_0;
  ctx = NULL;
  res = NULL;
  _ast_strdup_0 = NULL;

  methods[0] = HTTP_GET;
  methods[1] = HTTP_POST;
  methods[2] = HTTP_PUT;
  methods[3] = HTTP_DELETE;
  methods[4] = HTTP_HEAD;
  methods[5] = HTTP_OPTIONS;
  methods[6] = HTTP_PATCH;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));

  for (i = 0; i < 7; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.method = methods[i];
    req.url =
        (c_abstract_http_mock_strdup("http://127.0.0.1/test", &_ast_strdup_0),
         _ast_strdup_0);

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_send(ctx, &req, &res));
    if (res) {
      http_response_free(res);
      free(res);
      res = NULL;
    }
    http_request_free(&req);
  }

  http_wasm_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test sending requests with headers, body, and streaming chunks.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_send_headers_and_body(void) {
  struct HttpTransportContext *ctx;
  struct HttpConfig config;
  struct HttpResponse *res;
  struct HttpRequest req;
  char *_ast_strdup_0;
  int read_called;
  ctx = NULL;
  res = NULL;
  _ast_strdup_0 = NULL;
  read_called = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.timeout_ms = 1000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_config_apply(ctx, &config));

  /* Request with headers and body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url =
      (c_abstract_http_mock_strdup("http://127.0.0.1/post", &_ast_strdup_0),
       _ast_strdup_0);
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Content-Type", "application/json"));
  {
    char *b = (char *)malloc(16);
    ASSERT(b != NULL);
    memcpy(b, "{\"key\":\"value\"}", 16);
    req.body = b;
    req.body_len = 15;
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Request with read_chunk and on_chunk (success) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url =
      (c_abstract_http_mock_strdup("http://127.0.0.1/chunk", &_ast_strdup_0),
       _ast_strdup_0);
  req.read_chunk = wasm_mock_read_chunk;
  req.read_chunk_user_data = &read_called;
  req.on_chunk = wasm_mock_on_chunk_success;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Request with read_chunk_large causing buffer growth */
  read_called = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (c_abstract_http_mock_strdup("http://127.0.0.1/chunk_large",
                                         &_ast_strdup_0),
             _ast_strdup_0);
  req.expected_body_len = 10;
  req.read_chunk = wasm_mock_read_chunk_large;
  req.read_chunk_user_data = &read_called;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Request with read_chunk failure */
  read_called = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (c_abstract_http_mock_strdup("http://127.0.0.1/fail_read_chunk",
                                         &_ast_strdup_0),
             _ast_strdup_0);
  req.read_chunk = wasm_mock_read_chunk_fail;
  ASSERT_EQ(ECANCELED, http_wasm_send(ctx, &req, &res));
  ASSERT(res == NULL);
  http_request_free(&req);

  /* Request with on_chunk failure */
  read_called = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (c_abstract_http_mock_strdup("http://127.0.0.1/fail_chunk",
                                         &_ast_strdup_0),
             _ast_strdup_0);
  req.on_chunk = wasm_mock_on_chunk_fail;
  ASSERT_EQ(ECANCELED, http_wasm_send(ctx, &req, &res));
  ASSERT(res == NULL);
  http_request_free(&req);

  http_config_free(&config);
  http_wasm_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test failure paths for http_wasm_send under OOM/simulated errors.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_send_failures(void) {
  struct HttpTransportContext *ctx;
  struct HttpResponse *res;
  struct HttpRequest req;
  char *_ast_strdup_0;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  enum c_abstract_http_error rc;
  int read_called;
#endif
  ctx = NULL;
  res = NULL;
  (void)res;
  _ast_strdup_0 = NULL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  read_called = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url =
      (c_abstract_http_mock_strdup("http://127.0.0.1/test", &_ast_strdup_0),
       _ast_strdup_0);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Simulated fetch fail */
  g_mock_wasm_fetch_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_wasm_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_wasm_fetch_fail = 0;

  /* Simulated fetch timeout */
  g_mock_wasm_fetch_timeout = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_TIMEOUT, http_wasm_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_wasm_fetch_timeout = 0;

  /* Simulated http_response_init failure */
  g_mock_wasm_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_wasm_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_wasm_response_init_fail = 0;

  /* Simulated calloc failure inside emscripten_fetch */
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT(res == NULL);

  /* Simulated calloc failure for HttpResponse */
  g_mock_alloc_count = 2;
  g_mock_alloc_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);

  /* Simulated body malloc failure */
  g_mock_alloc_count = 3;
  g_mock_alloc_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);

  /* Headers add failure during response header parsing */
  g_mock_wasm_header_add_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_wasm_header_add_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);

  /* Headers allocation failure */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Fail", "Value"));
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  http_headers_free(&req.headers);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_init(&req.headers));

  /* Body buffer allocation failure */
  req.read_chunk = wasm_mock_read_chunk;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  req.read_chunk = NULL;

  /* Body buffer realloc failure */
  read_called = 0;
  req.read_chunk = wasm_mock_read_chunk_large;
  req.read_chunk_user_data = &read_called;
  req.expected_body_len = 10;
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_wasm_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT(res == NULL);
  req.read_chunk = NULL;
#endif

  http_request_free(&req);
  http_wasm_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Test sending multiple requests concurrently via wasm.
 *
 * @return GREATEST_TEST_RES_PASS on success.
 */
TEST test_wasm_send_multi(void) {
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_context_init(&ctx));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));

  req1.url = (c_abstract_http_mock_strdup("http://127.0.0.1/1", &_ast_strdup_1),
              _ast_strdup_1);
  req2.url = (c_abstract_http_mock_strdup("http://127.0.0.1/2", &_ast_strdup_2),
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
            http_wasm_send_multi(NULL, NULL, &multi, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_wasm_send_multi(ctx, NULL, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_wasm_send_multi(ctx, NULL, &multi, NULL));

  /* Multi send without loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wasm_send_multi(ctx, NULL, &multi, futures));
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

  /* Multi send with loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wasm_send_multi(ctx, loop, &multi, futures));
  if (futures[0]->response) {
    http_response_free(futures[0]->response);
    free(futures[0]->response);
  }
  if (futures[1]->response) {
    http_response_free(futures[1]->response);
    free(futures[1]->response);
  }
  http_loop_free(loop);

  /* Multi send with loop wakeup failure */
  {
    struct HttpLoopHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.wakeup = (int (*)(void *))xquic_test_mock_loop_wakeup_fail;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init_external(&loop, &hooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_wasm_send_multi(ctx, loop, &multi, futures));
    if (futures[0]->response) {
      http_response_free(futures[0]->response);
      free(futures[0]->response);
    }
    if (futures[1]->response) {
      http_response_free(futures[1]->response);
      free(futures[1]->response);
    }
    http_loop_free(loop);
  }

  /* Multi send with request failure */
  req1.parts.count = 1;
  req1.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_wasm_send_multi(ctx, NULL, &multi, futures));
  req1.parts.count = 0;

  free(futures[0]);
  free(futures[1]);
  http_request_free(&req1);
  http_request_free(&req2);
  http_wasm_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wasm_global_cleanup());
  PASS();
}

/**
 * @brief Suite definition for wasm backend.
 */
SUITE(http_wasm_suite) {
  RUN_TEST(test_wasm_global_lifecycle);
  RUN_TEST(test_wasm_context_lifecycle);
  RUN_TEST(test_wasm_config_application);
  RUN_TEST(test_wasm_send_validation);
  RUN_TEST(test_wasm_send_methods);
  RUN_TEST(test_wasm_send_headers_and_body);
  RUN_TEST(test_wasm_send_failures);
  RUN_TEST(test_wasm_send_multi);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_WASM_H */
