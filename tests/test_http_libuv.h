/**
 * @file test_http_libuv.h
 * @brief Integration tests for Liblibuv Backend.
 *
 * Verifies that the libuv wrapper correctly initializes, handles configuration,
 * sends requests, and maps failures to error enums.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_LIBUV_H
#define TEST_HTTP_LIBUV_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libuv.h>
#include <c_abstract_http/http_types.h>
#include "abstract_http_test_helpers/mock_server.h"
#include "mock_alloc.h"
/* clang-format on */

struct libuv_TestChunkState {
  /** @brief Documented */
  int call_count;
  /** @brief Documented */
  size_t total_bytes;
  /** @brief Documented */
  int abort_on_call;
};

struct libuv_TestUploadState {
  /** @brief Documented */
  const char *data;
  /** @brief Documented */
  size_t len;
  /** @brief Documented */
  size_t pos;
};

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error
c_abstract_http_test_libuv_method_str(enum HttpMethod method,
                                      const char **out_str);
extern enum c_abstract_http_error c_abstract_http_test_libuv_helpers(void);
#endif

/**
 * @brief Helper to initialize request for testing.
 *
 * @param[out] req Request.
 * @param[in] port Port.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
static enum c_abstract_http_error setup_libuv_request(struct HttpRequest *req,
                                                      int port) {
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
TEST test_libuv_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libuv_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_global_init());
  g_mock_libuv_global_init_fail = 0;
#endif

  PASS();
}

/** @brief Documented */
TEST test_libuv_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_context_init(NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT(ctx != NULL);
  http_libuv_context_free(ctx);
  http_libuv_context_free(NULL);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libuv_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_context_init(&ctx));
  g_mock_libuv_context_init_fail = 0;

  g_mock_libuv_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_context_init(&ctx));
  g_mock_libuv_config_init_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libuv_config_application(void) {
  char *_ast_strdup_agent = NULL;
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_user = NULL;
  char *_ast_strdup_pass = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_config_apply(NULL, &config));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  /* Re-apply with same strings to test replacement */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  /* Re-apply with NULL strings to test clearing */
  config.user_agent = NULL;
  config.proxy_url = NULL;
  config.proxy_username = NULL;
  config.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  free(_ast_strdup_agent);
  free(_ast_strdup_proxy);
  free(_ast_strdup_user);
  free(_ast_strdup_pass);
  http_config_free(&config);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, NULL));

  /* Multipart missing flattened body */
  req.parts.count = 1;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  req.parts.count = 0;

  /* Invalid URL */
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  http_request_free(&req);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_connection_failure(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.timeout_ms = 50;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, setup_libuv_request(&req, 59999));

  rc = http_libuv_send(ctx, &req, &res);
  ASSERT(rc == (enum c_abstract_http_error)ECONNREFUSED ||
         rc == C_ABSTRACT_HTTP_ERR_TIMEOUT ||
         rc == (enum c_abstract_http_error)EHOSTUNREACH ||
         rc == C_ABSTRACT_HTTP_ERR_IO);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */

static int libuv_mock_chunk_cb(void *user_data, const void *chunk,
                               size_t chunk_len) {
  struct libuv_TestChunkState *state;
  (void)chunk;
  state = (struct libuv_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return (int)ECANCELED;
  }
  return 0;
}

/** @brief Documented */
TEST test_libuv_send_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libuv_TestChunkState state;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            setup_libuv_request(&req, math_mock_server_get_port(server)));

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = libuv_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(NULL, res->body);
  ASSERT_EQ(0, res->body_len);
  ASSERT(state.call_count > 0);
  ASSERT(state.total_bytes > 0);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  mock_server_destroy(server);
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_chunked_abort(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libuv_TestChunkState state;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            setup_libuv_request(&req, math_mock_server_get_port(server)));

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1;
  req.on_chunk = libuv_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ((enum c_abstract_http_error)ECANCELED, rc);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  mock_server_destroy(server);
  PASS();
}

static int libuv_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                                size_t *out_read) {
  struct libuv_TestUploadState *state =
      (struct libuv_TestUploadState *)user_data;
  size_t remaining = state->len - state->pos;
  size_t to_copy = (remaining < buf_len) ? remaining : buf_len;

  if (to_copy > 0) {
    memcpy(buf, state->data + state->pos, to_copy);
    state->pos += to_copy;
  }
  *out_read = to_copy;
  return 0;
}

/** @brief Documented */
TEST test_libuv_send_upload_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libuv_TestUploadState up_state;
  const char *payload = "UPLOAD_TEST_DATA";
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            setup_libuv_request(&req, math_mock_server_get_port(server)));
  req.method = HTTP_POST;

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  req.read_chunk = libuv_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(up_state.len, up_state.pos);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  mock_server_destroy(server);
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_direct_body_and_urls(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));

  /* Test 1: URL with colon before slash, direct body and headers */
  {
    char *body_copy = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
    req.url = _ast_url;
    req.method = HTTP_POST;
    c_abstract_http_strdup("test_body", &body_copy);
    req.body = body_copy;
    req.body_len = 9;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "X-Custom", "Value"));

    rc = http_libuv_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT(res != NULL);
    ASSERT_EQ(200, res->status_code);
    ASSERT(res->body != NULL);
    ASSERT_STR_EQ("OK", res->body);
    http_response_free(res);
    free(res);
    res = NULL;
    http_request_free(&req);
  }

  /* Test 2: HTTPS URL without port */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("https://127.0.0.1/path", &_ast_url);
  req.url = _ast_url;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Test 3: URL without scheme */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("127.0.0.1/path", &_ast_url);
  req.url = _ast_url;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Test 4: Host only without slash or colon */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("localhost", &_ast_url);
  req.url = _ast_url;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_fault_injections(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  struct libuv_TestChunkState state;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;

  /* 1: Req buf alloc fail */
  g_mock_libuv_req_buf_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_req_buf_alloc_fail = 0;

  /* 2: Addrinfo fail */
  g_mock_libuv_addrinfo_fail = 1;
  ASSERT_EQ((enum c_abstract_http_error)EHOSTUNREACH,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_addrinfo_fail = 0;

  /* 3: Connect fail */
  g_mock_libuv_connect_fail = 1;
  ASSERT_EQ((enum c_abstract_http_error)ECONNREFUSED,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_connect_fail = 0;

  /* 4: Write fail */
  g_mock_libuv_write_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_write_fail = 0;

  /* 5: Read fail */
  g_mock_libuv_read_fail = 1;
  ASSERT_EQ((enum c_abstract_http_error)ECONNREFUSED,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_read_fail = 0;

  /* 6: Alloc cb fail */
  g_mock_libuv_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_alloc_fail = 0;

  /* 7: Res alloc fail */
  g_mock_libuv_res_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_res_alloc_fail = 0;

  /* 8: Res init fail */
  g_mock_libuv_res_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_res_init_fail = 0;

  /* 9: Body alloc fail */
  g_mock_libuv_body_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_body_alloc_fail = 0;

  /* 10: on_chunk abort */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1;
  req.on_chunk = libuv_mock_chunk_cb;
  req.on_chunk_user_data = &state;
  ASSERT_EQ((enum c_abstract_http_error)ECANCELED,
            http_libuv_send(ctx, &req, &res));
  req.on_chunk = NULL;

  /* 11: Body realloc fail */
  g_mock_libuv_body_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_body_realloc_fail = 0;

  http_request_free(&req);
  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
#endif
  PASS();
}

/** @brief Documented */
TEST test_libuv_methods(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  const char *str = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_test_libuv_method_str(HTTP_GET, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_GET, &str));
  ASSERT_STR_EQ("GET", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_POST, &str));
  ASSERT_STR_EQ("POST", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_PUT, &str));
  ASSERT_STR_EQ("PUT", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_DELETE, &str));
  ASSERT_STR_EQ("DELETE", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_PATCH, &str));
  ASSERT_STR_EQ("PATCH", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_HEAD, &str));
  ASSERT_STR_EQ("HEAD", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_OPTIONS, &str));
  ASSERT_STR_EQ("OPTIONS", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_TRACE, &str));
  ASSERT_STR_EQ("TRACE", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_QUERY, &str));
  ASSERT_STR_EQ("QUERY", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libuv_method_str(HTTP_CONNECT, &str));
  ASSERT_STR_EQ("CONNECT", str);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_test_libuv_method_str((enum HttpMethod)999, &str));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    enum c_abstract_http_error h_rc;
    g_mock_headers_init_fail = 1;
    h_rc = c_abstract_http_test_libuv_helpers();
    g_mock_headers_init_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, h_rc);
  }
  {
    struct HttpRequest req_test;
    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, setup_libuv_request(&req_test, 80));
    g_mock_headers_init_fail = 0;

    g_mock_alloc_count = 1;
    g_mock_alloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, setup_libuv_request(&req_test, 80));
    g_mock_alloc_fail = 0;
  }
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_libuv_helpers());
#endif
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_large_payloads(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  char *large_body = NULL;
  size_t i;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));

  /* Test with large headers */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  for (i = 0; i < 40; i++) {
    char k[32];
    char v[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(k, sizeof(k), "X-Header-%lu", (unsigned long)i);
#else
    sprintf(k, "X-Header-%lu", (unsigned long)i);
#endif
    memset(v, 'A', sizeof(v) - 1);
    v[sizeof(v) - 1] = '\0';
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_add(&req.headers, k, v));
  }
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  /* Test with large body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  large_body = (char *)malloc(5000);
  if (large_body) {
    memset(large_body, 'B', 4999);
    large_body[4999] = '\0';
    req.body = large_body;
    req.body_len = 4999;
  }
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.user_agent = (char *)"agent";
    g_mock_libuv_config_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_config_apply(ctx, &cfg));
    cfg.user_agent = NULL;

    cfg.proxy_url = (char *)"proxy";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_config_apply(ctx, &cfg));
    cfg.proxy_url = NULL;

    cfg.proxy_username = (char *)"user";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_config_apply(ctx, &cfg));
    cfg.proxy_username = NULL;

    cfg.proxy_password = (char *)"pwd";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_config_apply(ctx, &cfg));
    g_mock_libuv_config_init_fail = 0;
  }

  /* Test uv fail branches */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;

  g_mock_libuv_addrinfo_fail = 2;
  ASSERT_EQ((enum c_abstract_http_error)EHOSTUNREACH,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_addrinfo_fail = 0;

  g_mock_libuv_connect_fail = 2;
  ASSERT_EQ((enum c_abstract_http_error)ECONNREFUSED,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_connect_fail = 0;

  g_mock_libuv_write_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_write_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_write_fail = 0;

  g_mock_libuv_read_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_read_fail = 0;

  /* Test headers realloc fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  for (i = 0; i < 40; i++) {
    char k[32];
    char v[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(k, sizeof(k), "X-Header-%lu", (unsigned long)i);
#else
    sprintf(k, "X-Header-%lu", (unsigned long)i);
#endif
    memset(v, 'A', sizeof(v) - 1);
    v[sizeof(v) - 1] = '\0';
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_add(&req.headers, k, v));
  }
  g_mock_libuv_headers_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_headers_realloc_fail = 0;
  http_request_free(&req);

  /* Test body realloc fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  large_body = (char *)malloc(5000);
  if (large_body) {
    memset(large_body, 'B', 4999);
    large_body[4999] = '\0';
    req.body = large_body;
    req.body_len = 4999;
  }
  g_mock_libuv_body_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_body_alloc_fail = 0;
  http_request_free(&req);

  /* Test chunk accumulation and chunk realloc fail */
  {
    struct libuv_TestUploadState up_st;
    char *chunk_data = (char *)malloc(5000);
    if (chunk_data) {
      memset(chunk_data, 'C', 4999);
      chunk_data[4999] = '\0';
      up_st.data = chunk_data;
      up_st.len = 4999;
      up_st.pos = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      _ast_url = NULL;
      c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
      req.url = _ast_url;
      req.read_chunk = libuv_mock_upload_cb;
      req.read_chunk_user_data = &up_st;
      rc = http_libuv_send(ctx, &req, &res);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
      if (res) {
        http_response_free(res);
        free(res);
        res = NULL;
      }
      http_request_free(&req);

      up_st.pos = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      _ast_url = NULL;
      c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
      req.url = _ast_url;
      req.read_chunk = libuv_mock_upload_cb;
      req.read_chunk_user_data = &up_st;
      g_mock_libuv_body_realloc_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
      g_mock_libuv_body_realloc_fail = 0;
      http_request_free(&req);
      free(chunk_data);
    }
  }

  /* Test invalid method */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  req.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  http_request_free(&req);

  /* Test parse_url fail in send */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  g_mock_libuv_req_buf_alloc_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_req_buf_alloc_fail = 0;
  http_request_free(&req);
#endif

  http_libuv_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */
SUITE(http_libuv_suite) {
  RUN_TEST(test_libuv_global_lifecycle);
  RUN_TEST(test_libuv_context_lifecycle);
  RUN_TEST(test_libuv_config_application);
  RUN_TEST(test_libuv_send_invalid_arguments);
  RUN_TEST(test_libuv_send_connection_failure);
  RUN_TEST(test_libuv_send_chunked);
  RUN_TEST(test_libuv_send_chunked_abort);
  RUN_TEST(test_libuv_send_upload_chunked);
  RUN_TEST(test_libuv_send_direct_body_and_urls);
  RUN_TEST(test_libuv_send_fault_injections);
  RUN_TEST(test_libuv_send_large_payloads);
  RUN_TEST(test_libuv_methods);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_LIBUV_H */
