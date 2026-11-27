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
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libuv_method_str(enum HttpMethod method,
                                      const char **out_str);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libuv_helpers(void);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_libuv_helper_step(int step);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_libuv_socks_reply(const unsigned char *data,
                                           size_t len, int send_eof);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_libuv_pool_close_fail(int fail);
#endif

NO_DISCARD static c_abstract_http_error_t
libuv_mock_upload_cb_err(void *user_data, void *buf, size_t buf_len,
                         size_t *out_read) {
  if (user_data || buf || buf_len || out_read) {
    /* pass */
  }
  return C_ABSTRACT_HTTP_ERR_INVAL;
}

/**
 * @brief Helper to initialize request for testing.
 *
 * @param[out] req Request.
 * @param[in] port Port.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
NO_DISCARD static c_abstract_http_error_t
setup_libuv_request(struct HttpRequest *req, int port) {
  c_abstract_http_error_t rc;
  char *_ast_strdup_0 = NULL;
  char url[64];

  rc = http_request_init(req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  c89stringutils_snprintf(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#endif

  rc = c_abstract_http_strdup(url, &_ast_strdup_0);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    {
      c_abstract_http_error_t free_rc = http_request_free(req);
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        /* ignore */
      }
    }
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(NULL));

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
  free(config.user_agent);
  config.user_agent = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("agent", &_ast_strdup_agent));
  config.user_agent = _ast_strdup_agent;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_strdup("http://proxy.local:8080", &_ast_strdup_proxy));
  config.proxy_url = _ast_strdup_proxy;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("admin", &_ast_strdup_user));
  config.proxy_username = _ast_strdup_user;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("secret", &_ast_strdup_pass));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
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
  req.body = "x";
  req.body_len = 0;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  req.parts.count = 0;
  req.body = NULL;

  /* Invalid URL */
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_connection_failure(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.timeout_ms = 50;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, setup_libuv_request(&req, 59999));

  rc = http_libuv_send(ctx, &req, &res);
  ASSERT(rc == (c_abstract_http_error_t)ECONNREFUSED ||
         rc == C_ABSTRACT_HTTP_ERR_TIMEOUT ||
         rc == (c_abstract_http_error_t)EHOSTUNREACH ||
         rc == C_ABSTRACT_HTTP_ERR_IO);
  ASSERT(res == NULL);

  /* Test with timeout_ms = 0 */
  config.timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &config));
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  ASSERT(res == NULL);

  /* Test with body_len == 0 but req.body non-null */
  req.body = "dummy";
  req.body_len = 0;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  ASSERT(res == NULL);
  req.body = NULL;

  /* Test with read_chunk error */
  req.read_chunk = libuv_mock_upload_cb_err;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  ASSERT(res == NULL);
  req.read_chunk = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

/** @brief Documented */

NO_DISCARD static c_abstract_http_error_t
libuv_mock_chunk_cb(void *user_data, const void *chunk, size_t chunk_len) {
  struct libuv_TestChunkState *state;
  if (chunk) {
    /* pass */
  }
  state = (struct libuv_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call < 0) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return (c_abstract_http_error_t)ECANCELED;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Documented */
TEST test_libuv_send_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libuv_TestChunkState state;
  c_abstract_http_error_t rc;

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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
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
  c_abstract_http_error_t rc;

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
  ASSERT_EQ((c_abstract_http_error_t)ECANCELED, rc);
  ASSERT(res == NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
libuv_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
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
  return C_ABSTRACT_HTTP_SUCCESS;
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
  c_abstract_http_error_t rc;

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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
  PASS();
}

/** @brief Documented */
TEST test_libuv_send_direct_body_and_urls(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));

  /* Test 1: URL with colon before slash, direct body and headers */
  {
    char *body_copy = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
    req.url = _ast_url;
    req.method = HTTP_POST;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("test_body", &body_copy));
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
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Test 2: HTTPS URL without port */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("https://127.0.0.1/path", &_ast_url));
  req.url = _ast_url;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test 3: URL without scheme */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("127.0.0.1/path", &_ast_url));
  req.url = _ast_url;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test 4: Host only without slash or colon */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("localhost", &_ast_url));
  req.url = _ast_url;
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;

  /* 1: Req buf alloc fail */
  g_mock_libuv_req_buf_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_req_buf_alloc_fail = 0;

  /* 2: Addrinfo fail */
  g_mock_libuv_addrinfo_fail = 1;
  ASSERT_EQ((c_abstract_http_error_t)EHOSTUNREACH,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_addrinfo_fail = 0;

  /* 3: Connect fail */
  g_mock_libuv_connect_fail = 1;
  ASSERT_EQ((c_abstract_http_error_t)ECONNREFUSED,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_connect_fail = 0;

  /* 4: Write fail */
  g_mock_libuv_write_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_write_fail = 0;

  /* 5: Read fail */
  g_mock_libuv_read_fail = 1;
  ASSERT_EQ((c_abstract_http_error_t)ECONNREFUSED,
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
  ASSERT_EQ((c_abstract_http_error_t)ECANCELED,
            http_libuv_send(ctx, &req, &res));
  req.on_chunk = NULL;

  /* 11: Body realloc fail */
  g_mock_libuv_body_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_body_realloc_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
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
    c_abstract_http_error_t h_rc;
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

    g_mock_alloc_count = 0;
    g_mock_alloc_fail = 1;
    {
      c_abstract_http_error_t s_rc = setup_libuv_request(&req_test, 80);
      g_mock_alloc_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, s_rc);
    }
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
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));

  /* Test with large headers */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;
  for (i = 0; i < 40; i++) {
    char k[32];
    char v[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(k, sizeof(k), "X-Header-%lu", (unsigned long)i);
#else
    c89stringutils_snprintf(k, sizeof(k), "X-Header-%lu", (unsigned long)i);
#endif
    memset(v, 'A', sizeof(v) - 1);
    v[sizeof(v) - 1] = '\0';
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_add(&req.headers, k, v));
  }
  rc = http_libuv_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test with large body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
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
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    free(cfg.user_agent);
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;

  g_mock_libuv_addrinfo_fail = 2;
  ASSERT_EQ((c_abstract_http_error_t)EHOSTUNREACH,
            http_libuv_send(ctx, &req, &res));
  g_mock_libuv_addrinfo_fail = 0;

  g_mock_libuv_connect_fail = 2;
  ASSERT_EQ((c_abstract_http_error_t)ECONNREFUSED,
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test headers realloc fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;
  for (i = 0; i < 40; i++) {
    char k[32];
    char v[128];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(k, sizeof(k), "X-Header-%lu", (unsigned long)i);
#else
    c89stringutils_snprintf(k, sizeof(k), "X-Header-%lu", (unsigned long)i);
#endif
    memset(v, 'A', sizeof(v) - 1);
    v[sizeof(v) - 1] = '\0';
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_add(&req.headers, k, v));
  }
  g_mock_libuv_headers_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_headers_realloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test body realloc fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

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
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
      req.url = _ast_url;
      req.read_chunk = libuv_mock_upload_cb;
      req.read_chunk_user_data = &up_st;
      rc = http_libuv_send(ctx, &req, &res);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
      if (res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

      up_st.pos = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      _ast_url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
      req.url = _ast_url;
      req.read_chunk = libuv_mock_upload_cb;
      req.read_chunk_user_data = &up_st;
      g_mock_libuv_body_realloc_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
      g_mock_libuv_body_realloc_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
      free(chunk_data);
    }
  }

  /* Test invalid method */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;
  req.method = (enum HttpMethod)999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test parse_url fail in send */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;
  g_mock_libuv_req_buf_alloc_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_req_buf_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

TEST test_libuv_tls_spki_chunked_redirects_and_multi(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpMultiRequest multi;
  struct HttpRequest r1, r2;
  struct HttpFuture f1, f2;
  struct HttpFuture *futs[2];
  struct ModalityEventLoop *loop = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  /* TLS / X.509 / SPKI pinning config validation & verification */
  cfg.tls_version_mask = 0xFF00U;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_libuv_config_apply(ctx, &cfg));
  cfg.tls_version_mask = HTTP_TLS_VERSION_1_2 | HTTP_TLS_VERSION_1_3;

  cfg.client_cert_path = (char *)"cli.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_config_apply(ctx, &cfg));
  cfg.client_cert_path = NULL;

  cfg.verify_peer = 1;
  cfg.verify_host = 1;
  cfg.follow_redirects = 1;
  cfg.ca_path = (char *)"ca.pem";
  cfg.client_cert_path = (char *)"cli.pem";
  cfg.client_key_path = (char *)"cli.key";
  cfg.ca_buffer = (unsigned char *)"PEM";
  cfg.ca_buffer_len = 3;
  cfg.cert_pin_sha256 =
      (char *)"sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  cfg.ca_path = NULL;
  cfg.client_cert_path = NULL;
  cfg.client_key_path = NULL;
  cfg.ca_buffer = NULL;
  cfg.ca_buffer_len = 0;
  cfg.cert_pin_sha256 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com/chunked";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_STR_EQ("Hello, libuv", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* verify_host mismatch on untrusted.invalid -> ERR_ACCES */
  req.url = (char *)"https://untrusted.invalid/test";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_libuv_send(ctx, &req, &res));

  /* SPKI pin mismatch -> ERR_ACCES */
  cfg.cert_pin_sha256 = (char *)"sha256//badpin=";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  cfg.cert_pin_sha256 = NULL;
  req.url = (char *)"https://example.com/test";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_libuv_send(ctx, &req, &res));

  /* Reset clean config & test redirects (302 -> 200 and loop -> ERR_LOOP) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  req.url = (char *)"http://127.0.0.1:80/redir-once";
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127.0.0.1:80/redir-loop";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_LOOP, http_libuv_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* ModalityEventLoop + uv_run(UV_RUN_NOWAIT) concurrent send_multi */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_libuv_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  futs[0] = &f1;
  futs[1] = &f2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libuv_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libuv_send_multi(ctx, loop, &multi, futs));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
  r1.url = (char *)"https://example.com/chunked";
  r2.url = (char *)"http://127.0.0.1:80/two";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libuv_send_multi(ctx, loop, &multi, futs));
  ASSERT_EQ(1, f1.is_ready);
  ASSERT_EQ(1, f2.is_ready);
  ASSERT_STR_EQ("Hello, libuv", f1.response->body);
  ASSERT_STR_EQ("OK", f2.response->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
  free(f1.response);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
  free(f2.response);

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libuv_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
  free(f1.response);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
  free(f2.response);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int k;
    cfg.ca_path = (char *)"ca.pem";
    cfg.client_cert_path = (char *)"cli.pem";
    cfg.client_key_path = (char *)"cli.key";
    cfg.ca_buffer = (unsigned char *)"PEM";
    cfg.ca_buffer_len = 3;
    cfg.cert_pin_sha256 =
        (char *)"sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
    for (k = 0; k < 25; ++k) {
      c_abstract_http_error_t orc;
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_libuv_config_apply(ctx, &cfg);
      g_mock_alloc_fail = 0;

      res = NULL;
      r2.url = (char *)"http://127.0.0.1:80/redir-once";
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_libuv_send(ctx, &r2, &res);
      g_mock_alloc_fail = 0;
      if (res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }

      r2.url = (char *)"http://127.0.0.1:80/two";
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_libuv_send_multi(ctx, NULL, &multi, futs);
      g_mock_alloc_fail = 0;
      ASSERT((int)orc >= -2000 && (int)orc <= 2000);
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }
    }
    g_mock_alloc_count = 0;
    {
      struct HttpTransportContext *c2 = NULL;
      c_abstract_http_error_t hrc0;
      g_mock_alloc_count = 1;
      g_mock_alloc_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_context_init(&c2));
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;

      g_mock_headers_init_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      g_mock_headers_init_fail = 0;

      r2.url = (char *)"http://127.0.0.1:80/redir-once";
      g_mock_response_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      g_mock_response_free_fail = 0;

      r2.url = (char *)"http://127.0.0.1:80/chunked-hex";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;

      {
        struct libuv_TestChunkState cst;
        memset(&cst, 0, sizeof(cst));
        cst.abort_on_call = 1;
        r2.on_chunk = libuv_mock_chunk_cb;
        r2.on_chunk_user_data = &cst;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
                  http_libuv_send(ctx, &r2, &res));
        r2.on_chunk = NULL;
      }

      r2.url = (char *)"http://127.0.0.1:80/chunked-bad1";
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      r2.url = (char *)"http://127.0.0.1:80/chunked-bad2";
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      r2.url = (char *)"http://127.0.0.1:80/chunked-bad3";
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      r2.url = (char *)"http://127.0.0.1:80/chunked-eof";
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      r2.url = (char *)"http://127.0.0.1:80/chunked-empty";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      r2.url = (char *)"http://127.0.0.1:80/chunked-bad4";
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &r2, &res));
      r2.url = (char *)"http://127.0.0.1:80/chunked-huge";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
      ASSERT_EQ(261U, res->body_len);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      for (k = 0; k < 25; ++k) {
        c_abstract_http_error_t crc_h;
        res = NULL;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        crc_h = http_libuv_send(ctx, &r2, &res);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)crc_h >= -2000 && (int)crc_h <= 2000);
        if (res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
        }
      }
      r2.url = (char *)"http://127.0.0.1:80/chunked-big";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
      ASSERT_EQ(85U, res->body_len);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;

      for (k = 0; k < 25; ++k) {
        c_abstract_http_error_t crc;
        res = NULL;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        crc = http_libuv_send(ctx, &r2, &res);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)crc >= -2000 && (int)crc <= 2000);
        if (res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
        }
      }

      {
        struct libuv_TestChunkState cst_nomem;
        memset(&cst_nomem, 0, sizeof(cst_nomem));
        cst_nomem.abort_on_call = 0;
        r2.url = (char *)"http://127.0.0.1:80/chunked-hex";
        r2.on_chunk = libuv_mock_chunk_cb;
        r2.on_chunk_user_data = &cst_nomem;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
        cst_nomem.abort_on_call = -1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &r2, &res));
        r2.on_chunk = NULL;
      }

      /* context_free config_free failure & config_apply OOM sweep */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&c2));
      g_mock_config_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_context_free(c2));
      g_mock_config_free_fail = 0;
      c2 = NULL;

      cfg.ca_path = (char *)"ca.pem";
      cfg.client_cert_path = (char *)"c.pem";
      cfg.client_key_path = (char *)"k.pem";
      cfg.ca_buffer = (unsigned char *)"CABUF";
      cfg.ca_buffer_len = 5;
      cfg.cert_pin_sha256 = (char *)"sha256//pin";
      cfg.proxy_url = (char *)"http://127.0.0.1:8888";
      cfg.proxy_username = (char *)"u";
      cfg.proxy_password = (char *)"p";
      cfg.timeout_ms = 500;
      for (k = 1; k <= 10; ++k) {
        c_abstract_http_error_t arc;
        g_mock_http_types_strdup_fail = k;
        arc = http_libuv_config_apply(ctx, &cfg);
        g_mock_http_types_strdup_fail = 0;
        ASSERT((int)arc >= -2000 && (int)arc <= 2000);
      }
      for (k = 0; k < 12; ++k) {
        c_abstract_http_error_t arc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        arc = http_libuv_config_apply(ctx, &cfg);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)arc >= -2000 && (int)arc <= 2000);
      }
      cfg.proxy_url = NULL;
      cfg.ca_path = NULL;
      cfg.client_cert_path = NULL;
      cfg.client_key_path = NULL;
      cfg.ca_buffer = NULL;
      cfg.ca_buffer_len = 0;
      cfg.cert_pin_sha256 = NULL;
      {
        char *saved_ua = cfg.user_agent;
        cfg.user_agent = NULL;
        g_mock_libuv_config_init_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                  http_libuv_config_apply(ctx, &cfg));
        g_mock_libuv_config_init_fail = 0;
        g_mock_alloc_count = 0;
        g_mock_alloc_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                  http_libuv_config_apply(ctx, &cfg));
        g_mock_alloc_fail = 0;
        cfg.proxy_username = NULL;
        g_mock_libuv_config_init_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                  http_libuv_config_apply(ctx, &cfg));
        g_mock_libuv_config_init_fail = 0;
        g_mock_alloc_count = 0;
        g_mock_alloc_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                  http_libuv_config_apply(ctx, &cfg));
        g_mock_alloc_fail = 0;
        cfg.user_agent = saved_ua;
      }
      cfg.proxy_password = NULL;
      cfg.follow_redirects = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
      r2.url = (char *)"http://127.0.0.1:80/plain";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;

      /* uv_loop_init, uv_tcp_init, uv_timer_init, uv_timer_start failures */
      r2.url = (char *)"http://127.0.0.1:80/plain";
      for (k = 2; k <= 5; ++k) {
        c_abstract_http_error_t urc;
        g_mock_libuv_context_init_fail = k;
        urc = http_libuv_send(ctx, &r2, &res);
        g_mock_libuv_context_init_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, urc);
      }
      g_mock_libuv_context_init_fail = 2;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      hrc0 = http_libuv_send_multi(ctx, NULL, &multi, futs);
      g_mock_libuv_context_init_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, hrc0);

      /* Redirect branches: no Location, loop + response_free_fail=11, rel OOM +
       * response_free_fail */
      cfg.follow_redirects = 1;
      cfg.ca_path = NULL;
      cfg.client_cert_path = NULL;
      cfg.client_key_path = NULL;
      cfg.ca_buffer = NULL;
      cfg.ca_buffer_len = 0;
      cfg.cert_pin_sha256 = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));

      r2.url = (char *)"http://127.0.0.1:80/redir-no-loc";
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &r2, &res));
      ASSERT_EQ(302, res->status_code);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;

      r2.url = (char *)"http://127.0.0.1:80/redir-loop";
      g_mock_response_free_fail = 11;
      hrc0 = http_libuv_send(ctx, &r2, &res);
      g_mock_response_free_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, hrc0);

      r2.url = (char *)"http://127.0.0.1:80/redir-rel";
      for (k = 0; k < 25; ++k) {
        c_abstract_http_error_t rrc;
        res = NULL;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        rrc = http_libuv_send(ctx, &r2, &res);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        ASSERT((int)rrc >= -2000 && (int)rrc <= 2000);
        if (res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
        }
        res = NULL;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        g_mock_response_free_fail = 1;
        rrc = http_libuv_send(ctx, &r2, &res);
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

      /* send_multi: NULL future, sync done error, defer-once, slot error +
       * response_free_fail, loop ops 1..5 */
      {
        struct HttpFuture *bad_futs[2];
        bad_futs[0] = NULL;
        bad_futs[1] = &f2;
        r1.url = (char *)"http://127.0.0.1:80/m1";
        r2.url = (char *)"http://127.0.0.1:80/m2";
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                  http_libuv_send_multi(ctx, NULL, &multi, bad_futs));
      }

      g_mock_libuv_addrinfo_fail = 2;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      hrc0 = http_libuv_send_multi(ctx, NULL, &multi, futs);
      g_mock_libuv_addrinfo_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_HOSTUNREACH, hrc0);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));

      g_mock_libuv_context_init_fail = 6;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_libuv_send_multi(ctx, NULL, &multi, futs));
      g_mock_libuv_context_init_fail = 0;
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      /* Slot error with sl->res != NULL (in_hdrs OOM or chunked-bad) +
       * response_free_fail */
      r1.url = (char *)"http://127.0.0.1:80/chunked-bad1";
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      hrc0 = http_libuv_send_multi(ctx, NULL, &multi, futs);
      ASSERT((int)hrc0 >= -2000 && (int)hrc0 <= 2000);
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_response_free_fail = 1;
      hrc0 = http_libuv_send_multi(ctx, NULL, &multi, futs);
      g_mock_response_free_fail = 0;
      ASSERT((int)hrc0 >= -2000 && (int)hrc0 <= 2000);
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }
      r1.url = (char *)"http://127.0.0.1:80/m1";

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(1, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_libuv_send_multi(ctx, NULL, &multi, futs));
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(2, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_libuv_send_multi(ctx, loop, &multi, futs));
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_libuv_send_multi(ctx, loop, &multi, futs));
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(4, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_libuv_send_multi(ctx, NULL, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));

      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(5, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_libuv_send_multi(ctx, NULL, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));

      /* Helper steps 1..16 and response_free_fail countdown 1..10 */
      for (k = 1; k <= 16; ++k) {
        c_abstract_http_error_t src_step;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_libuv_helper_step(k));
        src_step = c_abstract_http_test_libuv_helpers();
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_libuv_helper_step(0));
        ASSERT((int)src_step >= -2000 && (int)src_step <= 2000);
        if (k >= 14) {
          g_mock_response_free_fail = 1;
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_libuv_helper_step(k));
          src_step = c_abstract_http_test_libuv_helpers();
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                    c_abstract_http_test_set_libuv_helper_step(0));
          g_mock_response_free_fail = 0;
          ASSERT((int)src_step >= -2000 && (int)src_step <= 2000);
        }
      }
      for (k = 1; k <= 10; ++k) {
        g_mock_response_free_fail = k;
        hrc0 = c_abstract_http_test_libuv_helpers();
        g_mock_response_free_fail = 0;
        ASSERT((int)hrc0 >= -2000 && (int)hrc0 <= 2000);
      }

      for (k = 0; k < 25; ++k) {
        c_abstract_http_error_t hrc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        hrc = c_abstract_http_test_libuv_helpers();
        g_mock_alloc_fail = 0;
        ASSERT((int)hrc >= -2000 && (int)hrc <= 2000);
      }
      g_mock_alloc_count = 0;
    }
    cfg.ca_path = NULL;
    cfg.client_cert_path = NULL;
    cfg.client_key_path = NULL;
    cfg.ca_buffer = NULL;
    cfg.ca_buffer_len = 0;
    cfg.cert_pin_sha256 = NULL;
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  r1.url = NULL;
  r2.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

TEST test_libuv_cookies(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const char *cval = NULL;
  char big_cookie[4200];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));

  memset(big_cookie, 'C', sizeof(big_cookie) - 1);
  big_cookie[sizeof(big_cookie) - 1] = '\0';
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_cookie_jar_set(&jar, "uv_big", big_cookie));

  cfg.cookie_jar = &jar;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"http://127.0.0.1:80/set-cookie";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_cookie_jar_get(&jar, "uv_sid", &cval));
  ASSERT_STR_EQ("uv77", cval);
#else
  (void)cval;
#endif

  /* Explicit Cookie header skips jar header injection */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Cookie", "manual=1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int step;
    struct HttpRequest r_oom;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r_oom));
    r_oom.url = (char *)"http://127.0.0.1:80/set-cookie";
    for (step = 0; step < 28; ++step) {
      c_abstract_http_error_t orc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_libuv_send(ctx, &r_oom, &res);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
    }
    r_oom.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r_oom));
  }
#endif

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)
/** @brief Verify SOCKS5 proxy tunneling and HTTP/1.1 keep-alive pooling */
TEST test_libuv_socks5_and_keep_alive(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct libuv_TestChunkState chunk_st;
  char long_url[320];
  char long_user[260];
  static const unsigned char s5_ipv6_ok[] = {
      0x05, 0x00, 0x05, 0x00, 0x00, 0x04, 0, 0, 0, 0, 0, 0,
      0,    0,    0,    0,    0,    0,    0, 0, 0, 1, 0, 80};
  static const unsigned char s5_dom_ok[] = {
      0x05, 0x00, 0x05, 0x00, 0x00, 0x03, 0x04, 't', 'e', 's', 't', 0, 80};
  static const unsigned char s5_bad_ver[] = {0x04, 0x00};
  static const unsigned char s5_no_method[] = {0x05, 0xFF};
  static const unsigned char s5_auth_deny[] = {0x05, 0x02, 0x01, 0x01};
  static const unsigned char s5_rep_bad_ver[] = {0x05, 0x00, 0x04,
                                                 0x00, 0x00, 0x01};
  static const unsigned char s5_rep_acces[] = {0x05, 0x00, 0x05,
                                               0x02, 0x00, 0x01};
  static const unsigned char s5_rep_net[] = {0x05, 0x00, 0x05,
                                             0x03, 0x00, 0x01};
  static const unsigned char s5_rep_host[] = {0x05, 0x00, 0x05,
                                              0x04, 0x00, 0x01};
  static const unsigned char s5_rep_refused[] = {0x05, 0x00, 0x05,
                                                 0x05, 0x00, 0x01};
  static const unsigned char s5_rep_gen[] = {0x05, 0x00, 0x05,
                                             0x01, 0x00, 0x01};
  static const unsigned char s5_rep_atyp[] = {0x05, 0x00, 0x05,
                                              0x00, 0x00, 0x09};
  static const unsigned char s5_short1[] = {0x05};
  static const unsigned char s5_auth_short[] = {0x05, 0x02, 0x01};
  static const unsigned char s5_conn_short3[] = {0x05, 0x00, 0x05, 0x00, 0x00};
  static const unsigned char s5_dom_short4[] = {0x05, 0x00, 0x05,
                                                0x00, 0x00, 0x03};
  static const unsigned char s5_dom_short_body[] = {0x05, 0x00, 0x05, 0x00,
                                                    0x00, 0x03, 0x08, 'a'};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* 1. Proxy scheme validation & invalid proxy URLs */
  cfg.proxy_url = (char *)"ftp://127.0.0.1:21";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_libuv_config_apply(ctx, &cfg));

  cfg.proxy_url = (char *)"socks5://:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  req.url = (char *)"http://127.0.0.1:8080/ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  cfg.proxy_url = (char *)"socks5://127.0.0.1:0";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  cfg.proxy_url = (char *)"socks5://127.0.0.1:70000";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  /* HTTP proxy absolute-URI target & trailing slash */
  cfg.proxy_url = (char *)"http://127.0.0.1/";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* 2. SOCKS5 no-auth IPv4, domain, socks5h://, IPv6/domain BND.ADDR, auth */
  cfg.proxy_url = (char *)"socks5://127.0.0.1:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Invalid IPv4 octets fall back to domain ATYP=0x03 */
  req.url = (char *)"http://999.1.1.1:8080/ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127..0.1:8080/ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127.0.0.1.5:8080/ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127.0.0:8080/ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* socks5h:// remote DNS + IPv6 and domain BND.ADDR replies */
  cfg.proxy_url = (char *)"socks5h://127.0.0.1";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  req.url = (char *)"http://example.local:8080/ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_ipv6_ok, sizeof(s5_ipv6_ok), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_dom_ok, sizeof(s5_dom_ok), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Overlong domain > 255 chars fails CONNECT packet build */
  memcpy(long_url, "http://", 7);
  memset(long_url + 7, 'x', 260);
  memcpy(long_url + 267, "/ok", 4);
  req.url = long_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(NULL, 0, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  req.url = (char *)"http://127.0.0.1:8080/ok";

  /* RFC 1929 username/password auth (with password & NULL password) */
  cfg.proxy_username = (char *)"alice";
  cfg.proxy_password = (char *)"secret";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Invalid empty or >255 username */
  cfg.proxy_username = (char *)"";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  memset(long_user, 'u', 256);
  long_user[256] = '\0';
  cfg.proxy_username = long_user;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  /* Auth denied & overlong domain after auth */
  cfg.proxy_username = (char *)"alice";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_auth_deny, sizeof(s5_auth_deny), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(NULL, 0, 0));
  req.url = long_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));
  req.url = (char *)"http://127.0.0.1:8080/ok";

  /* Partial auth + EOF */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_auth_short, sizeof(s5_auth_short), 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  /* SOCKS5 protocol/REP errors & partial/EOF replies */
  cfg.proxy_username = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_bad_ver, sizeof(s5_bad_ver), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_no_method, sizeof(s5_no_method), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_rep_bad_ver, sizeof(s5_rep_bad_ver), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_rep_acces, sizeof(s5_rep_acces), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_rep_net, sizeof(s5_rep_net), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_HOSTUNREACH, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_rep_host, sizeof(s5_rep_host), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_HOSTUNREACH, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_rep_refused, sizeof(s5_rep_refused), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CONNREFUSED, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_rep_gen, sizeof(s5_rep_gen), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_rep_atyp, sizeof(s5_rep_atyp), 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_libuv_socks_reply(
                                         s5_short1, sizeof(s5_short1), 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_conn_short3, sizeof(s5_conn_short3), 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_dom_short4, sizeof(s5_dom_short4), 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(
                s5_dom_short_body, sizeof(s5_dom_short_body), 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  /* Coalesced trailing byte after SOCKS5 reply + write_fail=2 */
  {
    static const unsigned char s5_coalesced[] = {
        0x05, 0x00, 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0, 80, 'H'};
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_libuv_socks_reply(
                  s5_coalesced, sizeof(s5_coalesced), 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_socks_reply(NULL, 0, 0));

  g_mock_libuv_write_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_write_fail = 0;

  /* 3. HTTP/1.1 Keep-Alive connection reuse, LRU/idle eviction, stale retry */
  cfg.proxy_url = NULL;
  cfg.keep_alive_enabled = 1;
  cfg.keep_alive_idle_ms = 500;
  cfg.max_idle_conns = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));

  req.url = (char *)"http://Example.Local:8080/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_STR_EQ("KA", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Case-insensitive host match reuses pooled socket even when connect fails */
  g_mock_libuv_connect_fail = 1;
  req.url = (char *)"http://EXAMPLE.local:8080/ka-split";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_STR_EQ("ABCD", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Streaming on_chunk + 204 + chunked over reused keep-alive connection */
  memset(&chunk_st, 0, sizeof(chunk_st));
  req.on_chunk = libuv_mock_chunk_cb;
  req.on_chunk_user_data = &chunk_st;
  req.url = (char *)"http://example.local:8080/ka-split";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(4U, chunk_st.total_bytes);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.on_chunk = NULL;

  req.url = (char *)"http://example.local:8080/ka-204";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(204, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://example.local:8080/chunked-empty";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://example.local:8080/ka-partial";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_STR_EQ("PK", res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_libuv_connect_fail = 0;

  /* 5-byte non-terminated chunked body & chunked-huge OOM in
   * uv_complete_keep_alive_read */
  req.url = (char *)"http://example.local:8081/chunked-bad4";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  {
    int cs;
    req.url = (char *)"http://example.local:8081/chunked-huge";
    for (cs = 0; cs < 20; ++cs) {
      c_abstract_http_error_t crc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = cs;
      crc = http_libuv_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (crc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
    }
  }

  /* Malformed chunked triggers error */
  req.url = (char *)"http://example.local:8081/chunked-bad1";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  /* Park 2nd origin, checkout 1st (non-tail memmove), then proxy mismatch + LRU
   */
  req.url = (char *)"http://other.local:8081/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://example.local:8080/ka-ok";
  g_mock_libuv_connect_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_libuv_connect_fail = 0;

  cfg.proxy_url = (char *)"http://127.0.0.1:8080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Reuse with matching proxy_url, then miss with different proxy_url -> LRU */
  g_mock_libuv_connect_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_libuv_connect_fail = 0;

  cfg.proxy_url = (char *)"http://127.0.0.1:8081";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Idle eviction via clock jump */
  cfg.proxy_url = NULL;
  cfg.keep_alive_idle_ms = 0; /* default 30000ms branch */
  cfg.max_idle_conns = 0;     /* default 8 branch */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  cfg.keep_alive_idle_ms = 500;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  g_mock_time_jump = 1;
  g_mock_time_jump_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_time_jump = 0;

  /* Stale socket recovery: write failure (10 fails once) & /ka-stale EOF */
  g_mock_libuv_write_fail = 10;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://example.local:8080/ka-stale";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  /* Connection: close in response and request */
  req.url = (char *)"http://example.local:8080/ka-close";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Connection", "close"));
  req.url = (char *)"http://example.local:8080/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Keep-alive error paths (uv_loop_init fail, arm fail with/without timer,
   * write_fail=2 on reused, pool_close_fail across checkin/checkout/clear/free,
   * and OOM sweep) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libuv_send(ctx, &req, &res));

  req.url = (char *)"http://example.local:8080/ka-ok";
  g_mock_libuv_context_init_fail = 2; /* uv_loop_init fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_context_init_fail = 3; /* uv_tcp_init fail (!has_socket) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  cfg.timeout_ms = 100;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  g_mock_libuv_context_init_fail = 4; /* uv_timer_init fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libuv_send(ctx, &req, &res));
  g_mock_libuv_context_init_fail = 0;

  /* Park socket, then test write_fail=2 on reused + pool_close_fail on retry */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  g_mock_libuv_write_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));
  g_mock_libuv_write_fail = 0;

  /* Mid-body error on non-reused keep-alive (*res != NULL + response_free_fail
   * + pool_close_fail) */
  req.url = (char *)"http://example.local:8080/chunked-bad1";
  g_mock_response_free_fail = 1;
  ASSERT(http_libuv_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
  g_mock_response_free_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));

  /* Pool close fail on LRU, checkout evict, checkin evict, disable, and
   * context_free */
  cfg.max_idle_conns = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  req.url = (char *)"http://example.local:8080/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  req.url = (char *)"http://other.local:8081/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));

  /* With response_free_fail = 1 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));
  req.url = (char *)"http://example.local:8080/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  g_mock_response_free_fail = 1;
  req.url = (char *)"http://other.local:8081/ka-ok";
  ASSERT(http_libuv_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
  g_mock_response_free_fail = 0;

  /* Checkout eviction fail & checkin eviction fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));
  req.url = (char *)"http://example.local:8080/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  g_mock_time_jump = 1;
  g_mock_time_jump_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_time_jump = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));
  cfg.max_idle_conns = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  req.url = (char *)"http://other.local:8081/ka-ok";
  g_mock_time_jump = 1;
  g_mock_time_jump_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_time_jump = 0;

  /* config_apply disable keep_alive fail & context_free fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  cfg.keep_alive_enabled = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));

  cfg.keep_alive_enabled = 1;
  cfg.proxy_url = (char *)"socks5://127.0.0.1:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));

  /* OOM sweep across SOCKS5 + keep-alive pool checkin */
  {
    struct HttpTransportContext *octx = NULL;
    int step;
    for (step = 0; step < 32; ++step) {
      c_abstract_http_error_t orc;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_init(&octx));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(octx, &cfg));
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_libuv_send(octx, &req, &res);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_context_free(octx));
    }
  }

  /* Park 1 entry on ctx, test clock fail, /ka-close destroy fail, and
   * context_free fail */
  g_mock_time_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_time_fail = 0;

  cfg.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_config_apply(ctx, &cfg));
  req.url = (char *)"http://example.local:8080/ka-close";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_send(ctx, &req, &res));
  g_mock_response_free_fail = 1;
  ASSERT(http_libuv_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
  g_mock_response_free_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));

  req.url = (char *)"http://example.local:8080/ka-ok";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libuv_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_set_libuv_pool_close_fail(0));

  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  cfg.proxy_url = NULL;
  cfg.proxy_username = NULL;
  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libuv_global_cleanup());
  PASS();
}
#endif

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
  RUN_TEST(test_libuv_tls_spki_chunked_redirects_and_multi);
  RUN_TEST(test_libuv_cookies);
#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)
  RUN_TEST(test_libuv_socks5_and_keep_alive);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_LIBUV_H */
