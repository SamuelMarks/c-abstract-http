/**
 * @file test_http_raw.h
 * @brief Integration and unit tests for the raw socket HTTP backend.
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_RAW_H
#define TEST_HTTP_RAW_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include "greatest.h"
#include <c_abstract_http/http_raw.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
#include "abstract_http_test_helpers/mock_server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* clang-format on */

static int raw_chunk_cb_success(void *user_data, const void *data, size_t len) {
  size_t *total = (size_t *)user_data;
  (void)data;
  if (total)
    *total += len;
  return 0;
}

static int raw_chunk_cb_fail(void *user_data, const void *data, size_t len) {
  (void)user_data;
  (void)data;
  (void)len;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_http_raw_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_global_cleanup());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_raw_context_free(NULL);
  http_raw_context_free(ctx);
  PASS();
}

TEST test_http_raw_config(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(NULL, &cfg));

  cfg.timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  cfg.timeout_ms = 5000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  http_config_free(&cfg);
  http_raw_context_free(ctx);
  PASS();
}

TEST test_http_raw_url_validation(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, NULL));

  /* NULL url */
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  /* Unsupported scheme */
  req.url = "ftp://localhost/test";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  /* Malformed scheme */
  req.url = "httpx://localhost";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  req.url = NULL;
  http_request_free(&req);
  http_raw_context_free(ctx);
  PASS();
}

TEST test_http_raw_send_requests(void) {
  struct HttpTransportContext *ctx = NULL;
  MockServerPtr srv = NULL;
  int port = 0;
  char url[128];
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  size_t chunk_bytes = 0;

  ASSERT_EQ(0, mock_server_init((MockServerPtr *)&srv));
  ASSERT_EQ(0, mock_server_start((MockServerPtr)srv));
  port = math_mock_server_get_port(srv);
  ASSERT(port > 0);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.timeout_ms = 200;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    http_config_free(&cfg);
  }

  /* 1. Standard GET */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif
  req.url = url;
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Custom", "Value"));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  http_response_free(res);
  free(res);
  res = NULL;
  req.url = NULL;
  http_request_free(&req);

  /* 2. POST with body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.method = HTTP_POST;
  req.body = "hello world";
  req.body_len = 11;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  http_response_free(res);
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  http_request_free(&req);

  /* 2b. POST with body != NULL and body_len == 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.method = HTTP_POST;
  req.body = "dummy";
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  http_response_free(res);
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  http_request_free(&req);

  /* 3. Other methods: PUT, DELETE, PATCH, HEAD, OPTIONS */
  {
    enum HttpMethod methods[5];
    size_t m_idx;
    methods[0] = HTTP_PUT;
    methods[1] = HTTP_DELETE;
    methods[2] = HTTP_PATCH;
    methods[3] = HTTP_HEAD;
    methods[4] = HTTP_OPTIONS;
    for (m_idx = 0; m_idx < sizeof(methods) / sizeof(methods[0]); ++m_idx) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      req.url = url;
      req.method = methods[m_idx];
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
      ASSERT(res != NULL);
      http_response_free(res);
      free(res);
      res = NULL;
      req.url = NULL;
      http_request_free(&req);
    }
  }

  /* 4. URL without explicit path: http://127.0.0.1:port */
  {
    char url_no_path[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url_no_path, sizeof(url_no_path), "http://127.0.0.1:%d", port);
#else
    sprintf(url_no_path, "http://127.0.0.1:%d", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_no_path;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT(res != NULL);
    http_response_free(res);
    free(res);
    res = NULL;
    req.url = NULL;
    http_request_free(&req);
  }

  /* 5. URL with path containing colon: http://127.0.0.1:port/path:with:colons
   */
  {
    char url_colon_path[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url_colon_path, sizeof(url_colon_path),
              "http://127.0.0.1:%d/path:with:colons", port);
#else
    sprintf(url_colon_path, "http://127.0.0.1:%d/path:with:colons", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_colon_path;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT(res != NULL);
    http_response_free(res);
    free(res);
    res = NULL;
    req.url = NULL;
    http_request_free(&req);
  }

  /* 5b. URL with path containing colon without port:
   * http://127.0.0.1/path:with:colons */
  {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1/path:with:colons";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
    ASSERT(res == NULL);
    req.url = NULL;
    http_request_free(&req);
  }

  /* 6. on_chunk streaming callback success */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  chunk_bytes = 0;
  req.on_chunk = raw_chunk_cb_success;
  req.on_chunk_user_data = &chunk_bytes;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT(chunk_bytes > 0);
  http_response_free(res);
  free(res);
  res = NULL;
  req.url = NULL;
  http_request_free(&req);

  /* 7. on_chunk streaming callback failure */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.on_chunk = raw_chunk_cb_fail;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  ASSERT(res == NULL);
  req.url = NULL;
  http_request_free(&req);

  /* 8. Large request buffer expansion (headers and body) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  {
    size_t h;
    char key[32], val[128];
    for (h = 0; h < 30; ++h) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(key, sizeof(key), "X-Header-%lu", (unsigned long)h);
      sprintf_s(val, sizeof(val),
                "Value-%lu-abcdefghijklmnopqrstuvwxyz0123456789",
                (unsigned long)h);
#else
      sprintf(key, "X-Header-%lu", (unsigned long)h);
      sprintf(val, "Value-%lu-abcdefghijklmnopqrstuvwxyz0123456789",
              (unsigned long)h);
#endif
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req.headers, key, val));
    }
  }
  {
    char big_body[2048];
    memset(big_body, 'A', sizeof(big_body));
    req.body = big_body;
    req.body_len = sizeof(big_body);
    req.method = HTTP_POST;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT(res != NULL);
    http_response_free(res);
    free(res);
    res = NULL;
  }
  req.url = NULL;
  req.body = NULL;
  http_request_free(&req);

  /* 9. HTTPS scheme URL parsing check */
  {
    char https_url[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(https_url, sizeof(https_url), "https://127.0.0.1:%d/test", port);
#else
    sprintf(https_url, "https://127.0.0.1:%d/test", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = https_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT(res != NULL);
    http_response_free(res);
    free(res);
    res = NULL;
    req.url = NULL;
    http_request_free(&req);
  }

  /* 10. HTTP scheme URL without port: http://127.0.0.1/path */
  {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1/path";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
    ASSERT(res == NULL);
    req.url = NULL;
    http_request_free(&req);
  }

  /* 11. HTTP scheme URL without port and without path: http://127.0.0.1 */
  {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
    ASSERT(res == NULL);
    req.url = NULL;
    http_request_free(&req);
  }

  mock_server_destroy(srv);
  http_raw_context_free(ctx);
  PASS();
}

TEST test_http_raw_send_network_errors(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* Connection to closed port */
  req.url = "http://127.0.0.1:1/closed";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  ASSERT(res == NULL);

  /* Invalid host resolution */
  req.url = "http://nonexistent.invalid.domain.test:80/";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  ASSERT(res == NULL);

  req.url = NULL;
  http_request_free(&req);
  http_raw_context_free(ctx);
  PASS();
}

TEST test_http_raw_multi_requests(void) {
  struct HttpTransportContext *ctx = NULL;
  MockServerPtr srv = NULL;
  int port = 0;
  char url[128];
  struct HttpRequest req1, req2;
  struct HttpMultiRequest multi;
  struct HttpFuture *futures[2];
  struct HttpFuture f1, f2;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.timeout_ms = 200;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    http_config_free(&cfg);
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_raw_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_raw_send_multi(ctx, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_raw_send_multi(ctx, NULL, &multi, NULL));

  ASSERT_EQ(0, mock_server_init((MockServerPtr *)&srv));
  ASSERT_EQ(0, mock_server_start((MockServerPtr)srv));
  port = math_mock_server_get_port(srv);

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
  req1.url = url;
  req2.url = url;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req2));

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  futures[0] = &f1;
  futures[1] = &f2;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_raw_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, f1.is_ready);
  ASSERT_EQ(1, f2.is_ready);
  ASSERT(f1.response != NULL);
  ASSERT(f2.response != NULL);

  http_response_free(f1.response);
  free(f1.response);
  http_response_free(f2.response);
  free(f2.response);

  /* Multi with failing request */
  req1.url = "http://127.0.0.1:1/closed";
  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_raw_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, f1.is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, f1.error_code);

  http_multi_request_free(&multi);
  req1.url = NULL;
  req2.url = NULL;
  http_request_free(&req1);
  http_request_free(&req2);
  mock_server_destroy(srv);
  http_raw_context_free(ctx);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_http_raw_oom_and_mock_failures(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr srv = NULL;
  int port = 0;
  char url[128];

  ASSERT_EQ(0, mock_server_init((MockServerPtr *)&srv));
  ASSERT_EQ(0, mock_server_start((MockServerPtr)srv));
  port = math_mock_server_get_port(srv);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif

  /* 1. Context init alloc failure */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_context_init(&ctx));
  g_mock_alloc_fail = 0;

  /* 1b. Context init config init failure */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_context_init(&ctx));
  g_mock_alloc_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;

  /* 2. parse_url malloc failures */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;

  /* 2b. parse_url default path malloc failure */
  req.url = "http://127.0.0.1:8080";
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;
  req.url = url;

  /* 3. gethostbyname failure hook */
  g_mock_raw_gethostbyname_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_gethostbyname_fail = 0;

  /* 4. socket failure hook */
  g_mock_socket_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_socket_fail = 0;

  /* 5. make_socket_nonblocking failure hooks */
  g_mock_raw_nonblocking_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_nonblocking_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_nonblocking_fail = 0;

  /* 6. connect failure hooks */
  g_mock_raw_connect_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_connect_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_connect_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  g_mock_raw_connect_fail = 0;

  /* 7. select timeout / failure on connect */
  g_mock_select_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_select_fail = 0;

  /* 7b. select timeout / failure in recv loop */
  g_mock_select_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_select_fail = 0;

  /* 8. make_socket_blocking failure hooks */
  g_mock_raw_blocking_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_blocking_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_blocking_fail = 0;

  /* 9. request_buf malloc failure */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;

  /* 10. send failure hook */
  g_mock_raw_send_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_send_fail = 0;

  /* 11. res calloc failure */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;

  /* 12. body malloc failure */
  g_mock_raw_realloc_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_raw_realloc_fail = 0;

  /* 13. recv failure */
  g_mock_recv_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_recv_fail = 0;

  /* 14. realloc fail in headers expansion */
  {
    size_t h;
    char key[32], val[128];
    for (h = 0; h < 50; ++h) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(key, sizeof(key), "X-Hdr-%lu", (unsigned long)h);
      sprintf_s(val, sizeof(val),
                "Val-%lu-"
                "01234567890123456789012345678901234567890123456789012345678901"
                "23456789",
                (unsigned long)h);
#else
      sprintf(key, "X-Hdr-%lu", (unsigned long)h);
      sprintf(val,
              "Val-%lu-"
              "0123456789012345678901234567890123456789012345678901234567890123"
              "456789",
              (unsigned long)h);
#endif
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req.headers, key, val));
    }
    g_mock_raw_realloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
    g_mock_raw_realloc_fail = 0;
    http_headers_free(&req.headers);
  }

  /* 15. realloc fail in body expansion */
  {
    char big_body[5000];
    memset(big_body, 'B', sizeof(big_body));
    g_mock_raw_realloc_fail = 2;
    req.body = big_body;
    req.body_len = sizeof(big_body);
    req.method = HTTP_POST;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
    g_mock_raw_realloc_fail = 0;
    req.body = NULL;
    req.body_len = 0;
    req.method = HTTP_GET;
  }

  /* 16. Response init failure hook */
  g_mock_raw_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_raw_response_init_fail = 0;

  /* 17. Recv body realloc failure hook */
  g_mock_raw_realloc_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_raw_realloc_fail = 0;

  /* 17b. Recv body normal expansion (>8192 bytes) */
  {
    char *big_resp = (char *)malloc(9500);
    if (big_resp) {
      memset(big_resp, 'X', 9499);
      memcpy(big_resp, "HTTP/1.1 200 OK\r\n\r\n", 19);
      big_resp[9499] = '\0';
      g_mock_recv_data = big_resp;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
      ASSERT(res != NULL);
      ASSERT_EQ(200, res->status_code);
      ASSERT_EQ(9480, res->body_len);
      g_mock_recv_data = NULL;
      http_response_free(res);
      free(res);
      res = NULL;
      free(big_resp);
    }
  }

  /* 18. Recv data without HTTP/1. prefix */
  g_mock_recv_data = "STATUS 200 OK\r\nX-Foo: Bar\r\n\r\nCustomBody";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 18b. Recv response with headers and empty body */
  g_mock_recv_data = "HTTP/1.1 204 No Content\r\nX-Foo: Bar\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(204, res->status_code);
  ASSERT_EQ(0, res->body_len);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 18c. Recv response with invalid header line (no colon) */
  g_mock_recv_data =
      "HTTP/1.1 200 OK\r\nInvalidHeaderNoColon\r\nX-Valid: Yes\r\n\r\nBody";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 18c2. Recv response with header line and no colon anywhere in headers */
  g_mock_recv_data =
      "HTTP/1.1 200 OK\r\nInvalidHeaderNoColonAnywhere\r\n\r\nBody";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 18d. Recv 0 bytes (EOF immediately) */
  g_mock_recv_data = "";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(0, res->body_len);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 18e. Recv response with status and body but no headers */
  g_mock_recv_data = "HTTP/1.1 200 OK\r\n\r\nBodyWithoutHeaders";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 18f. real_body malloc failure */
  g_mock_raw_realloc_fail = 6;
  g_mock_recv_data = "HTTP/1.1 200 OK\r\n\r\nBodyWithoutHeaders";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  g_mock_raw_realloc_fail = 0;
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 19. Recv data without CRLFCRLF (no headers) */
  g_mock_recv_data = "Raw body with no headers at all";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 19b. Recv data without CRLFCRLF and body malloc failure */
  g_mock_raw_realloc_fail = 7;
  g_mock_recv_data = "Raw body with no headers at all";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_raw_realloc_fail = 0;
  g_mock_recv_data = NULL;
  http_response_free(res);
  free(res);
  res = NULL;

  /* 20. Headers add failure during response parsing */
  g_mock_raw_realloc_fail = 5;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_raw_realloc_fail = 0;

  req.url = NULL;
  http_request_free(&req);
  mock_server_destroy(srv);
  http_raw_context_free(ctx);
  PASS();
}
#endif

SUITE(http_raw_suite) {
  RUN_TEST(test_http_raw_lifecycle);
  RUN_TEST(test_http_raw_config);
  RUN_TEST(test_http_raw_url_validation);
  RUN_TEST(test_http_raw_send_requests);
  RUN_TEST(test_http_raw_send_network_errors);
  RUN_TEST(test_http_raw_multi_requests);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_http_raw_oom_and_mock_failures);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_RAW_H */
