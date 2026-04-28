/**
 * @file test_http_apple.h
 * @brief Unit tests for the Apple Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, and parameter validation.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_TEST_HTTP_APPLE_H
#define C_CDD_TEST_HTTP_APPLE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_apple.h>
#include <c_abstract_http/http_types.h>
#include "cdd_test_helpers/mock_server.h"
/* clang-format on */

static int mock_on_chunk_cb(void *user_data, const void *chunk, size_t len) {
  int *calls = (int *)user_data;
  (void)chunk;
  (void)len;
  (*calls)++;
  return 0;
}

TEST test_apple_oom_branches(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(0, http_apple_context_init(&ctx));

#if defined(__APPLE__)

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_url_str");
  req.method = HTTP_GET;
  ASSERT_EQ(EINVAL, http_apple_send(ctx, &req, &res));
  http_request_free(&req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_url");
  req.method = HTTP_GET;
  ASSERT_EQ(EINVAL, http_apple_send(ctx, &req, &res));
  http_request_free(&req);

  /* fail_url_ref removed because urlRef parsing was optimized out */

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_request_ref");
  req.method = HTTP_GET;
  {
    int debug_rc = http_apple_send(ctx, &req, &res);
    if (debug_rc != ENOMEM)
      printf("test_apple_oom_branches: expected ENOMEM (%d), got %d\n", ENOMEM,
             debug_rc);
    ASSERT_EQ(ENOMEM, debug_rc);
  }
  http_request_free(&req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_body_data");
  req.method = HTTP_POST;
  req.body = strdup("test");
  req.body_len = 4;
  ASSERT_EQ(ENOMEM, http_apple_send(ctx, &req, &res));
  http_request_free(&req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_read_stream");
  req.method = HTTP_GET;
  ASSERT_EQ(ENOMEM, http_apple_send(ctx, &req, &res));
  http_request_free(&req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_read_stream_open");
  req.method = HTTP_GET;
  ASSERT_EQ(EIO, http_apple_send(ctx, &req, &res));
  http_request_free(&req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_mutable_data");
  req.method = HTTP_POST;
  req.read_chunk = (http_read_chunk_fn)1;
  req.expected_body_len = 10;
  ASSERT_EQ(ENOMEM, http_apple_send(ctx, &req, &res));
  http_request_free(&req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = strdup("http://fail_cb_rc");
  req.method = HTTP_POST;
  req.body = strdup("test");
  req.body_len = 4;
  req.on_chunk = mock_on_chunk_cb;
  {
    int calls = 0;
    req.on_chunk_user_data = &calls;
    ASSERT_EQ(ENOMEM, http_apple_send(ctx, &req, &res));
  }
  http_request_free(&req);
#endif

  http_apple_context_free(ctx);
  PASS();
}

TEST test_apple_oom(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(ENOMEM, http_apple_context_init(&ctx));
  g_mock_alloc_fail = 0;

  ASSERT_EQ(0, http_apple_context_init(&ctx));
  ASSERT_EQ(0, http_request_init(&req));

  req.url = "http://example.com";
  req.method = HTTP_GET;

  /* Test 124: malloc for *res fails */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(ENOMEM, http_apple_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;

  http_apple_context_free(ctx);
  PASS();
}

TEST test_apple_send_mock_server(void) {
#if defined(__APPLE__)
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr server = NULL;
  int port = 0;
  char url[256];
  int on_chunk_calls = 0;
  struct MockServerRequest mock_req;

  ASSERT_EQ(0, mock_server_init(&server));
  ASSERT_EQ(0, mock_server_start(server));
  port = math_mock_server_get_port(server);
  ASSERT(port > 0);
  sprintf(url, "http://127.0.0.1:%d/echo", port);

  ASSERT_EQ(0, http_apple_context_init(&ctx));

  /* Normal request */
  ASSERT_EQ(0, http_request_init(&req));
  req.url = (char *)malloc(strlen(url) + 1);
  strcpy(req.url, url);
  req.method = HTTP_POST;
  req.body = strdup("Hello Apple!");
  req.body_len = strlen("Hello Apple!");

  ASSERT_EQ(0, http_apple_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(res->body_len > 0);

  http_response_free(res);
  free(res);
  http_request_free(&req);
  res = NULL;

  /* On-chunk request */
  ASSERT_EQ(0, mock_server_wait_for_request(server, &mock_req));
  mock_server_request_cleanup(&mock_req);

  ASSERT_EQ(0, http_request_init(&req));
  req.url = (char *)malloc(strlen(url) + 1);
  strcpy(req.url, url);
  req.method = HTTP_GET;
  req.on_chunk = mock_on_chunk_cb;
  req.on_chunk_user_data = &on_chunk_calls;

  ASSERT_EQ(0, http_apple_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(on_chunk_calls > 0);

  http_response_free(res);
  free(res);
  http_request_free(&req);

  ASSERT_EQ(0, mock_server_wait_for_request(server, &mock_req));
  mock_server_request_cleanup(&mock_req);

  http_apple_context_free(ctx);
  mock_server_destroy(server);
#endif
  PASS();
}

TEST test_apple_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(0, http_apple_global_init());

  /* Init */
  ASSERT_EQ(EINVAL, http_apple_context_init(NULL));
  ASSERT_EQ(0, http_apple_context_init(&ctx));
  ASSERT(ctx != NULL);

  /* Free */
  http_apple_context_free(ctx);
  http_apple_context_free(NULL);

  http_apple_global_cleanup();

  PASS();
}

TEST test_apple_config(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;

  ASSERT_EQ(0, http_apple_context_init(&ctx));
  ASSERT(ctx != NULL);

  ASSERT_EQ(0, http_config_init(&cfg));

  ASSERT_EQ(EINVAL, http_apple_config_apply(NULL, &cfg));
  ASSERT_EQ(EINVAL, http_apple_config_apply(ctx, NULL));
  ASSERT_EQ(0, http_apple_config_apply(ctx, &cfg));

  http_config_free(&cfg);
  http_apple_context_free(ctx);
  PASS();
}

TEST test_apple_send_invalid(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(0, http_apple_context_init(&ctx));
  ASSERT_EQ(0, http_request_init(&req));

  ASSERT_EQ(EINVAL, http_apple_send(NULL, &req, &res));
  ASSERT_EQ(EINVAL, http_apple_send(ctx, NULL, &res));
  ASSERT_EQ(EINVAL, http_apple_send(ctx, &req, NULL));

#if defined(__APPLE__)
  /* Valid input but we need to mock a real URL so it fails gracefully */
  req.url = (char *)malloc(sizeof("http://localhost:1"));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1");
#endif
  req.method = HTTP_GET;
  /* Might fail with EIO due to no connection or return an allocated res */
  {
    int rc = http_apple_send(ctx, &req, &res);
    if (rc == 0) {
      if (res)
        http_response_free(res);
      free(res);
    }
  }
#else
  /* Valid input but not implemented (or no Apple OS) should return ENOSYS */
  ASSERT_EQ(ENOSYS, http_apple_send(ctx, &req, &res));
#endif

  http_request_free(&req);
  http_apple_context_free(ctx);
  PASS();
}

TEST test_apple_send_all_methods(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig cfg;
  int i;
  enum HttpMethod methods[] = {
      HTTP_POST,    HTTP_PUT,   HTTP_DELETE,  HTTP_PATCH,         HTTP_HEAD,
      HTTP_OPTIONS, HTTP_TRACE, HTTP_CONNECT, (enum HttpMethod)99 /* Invalid
                                                                     method */
  };

  ASSERT_EQ(0, http_apple_context_init(&ctx));
  ASSERT_EQ(0, http_config_init(&cfg));
  cfg.verify_peer = 0;
  ASSERT_EQ(0, http_apple_config_apply(ctx, &cfg));
  http_config_free(&cfg);

  for (i = 0; i < 9; i++) {
    ASSERT_EQ(0, http_request_init(&req));
    req.url = (char *)malloc(sizeof("http://localhost:1"));
    strcpy(req.url, "http://localhost:1");
    req.method = methods[i];
    http_headers_add(&req.headers, "X-Test", "Value");

    if (i == 8) {
      /* Invalid method may fail differently, let's just see if it crashes */
      http_apple_send(ctx, &req, &res);
    } else {
      int rc = http_apple_send(ctx, &req, &res);
      if (rc == 0 && res) {
        http_response_free(res);
        free(res);
        res = NULL;
      }
    }
    http_request_free(&req);
  }

  http_apple_context_free(ctx);
  PASS();
}

static int mock_read_chunk(void *user_data, void *buf, size_t buf_len,
                           size_t *out_read) {
  int *calls = (int *)user_data;
  if (*calls >= 2) {
    *out_read = 0;
    return 0;
  }
  (*calls)++;
  if (buf_len > 4)
    buf_len = 4;
  memcpy(buf, "test", buf_len);
  *out_read = buf_len;
  return 0;
}

static int mock_read_chunk_fail(void *user_data, void *buf, size_t buf_len,
                                size_t *out_read) {
  (void)user_data;
  (void)buf;
  (void)buf_len;
  (void)out_read;
  return EIO;
}

TEST test_apple_read_chunk(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig cfg;
  int calls = 0;

  ASSERT_EQ(0, http_apple_context_init(&ctx));
  ASSERT_EQ(0, http_config_init(&cfg));
  cfg.verify_peer = 0;
  ASSERT_EQ(0, http_apple_config_apply(ctx, &cfg));
  http_config_free(&cfg);

  /* Success chunk */
  ASSERT_EQ(0, http_request_init(&req));
  req.url = (char *)malloc(sizeof("http://localhost:1"));
  strcpy(req.url, "http://localhost:1");
  req.method = HTTP_POST;
  req.read_chunk = mock_read_chunk;
  req.read_chunk_user_data = &calls;
  req.expected_body_len = 8;

  /* Will fail to connect but it hits the read_chunk loop */
  http_apple_send(ctx, &req, &res);
  http_request_free(&req);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  /* Fail chunk */
  ASSERT_EQ(0, http_request_init(&req));
  req.url = (char *)malloc(sizeof("http://localhost:1"));
  strcpy(req.url, "http://localhost:1");
  req.method = HTTP_POST;
  req.read_chunk = mock_read_chunk_fail;
  req.read_chunk_user_data = NULL;
  req.expected_body_len = 8;

  /* Will fail with EIO */
  ASSERT_EQ(EIO, http_apple_send(ctx, &req, &res));
  http_request_free(&req);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  http_apple_context_free(ctx);
  PASS();
}

SUITE(http_apple_suite) {
  RUN_TEST(test_apple_send_mock_server);
  RUN_TEST(test_apple_read_chunk);
  RUN_TEST(test_apple_lifecycle);
  RUN_TEST(test_apple_config);
  RUN_TEST(test_apple_send_invalid);
  RUN_TEST(test_apple_send_all_methods);
  RUN_TEST(test_apple_oom_branches);
  RUN_TEST(test_apple_oom);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_CDD_TEST_HTTP_APPLE_H */
