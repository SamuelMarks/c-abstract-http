/**
 * @file test_http_libsoup3.h
 * @brief Integration tests for Libsoup3 Backend.
 *
 * Verifies that the Libsoup3 wrapper correctly initializes, handles
 * configuration, sends requests, and maps failures to error enums.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_LIBSOUP3_H
#define TEST_HTTP_LIBSOUP3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libsoup3.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error
c_abstract_http_test_libsoup3_method_str(enum HttpMethod method,
                                         const char **out);
extern enum c_abstract_http_error c_abstract_http_test_libsoup3_helpers(void);
#endif

struct libsoup3_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes */
  size_t total_bytes;
  /** @brief Abort on call */
  int abort_on_call;
};

struct libsoup3_TestUploadState {
  /** @brief Upload data */
  const char *data;
  /** @brief Length */
  size_t len;
  /** @brief Current position */
  size_t pos;
  /** @brief Should abort flag */
  int should_abort;
};

static int libsoup3_mock_chunk_cb(void *user_data, const void *chunk,
                                  size_t chunk_len) {
  struct libsoup3_TestChunkState *state;
  (void)chunk;
  state = (struct libsoup3_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return (int)ECANCELED;
  }
  return 0;
}

static int libsoup3_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                                   size_t *out_read) {
  struct libsoup3_TestUploadState *state =
      (struct libsoup3_TestUploadState *)user_data;
  size_t remaining;
  size_t to_copy;

  if (state->should_abort) {
    return -1;
  }
  remaining = state->len - state->pos;
  to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return 0;
}

static enum c_abstract_http_error
setup_libsoup3_request(struct HttpRequest *req, int port) {
  enum c_abstract_http_error rc;
  char *_ast_strdup_0 = NULL;
  char url[64];

  rc = http_request_init(req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif

  rc = c_abstract_http_strdup(url, &_ast_strdup_0);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    http_request_free(req);
    return rc;
  }
  req->url = _ast_strdup_0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Documented */
TEST test_libsoup3_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libsoup3_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libsoup3_global_init());
  g_mock_libsoup3_global_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_libsoup3_helpers());
#endif

  PASS();
}

/** @brief Documented */
TEST test_libsoup3_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_context_init(NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT(ctx != NULL);

  http_libsoup3_context_free(ctx);
  http_libsoup3_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libsoup3_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_context_init(&ctx));
  g_mock_libsoup3_context_init_fail = 0;

  g_mock_libsoup3_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_context_init(&ctx));
  g_mock_libsoup3_config_init_fail = 0;

  g_mock_libsoup3_session_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libsoup3_context_init(&ctx));
  g_mock_libsoup3_session_new_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_config_application(void) {
  char *_ast_strdup_agent = NULL;
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_user = NULL;
  char *_ast_strdup_pass = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_libsoup3_config_apply(NULL, &config));

  config.timeout_ms = 500;
  config.verify_peer = 0;
  config.follow_redirects = 0;
  c_abstract_http_strdup("agent", &_ast_strdup_agent);
  config.user_agent = _ast_strdup_agent;
  c_abstract_http_strdup("http://proxy.local:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  c_abstract_http_strdup("admin", &_ast_strdup_user);
  config.proxy_username = _ast_strdup_user;
  c_abstract_http_strdup("secret", &_ast_strdup_pass);
  config.proxy_password = _ast_strdup_pass;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  /* Re-apply with same strings to test replacement */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  /* Test cookie jar */
  config.cookie_jar = (struct HttpCookieJar *)1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));
  config.cookie_jar = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  /* Test invalid proxy URL */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libsoup3_uri_parse_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));
  g_mock_libsoup3_uri_parse_fail = 0;
#endif

  /* Re-apply with NULL strings to test clearing */
  config.user_agent = NULL;
  config.proxy_url = NULL;
  config.proxy_username = NULL;
  config.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.user_agent = (char *)"agent";
    g_mock_libsoup3_config_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_config_apply(ctx, &cfg));
    cfg.user_agent = NULL;

    cfg.proxy_url = (char *)"proxy";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_config_apply(ctx, &cfg));
    cfg.proxy_url = NULL;

    cfg.proxy_username = (char *)"user";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_config_apply(ctx, &cfg));
    cfg.proxy_username = NULL;

    cfg.proxy_password = (char *)"pwd";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_config_apply(ctx, &cfg));
    g_mock_libsoup3_config_init_fail = 0;
  }
#endif

  free(_ast_strdup_agent);
  free(_ast_strdup_proxy);
  free(_ast_strdup_user);
  free(_ast_strdup_pass);
  http_config_free(&config);
  http_libsoup3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_send(ctx, &req, NULL));

  /* Multipart with no body */
  req.parts.count = 1;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_send(ctx, &req, &res));
  req.parts.count = 0;

  /* Invalid method */
  req.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_send(ctx, &req, &res));
  req.method = HTTP_GET;

  http_request_free(&req);
  http_libsoup3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_send_connection_failure(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  config.timeout_ms = 50;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, setup_libsoup3_request(&req, 59999));

  rc = http_libsoup3_send(ctx, &req, &res);
  ASSERT(rc == (enum c_abstract_http_error)ECONNREFUSED ||
         rc == C_ABSTRACT_HTTP_ERR_TIMEOUT ||
         rc == (enum c_abstract_http_error)EHOSTUNREACH ||
         rc == C_ABSTRACT_HTTP_ERR_IO);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_libsoup3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_send_success(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  struct HttpConfig config;
  char *_ast_url = NULL;
  char *body_copy = NULL;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;
  req.method = HTTP_POST;
  c_abstract_http_strdup("POST_DATA", &body_copy);
  req.body = body_copy;
  req.body_len = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Custom", "Val"));

  rc = http_libsoup3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_NEQ(NULL, res);
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("OK", (char *)res->body);
  http_response_free(res);
  free(res);
  res = NULL;

  http_request_free(&req);
  http_config_free(&config);
  http_libsoup3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_send_chunked_and_upload(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  struct HttpConfig config;
  struct libsoup3_TestChunkState state;
  struct libsoup3_TestUploadState up_st;
  char *_ast_url = NULL;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  /* Chunked receive */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = libsoup3_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_libsoup3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_NEQ(NULL, res);
  ASSERT_EQ(NULL, res->body);
  ASSERT_EQ(0, res->body_len);
  ASSERT(state.call_count > 0);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Chunked receive abort */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1;
  req.on_chunk = libsoup3_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_libsoup3_send(ctx, &req, &res);
  ASSERT_EQ((enum c_abstract_http_error)ECANCELED, rc);
  ASSERT_EQ(NULL, res);
  http_request_free(&req);

  /* Upload chunked */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;
  req.method = HTTP_POST;
  up_st.data = "01234567890123456789012345678901234567890123456789"
               "01234567890123456789012345678901234567890123456789"
               "01234567890123456789012345678901234567890123456789"
               "01234567890123456789012345678901234567890123456789";
  up_st.len = 200;
  up_st.pos = 0;
  up_st.should_abort = 0;
  req.read_chunk = libsoup3_mock_upload_cb;
  req.read_chunk_user_data = &up_st;

  rc = http_libsoup3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_NEQ(NULL, res);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Upload chunked abort */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;
  req.method = HTTP_POST;
  up_st.data = "UPLOAD";
  up_st.len = 6;
  up_st.pos = 0;
  up_st.should_abort = 1;
  req.read_chunk = libsoup3_mock_upload_cb;
  req.read_chunk_user_data = &up_st;

  rc = http_libsoup3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  ASSERT_EQ(NULL, res);
  http_request_free(&req);

  http_config_free(&config);
  http_libsoup3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_send_faults(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  struct HttpConfig config;
  char *_ast_url = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:8080/test", &_ast_url);
  req.url = _ast_url;

  /* msg new fail */
  g_mock_libsoup3_msg_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libsoup3_send(ctx, &req, &res));
  g_mock_libsoup3_msg_new_fail = 0;

  /* send fail: connection refused */
  g_mock_libsoup3_send_fail = 1;
  ASSERT_EQ((enum c_abstract_http_error)ECONNREFUSED,
            http_libsoup3_send(ctx, &req, &res));

  /* send fail: timeout */
  g_mock_libsoup3_send_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_TIMEOUT, http_libsoup3_send(ctx, &req, &res));

  /* send fail: generic io */
  g_mock_libsoup3_send_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libsoup3_send(ctx, &req, &res));
  g_mock_libsoup3_send_fail = 0;

  /* res alloc fail */
  g_mock_libsoup3_res_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_send(ctx, &req, &res));
  g_mock_libsoup3_res_alloc_fail = 0;

  /* res init fail */
  g_mock_libsoup3_res_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_send(ctx, &req, &res));
  g_mock_libsoup3_res_init_fail = 0;

  /* body alloc fail */
  g_mock_libsoup3_body_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libsoup3_send(ctx, &req, &res));
  g_mock_libsoup3_body_alloc_fail = 0;

  http_request_free(&req);
  http_config_free(&config);
  http_libsoup3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libsoup3_global_cleanup());
#endif
  PASS();
}

/** @brief Documented */
TEST test_libsoup3_methods(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  const char *str = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_test_libsoup3_method_str(HTTP_GET, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_GET, &str));
  ASSERT_STR_EQ("GET", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_POST, &str));
  ASSERT_STR_EQ("POST", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_PUT, &str));
  ASSERT_STR_EQ("PUT", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_DELETE, &str));
  ASSERT_STR_EQ("DELETE", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_HEAD, &str));
  ASSERT_STR_EQ("HEAD", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_PATCH, &str));
  ASSERT_STR_EQ("PATCH", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_QUERY, &str));
  ASSERT_STR_EQ("QUERY", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_OPTIONS, &str));
  ASSERT_STR_EQ("OPTIONS", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_TRACE, &str));
  ASSERT_STR_EQ("TRACE", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libsoup3_method_str(HTTP_CONNECT, &str));
  ASSERT_STR_EQ("CONNECT", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_test_libsoup3_method_str(
                                           (enum HttpMethod)999, &str));
  {
    struct HttpRequest req_test;
    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, setup_libsoup3_request(&req_test, 80));
    g_mock_headers_init_fail = 0;

    g_mock_alloc_count = 1;
    g_mock_alloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, setup_libsoup3_request(&req_test, 80));
    g_mock_alloc_fail = 0;
  }
#endif
  PASS();
}

/** @brief Documented */
SUITE(http_libsoup3_suite) {
  RUN_TEST(test_libsoup3_global_lifecycle);
  RUN_TEST(test_libsoup3_context_lifecycle);
  RUN_TEST(test_libsoup3_config_application);
  RUN_TEST(test_libsoup3_send_invalid_arguments);
  RUN_TEST(test_libsoup3_send_connection_failure);
  RUN_TEST(test_libsoup3_send_success);
  RUN_TEST(test_libsoup3_send_chunked_and_upload);
  RUN_TEST(test_libsoup3_send_faults);
  RUN_TEST(test_libsoup3_methods);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_LIBSOUP3_H */
