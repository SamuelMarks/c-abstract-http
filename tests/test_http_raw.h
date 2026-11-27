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
#include "../src/crypto_utils.h"
#include "mock_alloc.h"
#include "abstract_http_test_helpers/mock_server.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_raw_edges(int step);
#endif

NO_DISCARD static c_abstract_http_error_t
raw_chunk_cb_success(void *user_data, const void *data, size_t len) {
  size_t *total = (size_t *)user_data;
  if (data && total)
    *total += len;
  return 0;
}

NO_DISCARD static c_abstract_http_error_t
raw_chunk_cb_fail(void *user_data, const void *data, size_t len) {
  if (user_data || data || len) {
    /* pass */
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_http_raw_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_global_cleanup());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT(ctx != NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
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
    cfg.timeout_ms = 5000;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 2. POST with body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.method = HTTP_POST;
  req.body = "hello world";
  req.body_len = 11;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 2b. POST with body != NULL and body_len == 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.method = HTTP_POST;
  req.body = "dummy";
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

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
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      req.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    }
  }

  /* 4. URL without explicit path: http://127.0.0.1:port */
  {
    char url_localhost[128];
    struct HttpRequest req_lh;
    struct HttpResponse *res_lh = NULL;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url_localhost, sizeof(url_localhost), "http://localhost:%d/test",
              port);
#else
    sprintf(url_localhost, "http://localhost:%d/test", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req_lh));
    req_lh.url = url_localhost;
    req_lh.method = HTTP_GET;
    if (http_raw_send(ctx, &req_lh, &res_lh) == C_ABSTRACT_HTTP_SUCCESS &&
        res_lh) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res_lh));
      free(res_lh);
    }
    req_lh.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req_lh));
  }
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
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
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
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* 5b. URL with path containing colon without port:
   * http://127.0.0.1/path:with:colons */
  {
    c_abstract_http_error_t raw_rc;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1/path:with:colons";
    raw_rc = http_raw_send(ctx, &req, &res);
    ASSERT(raw_rc == C_ABSTRACT_HTTP_SUCCESS ||
           raw_rc == C_ABSTRACT_HTTP_ERR_IO);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 7. on_chunk streaming callback failure */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.on_chunk = raw_chunk_cb_fail;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  ASSERT(res == NULL);
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

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
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  req.url = NULL;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 9. HTTPS scheme: ERR_NOTSUP without TLS hooks, or TLS handshake error
   * against plain-HTTP mock_server when OS TLS hooks are built in */
  {
    char https_url[128];
    c_abstract_http_error_t hrc;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(https_url, sizeof(https_url), "https://127.0.0.1:%d/test", port);
#else
    sprintf(https_url, "https://127.0.0.1:%d/test", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = https_url;
    hrc = http_raw_send(ctx, &req, &res);
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
    (defined(__APPLE__) || defined(_WIN32))
    ASSERT(hrc != C_ABSTRACT_HTTP_SUCCESS && hrc != C_ABSTRACT_HTTP_ERR_NOTSUP);
#else
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, hrc);
#endif
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* 10. HTTP scheme URL without port: http://127.0.0.1/path */
  {
    c_abstract_http_error_t raw_rc;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1/path";
    raw_rc = http_raw_send(ctx, &req, &res);
    ASSERT(raw_rc == C_ABSTRACT_HTTP_SUCCESS ||
           raw_rc == C_ABSTRACT_HTTP_ERR_IO);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* 11. HTTP scheme URL without port and without path: http://127.0.0.1 */
  {
    c_abstract_http_error_t raw_rc;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1";
    raw_rc = http_raw_send(ctx, &req, &res);
    ASSERT(raw_rc == C_ABSTRACT_HTTP_SUCCESS ||
           raw_rc == C_ABSTRACT_HTTP_ERR_IO);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
  PASS();
}

TEST test_http_raw_send_network_errors(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.timeout_ms = 500;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  }
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
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
    cfg.timeout_ms = 5000;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
  free(f1.response);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
  free(f2.response);

  /* Multi with failing request */
  req1.url = "http://127.0.0.1:1/closed";
  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_raw_send_multi(ctx, NULL, &multi, futures));
  ASSERT_EQ(1, f1.is_ready);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, f1.error_code);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  req1.url = NULL;
  req2.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
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

  /* 3. gethostbyname / getaddrinfo failure hook */
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
  g_mock_recv_data = "HTTP/1.1 200 OK\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_recv_data = NULL;
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
  g_mock_raw_res_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_raw_res_alloc_fail = 0;

  /* 12. body malloc failure */
  g_mock_raw_realloc_fail = 4;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_raw_realloc_fail = 0;
  g_mock_raw_realloc_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_raw_realloc_fail = 0;

  /* 13. recv failure */
  g_mock_recv_fail = 1;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_recv_fail = 0;
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
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_free(&req.headers));
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
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 18b. Recv response with headers and empty body */
  g_mock_recv_data = "HTTP/1.1 204 No Content\r\nX-Foo: Bar\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(204, res->status_code);
  ASSERT_EQ(0, res->body_len);
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 18c. Recv response with invalid header line (no colon) */
  g_mock_recv_data =
      "HTTP/1.1 200 OK\r\nInvalidHeaderNoColon\r\nX-Valid: Yes\r\n\r\nBody";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 18c2. Recv response with header line and no colon anywhere in headers */
  g_mock_recv_data =
      "HTTP/1.1 200 OK\r\nInvalidHeaderNoColonAnywhere\r\n\r\nBody";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 18d. Recv 0 bytes (EOF immediately) */
  g_mock_recv_data = "";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(0, res->body_len);
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 18e. Recv response with status and body but no headers */
  g_mock_recv_data = "HTTP/1.1 200 OK\r\n\r\nBodyWithoutHeaders";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 18f. real_body malloc failure */
  g_mock_raw_realloc_fail = 6;
  g_mock_recv_data = "HTTP/1.1 200 OK\r\n\r\nBodyWithoutHeaders";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  g_mock_raw_realloc_fail = 0;
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 19. Recv data without CRLFCRLF (no headers) */
  g_mock_recv_data = "Raw body with no headers at all";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  g_mock_recv_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 20. Headers add failure during response parsing */
  g_mock_raw_realloc_fail = 5;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_raw_realloc_fail = 0;
  g_mock_raw_realloc_fail = 5;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  ASSERT(res == NULL);
  g_mock_raw_realloc_fail = 0;

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
  PASS();
}

TEST test_http_raw_free_failures(void) {
  struct HttpTransportContext *ctx = NULL;
  c_abstract_http_error_t rc;
  rc = http_raw_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  g_mock_config_free_fail = 1;
  rc = http_raw_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_config_free_fail = 0;
  rc = http_raw_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}
#endif

struct raw_mock_tls_cfg {
  int open_fail;
  int hs_fail;
  int hs_wants;
  int write_fail;
  int write_wants;
  int write_zero;
  int read_fail;
  int read_wants;
  int close_fail;
  const char *cert_cn;
  const unsigned char *spki_der;
  size_t spki_len;
  const char *first_resp;
  const char *canned_resp;
  int calls;
};

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
struct raw_mock_tls_sess {
  int sock_fd;
  struct raw_mock_tls_cfg cfg;
  int hs_step;
  int wr_step;
  int rd_step;
  size_t canned_off;
};

NO_DISCARD static c_abstract_http_error_t
raw_mock_tls_open(void *provider_data, int sock_fd, const char *host,
                  const struct HttpConfig *config, void **out_session) {
  struct raw_mock_tls_cfg *pcfg = (struct raw_mock_tls_cfg *)provider_data;
  struct raw_mock_tls_sess *s;
  c_abstract_http_error_t rc;

  if (pcfg && pcfg->open_fail) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  if (config && pcfg && pcfg->cert_cn) {
    rc = c_abstract_http_tls_verify_handshake_config(
        config, host, pcfg->cert_cn, pcfg->spki_der, pcfg->spki_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  s = (struct raw_mock_tls_sess *)calloc(1, sizeof(*s));
  if (!s) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  s->sock_fd = sock_fd;
  if (pcfg) {
    s->cfg = *pcfg;
    if (pcfg->calls == 0 && pcfg->first_resp) {
      s->cfg.canned_resp = pcfg->first_resp;
    }
    pcfg->calls++;
  }
  *out_session = s;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
raw_mock_tls_handshake(void *session, int *out_want) {
  struct raw_mock_tls_sess *s = (struct raw_mock_tls_sess *)session;
  *out_want = 0;
  if (s->cfg.hs_fail) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  if (s->hs_step < s->cfg.hs_wants) {
    s->hs_step++;
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
raw_mock_tls_write(void *session, const void *buf, size_t len, size_t *out_n,
                   int *out_want) {
  struct raw_mock_tls_sess *s = (struct raw_mock_tls_sess *)session;
  long rv;
  *out_want = 0;
  *out_n = 0;
  if (s->cfg.write_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (s->wr_step < s->cfg.write_wants) {
    s->wr_step++;
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->cfg.write_zero) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->cfg.canned_resp) {
    *out_n = len;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(_WIN32)
  rv = (long)send((SOCKET)s->sock_fd, (const char *)buf, (int)len, 0);
#else
  rv = (long)send(s->sock_fd, buf, len, 0);
#endif
  if (rv <= 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  *out_n = (size_t)rv;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
raw_mock_tls_read(void *session, void *buf, size_t cap, size_t *out_n,
                  int *out_want) {
  struct raw_mock_tls_sess *s = (struct raw_mock_tls_sess *)session;
  int rv;
  *out_want = 0;
  *out_n = 0;
  if (s->cfg.read_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (s->cfg.read_wants < 0) {
    *out_want = 1;
    g_mock_select_fail = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->rd_step < s->cfg.read_wants) {
    s->rd_step++;
    *out_want = (s->cfg.canned_resp != NULL) ? 2 : 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->cfg.canned_resp) {
    size_t slen = strlen(s->cfg.canned_resp);
    size_t rem;
    if (s->canned_off >= slen) {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    rem = slen - s->canned_off;
    if (rem > cap) {
      rem = cap;
    }
    memcpy(buf, s->cfg.canned_resp + s->canned_off, rem);
    s->canned_off += rem;
    *out_n = rem;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(_WIN32)
  rv = (int)recv((SOCKET)s->sock_fd, (char *)buf, (int)cap, 0);
#else
  rv = (int)recv(s->sock_fd, buf, cap, 0);
#endif
  if (rv < 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  *out_n = (size_t)rv;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t raw_mock_tls_close(void *session) {
  struct raw_mock_tls_sess *s = (struct raw_mock_tls_sess *)session;
  int fail = s ? s->cfg.close_fail : 0;
  free(s);
  return fail ? C_ABSTRACT_HTTP_ERR_IO : C_ABSTRACT_HTTP_SUCCESS;
}

struct raw_upload_state {
  int step;
  int fail_on;
};

NO_DISCARD static c_abstract_http_error_t raw_upload_read_cb(void *user_data,
                                                             void *buf,
                                                             size_t max_len,
                                                             size_t *out_read) {
  struct raw_upload_state *st = (struct raw_upload_state *)user_data;
  (void)max_len;
  if (st->fail_on > 0 && st->step + 1 >= st->fail_on) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  if (st->step == 0) {
    memcpy(buf, "hello-", 6);
    *out_read = 6;
    st->step = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->step == 1) {
    memcpy(buf, "stream", 6);
    *out_read = 6;
    st->step = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_read = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int g_redir_counter = 0;

TEST test_http_raw_features_and_coverage(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpRawTlsHooks hooks;
  struct raw_mock_tls_cfg tcfg;
  MockServerPtr srv = NULL;
  int port = 0;
  char url[128];
  char https_url[128];
  size_t chunk_total = 0;

  ASSERT_EQ(0, mock_server_init(&srv));
  ASSERT_EQ(0, mock_server_start(srv));
  port = math_mock_server_get_port(srv);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/feat", port);
  sprintf_s(https_url, sizeof(https_url), "https://127.0.0.1:%d/feat", port);
#else
  sprintf(url, "http://127.0.0.1:%d/feat", port);
  sprintf(https_url, "https://127.0.0.1:%d/feat", port);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  /* Config validation branches */
  cfg.version_mask = HTTP_VERSION_2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_raw_config_apply(ctx, &cfg));
  cfg.version_mask = HTTP_VERSION_1_0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  cfg.proxy_url = (char *)"ftp://127.0.0.1:21";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_raw_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"socks5://127.0.0.1:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"socks5h://127.0.0.1:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"badproxy";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(ctx, &cfg));
  cfg.proxy_url = NULL;

  cfg.client_cert_path = (char *)"cert.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(ctx, &cfg));
  cfg.client_cert_path = NULL;
  cfg.client_key_path = (char *)"key.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(ctx, &cfg));
  cfg.client_key_path = NULL;
  cfg.ca_buffer_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_config_apply(ctx, &cfg));
  cfg.ca_buffer_len = 0;

  cfg.ca_path = (char *)"ca.pem";
  cfg.client_cert_path = (char *)"cert.pem";
  cfg.client_key_path = (char *)"key.pem";
  cfg.cert_pin_sha256 =
      (char *)"sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
  cfg.ca_buffer = (unsigned char *)"PEM";
  cfg.ca_buffer_len = 3;
  cfg.timeout_ms = 10000;
  cfg.connect_timeout_ms = 10000;
  cfg.read_timeout_ms = 10000;
  cfg.write_timeout_ms = 10000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  /* Reset non-heap test pointers before http_config_free */
  cfg.ca_path = NULL;
  cfg.client_cert_path = NULL;
  cfg.client_key_path = NULL;
  cfg.cert_pin_sha256 = NULL;
  cfg.ca_buffer = NULL;
  cfg.ca_buffer_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  /* TLS hooks registration & validation */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_set_tls_hooks(NULL, NULL));
  memset(&hooks, 0, sizeof(hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_set_tls_hooks(ctx, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, NULL));

  memset(&tcfg, 0, sizeof(tcfg));
  tcfg.cert_cn = "127.0.0.1";
  tcfg.spki_der = (const unsigned char *)"abc";
  tcfg.spki_len = 3;
  tcfg.hs_wants = 1;
  tcfg.write_wants = 1;
  tcfg.read_wants = 1;

  hooks.provider_data = &tcfg;
  hooks.open = raw_mock_tls_open;
  hooks.handshake = raw_mock_tls_handshake;
  hooks.write = raw_mock_tls_write;
  hooks.read = raw_mock_tls_read;
  hooks.close = raw_mock_tls_close;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));

  /* HTTPS with SPKI pin pass & mismatch */
  cfg.cert_pin_sha256 =
      (char *)"sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.cert_pin_sha256 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = https_url;
  req.method = HTTP_TRACE;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  cfg.cert_pin_sha256 = (char *)"sha256//badpin=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.cert_pin_sha256 = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.tls_version_mask = 0xFF00U;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_raw_send(ctx, &req, &res));
  cfg.tls_version_mask = HTTP_TLS_VERSION_DEFAULT;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  /* TLS error paths */
  tcfg.open_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));
  tcfg.open_fail = 0;
  tcfg.hs_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));
  tcfg.hs_fail = 0;
  tcfg.write_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.write_fail = 0;
  tcfg.write_zero = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.write_zero = 0;
  tcfg.hs_wants = 1;
  g_mock_select_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_select_fail = 0;
  tcfg.hs_wants = 0;

  tcfg.read_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  tcfg.read_fail = 0;

  tcfg.read_wants = -1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_select_fail = 0;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_select_fail = 0;
  tcfg.read_wants = 0;
  tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
  tcfg.close_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.close_fail = 0;
  tcfg.canned_resp = NULL;

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, NULL));

  /* Transfer-Encoding: chunked decoding + on_chunk + error cases */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  g_mock_recv_data = "HTTP/1.1 200 OK\r\n"
                     "transfer-encoding: \t chunked\r\n\r\n"
                     "5;ext=1\r\nHello\r\n"
                     "7\r\n, World\r\n"
                     "A\r\n-012345678\r\n"
                     "a\r\n9abcdefghi\r\n"
                     "0\r\nX-Trail: 1\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("Hello, World-0123456789abcdefghi", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Chunked with on_chunk streaming */
  chunk_total = 0;
  req.on_chunk = raw_chunk_cb_success;
  req.on_chunk_user_data = &chunk_total;
  g_mock_recv_data = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                     "5\r\nHello\r\n0\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(5U, chunk_total);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.on_chunk = raw_chunk_cb_fail;
  g_mock_recv_data = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                     "5\r\nHello\r\n0\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  req.on_chunk = NULL;

  /* Malformed chunked responses (deterministic EOF via canned_resp) */
  tcfg.hs_wants = 0;
  tcfg.write_wants = 0;
  tcfg.read_wants = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));
  req.url = https_url;
  tcfg.canned_resp =
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZZ\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.canned_resp =
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n;ext\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.canned_resp = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.canned_resp =
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabc";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.canned_resp =
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabcde\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  tcfg.canned_resp = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, NULL));
  g_mock_recv_data = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Streaming read_chunk upload (chunked + fixed expected_body_len + abort) */
  {
    struct raw_upload_state up_st;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url;
    req.method = HTTP_POST;
    req.read_chunk = raw_upload_read_cb;
    req.read_chunk_user_data = &up_st;

    memset(&up_st, 0, sizeof(up_st));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    memset(&up_st, 0, sizeof(up_st));
    req.expected_body_len = 12;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    memset(&up_st, 0, sizeof(up_st));
    up_st.fail_on = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, http_raw_send(ctx, &req, &res));
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Multipart req->parts flattening */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.parts.count = 1;
  req.parts.parts = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));
  req.parts.count = 0;

  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "User-Agent", "CustomUA/1.0"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_add_part(&req, "field", "f.txt", "text/plain",
                                  "multipart-payload", 17));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* HTTP Proxy + Proxy-Authorization + HTTPS CONNECT tunnel */
  {
    char proxy_str[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(proxy_str, sizeof(proxy_str), "http://127.0.0.1:%d", port);
#else
    sprintf(proxy_str, "http://127.0.0.1:%d", port);
#endif
    cfg.proxy_url = proxy_str;
    cfg.proxy_username = (char *)"puser";
    cfg.proxy_password = (char *)"ppass";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    cfg.proxy_url = NULL;
    cfg.proxy_username = NULL;
    cfg.proxy_password = NULL;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    /* HTTPS over HTTP proxy CONNECT (mock_server returns 200 OK!) */
    tcfg.hs_wants = 0;
    tcfg.write_wants = 0;
    tcfg.read_wants = 0;
    tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\nProxied";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));
    req.url = https_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_STR_EQ("Proxied", res->body);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    /* Proxy CONNECT 403 refusal */
    g_mock_recv_data = "HTTP/1.1 403 Forbidden\r\n\r\n";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));
    g_mock_recv_data = NULL;
    tcfg.canned_resp = NULL;

    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  }

  /* Redirects: 1-hop 302 POST->GET success, loop -> ERR_LOOP, and 302 without
   * Location */
  cfg.follow_redirects = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = https_url;
  req.method = HTTP_POST;
  req.body = "post";
  req.body_len = 4;
  tcfg.calls = 0;
  tcfg.first_resp =
      "HTTP/1.1 302 Found\r\nLocation: /next\r\nContent-Length: 0\r\n\r\n";
  tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nGoal";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("Goal", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  tcfg.first_resp = NULL;

  tcfg.canned_resp =
      "HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_LOOP, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 11;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;

  tcfg.canned_resp =
      "HTTP/1.1 302 Found\r\nLocation: /rel-oom\r\nContent-Length: 0\r\n\r\n";
  {
    int rk;
    for (rk = 0; rk < 20; ++rk) {
      c_abstract_http_error_t rrc;
      res = NULL;
      g_mock_alloc_count = rk;
      g_mock_alloc_fail = 1;
      rrc = http_raw_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      ASSERT((int)rrc >= -2000 && (int)rrc <= 2000);
      if (res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
      res = NULL;
      g_mock_alloc_count = rk;
      g_mock_alloc_fail = 1;
      g_mock_response_free_fail = 1;
      rrc = http_raw_send(ctx, &req, &res);
      g_mock_response_free_fail = 0;
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      ASSERT((int)rrc >= -2000 && (int)rrc <= 2000);
      if (res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
    }
  }

  tcfg.canned_resp = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, NULL));

  req.url = url;
  g_mock_select_fail = 2;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_select_fail = 0;
  g_mock_recv_data = "HTTP/1.1 302 Found\r\nContent-Length: 0\r\n\r\n";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(302, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_recv_data = NULL;
  req.url = NULL;
  req.body = NULL;
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  (void)g_redir_counter;

  /* ModalityEventLoop multiplexed send_multi: caller loop, empty batch, TLS,
   * read_chunk */
  {
    struct ModalityEventLoop *loop = NULL;
    struct HttpMultiRequest multi;
    struct HttpRequest r1, r2;
    struct HttpFuture f1, f2;
    struct HttpFuture *futs[2];
    struct raw_upload_state up_st;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    futs[0] = &f1;
    futs[1] = &f2;

    /* Empty batch with loop and without loop */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_raw_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_raw_send_multi(ctx, loop, &multi, futs));

    tcfg.canned_resp = "HTTP/1.1 200 OK\r\nTransfer-Encoding: "
                       "chunked\r\n\r\n4\r\nDone\r\n0\r\n\r\n";
    tcfg.hs_wants = 1;
    tcfg.write_wants = 1;
    tcfg.read_wants = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
    r1.url = https_url;
    r2.url = url;
    r2.method = HTTP_POST;
    memset(&up_st, 0, sizeof(up_st));
    r2.read_chunk = raw_upload_read_cb;
    r2.read_chunk_user_data = &up_st;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_raw_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(1, f1.is_ready);
    ASSERT_EQ(1, f2.is_ready);
    ASSERT_STR_EQ("Done", f1.response->body);
    ASSERT_EQ(200, f2.response->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

    r1.url = NULL;
    r2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  }

  /* OOM & edge sweeps across config_apply, send, and send_multi */
  {
    int step;
    char proxy_str[128];
    struct HttpConfig ocfg;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&ocfg));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(proxy_str, sizeof(proxy_str), "http://127.0.0.1:%d", port);
#else
    sprintf(proxy_str, "http://127.0.0.1:%d", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("OomAgent/1.0", &ocfg.user_agent));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(proxy_str, &ocfg.proxy_url));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("u", &ocfg.proxy_username));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("p", &ocfg.proxy_password));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("ca.pem", &ocfg.ca_path));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("cc.pem", &ocfg.client_cert_path));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("ck.pem", &ocfg.client_key_path));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("sha256//pin=", &ocfg.cert_pin_sha256));
    ocfg.ca_buffer = (unsigned char *)malloc(4);
    memcpy(ocfg.ca_buffer, "ABCD", 4);
    ocfg.ca_buffer_len = 4;

    for (step = 0; step < 15; ++step) {
      c_abstract_http_error_t arc;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      arc = http_raw_config_apply(ctx, &ocfg);
      g_mock_alloc_fail = 0;
      if (arc == C_ABSTRACT_HTTP_SUCCESS) {
        break;
      }
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&ocfg));

    /* Long URL + proxy_auth + read_chunk + multipart + chunked OOM sweeps */
    {
      char long_url[1400];
      struct raw_upload_state up_st;
      size_t k;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));
      tcfg.hs_wants = 0;
      tcfg.write_wants = 0;
      tcfg.read_wants = 0;
      tcfg.canned_resp =
          "HTTP/1.1 200 OK\r\nTransfer-Encoding: GZIP, CHUNKED\r\n\r\n"
          "4\r\nPart\r\n80\r\n"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\r\n"
          "0\r\n\r\n";

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(long_url, sizeof(long_url), "http://127.0.0.1:%d/", port);
#else
      sprintf(long_url, "http://127.0.0.1:%d/", port);
#endif
      k = strlen(long_url);
      while (k + 2 < sizeof(long_url)) {
        long_url[k++] = 'x';
      }
      long_url[k] = '\0';

      cfg.proxy_url = proxy_str;
      cfg.proxy_username = (char *)"u";
      cfg.proxy_password = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
      cfg.proxy_url = NULL;
      cfg.proxy_username = NULL;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      req.url = long_url;
      req.method = HTTP_POST;
      req.read_chunk = raw_upload_read_cb;
      req.read_chunk_user_data = &up_st;
      for (step = 0; step < 25; ++step) {
        c_abstract_http_error_t orc;
        memset(&up_st, 0, sizeof(up_st));
        g_mock_alloc_fail = 1;
        g_mock_alloc_count = step;
        orc = http_raw_send(ctx, &req, &res);
        g_mock_alloc_fail = 0;
        if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
          break;
        }
      }

      /* read_chunk with explicit Content-Length header */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req.headers, "Content-Length", "12"));
      memset(&up_st, 0, sizeof(up_st));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      req.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

      /* Proxy tunnel failure branches (OOM, send fail, select fail, recv fail,
       * oversized) */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      req.url = https_url;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = 3;
      ASSERT(http_raw_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
      g_mock_alloc_count = 4;
      ASSERT(http_raw_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
      g_mock_alloc_fail = 0;
      g_mock_raw_send_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
      g_mock_raw_send_fail = 0;
      g_mock_recv_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
      g_mock_recv_fail = 0;
      {
        char big_tun[1100];
        memset(big_tun, 'Z', sizeof(big_tun) - 1);
        big_tun[sizeof(big_tun) - 1] = '\0';
        g_mock_recv_data = big_tun;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
        g_mock_recv_data = NULL;
      }
      req.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

      /* Reset proxy_url and sweep OOM on chunked response + non-chunked
       * Transfer-Encoding */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      req.url = https_url;
      for (step = 0; step < 25; ++step) {
        c_abstract_http_error_t orc;
        g_mock_alloc_fail = 1;
        g_mock_alloc_count = step;
        orc = http_raw_send(ctx, &req, &res);
        g_mock_alloc_fail = 0;
        if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
          break;
        }
      }
      tcfg.canned_resp =
          "HTTP/1.1 200 OK\r\nTransfer-Encoding: identity\r\n\r\nPlain";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
      ASSERT_STR_EQ("Plain", res->body);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;

      /* HTTP/0.9 headerless response with on_chunk */
      {
        size_t cbytes = 0;
        req.on_chunk = raw_chunk_cb_success;
        req.on_chunk_user_data = &cbytes;
        tcfg.canned_resp = "RAW09";
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
        ASSERT_EQ(5U, cbytes);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
        req.on_chunk = NULL;
      }

      /* Multipart OOM & headers_init/free failures */
      req.method = HTTP_POST;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req.headers, "X-MP", "1"));
      ASSERT_EQ(
          C_ABSTRACT_HTTP_SUCCESS,
          http_request_add_part(&req, "f", "a.txt", "text/plain", "v", 1));
      tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
      for (step = 0; step < 12; ++step) {
        c_abstract_http_error_t orc;
        g_mock_alloc_fail = 1;
        g_mock_alloc_count = step;
        orc = http_raw_send(ctx, &req, &res);
        g_mock_alloc_fail = 0;
        if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
          break;
        }
      }
      g_mock_headers_init_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
      g_mock_headers_init_fail = 0;
      g_mock_headers_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
      g_mock_headers_free_fail = 0;
      tcfg.canned_resp = NULL;
      req.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    }

    /* Reset clean config with short timeout for send_multi timeout test */
    cfg.timeout_ms = 25;
    cfg.follow_redirects = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

    /* URL validation edge cases: invalid port & empty host & CONNECT method */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "http://127.0.0.1:0/badport";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));
    req.url = "http:///emptyhost";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));
    req.url = url;
    req.method = HTTP_CONNECT;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "Content-Length", "0"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(NULL, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* send_multi: redirect, HTTPS without TLS, invalid URL, null slot, timeout
     */
    {
      struct HttpMultiRequest multi;
      struct HttpRequest r1, r2, r3;
      struct HttpFuture f1, f2, f3;
      struct HttpFuture *futs[3];

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r3));

      /* 1. Null request / future slots & HTTPS without TLS & invalid URL */
      r1.url = https_url;
      r2.url = "bad-url";
      r3.url = "http://nonexistent.invalid.domain.test:80/";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, NULL));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r3));
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      memset(&f3, 0, sizeof(f3));
      futs[0] = &f1;
      futs[1] = &f2;
      futs[2] = NULL;
      ASSERT(http_raw_send_multi(ctx, NULL, &multi, futs) !=
             C_ABSTRACT_HTTP_SUCCESS);
      multi.requests[0] = NULL;
      futs[2] = &f3;
      ASSERT(http_raw_send_multi(ctx, NULL, &multi, futs) !=
             C_ABSTRACT_HTTP_SUCCESS);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));

      /* 2. send_multi redirect 302 -> 200 & redirect loop -> ERR_LOOP */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(ctx, &hooks));
      tcfg.hs_wants = 0;
      tcfg.write_wants = 0;
      tcfg.read_wants = 0;
      tcfg.calls = 0;
      tcfg.first_resp =
          "HTTP/1.1 302 Found\r\nLocation: /next\r\nContent-Length: 0\r\n\r\n";
      tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
      r1.url = https_url;
      r1.method = HTTP_POST;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
      memset(&f1, 0, sizeof(f1));
      futs[0] = &f1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_raw_send_multi(ctx, NULL, &multi, futs));
      ASSERT_EQ(200, f1.response->status_code);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
      tcfg.first_resp = NULL;

      tcfg.canned_resp = "HTTP/1.1 302 Found\r\nLocation: "
                         "https://127.0.0.1:1/x\r\nContent-Length: 0\r\n\r\n";
      memset(&f1, 0, sizeof(f1));
      ASSERT(http_raw_send_multi(ctx, NULL, &multi, futs) !=
             C_ABSTRACT_HTTP_SUCCESS);

      tcfg.canned_resp =
          "HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n";
      memset(&f1, 0, sizeof(f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_LOOP,
                http_raw_send_multi(ctx, NULL, &multi, futs));

      /* 3. send_multi timer timeout (read_wants = 999999 with want=1 on idle
       * socket) */
      tcfg.canned_resp = NULL;
      tcfg.read_wants = 999999;
      memset(&f1, 0, sizeof(f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_TIMEOUT,
                http_raw_send_multi(ctx, NULL, &multi, futs));
      tcfg.read_wants = 0;

      /* 4. OOM sweep on send_multi */
      tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
      for (step = 0; step < 30; ++step) {
        c_abstract_http_error_t mrc;
        memset(&f1, 0, sizeof(f1));
        g_mock_alloc_fail = 1;
        g_mock_alloc_count = step;
        mrc = http_raw_send_multi(ctx, NULL, &multi, futs);
        g_mock_alloc_fail = 0;
        if (mrc == C_ABSTRACT_HTTP_SUCCESS && f1.response) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
          free(f1.response);
        }
      }
      tcfg.canned_resp = NULL;

      r1.url = NULL;
      r2.url = NULL;
      r3.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r3));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    }

    /* OOM sweep across build_raw_request + proxy auth + multipart + chunked */
    {
      int k;
      struct HttpRequest mp_req;
      struct HttpMultiRequest mp_multi;
      struct HttpFuture mp_fut;
      struct HttpFuture *mp_futs[1];
      mp_futs[0] = &mp_fut;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup("http://127.0.0.1:1", &cfg.proxy_url));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup("u", &cfg.proxy_username));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup("p", &cfg.proxy_password));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&mp_req));
      mp_req.url = (char *)"http://example.com/p";
      mp_req.method = HTTP_POST;
      mp_req.body = "hi";
      mp_req.body_len = 2;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&mp_req.headers, "X-Test", "val"));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&mp_multi));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_multi_request_add(&mp_multi, &mp_req));
      for (k = 0; k < 20; ++k) {
        c_abstract_http_error_t orc;
        memset(&mp_fut, 0, sizeof(mp_fut));
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        orc = http_raw_send_multi(ctx, NULL, &mp_multi, mp_futs);
        g_mock_alloc_fail = 0;
        ASSERT((int)orc >= -2000 && (int)orc <= 2000);
        if (mp_fut.response) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_response_free(mp_fut.response));
          free(mp_fut.response);
        }
      }
      g_mock_alloc_count = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&mp_multi));
      mp_req.url = NULL;
      mp_req.body = NULL;
      mp_req.body_len = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&mp_req));
    }

    {
      int st;
      int k;
      struct ModalityEventLoop *rloop = NULL;
      struct HttpRequest mr;
      struct HttpMultiRequest mm;
      struct HttpFuture mf;
      struct HttpFuture *mfs[1];

      cfg.ca_buffer = (unsigned char *)"CABUF";
      cfg.ca_buffer_len = 5;
      for (k = 0; k < 10; ++k) {
        c_abstract_http_error_t arc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        arc = http_raw_config_apply(ctx, &cfg);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)arc >= -2000 && (int)arc <= 2000);
      }
      cfg.ca_buffer = NULL;
      cfg.ca_buffer_len = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

      for (st = 0; st <= 59; ++st) {
        c_abstract_http_error_t erc;
        if (st == 10) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_loop_op_fail(6, 0));
        } else if (st == 11 || st == 12) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_loop_op_fail(7, 0));
        } else if (st == 34 || st == 36) {
          g_mock_select_fail = 1;
        } else if (st == 39) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_raw_edges(39));
          g_mock_headers_free_fail = 1;
        } else if (st == 55) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_loop_op_fail(8, 0));
        } else if (st == 57) {
          g_mock_alloc_count = 0;
          g_mock_alloc_fail = 1;
        }
        erc = c_abstract_http_test_raw_edges(st);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_loop_op_fail(0, 0));
        g_mock_alloc_fail = 0;
        g_mock_select_fail = 0;
        g_mock_headers_free_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }

      /* Cover event_loop.c:952 (epoll/kqueue_sync_fd error in
       * http_loop_remove_fd) */
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__)) ||         \
    defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) ||     \
    defined(__DragonFly__)
      {
        struct ModalityEventLoop *kloop = NULL;
        int pfd[2] = {-1, -1};
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&kloop));
        if (pipe(pfd) == 0) {
          c_abstract_http_error_t rm_rc;
          ASSERT_EQ(
              C_ABSTRACT_HTTP_SUCCESS,
              http_loop_add_fd(kloop, pfd[0], HTTP_LOOP_READ, NULL, NULL));
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_loop_op_fail(9, 0));
          rm_rc = http_loop_remove_fd(kloop, pfd[0]);
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_loop_op_fail(0, 0));
          ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rm_rc);
          close(pfd[0]);
          close(pfd[1]);
        }
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(kloop));
      }
#endif

      for (k = 2; k <= 8; ++k) {
        c_abstract_http_error_t erc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        erc = c_abstract_http_test_raw_edges(57);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }

      /* Negative g_mock_headers_free_fail (-1 and -2) on step 39 -> covers
       * 2255-2258 */
      g_mock_headers_free_fail = -1;
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(39);
        g_mock_headers_free_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, erc);
      }
      g_mock_headers_free_fail = -2;
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(39);
        g_mock_headers_free_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }

      /* Cover raw_ensure_req_cap countdowns 10..17 across steps 2..6 */
      for (st = 2; st <= 6; ++st) {
        for (k = 10; k <= 17; ++k) {
          c_abstract_http_error_t erc;
          g_mock_raw_realloc_fail = k;
          erc = c_abstract_http_test_raw_edges(st);
          g_mock_raw_realloc_fail = 0;
          ASSERT((int)erc >= -2000 && (int)erc <= 2000);
        }
        for (k = 0; k < 10; ++k) {
          c_abstract_http_error_t erc;
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          erc = c_abstract_http_test_raw_edges(st);
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)erc >= -2000 && (int)erc <= 2000);
        }
      }

      /* Cover step 35 (resolve_redirect_url OOM), step 39 + response_free_fail,
       * step 40 + loop_op_fail(1,0), step 9 + loop_op_fail(5,0) & (1,0), step
       * 33 + response_free_fail */
      for (k = 0; k < 5; ++k) {
        c_abstract_http_error_t erc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        erc = c_abstract_http_test_raw_edges(35);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
      g_mock_headers_free_fail = 1;
      g_mock_response_free_fail = 1;
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(39);
        g_mock_response_free_fail = 0;
        g_mock_headers_free_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(1, 0));
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(40);
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
      {
        int ac;
        for (ac = 0; ac <= 6; ++ac) {
          c_abstract_http_error_t orc;
          g_mock_alloc_fail = 1;
          g_mock_alloc_count = ac;
          orc = c_abstract_http_test_raw_edges(40);
          g_mock_alloc_fail = 0;
          ASSERT((int)orc >= -2000 && (int)orc <= 2000);
        }
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 0));
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(9);
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(1, 0));
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(18);
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
      g_mock_response_free_fail = 1;
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(33);
        g_mock_response_free_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(4, 0));
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(9);
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, erc);
      }
#if !defined(_WIN32)
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(8, 0));
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(9);
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, erc);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(8, 0));
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(31);
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, erc);
      }
      g_mock_raw_nonblocking_fail = 1;
      {
        c_abstract_http_error_t erc = c_abstract_http_test_raw_edges(49);
        g_mock_raw_nonblocking_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }
#endif

      for (st = 13; st <= 16; ++st) {
        c_abstract_http_error_t erc;
        g_mock_response_free_fail = 1;
        erc = c_abstract_http_test_raw_edges(st);
        g_mock_response_free_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
        g_mock_raw_response_init_fail = 1;
        erc = c_abstract_http_test_raw_edges(st);
        g_mock_raw_response_init_fail = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
        for (k = 0; k < 8; ++k) {
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          erc = c_abstract_http_test_raw_edges(st);
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)erc >= -2000 && (int)erc <= 2000);
        }
      }

      for (k = 0; k < 8; ++k) {
        c_abstract_http_error_t erc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        erc = c_abstract_http_test_raw_edges(30);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)erc >= -2000 && (int)erc <= 2000);
      }

      /* send_multi loop op failures + NULL future/req + headers_free_fail */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&rloop));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&mr));
      mr.url = (char *)"http://127.0.0.1:1/x";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_request_add_part(&mr, "f", "f.txt", "text/plain", "a", 1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&mr.headers, "X-H", "1"));
      for (k = 0; k < 8; ++k) {
        c_abstract_http_error_t prc;
        struct HttpResponse *pres = NULL;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        g_mock_headers_free_fail = 1;
        prc = http_raw_send(ctx, &mr, &pres);
        g_mock_headers_free_fail = 0;
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)prc >= -2000 && (int)prc <= 2000);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&mm));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&mm, &mr));
      mfs[0] = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                http_raw_send_multi(ctx, NULL, &mm, mfs));

      mfs[0] = &mf;
      for (k = 0; k < 5; ++k) {
        c_abstract_http_error_t mrc;
        memset(&mf, 0, sizeof(mf));
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_loop_op_fail(1, 0));
        mrc = http_raw_send_multi(ctx, NULL, &mm, mfs);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_loop_op_fail(0, 0));
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      mr.url = (char *)"://bad";
      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 0));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, rloop, &mm, mfs);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_loop_op_fail(0, 0));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mrc);
      }
      mr.url = (char *)"http://127.0.0.1:1/x";
      memset(&mf, 0, sizeof(mf));
      g_mock_headers_free_fail = 1;
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, NULL, &mm, mfs);
        g_mock_headers_free_fail = 0;
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      memset(&mf, 0, sizeof(mf));
      g_mock_headers_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(1, 0));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, NULL, &mm, mfs);
        g_mock_headers_free_fail = 0;
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(1, 0));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, NULL, &mm, mfs);
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(2, 0));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, rloop, &mm, mfs);
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 1));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, rloop, &mm, mfs);
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(4, 0));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, NULL, &mm, mfs);
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      memset(&mf, 0, sizeof(mf));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(5, 0));
      {
        c_abstract_http_error_t mrc = http_raw_send_multi(ctx, NULL, &mm, mfs);
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      }

      mr.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&mm));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&mr));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(rloop));
    }
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
  PASS();
}
TEST test_http_raw_socks5_proxy(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr srv = NULL;
  int port = 0;
  char target_url[128];
  char socks5_url[128];
  char socks5h_url[128];
  char long_host_url[320];
  char long_cred[270];

  /* Canned binary SOCKS5 + HTTP responses */
  static const unsigned char s5_noauth_ipv4_ok[] = {
      0x05, 0x00, 0x05, 0x00, 0x00, 0x01, 127,  0,   0,   1,   0x00,
      0x50, 'H',  'T',  'T',  'P',  '/',  '1',  '.', '1', ' ', '2',
      '0',  '0',  ' ',  'O',  'K',  '\r', '\n', 'C', 'o', 'n', 't',
      'e',  'n',  't',  '-',  'L',  'e',  'n',  'g', 't', 'h', ':',
      ' ',  '2',  '\r', '\n', '\r', '\n', 'S',  '5'};
  static const unsigned char s5_auth_ipv6_ok[] = {
      0x05, 0x02, 0x01, 0x00, 0x05, 0x00, 0x00, 0x04, 0,    0,    0,
      0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
      0,    1,    0x01, 0xBB, 'H',  'T',  'T',  'P',  '/',  '1',  '.',
      '1',  ' ',  '2',  '0',  '0',  ' ',  'O',  'K',  '\r', '\n', 'C',
      'o',  'n',  't',  'e',  'n',  't',  '-',  'L',  'e',  'n',  'g',
      't',  'h',  ':',  ' ',  '2',  '\r', '\n', '\r', '\n', 'V',  '6'};
  static const unsigned char s5_auth_dom_ok[] = {
      0x05, 0x02, 0x01, 0x00, 0x05, 0x00, 0x00, 0x03, 0x04, 't',  'e',
      's',  't',  0x00, 0x50, 'H',  'T',  'T',  'P',  '/',  '1',  '.',
      '1',  ' ',  '2',  '0',  '0',  ' ',  'O',  'K',  '\r', '\n', 'C',
      'o',  'n',  't',  'e',  'n',  't',  '-',  'L',  'e',  'n',  'g',
      't',  'h',  ':',  ' ',  '2',  '\r', '\n', '\r', '\n', 'D',  'M'};
  static const unsigned char s5_bad_ver[] = {0x04, 0x00};
  static const unsigned char s5_method_ff[] = {0x05, 0xFF};
  static const unsigned char s5_unsolicited_auth[] = {0x05, 0x02};
  static const unsigned char s5_auth_deny[] = {0x05, 0x02, 0x01, 0x01};
  static const unsigned char s5_rep_bad_ver[] = {0x05, 0x00, 0x04,
                                                 0x00, 0x00, 0x01};
  static const unsigned char s5_rep_02[] = {0x05, 0x00, 0x05, 0x02, 0x00, 0x01};
  static const unsigned char s5_rep_03[] = {0x05, 0x00, 0x05, 0x03, 0x00, 0x01};
  static const unsigned char s5_rep_04[] = {0x05, 0x00, 0x05, 0x04, 0x00, 0x01};
  static const unsigned char s5_rep_05[] = {0x05, 0x00, 0x05, 0x05, 0x00, 0x01};
  static const unsigned char s5_rep_01[] = {0x05, 0x00, 0x05, 0x01, 0x00, 0x01};
  static const unsigned char s5_bad_atyp[] = {0x05, 0x00, 0x05,
                                              0x00, 0x00, 0x09};
  static const unsigned char s5_dom_trunc[] = {0x05, 0x00, 0x05,
                                               0x00, 0x00, 0x03};

  ASSERT_EQ(0, mock_server_init(&srv));
  ASSERT_EQ(0, mock_server_start(srv));
  port = math_mock_server_get_port(srv);

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(target_url, sizeof(target_url), "http://127.0.0.1:%d/s5", port);
  sprintf_s(socks5_url, sizeof(socks5_url), "socks5://127.0.0.1:%d/p", port);
  sprintf_s(socks5h_url, sizeof(socks5h_url), "socks5h://127.0.0.1:%d", port);
#else
  sprintf(target_url, "http://127.0.0.1:%d/s5", port);
  sprintf(socks5_url, "socks5://127.0.0.1:%d/p", port);
  sprintf(socks5h_url, "socks5h://127.0.0.1:%d", port);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = target_url;

  /* 1. raw_parse_proxy_url malformed cases (bad port, empty host, OOM, default
   * port/path forms) */
  cfg.proxy_url = (char *)"socks5://127.0.0.1:99999";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  cfg.proxy_url = (char *)"socks5://:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  /* Path with ':' after '/' and host without port */
  cfg.proxy_url = (char *)"socks5://127.0.0.1/foo:bar";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_raw_connect_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  cfg.proxy_url = (char *)"socks5h://127.0.0.1";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_connect_fail = 0;

  /* Corrupt internal proxy_url scheme to hit raw_parse_proxy_url fallback */
  cfg.proxy_url = socks5_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2; /* host + path succeed, proxy_host malloc fails */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_raw_send(ctx, &req, &res));
  g_mock_alloc_fail = 0;

  /* 2. socks5:// no-auth + IPv4 BND.ADDR success */
  g_mock_recv_bin = s5_noauth_ipv4_ok;
  g_mock_recv_bin_len = sizeof(s5_noauth_ipv4_ok);
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("S5", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 3. socks5h:// RFC 1929 username/password + IPv6 and Domain BND.ADDR +
   * send_multi */
  cfg.proxy_url = socks5h_url;
  cfg.proxy_username = (char *)"alice";
  cfg.proxy_password = (char *)"secret";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  g_mock_recv_bin = s5_auth_ipv6_ok;
  g_mock_recv_bin_len = sizeof(s5_auth_ipv6_ok);
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("V6", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* NULL password with non-NULL username + Domain BND.ADDR + send_multi */
  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  {
    struct HttpMultiRequest mm;
    struct HttpFuture mf;
    struct HttpFuture *mfs[1];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&mm));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&mm, &req));
    memset(&mf, 0, sizeof(mf));
    mfs[0] = &mf;

    g_mock_recv_bin = s5_auth_dom_ok;
    g_mock_recv_bin_len = sizeof(s5_auth_dom_ok);
    g_mock_recv_bin_eof = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_raw_send_multi(ctx, NULL, &mm, mfs));
    ASSERT(mf.is_ready != 0);
    ASSERT_EQ(200, mf.response->status_code);
    ASSERT_STR_EQ("DM", mf.response->body);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf.response));
    free(mf.response);
    mf.response = NULL;

    /* send_multi error branch over SOCKS5 */
    g_mock_recv_bin = s5_method_ff;
    g_mock_recv_bin_len = sizeof(s5_method_ff);
    g_mock_recv_bin_eof = 1;
    memset(&mf, 0, sizeof(mf));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
              http_raw_send_multi(ctx, NULL, &mm, mfs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&mm));
  }

  /* 4. SOCKS5 protocol error & truncation branches */
  cfg.proxy_url = socks5_url;
  cfg.proxy_username = NULL;
  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  /* Greeting write fail & CONNECT write fail (2nd write) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_test_raw_edges(60));
  g_mock_raw_send_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_send_fail = 11;
  g_mock_recv_bin = s5_noauth_ipv4_ok;
  g_mock_recv_bin_len = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_send_fail = 0;

  /* Greeting select fail & EOF */
  g_mock_select_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_select_fail = 0;

  g_mock_recv_bin = s5_bad_ver;
  g_mock_recv_bin_len = 0;
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  /* Bad SOCKS version, 0xFF no acceptable method, unsolicited 0x02 */
  g_mock_recv_bin = s5_bad_ver;
  g_mock_recv_bin_len = sizeof(s5_bad_ver);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_method_ff;
  g_mock_recv_bin_len = sizeof(s5_method_ff);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_unsolicited_auth;
  g_mock_recv_bin_len = sizeof(s5_unsolicited_auth);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));

  /* RFC 1929 empty/overlong credentials, auth write/read/status failures */
  cfg.proxy_username = (char *)"";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = s5_unsolicited_auth;
  g_mock_recv_bin_len = sizeof(s5_unsolicited_auth);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  memset(long_cred, 'u', 256);
  long_cred[256] = '\0';
  cfg.proxy_username = long_cred;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = s5_unsolicited_auth;
  g_mock_recv_bin_len = sizeof(s5_unsolicited_auth);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  cfg.proxy_username = (char *)"u";
  cfg.proxy_password = long_cred;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = s5_unsolicited_auth;
  g_mock_recv_bin_len = sizeof(s5_unsolicited_auth);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));

  cfg.proxy_password = (char *)"p";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_raw_send_fail = 11;
  g_mock_recv_bin = s5_unsolicited_auth;
  g_mock_recv_bin_len = sizeof(s5_unsolicited_auth);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_send_fail = 0;
  g_mock_recv_bin = s5_auth_deny;
  g_mock_recv_bin_len = sizeof(s5_auth_deny);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_unsolicited_auth;
  g_mock_recv_bin_len = sizeof(s5_unsolicited_auth);
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  /* Overlong target hostname on socks5h:// (> 255 chars) */
  cfg.proxy_url = socks5h_url;
  cfg.proxy_username = NULL;
  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  memcpy(long_host_url, "http://", 7);
  memset(long_host_url + 7, 'a', 258);
  memcpy(long_host_url + 265, "/x", 3);
  req.url = long_host_url;
  g_mock_recv_bin = s5_noauth_ipv4_ok;
  g_mock_recv_bin_len = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_raw_send(ctx, &req, &res));
  req.url = target_url;

  /* Unresolvable target host on socks5:// (local DNS failure) */
  cfg.proxy_url = socks5_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  req.url = (char *)"http://invalid.nonexistent.cah.invalid/x";
  g_mock_recv_bin = s5_noauth_ipv4_ok;
  g_mock_recv_bin_len = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  req.url = target_url;

  /* CONNECT reply error codes & truncated reply header / domain BND.ADDR */
  g_mock_recv_bin = s5_noauth_ipv4_ok;
  g_mock_recv_bin_len = 3;
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_rep_bad_ver;
  g_mock_recv_bin_len = sizeof(s5_rep_bad_ver);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_rep_02;
  g_mock_recv_bin_len = sizeof(s5_rep_02);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_rep_03;
  g_mock_recv_bin_len = sizeof(s5_rep_03);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_HOSTUNREACH, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_rep_04;
  g_mock_recv_bin_len = sizeof(s5_rep_04);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_HOSTUNREACH, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_rep_05;
  g_mock_recv_bin_len = sizeof(s5_rep_05);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CONNREFUSED, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_rep_01;
  g_mock_recv_bin_len = sizeof(s5_rep_01);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_bad_atyp;
  g_mock_recv_bin_len = sizeof(s5_bad_atyp);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = s5_dom_trunc;
  g_mock_recv_bin_len = sizeof(s5_dom_trunc);
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  g_mock_recv_bin = NULL;
  g_mock_recv_bin_len = 0;
  g_mock_recv_bin_eof = 0;

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  cfg.proxy_url = NULL;
  cfg.proxy_username = NULL;
  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
  PASS();
}

/** @brief Verify HTTP/1.1 keep-alive connection pooling, eviction, and OOM */
TEST test_http_raw_keep_alive_pool(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr srv = NULL;
  MockServerPtr srv2 = NULL;
  int port = 0;
  int port2 = 0;
  char url1[128];
  char url2[128];
  char proxy_url[128];
  char proxy_url2[128];
  char https_url1[128];
  char https_url2[128];
  static const unsigned char ka_resp[] =
      "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nKA";
  static const unsigned char close_resp[] =
      "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\nCL";
  static const unsigned char eof_resp[] = "HTTP/1.1 200 OK\r\n\r\nEOF";
  static const unsigned char bad_chunk_resp[] =
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZZ\r\n\r\n";

  ASSERT_EQ(0, mock_server_init(&srv));
  ASSERT_EQ(0, mock_server_start(srv));
  port = math_mock_server_get_port(srv);

  ASSERT_EQ(0, mock_server_init(&srv2));
  ASSERT_EQ(0, mock_server_start(srv2));
  port2 = math_mock_server_get_port(srv2);

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url1, sizeof(url1), "http://127.0.0.1:%d/ka1", port);
  sprintf_s(url2, sizeof(url2), "http://127.0.0.1:%d/ka2", port2);
  sprintf_s(proxy_url, sizeof(proxy_url), "http://127.0.0.1:%d", port);
  sprintf_s(proxy_url2, sizeof(proxy_url2), "http://127.0.0.1:%d", port2);
  sprintf_s(https_url1, sizeof(https_url1), "https://127.0.0.1:%d/s1", port);
  sprintf_s(https_url2, sizeof(https_url2), "https://127.0.0.1:%d/s2", port2);
#else
  sprintf(url1, "http://127.0.0.1:%d/ka1", port);
  sprintf(url2, "http://127.0.0.1:%d/ka2", port2);
  sprintf(proxy_url, "http://127.0.0.1:%d", port);
  sprintf(proxy_url2, "http://127.0.0.1:%d", port2);
  sprintf(https_url1, "https://127.0.0.1:%d/s1", port);
  sprintf(https_url2, "https://127.0.0.1:%d/s2", port2);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  cfg.keep_alive_enabled = 1;
  cfg.keep_alive_idle_ms = 500;
  cfg.max_idle_conns = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-KeepAlive-Test", "1"));

  /* 1. First request parks socket; second request reuses it without connect */
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  g_mock_recv_bin_eof = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_STR_EQ("KA", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  g_mock_raw_connect_fail = 1;
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_STR_EQ("KA", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_raw_connect_fail = 0;

  /* 2. Park 2nd origin, checkout 1st (non-tail memmove), then LRU evict */
  req.url = url2;
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Checkout url1 (index 0 while count==2 -> memmove in checkout) */
  req.url = url1;
  g_mock_raw_connect_fail = 1;
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_raw_connect_fail = 0;

  /* Proxy URL mismatch & match + LRU eviction when max_idle_conns == 2 */
  cfg.proxy_url = proxy_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Reuse with matching non-NULL proxy_url */
  g_mock_raw_connect_fail = 1;
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_raw_connect_fail = 0;

  /* Different non-NULL proxy_url triggers miss & LRU eviction */
  cfg.proxy_url = proxy_url2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 3. Idle-timeout eviction (partial + full) via mock clock jump */
  cfg.proxy_url = NULL;
  cfg.max_idle_conns = 0; /* exercises default 8 capacity + pool growth > 4 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_time_jump = 1;
  g_mock_time_jump_count = 0;
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_time_jump = 0;

  /* 4. Stale pooled connection recovery on write error & read EOF */
  /* 4a: stale write fail -> reconnect succeeds */
  g_mock_raw_send_fail = 10; /* fails 1st send(), succeeds on reconnect */
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_raw_send_fail = 0;

  /* 4b: stale write fail -> reconnect dial fails */
  g_mock_raw_send_fail = 10;
  g_mock_raw_connect_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_raw_connect_fail = 0;
  g_mock_raw_send_fail = 0;

  /* Re-seed pool from real mock_server (which sends Content-Length then closes
   * socket), so the next request gets a real 0-byte EOF on reused socket and
   * transparently reconnects to mock_server! */
  g_mock_recv_bin = NULL;
  g_mock_recv_bin_len = 0;
  g_mock_recv_bin_eof = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.keep_alive_enabled = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  cfg.keep_alive_enabled = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 4d: stale read EOF -> reconnect dial fails (+ response_free_fail) */
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = 0;
  g_mock_recv_bin_eof = 1;
  g_mock_raw_connect_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));

  /* Repeat with g_mock_response_free_fail = 1 */
  g_mock_raw_connect_fail = 0;
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  g_mock_recv_bin_eof = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = 0;
  g_mock_recv_bin_eof = 1;
  g_mock_raw_connect_fail = 1;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_raw_connect_fail = 0;

  /* 5. Non-keep-alive conditions: Response Connection: close, EOF body,
   * Request Connection: close, HTTP/1.0, malformed chunked response */
  g_mock_recv_bin = close_resp;
  g_mock_recv_bin_len = sizeof(close_resp) - 1;
  g_mock_recv_bin_eof = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  g_mock_recv_bin = eof_resp;
  g_mock_recv_bin_len = sizeof(eof_resp) - 1;
  g_mock_recv_bin_eof = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Connection", "close"));
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  g_mock_recv_bin_eof = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url1;
  cfg.version_mask = HTTP_VERSION_1_0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = ka_resp;
  g_mock_recv_bin_len = sizeof(ka_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  cfg.version_mask = HTTP_VERSION_DEFAULT;
  cfg.keep_alive_idle_ms = 0; /* default 30000ms branch */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));
  g_mock_recv_bin = bad_chunk_resp;
  g_mock_recv_bin_len = sizeof(bad_chunk_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 1;
  g_mock_recv_bin = bad_chunk_resp;
  g_mock_recv_bin_len = sizeof(bad_chunk_resp) - 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;

  /* 6. OOM sweep across keep-alive checkin (realloc + host/proxy strdup) */
  {
    struct HttpTransportContext *octx = NULL;
    int k;
    cfg.proxy_url = proxy_url;
    for (k = 0; k < 14; ++k) {
      c_abstract_http_error_t orc;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&octx));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(octx, &cfg));
      g_mock_recv_bin = ka_resp;
      g_mock_recv_bin_len = sizeof(ka_resp) - 1;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = k;
      orc = http_raw_send(octx, &req, &res);
      g_mock_alloc_fail = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(octx));
    }
    /* Also checkin failure + response_free_fail */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&octx));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(octx, &cfg));
    g_mock_recv_bin = ka_resp;
    g_mock_recv_bin_len = sizeof(ka_resp) - 1;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 8;
    g_mock_response_free_fail = 1;
    {
      c_abstract_http_error_t orc = http_raw_send(octx, &req, &res);
      ASSERT(orc != C_ABSTRACT_HTTP_SUCCESS);
    }
    g_mock_response_free_fail = 0;
    g_mock_alloc_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(octx));
    cfg.proxy_url = NULL;
  }

  /* 7. Pooled TLS hooks close_fail error percolation (evict, LRU, clear,
   * stale) */
  {
    struct HttpTransportContext *tctx = NULL;
    struct HttpRawTlsHooks thooks;
    struct raw_mock_tls_cfg tcfg;
    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
    memset(&thooks, 0, sizeof(thooks));
    thooks.provider_data = &tcfg;
    thooks.open = raw_mock_tls_open;
    thooks.handshake = raw_mock_tls_handshake;
    thooks.read = raw_mock_tls_read;
    thooks.write = raw_mock_tls_write;
    thooks.close = raw_mock_tls_close;

    g_mock_recv_bin = NULL;
    g_mock_recv_bin_len = 0;
    g_mock_recv_bin_eof = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&tctx));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(tctx, &thooks));
    cfg.keep_alive_idle_ms = 500;
    cfg.max_idle_conns = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(tctx, &cfg));

    /* Park https_url1 with close_fail = 1 -> context_free percolates close err
     */
    tcfg.close_fail = 1;
    req.url = https_url1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_context_free(tctx));

    /* Evict expired close_fail on checkout & checkin + LRU close_fail */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_init(&tctx));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_set_tls_hooks(tctx, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(tctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    /* LRU eviction on https_url2 with close_fail=1 (+ response_free_fail) */
    req.url = https_url2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));

    req.url = https_url1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = https_url2;
    g_mock_response_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));
    g_mock_response_free_fail = 0;

    /* Stale read EOF on TLS session with close_fail=1 */
    req.url = https_url1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    tcfg.canned_resp = "";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));
    tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";

    /* Re-park https_url1, then jump time so checkout evicts with close_fail=1
     */
    req.url = https_url1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    g_mock_time_jump = 1;
    g_mock_time_jump_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));
    g_mock_time_jump = 0;

    /* Park https_url1, then jump time ONLY during checkin so checkin evicts
     * with close_fail=1 */
    cfg.max_idle_conns = 4;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(tctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = https_url2;
    g_mock_time_jump = 1;
    g_mock_time_jump_count = 1; /* checkout sees normal time, checkin jumps! */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));
    g_mock_time_jump = 0;

    /* Stale write fail on TLS session with close_fail=1 */
    req.url = https_url1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    g_mock_raw_send_fail = 10;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    g_mock_raw_send_fail = 0;

    /* 1. Clock fail on raw_pool_evict_expired */
    g_mock_time_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));
    g_mock_time_fail = 0;

    /* 2. Both initial write AND redial retry write fail on reused socket */
    g_mock_raw_send_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_send(tctx, &req, &res));
    g_mock_raw_send_fail = 0;

    /* 3. Stale EOF in recv loop on tctx -> redial -> retry write fails */
    req.url = https_url1;
    tcfg.close_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    {
      c_abstract_http_error_t tmp_rc;
      tcfg.canned_resp = "";
      g_mock_raw_send_fail = 11; /* 1st write ok (11->10), 2nd write fails! */
      tmp_rc = http_raw_send(tctx, &req, &res);
      g_mock_raw_send_fail = 0;
      tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, tmp_rc);

      /* Repeat with g_mock_response_free_fail = 1 */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      tcfg.canned_resp = "";
      g_mock_raw_send_fail = 11;
      g_mock_response_free_fail = 1;
      tmp_rc = http_raw_send(tctx, &req, &res);
      g_mock_response_free_fail = 0;
      g_mock_raw_send_fail = 0;
      tcfg.canned_resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
      ASSERT(tmp_rc != C_ABSTRACT_HTTP_SUCCESS);
    }

    /* 4. Connection: close request on tctx with close_fail + response_free_fail
     */
    req.url = https_url1;
    tcfg.close_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "Connection", "close"));
    {
      c_abstract_http_error_t tmp_rc;
      g_mock_response_free_fail = 1;
      tmp_rc = http_raw_send(tctx, &req, &res);
      g_mock_response_free_fail = 0;
      tcfg.close_fail = 0;
      ASSERT(tmp_rc != C_ABSTRACT_HTTP_SUCCESS);
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_free(&req.headers));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_init(&req.headers));

    /* 5. Non-NULL http_raw_set_tls_hooks when raw_pool_clear fails */
    tcfg.close_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_set_tls_hooks(tctx, &thooks));

    /* Park again for NULL set_tls_hooks fail */
    tcfg.close_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_send(tctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_raw_set_tls_hooks(tctx, NULL));
    tcfg.close_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(tctx));
  }

  /* Disable keep_alive flushes pool */
  cfg.keep_alive_enabled = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_config_apply(ctx, &cfg));

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_raw_context_free(ctx));
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
  RUN_TEST(test_http_raw_free_failures);
  RUN_TEST(test_http_raw_features_and_coverage);
  RUN_TEST(test_http_raw_socks5_proxy);
  RUN_TEST(test_http_raw_keep_alive_pool);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_RAW_H */
