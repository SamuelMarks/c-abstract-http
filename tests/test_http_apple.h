
/**
 * @file test_http_apple.h
 * @brief Unit tests for the Apple Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, and parameter validation.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H
#define C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_apple.h>
#include <c_abstract_http/http_types.h>
#include "abstract_http_test_helpers/mock_server.h"
/* clang-format on */

#if defined(__APPLE__) && (!defined(MAC_OS_X_VERSION_MIN_REQUIRED) ||          \
                           MAC_OS_X_VERSION_MIN_REQUIRED < 101100)
#define TEST_APPLE_HAVE_CFSTREAM 1
#else
#define TEST_APPLE_HAVE_CFSTREAM 0
#endif

#if defined(__APPLE__) && defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_apple_tls_challenge(struct HttpTransportContext *ctx,
                                         int mode,
                                         const unsigned char *cert_der,
                                         size_t cert_der_len, long *out_disp,
                                         int *out_has_cred);
#endif

NO_DISCARD static c_abstract_http_error_t
mock_on_chunk_cb(void *user_data, const void *chunk, size_t len) {
  int *calls = (int *)user_data;
  (void)chunk;
  (void)len;
  (*calls)++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_on_chunk_abort_cb(void *user_data, const void *chunk, size_t len) {
  (void)user_data;
  (void)chunk;
  (void)len;
  return C_ABSTRACT_HTTP_ERR_CANCELED;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && TEST_APPLE_HAVE_CFSTREAM
TEST test_apple_oom_branches(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));

#if defined(__APPLE__)

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_url_str", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_url", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* fail_url_ref removed because urlRef parsing was optimized out */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_request_ref", &req.url));
  req.method = HTTP_GET;
  {
    int debug_rc = http_apple_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, debug_rc);
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_body_data", &req.url));
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("test", (char **)&req.body));
  req.body_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_read_stream", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_read_stream_open", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_mutable_data", &req.url));
  req.method = HTTP_POST;
  req.read_chunk = (http_read_chunk_fn)1;
  req.expected_body_len = 10;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_cb_rc", &req.url));
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("test", (char **)&req.body));
  req.body_len = 4;
  req.on_chunk = mock_on_chunk_cb;
  {
    int calls = 0;
    req.on_chunk_user_data = &calls;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_apple_send(ctx, &req, &res));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_stream_client", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_cb_edges", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_after_body", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_read_negative", &req.url));
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_body_alloc", &req.url));
  req.method = HTTP_GET;
  {
    int rc_val = http_apple_send(ctx, &req, &res);
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_val, "%d");
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_runloop_with_body", &req.url));
  req.method = HTTP_GET;
  {
    int rc_val = http_apple_send(ctx, &req, &res);
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_IO, rc_val, "%d");
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_extract_response", &req.url));
  req.method = HTTP_GET;
  {
    int rc_val = http_apple_send(ctx, &req, &res);
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_val, "%d");
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_body_malloc", &req.url));
  req.method = HTTP_GET;
  {
    int rc_val = http_apple_send(ctx, &req, &res);
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_val, "%d");
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_cb_edges", &req.url));
  req.method = HTTP_GET;
  {
    int rc_val = http_apple_send(ctx, &req, &res);
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_SUCCESS, rc_val, "%d");
  }
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_apple_oom(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = http_apple_context_init(&ctx);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  {
    int rc_test_tmp = http_apple_context_init(&ctx);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  req.url = "http://example.com";
  req.method = HTTP_GET;

  /* Invalid URL string creating NULL CFURL */
  req.url = "ht tp:// bad";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));

  /* Non-null body with body_len == 0 */
  req.url = "http://example.com";
  req.body = (void *)"dummy";
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  req.body = NULL;

  /* Test 124: malloc for *res fails */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = http_apple_send(ctx, &req, &res);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  g_mock_headers_init_fail = 1;
  {
    int rc_val = http_apple_send(ctx, &req, &res);
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_IO, rc_val, "%d");
  }
  g_mock_headers_init_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}
#endif

/** @brief Documented */
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);
  ASSERT(port > 0);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/echo", port);
#else
  c89stringutils_snprintf(url, sizeof(url), "http://127.0.0.1:%d/echo", port);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));

  /* Normal request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)malloc(strlen(url) + 1);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, strlen(url) + 1, url);
#else
  strcpy(req.url, url);
#endif
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("Hello Apple!", (char **)&req.body));
  req.body_len = strlen("Hello Apple!");

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(res->body_len > 0);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  res = NULL;

  /* On-chunk request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_wait_for_request(server, &mock_req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(&mock_req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)malloc(strlen(url) + 1);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, strlen(url) + 1, url);
#else
  strcpy(req.url, url);
#endif
  req.method = HTTP_GET;
  req.on_chunk = mock_on_chunk_cb;
  req.on_chunk_user_data = &on_chunk_calls;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT(on_chunk_calls > 0);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_wait_for_request(server, &mock_req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(&mock_req));

  /* Test real on_chunk returning non-zero error */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url;
  req.method = HTTP_GET;
  req.on_chunk = mock_on_chunk_abort_cb;
  res = NULL;
  ASSERT_NEQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_wait_for_request(server, &mock_req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(&mock_req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  PASS();
}

/** @brief Documented */
TEST test_apple_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_global_init());

  /* Init */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT(ctx != NULL);

  /* Free */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_global_cleanup());

  PASS();
}

/** @brief Documented */
TEST test_apple_config(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT(ctx != NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_config_apply(NULL, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}

/** @brief Documented */
TEST test_apple_send_invalid(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, NULL));

#if defined(__APPLE__)
  /* Valid input but we need to mock a real URL so it fails gracefully */
  req.url = (char *)malloc(sizeof("http://localhost:1"));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1");
#endif
  req.method = HTTP_GET;
  /* Might fail with C_ABSTRACT_HTTP_ERR_IO due to no connection or return an
   * allocated res */
  {
    c_abstract_http_error_t rc = http_apple_send(ctx, &req, &res);
    ASSERT(rc != 0);
  }
#else
  /* Valid input but not implemented (or no Apple OS) should return ENOSYS */
  ASSERT_EQ(ENOSYS, http_apple_send(ctx, &req, &res));
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}

/** @brief Documented */
TEST test_apple_send_all_methods(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig cfg;
  int i;
  const enum HttpMethod methods[] = {

      HTTP_POST,    HTTP_PUT,   HTTP_DELETE,  HTTP_PATCH,         HTTP_HEAD,
      HTTP_OPTIONS, HTTP_TRACE, HTTP_CONNECT, (enum HttpMethod)99 /* Invalid
                                                                     method */
  };

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  cfg.verify_peer = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

  for (i = 0; i < 9; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)malloc(sizeof("http://localhost:1"));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
    strcpy(req.url, "http://localhost:1");
#endif
    req.method = methods[i];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "X-Test", "Value"));

    if (i == 8) {
      /* Invalid method may fail differently, let's just see if it crashes */
      c_abstract_http_error_t rc_invalid = http_apple_send(ctx, &req, &res);
      ASSERT(rc_invalid != C_ABSTRACT_HTTP_SUCCESS);
    } else {
      c_abstract_http_error_t rc = http_apple_send(ctx, &req, &res);
      ASSERT(rc != 0);
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
mock_read_chunk(void *user_data, void *buf, size_t buf_len, size_t *out_read) {
  int *calls = (int *)user_data;
  if (*calls >= 2) {
    *out_read = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  (*calls)++;
  if (buf_len > 4)
    buf_len = 4;
  memcpy(buf, "test", buf_len);
  *out_read = buf_len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_read_chunk_fail(void *user_data, void *buf, size_t buf_len,
                     size_t *out_read) {
  (void)user_data;
  (void)buf;
  (void)buf_len;
  (void)out_read;
  return C_ABSTRACT_HTTP_ERR_IO;
}

/** @brief Documented */
TEST test_apple_read_chunk(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig cfg;
  int calls = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  cfg.verify_peer = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

  /* Success chunk */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)malloc(sizeof("http://localhost:1"));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1");
#endif
  req.method = HTTP_POST;
  req.read_chunk = mock_read_chunk;
  req.read_chunk_user_data = &calls;
  req.expected_body_len = 8;

  /* Will fail to connect but it hits the read_chunk loop */
  ASSERT(http_apple_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }

  /* Fail chunk */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)malloc(sizeof("http://localhost:1"));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1");
#endif
  req.method = HTTP_POST;
  req.read_chunk = mock_read_chunk_fail;
  req.read_chunk_user_data = NULL;
  req.expected_body_len = 8;

  /* Will fail with C_ABSTRACT_HTTP_ERR_IO */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}

/** @brief Documented */

/** @brief Documented */
TEST test_apple_send_multi(void) {
#if defined(__APPLE__)
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req1, req2;
  struct HttpMultiRequest multi;
  struct HttpFuture *future1 = NULL, *future2 = NULL;
  struct HttpFuture *futures[2];
  struct ModalityEventLoop *loop = NULL;
  int port;
  char url[256];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/echo", port);
#else
  c89stringutils_snprintf(url, sizeof(url), "http://127.0.0.1:%d/echo", port);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url, &req1.url));
  req1.method = HTTP_GET;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url, &req2.url));
  req2.method = HTTP_GET;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req2));

  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  future2 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  futures[1] = future2;

  rc = http_apple_send_multi(ctx, loop, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  while (!future1->is_ready || !future2->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }

  ASSERT_EQ(200, future1->response->status_code);
  ASSERT_EQ(200, future2->response->status_code);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
  free(future1->response);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future2->response));
  free(future2->response);
  free(future1);
  free(future2);

  /* Test invalid args */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send_multi(NULL, loop, &multi, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send_multi(ctx, loop, NULL, futures));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send_multi(ctx, loop, &multi, NULL));

  /* Test NULL loop */
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  future2 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  futures[1] = future2;
  g_mock_pthread_create_sync = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, NULL, &multi, futures));
  g_mock_pthread_create_sync = 0;
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  if (future2->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future2->response));
    free(future2->response);
  }
  free(future1);
  free(future2);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  PASS();
}

NO_DISCARD static c_abstract_http_error_t dummy_apple_fail_wakeup(void *ctx) {
  (void)ctx;
  return C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD static c_abstract_http_error_t
dummy_apple_add_fd_fail(void *ctx, int fd, int events, http_loop_cb cb,
                        void *user_data) {
  (void)ctx;
  (void)fd;
  (void)events;
  (void)cb;
  (void)user_data;
  return C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD static c_abstract_http_error_t
dummy_apple_add_fd_capture(void *ctx, int fd, int events, http_loop_cb cb,
                           void *user_data) {
  void **pack = (void **)ctx;
  (void)events;
  *(http_loop_cb *)pack[0] = cb;
  *(int *)pack[1] = fd;
  *(void **)pack[2] = user_data;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t dummy_apple_rm_fd_fail(void *ctx,
                                                                 int fd) {
  (void)ctx;
  (void)fd;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_apple_send_multi_branches(void) {
#if defined(__APPLE__)
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req1;
  struct HttpMultiRequest multi;
  struct HttpFuture *future1 = NULL;
  struct HttpFuture *futures[2];
  struct ModalityEventLoop *loop = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));

  /* Test all methods with payload and verify_peer = 0 */
  {
    enum HttpMethod methods[8];
    size_t m;
    methods[0] = HTTP_POST;
    methods[1] = HTTP_PUT;
    methods[2] = HTTP_DELETE;
    methods[3] = HTTP_PATCH;
    methods[4] = HTTP_HEAD;
    methods[5] = HTTP_OPTIONS;
    methods[6] = HTTP_TRACE;
    methods[7] = HTTP_CONNECT;
    for (m = 0; m < sizeof(methods) / sizeof(methods[0]); ++m) {
      struct HttpRequest req_m;
      struct HttpConfig cfg;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req_m));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_mock_strdup("http://127.0.0.1:1/nonexistent",
                                            &req_m.url));
      req_m.method = methods[m];
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req_m.headers, "X-Multi-Test", "Value"));
      ASSERT_EQ(
          C_ABSTRACT_HTTP_SUCCESS,
          c_abstract_http_mock_strdup("sample payload", (char **)&req_m.body));
      req_m.body_len = strlen("sample payload");
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_multi_request_add(&multi, &req_m));
      future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
      futures[0] = future1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
      cfg.verify_peer = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_apple_send_multi(ctx, loop, &multi, futures));
      while (!future1->is_ready) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
      cfg.verify_peer = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
      if (future1->response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_response_free(future1->response));
        free(future1->response);
      }
      free(future1);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req_m));
    }
  }

  /* Test fail_multi_url_str */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  req1.url = "ht tp:// bad";
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, future1->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(future1));
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));

  /* Test non-null body with body_len == 0 in multi */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  req1.url = "http://example.com";
  req1.method = HTTP_POST;
  req1.body = (void *)"dummy";
  req1.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  req1.body = NULL;

  /* Test invalid method in multi */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  req1.url = "http://127.0.0.1:1/badmethod";
  req1.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, future1->error_code);
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && TEST_APPLE_HAVE_CFSTREAM
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_multi_url_str", &req1.url));
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            future1->response->status_code ? 0 : C_ABSTRACT_HTTP_ERR_INVAL);
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test fail_multi_url */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_multi_url", &req1.url));
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test fail_multi_request_ref */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_multi_request_ref", &req1.url));
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test fail_multi_stream */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_multi_stream", &req1.url));
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test fail_multi_client */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_multi_client", &req1.url));
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  do {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  } while (!future1->is_ready);
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test fail_multi_open */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://fail_multi_open", &req1.url));
  req1.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
  }
  if (future1->response) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
    free(future1->response);
  }
  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test wctx malloc and pthread_create failure */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://example.com", &req1.url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_apple_send_multi(ctx, loop, &multi, futures));
  g_mock_alloc_fail = 0;

  g_mock_pthread_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_apple_send_multi(ctx, loop, &multi, futures));
  g_mock_pthread_fail = 0;

  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));

  /* Test sync worker mock failures */
  g_mock_pthread_create_sync = 1;

  /* states alloc fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://example.com", &req1.url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  g_mock_alloc_fail = 0;

  /* streams alloc fail */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  g_mock_alloc_fail = 0;

  /* res alloc fail */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  g_mock_alloc_fail = 0;

  /* res init fail */
  g_mock_headers_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send_multi(ctx, loop, &multi, futures));
  g_mock_headers_init_fail = 0;

  free(future1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
  g_mock_pthread_create_sync = 0;

  /* Test wakeup failure */
  {
    struct ModalityEventLoop *fail_loop = NULL;
    struct HttpLoopHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.wakeup = dummy_apple_fail_wakeup;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_init_external(&fail_loop, &hooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_mock_strdup("http://example.com", &req1.url));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
    future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
    futures[0] = future1;

    g_mock_pthread_create_sync = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_send_multi(ctx, fail_loop, &multi, futures));
    g_mock_pthread_create_sync = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(future1));
    free(future1);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(fail_loop));
  }

  /* Test extract_response failure in apple_multi_worker */
  {
    MockServerPtr srv_ext = NULL;
    char ext_url[128];
    struct MockServerRequest mock_ext_req;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&srv_ext));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(srv_ext));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(ext_url, sizeof(ext_url),
              "http://127.0.0.1:%d/fail_extract_response",
              math_mock_server_get_port(srv_ext));
#else
    c89stringutils_snprintf(ext_url, sizeof(ext_url),
                            "http://127.0.0.1:%d/fail_extract_response",
                            math_mock_server_get_port(srv_ext));
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_mock_strdup(ext_url, &req1.url));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
    future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
    futures[0] = future1;
    g_mock_pthread_create_sync = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_send_multi(ctx, loop, &multi, futures));
    g_mock_pthread_create_sync = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, future1->error_code);
    if (future1->response) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(future1->response));
      free(future1->response);
    }
    free(future1);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_wait_for_request(srv_ext, &mock_ext_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_request_cleanup(&mock_ext_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv_ext));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
  }
#endif /* defined(C_ABSTRACT_HTTP_TEST_OOM) && TEST_APPLE_HAVE_CFSTREAM */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
#endif
  PASS();
}

#if defined(__APPLE__)
struct AppleMiniSrv {
  int lfd;
  volatile int stop;
};

static void *apple_mini_srv_worker(void *arg) {
  struct AppleMiniSrv *srv = (struct AppleMiniSrv *)arg;
  char *large_buf = (char *)malloc(20000);
  if (large_buf) {
    memset(large_buf, 'Z', 20000);
  }
  while (!srv->stop) {
    fd_set rfds;
    struct timeval tv;
    int sret;
    FD_ZERO(&rfds);
    FD_SET(srv->lfd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 15000;
    sret = select(srv->lfd + 1, &rfds, NULL, NULL, &tv);
    if (sret > 0 && FD_ISSET(srv->lfd, &rfds)) {
      struct sockaddr_in cli;
      socklen_t clen = sizeof(cli);
      int cfd = accept(srv->lfd, (struct sockaddr *)&cli, &clen);
      if (cfd >= 0) {
        char rbuf[2048];
        ssize_t nr = recv(cfd, rbuf, sizeof(rbuf) - 1, 0);
        if (nr > 0) {
          rbuf[nr] = '\0';
          if (strstr(rbuf, "/slow") != NULL) {
            usleep(180000);
            {
              const char *hdr = "HTTP/1.1 200 OK\r\nContent-Length: "
                                "2\r\nConnection: close\r\n\r\nok";
              send(cfd, hdr, strlen(hdr), 0);
            }
          } else if (strstr(rbuf, "/empty") != NULL) {
            const char *hdr = "HTTP/1.1 204 No Content\r\nContent-Length: "
                              "0\r\nX-Empty: 1\r\nConnection: close\r\n\r\n";
            send(cfd, hdr, strlen(hdr), 0);
          } else if (strstr(rbuf, "/large") != NULL && large_buf) {
            const char *hdr =
                "HTTP/1.1 200 OK\r\nContent-Length: 20000\r\nX-Mode: "
                "large\r\nConnection: close\r\n\r\n";
            send(cfd, hdr, strlen(hdr), 0);
            send(cfd, large_buf, 10000, 0);
            usleep(10000);
            send(cfd, large_buf + 10000, 10000, 0);
          } else {
            const char *hdr =
                "HTTP/1.1 200 OK\r\nContent-Length: 11\r\nX-Bridge: "
                "nsurlsession\r\nConnection: close\r\n\r\nhello apple";
            send(cfd, hdr, strlen(hdr), 0);
          }
          shutdown(cfd, SHUT_WR);
          {
            struct timeval rtv;
            rtv.tv_sec = 0;
            rtv.tv_usec = 5000;
            setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));
            while (recv(cfd, rbuf, sizeof(rbuf), 0) > 0) {
            }
          }
        }
        close(cfd);
      }
    }
  }
  free(large_buf);
  return NULL;
}

TEST test_apple_nsurlsession_bridge(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct AppleMiniSrv srv;
  struct sockaddr_in sin;
  socklen_t slen = sizeof(sin);
  pthread_t srv_tid;
  int port;
  char url_ok[128];
  char url_empty[128];
  char url_large[128];
  char url_slow[128];
  int chunk_calls = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_set_use_nsurlsession(NULL, 1));

  srv.lfd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT(srv.lfd >= 0);
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sin.sin_port = 0;
  ASSERT_EQ(0, bind(srv.lfd, (struct sockaddr *)&sin, sizeof(sin)));
  ASSERT_EQ(0, listen(srv.lfd, 32));
  ASSERT_EQ(0, getsockname(srv.lfd, (struct sockaddr *)&sin, &slen));
  port = (int)ntohs(sin.sin_port);
  ASSERT(port > 0);

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_ok, sizeof(url_ok), "http://127.0.0.1:%d/ok", port);
  sprintf_s(url_empty, sizeof(url_empty), "http://127.0.0.1:%d/empty", port);
  sprintf_s(url_large, sizeof(url_large), "http://127.0.0.1:%d/large", port);
  sprintf_s(url_slow, sizeof(url_slow), "http://127.0.0.1:%d/slow", port);
#else
  c89stringutils_snprintf(url_ok, sizeof(url_ok), "http://127.0.0.1:%d/ok",
                          port);
  c89stringutils_snprintf(url_empty, sizeof(url_empty),
                          "http://127.0.0.1:%d/empty", port);
  c89stringutils_snprintf(url_large, sizeof(url_large),
                          "http://127.0.0.1:%d/large", port);
  c89stringutils_snprintf(url_slow, sizeof(url_slow),
                          "http://127.0.0.1:%d/slow", port);
#endif

  srv.stop = 0;
  ASSERT_EQ(0, pthread_create(&srv_tid, NULL, apple_mini_srv_worker, &srv));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));

#if TEST_APPLE_HAVE_CFSTREAM
  /* Exercise >8192-byte multi-chunk response accumulation on legacy CFStream */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_set_use_nsurlsession(ctx, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url_large;
  req.method = HTTP_GET;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(20000U, (unsigned int)res->body_len);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  /* Exercise active_cf_workers drain wait in http_apple_context_free */
  {
    struct HttpTransportContext *ctx_drain = NULL;
    struct HttpRequest slow_req;
    struct HttpMultiRequest slow_multi;
    struct HttpFuture slow_fut;
    struct HttpFuture *slow_futs[1];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx_drain));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_set_use_nsurlsession(ctx_drain, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&slow_req));
    slow_req.url = url_slow;
    slow_req.method = HTTP_GET;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&slow_multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_multi_request_add(&slow_multi, &slow_req));
    memset(&slow_fut, 0, sizeof(slow_fut));
    slow_futs[0] = &slow_fut;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_send_multi(ctx_drain, NULL, &slow_multi, slow_futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx_drain));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&slow_fut));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&slow_multi));
    slow_req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&slow_req));
  }
#else
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            http_apple_set_use_nsurlsession(ctx, 0));
#endif

  /* Switch to modern NSURLSession pure-C89 bridge */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_set_use_nsurlsession(ctx, 1));

  /* 1. Invalid method & URL validation */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url_ok;
  req.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));

  req.method = HTTP_GET;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));
  req.url = (char *)"";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));
  req.url = (char *)"ht tp:// bad";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));

  /* 2. POST with headers + body -> 200 OK with headers & body */
  req.url = url_ok;
  req.method = HTTP_POST;
  req.body = "payload";
  req.body_len = 7;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Client", "c89"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("hello apple", (const char *)res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 3. Empty 204 response + non-NULL 0-length request body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url_empty;
  req.method = HTTP_POST;
  req.body = "ignored";
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(204, res->status_code);
  ASSERT_EQ(0U, (unsigned int)res->body_len);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 4. Streaming upload (read_chunk success & failure) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url_ok;
  req.method = HTTP_PUT;
  chunk_calls = 0;
  req.read_chunk = mock_read_chunk;
  req.read_chunk_user_data = &chunk_calls;
  req.expected_body_len = 8;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.read_chunk = mock_read_chunk_fail;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 5. Streaming download (on_chunk success & abort) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = url_ok;
  chunk_calls = 0;
  req.on_chunk = mock_on_chunk_cb;
  req.on_chunk_user_data = &chunk_calls;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
  ASSERT(chunk_calls > 0);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.on_chunk = mock_on_chunk_abort_cb;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, http_apple_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 6. Connection error & timeout (-1001) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"http://127.0.0.1:1/refused";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  cfg.timeout_ms = 50;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  req.url = url_slow;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_TIMEOUT, http_apple_send(ctx, &req, &res));
  cfg.timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 7. Multi-request via NSURLSession (loop, NULL loop, failing wakeup) */
  {
    struct HttpRequest mreq1, mreq2;
    struct HttpMultiRequest multi;
    struct HttpFuture f1, f2;
    struct HttpFuture *futs[2];
    struct ModalityEventLoop *loop = NULL;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&mreq1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&mreq2));
    mreq1.url = url_ok;
    mreq2.url = (char *)"ht tp:// bad";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &mreq1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &mreq2));
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    futs[0] = &f1;
    futs[1] = &f2;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_send_multi(ctx, loop, &multi, futs));
    while (!f1.is_ready || !f2.is_ready) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(loop));
    }
    ASSERT_EQ(200, f1.response->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, f2.error_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    /* Sync worker with NULL loop and failing wakeup */
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    multi.count = 1;
    g_mock_pthread_create_sync = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);

    {
      struct ModalityEventLoop *fail_loop = NULL;
      struct HttpLoopHooks hooks;
      memset(&hooks, 0, sizeof(hooks));
      hooks.wakeup = dummy_apple_fail_wakeup;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_loop_init_external(&fail_loop, &hooks));
      memset(&f1, 0, sizeof(f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_apple_send_multi(ctx, fail_loop, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(fail_loop));
    }
    g_mock_pthread_create_sync = 0;
#else
    /* Async multi with NULL loop */
    memset(&f1, 0, sizeof(f1));
    multi.count = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_apple_send_multi(ctx, NULL, &multi, futs));
    while (!f1.is_ready) {
      usleep(5000);
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
#endif
    multi.count = 2;
    mreq1.url = NULL;
    mreq2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&mreq1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&mreq2));
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* 8. OOM sweep across NSURLSession allocations & cleanup */
  {
    int step;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_ok;

    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
    g_mock_headers_init_fail = 0;

    req.url = (char *)"http://127.0.0.1:1/refused";
    g_mock_response_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
    g_mock_response_free_fail = 0;

    req.url = url_ok;
    for (step = 0; step < 25; ++step) {
      c_abstract_http_error_t orc;
      res = NULL;
      g_mock_alloc_count = step;
      g_mock_alloc_fail = 1;
      orc = http_apple_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
    }
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 9. Granular HttpConfig options (user_agent, proxy_url, proxy_auth, H3,
     * TLS versions, read/connect/write timeouts) + OOM sweep */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.proxy_url = (char *)"badproxy";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_config_apply(ctx, &cfg));
    cfg.proxy_url = NULL;
    free(cfg.user_agent);
    cfg.user_agent = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("AppleBot/1.0", &cfg.user_agent));
    {
      char pbuf[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(pbuf, sizeof(pbuf), "http://127.0.0.1:%d", port);
#else
      c89stringutils_snprintf(pbuf, sizeof(pbuf), "http://127.0.0.1:%d", port);
#endif
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup(pbuf, &cfg.proxy_url));
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("puser", &cfg.proxy_username));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("ppass", &cfg.proxy_password));
    cfg.connect_timeout_ms = 500;
    cfg.read_timeout_ms = 1000;
    cfg.write_timeout_ms = 500;
    cfg.verify_host = 1;
    cfg.version_mask = HTTP_VERSION_3;
    cfg.http3_fallback = 0;
    cfg.tls_version_mask = HTTP_TLS_VERSION_1_3;

    for (step = 0; step <= 8; ++step) {
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      if (http_apple_config_apply(ctx, &cfg) != C_ABSTRACT_HTTP_SUCCESS) {
      }
      g_mock_alloc_fail = 0;
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));

    /* Send real request through AppleMiniSrv configured as HTTP proxy! */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_ok;
    req.method = HTTP_POST;
    req.body = (void *)"bodydata";
    req.body_len = 8;
    for (step = 0; step <= 6; ++step) {
      res = NULL;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      if (http_apple_send(ctx, &req, &res) == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
      g_mock_alloc_fail = 0;
    }
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.body = NULL;
    req.body_len = 0;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* Exercise proxy_url without :port or / (default 8080) and empty host */
    free(cfg.proxy_url);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http://127.0.0.1", &cfg.proxy_url));
    cfg.tls_version_mask = HTTP_TLS_VERSION_1_2;
    cfg.read_timeout_ms = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)"http://127.0.0.1:1/x";
    res = NULL;
    if (http_apple_send(ctx, &req, &res) == C_ABSTRACT_HTTP_SUCCESS && res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
    }
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    free(cfg.proxy_url);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http:///bad", &cfg.proxy_url));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_ok;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));
    {
      struct HttpMultiRequest bmulti;
      struct HttpFuture bf1;
      struct HttpFuture *bfuts[1];
      struct ModalityEventLoop *bloop = NULL;
      bfuts[0] = &bf1;
      memset(&bf1, 0, sizeof(bf1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&bloop));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&bmulti));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&bmulti, &req));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                http_apple_send_multi(ctx, bloop, &bmulti, bfuts));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&bmulti));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(bloop));
    }
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    free(cfg.proxy_url);
    cfg.proxy_url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_ok;
    for (step = 0; step <= 10; ++step) {
      c_abstract_http_error_t orc;
      res = NULL;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_apple_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
    }
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

    /* 10. Multipart req->parts in http_apple_send & http_apple_send_multi */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = url_ok;
    req.method = HTTP_POST;
    req.parts.count = 1;
    req.parts.parts = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_send(ctx, &req, &res));
    req.parts.count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_request_add_part(&req, "field1", NULL, NULL, "val1", 4));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "X-Part", "1"));
    for (step = 0; step <= 12; ++step) {
      c_abstract_http_error_t orc;
      res = NULL;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_apple_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
    }
    res = NULL;
    g_mock_headers_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
    req.url = (char *)"ht tp:// bad";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_send(ctx, &req, &res));
    g_mock_alloc_fail = 0;
    req.url = url_ok;
    g_mock_headers_free_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);

    /* 11. ModalityEventLoop http_loop_run kqueue pipe-bridged send_multi +
     * 0-count + all-invalid + OOM */
    {
      struct HttpMultiRequest multi2;
      struct HttpFuture mf1, mf2;
      struct HttpFuture *mfuts[2];
      struct ModalityEventLoop *mloop = NULL;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&mloop));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi2));
      mfuts[0] = &mf1;
      mfuts[1] = &mf2;

      /* Empty multi batch */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_apple_send_multi(ctx, mloop, &multi2, mfuts));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_apple_send_multi(ctx, NULL, &multi2, mfuts));

      /* NULL slot -> launched == 0 */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi2, &req));
      multi2.requests[0] = NULL;
      memset(&mf1, 0, sizeof(mf1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_apple_send_multi(ctx, mloop, &multi2, mfuts));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mf1.error_code);

      /* Real concurrent multipart + GET driven by http_loop_run(mloop) */
      multi2.count = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi2, &req));
      memset(&mf1, 0, sizeof(mf1));
      g_mock_pthread_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_apple_send_multi(ctx, mloop, &multi2, mfuts));
      g_mock_pthread_fail = 0;
      for (step = 0; step <= 3; ++step) {
        g_mock_alloc_fail = 1;
        g_mock_alloc_count = step;
        if (http_apple_send_multi(ctx, mloop, &multi2, mfuts) ==
            C_ABSTRACT_HTTP_SUCCESS) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(mloop));
          if (mf1.response) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                      http_response_free(mf1.response));
            free(mf1.response);
            mf1.response = NULL;
          }
        }
        g_mock_alloc_fail = 0;
      }

      /* Multipart headers_free_fail on completed slot + bad-URL slot */
      memset(&mf1, 0, sizeof(mf1));
      g_mock_headers_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_apple_send_multi(ctx, mloop, &multi2, mfuts));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(mloop));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mf1.error_code);
      req.url = (char *)"ht tp:// bad";
      memset(&mf1, 0, sizeof(mf1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_apple_send_multi(ctx, mloop, &multi2, mfuts));
      req.url = url_ok;
      g_mock_headers_free_fail = 0;

      /* 1-item failing batch + 2-item succeeding batch + NULL-loop async */
      {
        struct HttpRequest fail_req;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&fail_req));
        fail_req.url = (char *)"http://127.0.0.1:1/refused";

        multi2.count = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_multi_request_add(&multi2, &fail_req));
        memset(&mf1, 0, sizeof(mf1));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_apple_send_multi(ctx, mloop, &multi2, mfuts));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(mloop));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mf1.error_code);

        multi2.count = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_multi_request_add(&multi2, &req));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_multi_request_add(&multi2, &req));
        memset(&mf1, 0, sizeof(mf1));
        memset(&mf2, 0, sizeof(mf2));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_apple_send_multi(ctx, mloop, &multi2, mfuts));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(mloop));
        ASSERT_EQ(200, mf1.response->status_code);
        ASSERT_EQ(200, mf2.response->status_code);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf1.response));
        free(mf1.response);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf2.response));
        free(mf2.response);

        /* Async NULL loop + async failing wakeup loop */
        multi2.count = 1;
        memset(&mf1, 0, sizeof(mf1));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_apple_send_multi(ctx, NULL, &multi2, mfuts));
        while (!mf1.is_ready) {
          usleep(2000);
        }
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf1.response));
        free(mf1.response);

        {
          struct ModalityEventLoop *fl = NULL;
          struct HttpLoopHooks fhooks;
          memset(&fhooks, 0, sizeof(fhooks));
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_loop_init_external(&fl, &fhooks));
          memset(&mf1, 0, sizeof(mf1));
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_apple_send_multi(ctx, fl, &multi2, mfuts));
          while (!mf1.is_ready) {
            usleep(2000);
          }
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf1.response));
          free(mf1.response);
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(fl));

          memset(&fhooks, 0, sizeof(fhooks));
          fhooks.wakeup = dummy_apple_fail_wakeup;
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_loop_init_external(&fl, &fhooks));
          multi2.requests[0] = NULL;
          memset(&mf1, 0, sizeof(mf1));
          ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                    http_apple_send_multi(ctx, fl, &multi2, mfuts));
          multi2.requests[0] = &req;
          memset(&mf1, 0, sizeof(mf1));
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_apple_send_multi(ctx, fl, &multi2, mfuts));
          while (!mf1.is_ready) {
            usleep(2000);
          }
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf1.response));
          free(mf1.response);
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(fl));
        }

        /* External loop add_fd failure + remove_fd failure on empty & active */
        {
          struct ModalityEventLoop *el = NULL;
          struct HttpLoopHooks ehooks;
          http_loop_cb pipe_cb = NULL;
          int pipe_fd = -1;
          void *pipe_ud = NULL;
          void *pack[3];
          pack[0] = &pipe_cb;
          pack[1] = &pipe_fd;
          pack[2] = &pipe_ud;
          memset(&ehooks, 0, sizeof(ehooks));
          ehooks.add_fd = dummy_apple_add_fd_fail;
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_loop_init_external(&el, &ehooks));
          memset(&mf1, 0, sizeof(mf1));
          ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                    http_apple_send_multi(ctx, el, &multi2, mfuts));
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(el));

          memset(&ehooks, 0, sizeof(ehooks));
          ehooks.external_context = pack;
          ehooks.add_fd = dummy_apple_add_fd_capture;
          ehooks.remove_fd = dummy_apple_rm_fd_fail;
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_loop_init_external(&el, &ehooks));
          /* Empty launched == 0 remove_fd fail */
          multi2.requests[0] = NULL;
          memset(&mf1, 0, sizeof(mf1));
          ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                    http_apple_send_multi(ctx, el, &multi2, mfuts));
          /* Active batch pipe_cb remove_fd fail */
          multi2.requests[0] = &req;
          pipe_cb = NULL;
          pipe_fd = -1;
          memset(&mf1, 0, sizeof(mf1));
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    http_apple_send_multi(ctx, el, &multi2, mfuts));
          ASSERT(pipe_cb != NULL);
          {
            fd_set rset;
            struct timeval tv;
            FD_ZERO(&rset);
            FD_SET((size_t)pipe_fd, &rset);
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            select(pipe_fd + 1, &rset, NULL, NULL, &tv);
            ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                      pipe_cb(el, pipe_fd, HTTP_LOOP_READ, pipe_ud));
          }
          ASSERT_EQ(1, mf1.is_ready);
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf1.response));
          free(mf1.response);
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(el));
        }

        multi2.count = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_multi_request_add(&multi2, &fail_req));
        memset(&mf1, 0, sizeof(mf1));
        memset(&mf2, 0, sizeof(mf2));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_apple_send_multi(ctx, mloop, &multi2, mfuts));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(mloop));
        ASSERT_EQ(1, mf1.is_ready);
        ASSERT_EQ(1, mf2.is_ready);
        ASSERT_EQ(200, mf1.response->status_code);
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mf2.error_code);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(mf1.response));
        free(mf1.response);
        fail_req.url = NULL;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&fail_req));
      }

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(mloop));
    }

    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }
#endif

  srv.stop = 1;
  pthread_join(srv_tid, NULL);
  close(srv.lfd);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_free(ctx));
  PASS();
}
#endif /* defined(__APPLE__) */

#if defined(__APPLE__) && defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Verify Apple NSURLSession custom CA, mTLS, and SPKI SHA-256 pinning
 */
TEST test_apple_tls_ca_mtls_and_spki_pinning(void) {
  static const unsigned char k_apple_ec_cert1_der[] = {
      0x30, 0x82, 0x01, 0x5C, 0x30, 0x82, 0x01, 0x02, 0xA0, 0x03, 0x02, 0x01,
      0x02, 0x02, 0x14, 0x1E, 0xDD, 0x38, 0xBE, 0x9C, 0x21, 0x53, 0xB9, 0x7F,
      0xAE, 0x36, 0xA0, 0xEF, 0x8E, 0x23, 0x72, 0x0C, 0x5B, 0xDB, 0x23, 0x30,
      0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30,
      0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x09,
      0x31, 0x32, 0x37, 0x2E, 0x30, 0x2E, 0x30, 0x2E, 0x31, 0x30, 0x1E, 0x17,
      0x0D, 0x32, 0x36, 0x31, 0x31, 0x32, 0x36, 0x32, 0x32, 0x32, 0x34, 0x32,
      0x31, 0x5A, 0x17, 0x0D, 0x33, 0x36, 0x31, 0x31, 0x32, 0x33, 0x32, 0x32,
      0x32, 0x34, 0x32, 0x31, 0x5A, 0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06,
      0x03, 0x55, 0x04, 0x03, 0x0C, 0x09, 0x31, 0x32, 0x37, 0x2E, 0x30, 0x2E,
      0x30, 0x2E, 0x31, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48,
      0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03,
      0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x0F, 0x05, 0x68, 0x00, 0x97, 0x74,
      0x03, 0x0D, 0x4C, 0x23, 0x98, 0xDD, 0x33, 0x6D, 0xAA, 0xBA, 0xA6, 0xDF,
      0xFB, 0xC0, 0x96, 0x99, 0x50, 0x2A, 0x17, 0xA7, 0x97, 0x3D, 0x7E, 0xC1,
      0x0C, 0x31, 0x34, 0xCF, 0x18, 0x73, 0xA9, 0x98, 0xDB, 0x65, 0x7C, 0xB8,
      0x36, 0x22, 0x17, 0x5E, 0xCD, 0xA4, 0xA4, 0x8C, 0x41, 0xB1, 0x39, 0x68,
      0x71, 0x5B, 0xF4, 0x47, 0xF8, 0x10, 0x35, 0xF8, 0x10, 0x49, 0xA3, 0x32,
      0x30, 0x30, 0x30, 0x1D, 0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04,
      0x14, 0xFD, 0xBC, 0x73, 0x0F, 0x88, 0xB5, 0x9D, 0x62, 0x57, 0x90, 0xAE,
      0xEA, 0x78, 0x71, 0xE6, 0xD0, 0xF4, 0x0D, 0x8D, 0xE0, 0x30, 0x0F, 0x06,
      0x03, 0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x05, 0x30, 0x03, 0x01,
      0x01, 0xFF, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04,
      0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x20, 0x64, 0xB0, 0x32,
      0x4F, 0x49, 0x7C, 0x82, 0x48, 0x66, 0xE2, 0xFB, 0x7B, 0xB4, 0x13, 0x60,
      0xC1, 0xA0, 0xAB, 0x13, 0xDC, 0xA8, 0xBF, 0x3E, 0xEE, 0xB6, 0xA9, 0xB2,
      0x07, 0x22, 0x7F, 0x91, 0x2E, 0x02, 0x21, 0x00, 0xAA, 0xAF, 0x36, 0xA9,
      0x10, 0x12, 0xD0, 0x68, 0x4D, 0xF2, 0x98, 0x91, 0xBB, 0xB8, 0x80, 0x9C,
      0xB3, 0x5A, 0xD5, 0x4D, 0xB5, 0x1C, 0x64, 0xCA, 0x8C, 0xDC, 0x08, 0xAA,
      0xFC, 0x8F, 0x58, 0xE6};
  static const unsigned char k_apple_ec_key1_der[] = {
      0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x5C, 0x53, 0x23, 0x37,
      0xAF, 0xA3, 0x9A, 0x9D, 0x38, 0xBF, 0xFA, 0x7D, 0x98, 0x46, 0xF9,
      0xD4, 0x9D, 0xD1, 0xDF, 0x36, 0xFC, 0xB9, 0x27, 0x13, 0x08, 0xE8,
      0xB7, 0xCC, 0x79, 0xAD, 0x64, 0x8E, 0xA0, 0x0A, 0x06, 0x08, 0x2A,
      0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0xA1, 0x44, 0x03, 0x42,
      0x00, 0x04, 0x0F, 0x05, 0x68, 0x00, 0x97, 0x74, 0x03, 0x0D, 0x4C,
      0x23, 0x98, 0xDD, 0x33, 0x6D, 0xAA, 0xBA, 0xA6, 0xDF, 0xFB, 0xC0,
      0x96, 0x99, 0x50, 0x2A, 0x17, 0xA7, 0x97, 0x3D, 0x7E, 0xC1, 0x0C,
      0x31, 0x34, 0xCF, 0x18, 0x73, 0xA9, 0x98, 0xDB, 0x65, 0x7C, 0xB8,
      0x36, 0x22, 0x17, 0x5E, 0xCD, 0xA4, 0xA4, 0x8C, 0x41, 0xB1, 0x39,
      0x68, 0x71, 0x5B, 0xF4, 0x47, 0xF8, 0x10, 0x35, 0xF8, 0x10, 0x49};
  static const unsigned char k_apple_ec_cert2_der[] = {
      0x30, 0x82, 0x01, 0x5C, 0x30, 0x82, 0x01, 0x02, 0xA0, 0x03, 0x02, 0x01,
      0x02, 0x02, 0x14, 0x6A, 0x6B, 0xA5, 0x0A, 0xD0, 0x86, 0x3D, 0x91, 0xA3,
      0x9A, 0x28, 0x4F, 0xC8, 0xB5, 0x86, 0x09, 0x49, 0xFC, 0x8B, 0xBF, 0x30,
      0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30,
      0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x09,
      0x31, 0x32, 0x37, 0x2E, 0x30, 0x2E, 0x30, 0x2E, 0x31, 0x30, 0x1E, 0x17,
      0x0D, 0x32, 0x36, 0x31, 0x31, 0x32, 0x36, 0x32, 0x32, 0x32, 0x34, 0x32,
      0x31, 0x5A, 0x17, 0x0D, 0x33, 0x36, 0x31, 0x31, 0x32, 0x33, 0x32, 0x32,
      0x32, 0x34, 0x32, 0x31, 0x5A, 0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06,
      0x03, 0x55, 0x04, 0x03, 0x0C, 0x09, 0x31, 0x32, 0x37, 0x2E, 0x30, 0x2E,
      0x30, 0x2E, 0x31, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48,
      0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03,
      0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xA3, 0xA2, 0xDE, 0x86, 0xD9, 0x63,
      0xF8, 0x18, 0xD5, 0x76, 0xE8, 0x3E, 0xFB, 0xC8, 0x63, 0xFB, 0x93, 0xF4,
      0xF9, 0xC9, 0x4E, 0xFB, 0x91, 0x5C, 0xA4, 0x96, 0x27, 0xDB, 0x6D, 0x14,
      0xC8, 0x7F, 0x02, 0x4D, 0xA1, 0xB3, 0x61, 0x5A, 0x73, 0xA2, 0xCA, 0x06,
      0x1D, 0xBE, 0x9E, 0xCB, 0xA8, 0xFD, 0x59, 0xBE, 0xA8, 0x8E, 0xDA, 0xC4,
      0xE1, 0x8F, 0x02, 0xE9, 0x51, 0xA3, 0x57, 0xDC, 0xBB, 0x00, 0xA3, 0x32,
      0x30, 0x30, 0x30, 0x1D, 0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04,
      0x14, 0x1C, 0x49, 0xC0, 0x31, 0x86, 0xE4, 0xBA, 0x57, 0xEA, 0xB1, 0x33,
      0x9B, 0xD6, 0xD1, 0xC5, 0xCC, 0x80, 0xF2, 0x97, 0x36, 0x30, 0x0F, 0x06,
      0x03, 0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x05, 0x30, 0x03, 0x01,
      0x01, 0xFF, 0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04,
      0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0xA6, 0xC1,
      0x4B, 0x5A, 0xC4, 0x19, 0x92, 0x07, 0xAB, 0x85, 0x44, 0x22, 0xBC, 0xBC,
      0xA5, 0x5D, 0x7A, 0x4B, 0x25, 0x6B, 0xDC, 0xA5, 0x64, 0x1C, 0x2B, 0x94,
      0x4A, 0x4A, 0xBF, 0x8C, 0x87, 0x5A, 0x02, 0x20, 0x7A, 0xA0, 0x69, 0xEE,
      0x4F, 0x7B, 0x23, 0xFF, 0x40, 0x68, 0x30, 0x2D, 0x21, 0x1E, 0x7B, 0xCA,
      0xCC, 0x5C, 0x8D, 0xA4, 0x18, 0x95, 0x6B, 0x16, 0x5C, 0xFE, 0xA6, 0x0F,
      0xC5, 0x7E, 0x8A, 0x9D};
  static const unsigned char k_apple_ec_key2_der[] = {
      0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0xDF, 0xED, 0x71, 0xC6,
      0x6C, 0xFF, 0xF9, 0x63, 0xEE, 0xDE, 0x6A, 0xE8, 0x18, 0xF8, 0x93,
      0xFD, 0xE2, 0xEF, 0xBF, 0xE6, 0xA8, 0x81, 0xC6, 0x80, 0x04, 0xA5,
      0xBF, 0xE7, 0xA1, 0xC9, 0xE7, 0x16, 0xA0, 0x0A, 0x06, 0x08, 0x2A,
      0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0xA1, 0x44, 0x03, 0x42,
      0x00, 0x04, 0xA3, 0xA2, 0xDE, 0x86, 0xD9, 0x63, 0xF8, 0x18, 0xD5,
      0x76, 0xE8, 0x3E, 0xFB, 0xC8, 0x63, 0xFB, 0x93, 0xF4, 0xF9, 0xC9,
      0x4E, 0xFB, 0x91, 0x5C, 0xA4, 0x96, 0x27, 0xDB, 0x6D, 0x14, 0xC8,
      0x7F, 0x02, 0x4D, 0xA1, 0xB3, 0x61, 0x5A, 0x73, 0xA2, 0xCA, 0x06,
      0x1D, 0xBE, 0x9E, 0xCB, 0xA8, 0xFD, 0x59, 0xBE, 0xA8, 0x8E, 0xDA,
      0xC4, 0xE1, 0x8F, 0x02, 0xE9, 0x51, 0xA3, 0x57, 0xDC, 0xBB, 0x00};
  static const unsigned char k_apple_ec_id1_p12[] = {
      0x30, 0x82, 0x03, 0x6A, 0x02, 0x01, 0x03, 0x30, 0x82, 0x03, 0x28, 0x06,
      0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x01, 0xA0, 0x82,
      0x03, 0x19, 0x04, 0x82, 0x03, 0x15, 0x30, 0x82, 0x03, 0x11, 0x30, 0x82,
      0x02, 0x07, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07,
      0x06, 0xA0, 0x82, 0x01, 0xF8, 0x30, 0x82, 0x01, 0xF4, 0x02, 0x01, 0x00,
      0x30, 0x82, 0x01, 0xED, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D,
      0x01, 0x07, 0x01, 0x30, 0x1C, 0x06, 0x0A, 0x2A, 0x86, 0x48, 0x86, 0xF7,
      0x0D, 0x01, 0x0C, 0x01, 0x03, 0x30, 0x0E, 0x04, 0x08, 0xC5, 0x85, 0x65,
      0xEF, 0x44, 0xF7, 0x8F, 0x7F, 0x02, 0x02, 0x08, 0x00, 0x80, 0x82, 0x01,
      0xC0, 0xAC, 0xDD, 0xBF, 0x6A, 0xB8, 0x67, 0x0B, 0x67, 0xF9, 0x91, 0x2F,
      0xFB, 0x7B, 0x3D, 0x80, 0x44, 0xAC, 0xFF, 0x70, 0xC4, 0x5A, 0xCF, 0xBD,
      0xBC, 0x73, 0x9E, 0x37, 0x72, 0x37, 0x61, 0xF0, 0x5D, 0x5E, 0x1E, 0x15,
      0xE2, 0x7C, 0xA8, 0x4A, 0x96, 0x4E, 0xC1, 0x7B, 0xCF, 0x83, 0x35, 0x99,
      0x61, 0x90, 0x2F, 0x70, 0xFA, 0xCF, 0x14, 0xEB, 0x92, 0x84, 0x73, 0x2F,
      0x47, 0xBD, 0xFF, 0x25, 0x3B, 0x6C, 0x41, 0x4C, 0xDE, 0x9F, 0x08, 0x12,
      0xB0, 0x78, 0x87, 0x90, 0x20, 0x45, 0xAD, 0xE8, 0xBE, 0x9E, 0x79, 0xB4,
      0x62, 0xDE, 0xC5, 0xAA, 0xA4, 0x7A, 0x7D, 0x23, 0x0B, 0xA4, 0xF7, 0x22,
      0x33, 0xF7, 0x1B, 0x68, 0xD7, 0x6F, 0xC9, 0x09, 0xB8, 0x15, 0x32, 0x17,
      0xF7, 0x52, 0x35, 0xA1, 0xBE, 0xF3, 0xBE, 0xB8, 0x7C, 0x46, 0xA6, 0xC3,
      0x43, 0x66, 0x70, 0xCE, 0xEB, 0x07, 0x9E, 0x6C, 0x2C, 0xB7, 0xBD, 0x29,
      0xF0, 0x75, 0xB6, 0x4B, 0x00, 0x5E, 0x2A, 0x49, 0x89, 0xD1, 0xBB, 0xCE,
      0xBB, 0xD6, 0xA7, 0x7F, 0x9F, 0xD6, 0x9A, 0x57, 0xB9, 0xBE, 0x84, 0x7A,
      0x17, 0xB9, 0xB0, 0x41, 0xA4, 0xA4, 0xA2, 0xC7, 0x8E, 0xC5, 0x5A, 0x8B,
      0x73, 0x46, 0xED, 0x09, 0x08, 0x4B, 0x05, 0xDE, 0xD6, 0xCB, 0xB2, 0xFB,
      0x2A, 0x42, 0xC6, 0x3E, 0x94, 0xAF, 0xC5, 0x40, 0x76, 0x96, 0xC7, 0xBC,
      0x05, 0xC6, 0xAC, 0xDA, 0x78, 0xA3, 0x43, 0xFF, 0xCB, 0xBB, 0xEE, 0xBC,
      0x8B, 0x68, 0x7B, 0x3B, 0x59, 0xB5, 0x0F, 0x95, 0x4A, 0xB5, 0x01, 0x74,
      0x20, 0x59, 0xAF, 0xB9, 0x6D, 0xF2, 0xCA, 0x79, 0xFB, 0xE0, 0x40, 0xA8,
      0x4F, 0x65, 0x5D, 0x0D, 0x62, 0x15, 0xAE, 0xAE, 0xDF, 0x02, 0x63, 0x41,
      0x81, 0x0B, 0xB7, 0x57, 0x47, 0x89, 0x91, 0xA7, 0x16, 0x2C, 0x75, 0x45,
      0x6E, 0x72, 0xF7, 0x10, 0xF4, 0x56, 0xAF, 0x89, 0xE3, 0x1B, 0x36, 0x8C,
      0xF6, 0xBF, 0x20, 0xDD, 0x7E, 0xDD, 0x33, 0x5C, 0x6C, 0x77, 0xDD, 0x7C,
      0x0B, 0xF9, 0x9B, 0x0E, 0xA9, 0x62, 0x49, 0xFB, 0xAE, 0x79, 0xD0, 0x92,
      0x3E, 0x90, 0x2C, 0x59, 0xCB, 0xD9, 0xA6, 0x8E, 0x78, 0x8A, 0x7E, 0x06,
      0x97, 0x8F, 0xCE, 0x58, 0x61, 0x4D, 0x17, 0xAF, 0xEB, 0xD2, 0xDE, 0x53,
      0x5B, 0x21, 0x26, 0xEB, 0xD2, 0xD0, 0x21, 0x91, 0x3E, 0xF9, 0x81, 0x97,
      0x5D, 0x40, 0x7B, 0x6A, 0x68, 0xE5, 0x6E, 0x0E, 0xCE, 0x4E, 0x09, 0xD6,
      0x8F, 0xDE, 0x87, 0xB0, 0x57, 0x7A, 0x3F, 0xFC, 0x4C, 0xE8, 0xA1, 0x71,
      0xA6, 0x2E, 0x5D, 0x75, 0x42, 0xE3, 0x93, 0x32, 0x7C, 0xE2, 0xE9, 0xA0,
      0x97, 0xC1, 0x7E, 0x69, 0x28, 0x59, 0x85, 0x3F, 0x27, 0x1C, 0x61, 0xB0,
      0x88, 0xBB, 0x12, 0xF7, 0x70, 0x60, 0xD2, 0x60, 0x26, 0x7C, 0xF2, 0x71,
      0xA8, 0x86, 0xB8, 0x52, 0x14, 0xED, 0xF8, 0x4B, 0xD0, 0xB5, 0x71, 0xEB,
      0x19, 0x19, 0xE5, 0xAD, 0x0F, 0x5A, 0x51, 0x94, 0x22, 0x60, 0xCE, 0x9C,
      0x24, 0x80, 0x36, 0xC6, 0xBA, 0x8C, 0x04, 0x33, 0x37, 0xB5, 0x85, 0xEE,
      0x38, 0xBA, 0x23, 0xFE, 0x11, 0x67, 0x95, 0xAA, 0xA1, 0xEC, 0xB5, 0xEB,
      0x32, 0x89, 0x84, 0xFD, 0x18, 0x88, 0x3F, 0xEA, 0xA0, 0x48, 0x2D, 0x3F,
      0xC7, 0x0B, 0x02, 0xCD, 0xFE, 0x30, 0x82, 0x01, 0x02, 0x06, 0x09, 0x2A,
      0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x01, 0xA0, 0x81, 0xF4, 0x04,
      0x81, 0xF1, 0x30, 0x81, 0xEE, 0x30, 0x81, 0xEB, 0x06, 0x0B, 0x2A, 0x86,
      0x48, 0x86, 0xF7, 0x0D, 0x01, 0x0C, 0x0A, 0x01, 0x02, 0xA0, 0x81, 0xB4,
      0x30, 0x81, 0xB1, 0x30, 0x1C, 0x06, 0x0A, 0x2A, 0x86, 0x48, 0x86, 0xF7,
      0x0D, 0x01, 0x0C, 0x01, 0x03, 0x30, 0x0E, 0x04, 0x08, 0xD3, 0x81, 0x46,
      0x31, 0xD9, 0x42, 0xF5, 0x1E, 0x02, 0x02, 0x08, 0x00, 0x04, 0x81, 0x90,
      0x9C, 0xAA, 0xDD, 0x42, 0x71, 0x48, 0xB8, 0xC0, 0xF5, 0xAE, 0xA1, 0xC3,
      0x31, 0xD7, 0xE4, 0xDF, 0xD5, 0x26, 0xB9, 0x70, 0x17, 0x62, 0xC6, 0xF9,
      0x31, 0xF9, 0x34, 0x4F, 0x6D, 0x3A, 0x04, 0xD2, 0x34, 0xCA, 0xAD, 0x4D,
      0x9B, 0xAE, 0x93, 0xA4, 0x95, 0xF5, 0xED, 0x17, 0xAA, 0xFE, 0x36, 0xD3,
      0xD5, 0xA1, 0xC4, 0xD1, 0x2C, 0x07, 0x42, 0xB5, 0x14, 0x18, 0xC8, 0xEB,
      0xA6, 0x60, 0x25, 0x06, 0x50, 0xBC, 0x90, 0x2C, 0xCC, 0x33, 0x4C, 0xBF,
      0xB0, 0xD4, 0x24, 0xD7, 0x58, 0x76, 0x57, 0x99, 0xDB, 0x5C, 0x62, 0x14,
      0x59, 0x54, 0xEB, 0x75, 0x8E, 0xD1, 0x80, 0x8A, 0x79, 0x45, 0x30, 0x6D,
      0x51, 0x08, 0xA2, 0xF0, 0x05, 0x1C, 0x2E, 0x51, 0x8D, 0xFF, 0xA2, 0xF6,
      0x0C, 0x04, 0x46, 0x4A, 0x37, 0x91, 0xC9, 0xC7, 0x09, 0xE6, 0xB6, 0xBF,
      0x04, 0x53, 0xDE, 0xB9, 0xB4, 0x9F, 0xFD, 0x56, 0x7B, 0x7A, 0xD8, 0x40,
      0x14, 0xB0, 0x80, 0x81, 0xF2, 0x8F, 0x5B, 0xC1, 0x66, 0xF1, 0xD8, 0x0B,
      0x31, 0x25, 0x30, 0x23, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D,
      0x01, 0x09, 0x15, 0x31, 0x16, 0x04, 0x14, 0x87, 0x95, 0x79, 0xFD, 0xE6,
      0xA6, 0xD3, 0x31, 0x87, 0x76, 0x37, 0xAF, 0x3C, 0x6E, 0x34, 0xDC, 0xA0,
      0x63, 0x73, 0xCA, 0x30, 0x39, 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B,
      0x0E, 0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14, 0xA0, 0xC6, 0xA4, 0x21,
      0x31, 0xD3, 0xD0, 0x03, 0x50, 0x95, 0x3D, 0x37, 0x87, 0x45, 0xDA, 0x1E,
      0xBC, 0x51, 0xE5, 0xE3, 0x04, 0x10, 0xED, 0xEA, 0x22, 0xE4, 0x2F, 0xDB,
      0x39, 0xBE, 0x9F, 0xDC, 0x14, 0x2F, 0x9D, 0x38, 0x5C, 0x91, 0x02, 0x02,
      0x08, 0x00};
  static const unsigned char k_bad_bytes[] = {'B', 'A', 'D'};
  static const unsigned char k_seq_not_cert[] = {0x30, 0x02, 0x05, 0x00};
  static const unsigned char k_trunc_seq[] = {0x30, 0x05, 0x00};
  static const unsigned char k_bad_utf8_pw[] = {0xFF, 0xFE};
  const char *ca_file = "cah_apple_ca_tmp.der";
  const char *cert1_file = "cah_apple_cert1_tmp.der";
  const char *key1_file = "cah_apple_key1_tmp.der";
  const char *key2_file = "cah_apple_key2_tmp.der";
  const char *p12_file = "cah_apple_id1_tmp.p12";
  const char *pw_file = "cah_apple_pw_tmp.txt";
  const char *bad_utf8_file = "cah_apple_badutf8_tmp.bin";
  const char *bad_file = "cah_apple_bad_tmp.bin";
  const char *seq_file = "cah_apple_seq_tmp.der";
  const char *trunc_file = "cah_apple_trunc_tmp.der";
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  long disp = -1;
  int has_cred = 0;
  FILE *fp = NULL;

  fp = fopen(ca_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_apple_ec_cert2_der),
            fwrite(k_apple_ec_cert2_der, 1, sizeof(k_apple_ec_cert2_der), fp));
  fclose(fp);

  fp = fopen(cert1_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_apple_ec_cert1_der),
            fwrite(k_apple_ec_cert1_der, 1, sizeof(k_apple_ec_cert1_der), fp));
  fclose(fp);

  fp = fopen(key1_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_apple_ec_key1_der),
            fwrite(k_apple_ec_key1_der, 1, sizeof(k_apple_ec_key1_der), fp));
  fclose(fp);

  fp = fopen(key2_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_apple_ec_key2_der),
            fwrite(k_apple_ec_key2_der, 1, sizeof(k_apple_ec_key2_der), fp));
  fclose(fp);

  fp = fopen(p12_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_apple_ec_id1_p12),
            fwrite(k_apple_ec_id1_p12, 1, sizeof(k_apple_ec_id1_p12), fp));
  fclose(fp);

  fp = fopen(pw_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(5U, fwrite("cah\r\n", 1, 5, fp));
  fclose(fp);

  fp = fopen(bad_utf8_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_bad_utf8_pw),
            fwrite(k_bad_utf8_pw, 1, sizeof(k_bad_utf8_pw), fp));
  fclose(fp);

  fp = fopen(bad_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_bad_bytes),
            fwrite(k_bad_bytes, 1, sizeof(k_bad_bytes), fp));
  fclose(fp);

  fp = fopen(seq_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_seq_not_cert),
            fwrite(k_seq_not_cert, 1, sizeof(k_seq_not_cert), fp));
  fclose(fp);

  fp = fopen(trunc_file, "wb");
  ASSERT(fp != NULL);
  ASSERT_EQ(sizeof(k_trunc_seq),
            fwrite(k_trunc_seq, 1, sizeof(k_trunc_seq), fp));
  fclose(fp);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  /* 1. Validation & bad CA/mTLS file errors */
  cfg.client_cert_path = (char *)cert1_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = NULL;
  cfg.client_key_path = (char *)key1_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_config_apply(ctx, &cfg));
  cfg.client_key_path = NULL;
  cfg.ca_buffer_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_apple_config_apply(ctx, &cfg));
  cfg.ca_buffer_len = 0;

  cfg.ca_path = (char *)"nonexistent_apple_ca.der";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.ca_path = (char *)bad_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.ca_path = (char *)seq_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.ca_path = NULL;
  cfg.ca_buffer = (unsigned char *)k_seq_not_cert;
  cfg.ca_buffer_len = sizeof(k_seq_not_cert);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.ca_buffer = NULL;
  cfg.ca_buffer_len = 0;

  cfg.client_cert_path = (char *)"nonexistent_apple_cli.der";
  cfg.client_key_path = (char *)key1_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)cert1_file;
  cfg.client_key_path = (char *)"nonexistent_apple_key.der";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)bad_file;
  cfg.client_key_path = (char *)bad_utf8_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)cert1_file;
  cfg.client_key_path = (char *)bad_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)cert1_file;
  cfg.client_key_path = (char *)trunc_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)seq_file;
  cfg.client_key_path = (char *)key1_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)cert1_file;
  cfg.client_key_path = (char *)seq_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)cert1_file;
  cfg.client_key_path = (char *)key2_file; /* mismatched cert + key */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));
  cfg.client_cert_path = (char *)p12_file;
  cfg.client_key_path = (char *)bad_file; /* wrong passphrase -> fallback "" */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_apple_config_apply(ctx, &cfg));

  /* 2. Challenge handling BEFORE identity / CA configured */
  cfg.client_cert_path = NULL;
  cfg.client_key_path = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));

  /* HTTPBasic -> PerformDefaultHandling (1) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         ctx, 12, NULL, 0, &disp, &has_cred));
  ASSERT_EQ(1L, disp);
  ASSERT_EQ(0, has_cred);

  /* ClientCertificate without identity -> PerformDefaultHandling (1) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         ctx, 11, NULL, 0, &disp, &has_cred));
  ASSERT_EQ(1L, disp);
  ASSERT_EQ(0, has_cred);

  /* ServerTrust with NULL trust (ctx != NULL and ctx == NULL) -> Cancel (2) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         ctx, 10, NULL, 0, &disp, &has_cred));
  ASSERT_EQ(2L, disp);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         NULL, 10, NULL, 0, &disp, &has_cred));
  ASSERT_EQ(2L, disp);

  /* Self-signed cert1 without custom CA (ctx != NULL and ctx == NULL) -> Cancel
   * (2) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                ctx, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(2L, disp);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                NULL, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(2L, disp);

  /* verify_peer = 0 accepts self-signed cert1 -> UseCredential (0) */
  cfg.verify_peer = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                ctx, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(0L, disp);
  ASSERT_EQ(1, has_cred);

  /* 3. Custom CA mismatch (ca_file = cert2 != cert1) -> Cancel (2) */
  cfg.verify_peer = 1;
  cfg.ca_path = (char *)ca_file;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                ctx, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(2L, disp);

  /* 4. Multi-anchor CA (ca_path=cert2 + ca_buffer=cert1) + PKCS#12 mTLS +
   * RFC 7469 SPKI SHA-256 pin -> UseCredential (0) for both ServerTrust &
   * ClientCertificate! */
  cfg.ca_buffer = (unsigned char *)k_apple_ec_cert1_der;
  cfg.ca_buffer_len = sizeof(k_apple_ec_cert1_der);
  cfg.client_cert_path = (char *)p12_file;
  cfg.client_key_path = (char *)pw_file;
  cfg.cert_pin_sha256 =
      (char *)"sha256//kVOGvKOTUhYE2sCui1dbkt2rxEWjef3OK60vcwqdUV0=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                ctx, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(0L, disp);
  ASSERT_EQ(1, has_cred);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         ctx, 11, NULL, 0, &disp, &has_cred));
  ASSERT_EQ(0L, disp);
  ASSERT_EQ(1, has_cred);

  /* Re-apply with DER EC key + cert mTLS & SecKey external-rep SHA-256 pin! */
  cfg.client_cert_path = (char *)cert1_file;
  cfg.client_key_path = (char *)key1_file;
  cfg.cert_pin_sha256 =
      (char *)"sha256//KXjgUz7hjhsl4fSkZk2RQdc3c7XvKkR1MJv4AhhtTss=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                ctx, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(0L, disp);
  ASSERT_EQ(1, has_cred);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         ctx, 11, NULL, 0, &disp, &has_cred));
  ASSERT_EQ(0L, disp);
  ASSERT_EQ(1, has_cred);

  /* SPKI pin mismatch -> Cancel (2) */
  cfg.cert_pin_sha256 =
      (char *)"sha256//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_apple_tls_challenge(
                ctx, 10, k_apple_ec_cert1_der, sizeof(k_apple_ec_cert1_der),
                &disp, &has_cred));
  ASSERT_EQ(2L, disp);

  /* 5. didCompleteWithError: TLS error codes (-1202 & -999 after cancel) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, c_abstract_http_test_apple_tls_challenge(
                                           ctx, 1, NULL, 0, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, c_abstract_http_test_apple_tls_challenge(
                                           ctx, 2, NULL, 0, NULL, NULL));

  /* 6. OOM sweep across http_apple_config_apply & release_tls_fail */
  {
    int step;
    for (step = 0; step < 18; ++step) {
      c_abstract_http_error_t orc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_apple_config_apply(ctx, &cfg);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      ASSERT((int)orc >= -2000 && (int)orc <= 2000);
    }
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         ctx, 3, NULL, 0, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_apple_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                         NULL, 4, NULL, 0, NULL, NULL));
  {
    static const unsigned char bad_der[2] = {0x30, 0x00};
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_apple_tls_challenge(
                                           NULL, 5, NULL, 0, NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
              c_abstract_http_test_apple_tls_challenge(
                  NULL, 6, bad_der, sizeof(bad_der), NULL, NULL));
    ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_ACCES,
        c_abstract_http_test_apple_tls_challenge(NULL, 6, NULL, 0, NULL, NULL));
  }

  cfg.ca_path = NULL;
  cfg.ca_buffer = NULL;
  cfg.ca_buffer_len = 0;
  cfg.client_cert_path = NULL;
  cfg.client_key_path = NULL;
  cfg.cert_pin_sha256 = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

  remove(ca_file);
  remove(cert1_file);
  remove(key1_file);
  remove(key2_file);
  remove(p12_file);
  remove(pw_file);
  remove(bad_utf8_file);
  remove(bad_file);
  remove(seq_file);
  remove(trunc_file);
  PASS();
}
#endif

SUITE(http_apple_suite) {
  RUN_TEST(test_apple_send_multi_branches);
  RUN_TEST(test_apple_send_multi);

  RUN_TEST(test_apple_send_mock_server);
  RUN_TEST(test_apple_read_chunk);
  RUN_TEST(test_apple_lifecycle);
  RUN_TEST(test_apple_config);
  RUN_TEST(test_apple_send_invalid);
  RUN_TEST(test_apple_send_all_methods);
#if defined(__APPLE__)
  RUN_TEST(test_apple_nsurlsession_bridge);
#endif
#if defined(__APPLE__) && defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_apple_tls_ca_mtls_and_spki_pinning);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && TEST_APPLE_HAVE_CFSTREAM
  RUN_TEST(test_apple_oom_branches);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_apple_oom);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H */
