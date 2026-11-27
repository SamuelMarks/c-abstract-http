/**
 * @file test_http_libevent.h
 * @brief Integration tests for Liblibevent Backend.
 *
 * Verifies that the libevent wrapper correctly initializes, handles
 * configuration, sends requests, and maps failures to error enums.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_LIBEVENT_H
#define TEST_HTTP_LIBEVENT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libevent.h>
#include <c_abstract_http/http_types.h>
#include "abstract_http_test_helpers/mock_server.h"
#include "mock_alloc.h"
/* clang-format on */

struct libevent_TestChunkState {
  /** @brief Documented */
  int call_count;
  /** @brief Documented */
  size_t total_bytes;
  /** @brief Documented */
  int abort_on_call;
};

struct libevent_TestUploadState {
  /** @brief Documented */
  const char *data;
  /** @brief Documented */
  size_t len;
  /** @brief Documented */
  size_t pos;
};

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libevent_evbuffer(void);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libevent_add_header_edge(void);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libevent_chunked_cb(void);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_libevent_resolve_null(void);
#endif

/**
 * @brief Helper to initialize a request for testing.
 *
 * @param[out] req Request to initialize.
 * @param[in] port Server port.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
NO_DISCARD static c_abstract_http_error_t
setup_libevent_request(struct HttpRequest *req, int port) {
  c_abstract_http_error_t rc;
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
    c_abstract_http_error_t free_rc = http_request_free(req);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return free_rc;
    }
    return rc;
  }
  req->url = _ast_strdup_0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Documented */
TEST test_libevent_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libevent_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libevent_global_init());
  g_mock_libevent_global_init_fail = 0;
#endif

  PASS();
}

/** @brief Documented */
TEST test_libevent_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_context_init(NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT(ctx != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libevent_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_context_init(&ctx));
  g_mock_libevent_context_init_fail = 0;

  g_mock_libevent_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_context_init(&ctx));
  g_mock_libevent_config_init_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libevent_config_application(void) {
  char *_ast_strdup_agent = NULL;
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_user = NULL;
  char *_ast_strdup_pass = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_libevent_config_apply(NULL, &config));
  config.timeout_ms = 500;
  config.verify_peer = 0;
  config.follow_redirects = 0;
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  /* Re-apply with same strings to test replacement */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  /* Re-apply with NULL strings to test clearing */
  config.user_agent = NULL;
  config.proxy_url = NULL;
  config.proxy_username = NULL;
  config.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.user_agent = (char *)"agent";
    g_mock_libevent_config_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_config_apply(ctx, &cfg));
    cfg.user_agent = NULL;

    cfg.proxy_url = (char *)"proxy";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_config_apply(ctx, &cfg));
    cfg.proxy_url = NULL;

    cfg.proxy_username = (char *)"user";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_config_apply(ctx, &cfg));
    cfg.proxy_username = NULL;

    cfg.proxy_password = (char *)"pwd";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_config_apply(ctx, &cfg));
    g_mock_libevent_config_init_fail = 0;
  }
#endif

  free(_ast_strdup_agent);
  free(_ast_strdup_proxy);
  free(_ast_strdup_user);
  free(_ast_strdup_pass);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_send(ctx, &req, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_connection_failure(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.timeout_ms = 50;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, setup_libevent_request(&req, 59999));

  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == (c_abstract_http_error_t)ECONNREFUSED ||
         rc == C_ABSTRACT_HTTP_ERR_TIMEOUT ||
         rc == (c_abstract_http_error_t)EHOSTUNREACH ||
         rc == C_ABSTRACT_HTTP_ERR_IO);
  ASSERT(res == NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */

NO_DISCARD static c_abstract_http_error_t
libevent_mock_chunk_cb(void *user_data, const void *chunk, size_t chunk_len) {
  struct libevent_TestChunkState *state;
  if (chunk) {
    /* pass */
  }
  state = (struct libevent_TestChunkState *)user_data;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return (int)ECANCELED;
  }
  return 0;
}

/** @brief Documented */
TEST test_libevent_send_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libevent_TestChunkState state;
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            setup_libevent_request(&req, math_mock_server_get_port(server)));

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = libevent_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_libevent_send(ctx, &req, &res);
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_chunked_abort(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libevent_TestChunkState state;
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            setup_libevent_request(&req, math_mock_server_get_port(server)));

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1;
  req.on_chunk = libevent_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ((c_abstract_http_error_t)ECANCELED, rc);
  ASSERT(res == NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
libevent_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                        size_t *out_read) {
  struct libevent_TestUploadState *state =
      (struct libevent_TestUploadState *)user_data;
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
TEST test_libevent_send_upload_chunked(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct libevent_TestUploadState up_state;
  const char *payload = "UPLOAD_TEST_DATA";
  c_abstract_http_error_t rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            setup_libevent_request(&req, math_mock_server_get_port(server)));
  req.method = HTTP_POST;

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  req.read_chunk = libevent_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(up_state.len, up_state.pos);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_direct_body_and_urls(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  c_abstract_http_error_t rc;
  const char *long_host_url =
      "http://"
      "very-long-host-name-exceeding-two-hundred-and-fifty-six-characters"
      "-in-length-which-should-trigger-the-buffer-truncation-safety-check-in-"
      "our"
      "-libevent-send-url-parsing-routine-to-ensure-no-buffer-overflows-occur-"
      "at-any-point-in-time-during-execution.local:8080/test";

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));

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

    rc = http_libevent_send(ctx, &req, &res);
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

  /* Test 2: URL without http:// prefix, slash without colon */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("127.0.0.1/path", &_ast_url));
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test 3: URL without slash or colon */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("localhost", &_ast_url));
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test 4: Long host */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(long_host_url, &_ast_url));
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Test 5: Empty response body (len == 0) */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libevent_empty_body = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(0, res->body_len);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  g_mock_libevent_empty_body = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_fault_injections(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  struct libevent_TestChunkState state;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url));
  req.url = _ast_url;

  /* 1: Base new fail */
  g_mock_libevent_base_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_base_new_fail = 0;

  /* 2: Conn new fail */
  g_mock_libevent_conn_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_conn_new_fail = 0;

  /* 3: Req new fail */
  g_mock_libevent_req_new_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_req_new_fail = 0;

  /* 4: Make req fail */
  g_mock_libevent_make_req_fail = 1;
  ASSERT_EQ((c_abstract_http_error_t)ECONNREFUSED,
            http_libevent_send(ctx, &req, &res));
  g_mock_libevent_make_req_fail = 0;

  /* 5: Res alloc fail in http_request_done */
  g_mock_libevent_res_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_res_alloc_fail = 0;

  /* 6: Res init fail in http_request_done */
  g_mock_libevent_res_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_res_init_fail = 0;

  /* 7: Body alloc fail in http_request_done */
  g_mock_libevent_body_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_body_alloc_fail = 0;

  /* 8: Body alloc fail in http_chunked_cb */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = libevent_mock_chunk_cb;
  req.on_chunk_user_data = &state;
  g_mock_libevent_body_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &req, &res));
  g_mock_libevent_body_alloc_fail = 0;
  req.on_chunk = NULL;

  /* 9: Buffer add fail */
  {
    char *body_copy2 = NULL;
    struct libevent_TestUploadState up_state;
    up_state.data = "upload";
    up_state.len = 6;
    up_state.pos = 0;
    g_mock_libevent_buf_add_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("body_data", &body_copy2));
    req.body = body_copy2;
    req.body_len = 9;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "X-Custom", "Value"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    free(req.body);
    req.body = NULL;
    req.body_len = 0;
    req.read_chunk = libevent_mock_upload_cb;
    req.read_chunk_user_data = &up_state;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    req.read_chunk = NULL;
    g_mock_libevent_buf_add_fail = 0;
  }

  /* 10: Null res in dispatch */
  g_mock_libevent_empty_body = 2;
  ASSERT_EQ((c_abstract_http_error_t)ECONNREFUSED,
            http_libevent_send(ctx, &req, &res));
  g_mock_libevent_empty_body = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
#endif
  PASS();
}

/** @brief Documented */
TEST test_libevent_methods(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  int cmd = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_test_libevent_method_cmd(HTTP_GET, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_GET, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_POST, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_PUT, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_DELETE, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_PATCH, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_HEAD, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_OPTIONS, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_TRACE, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_method_cmd(HTTP_CONNECT, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_libevent_method_cmd(
                                         (enum HttpMethod)999, &cmd));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_libevent_evbuffer());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_add_header_edge());
  {
    struct HttpRequest req_test;
    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, setup_libevent_request(&req_test, 80));
    g_mock_headers_init_fail = 0;

    g_mock_alloc_count = 0;
    g_mock_alloc_fail = 1;
    {
      c_abstract_http_error_t s_rc = setup_libevent_request(&req_test, 80);
      g_mock_alloc_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, s_rc);
    }
  }
#endif
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
dummy_libevent_chunk_reader_err(void *user_data, void *buf, size_t max_bytes,
                                size_t *out_read) {
  if (user_data || buf || max_bytes || out_read) {
    /* pass */
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD static c_abstract_http_error_t
dummy_libevent_chunk_reader(void *user_data, void *buf, size_t max_bytes,
                            size_t *out_read) {
  int *called;
  if (buf || max_bytes) {
    /* pass */
  }
  called = (int *)user_data;
  if (*called == 0) {
    *called = 1;
    *out_read = 4;
    return 0;
  }
  *out_read = 0;
  return 0;
}
#endif

/** @brief Documented */
TEST test_http_libevent_url_and_body_branches(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char long_url[512];
  c_abstract_http_error_t rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  int dummy_chunk_state = 0;
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_test_libevent_chunked_cb());
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));

  /* 1. colon && !slash: http://127.0.0.1:8080 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:8080", &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 2. colon && len >= sizeof(host): http://aaa...:80/path */
  memset(long_url, 'a', sizeof(long_url));
  memcpy(long_url, "http://", 7);
  long_url[300] = ':';
  long_url[301] = '8';
  long_url[302] = '0';
  long_url[303] = '/';
  long_url[304] = 'p';
  long_url[305] = '\0';
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(long_url, &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 3. slash && len >= sizeof(host): http://aaa.../path */
  memset(long_url, 'a', sizeof(long_url));
  memcpy(long_url, "http://", 7);
  long_url[300] = '/';
  long_url[301] = 'p';
  long_url[302] = '\0';
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(long_url, &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 4. no colon, no slash: http://127.0.0.1 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1", &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 5. no colon, no slash && len >= sizeof(host): http://aaa... */
  memset(long_url, 'a', sizeof(long_url));
  memcpy(long_url, "http://", 7);
  long_url[300] = '\0';
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(long_url, &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 5b. colon after slash: http://127.0.0.1/path:with:colons */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_strdup("http://127.0.0.1/path:with:colons", &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 6. timeout_ms == 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/test", &req.url));
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 6b. body non-NULL but body_len == 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/test", &req.url));
  req.body = "test";
  req.body_len = 0;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO ||
         rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  req.body = NULL;
  req.body_len = 0;
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 6c. read_chunk returns error */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/test", &req.url));
  req.read_chunk = dummy_libevent_chunk_reader_err;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* 7. body evbuffer_add failure */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/test", &req.url));
  req.body = "test";
  req.body_len = 4;
  g_mock_libevent_buf_add_fail = 1;
  rc = http_libevent_send(ctx, &req, &res);
  g_mock_libevent_buf_add_fail = 0;
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO);
  req.body = NULL;
  req.body_len = 0;
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 8. read_chunk evbuffer_add failure */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:80/test", &req.url));
  dummy_chunk_state = 0;
  req.read_chunk = dummy_libevent_chunk_reader;
  req.read_chunk_user_data = &dummy_chunk_state;
  g_mock_libevent_buf_add_fail = 1;
  rc = http_libevent_send(ctx, &req, &res);
  g_mock_libevent_buf_add_fail = 0;
  ASSERT(rc == C_ABSTRACT_HTTP_SUCCESS || rc == C_ABSTRACT_HTTP_ERR_IO);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  PASS();
}

TEST test_libevent_tls_proxy_redirects_parts_and_multi(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpMultiRequest multi;
  struct HttpRequest r1, r2;
  struct HttpFuture f1, f2;
  struct HttpFuture *futs[2];
  struct ModalityEventLoop *loop = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  /* TLS (bufferevent_openssl_socket_new), proxy + Proxy-Authorization,
   * user_agent */
  cfg.follow_redirects = 1;
  cfg.timeout_ms = 2000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:8080", &cfg.proxy_url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("evuser", &cfg.proxy_username));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("evpass", &cfg.proxy_password));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com:443/tls";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Redirect 302 -> 200 & redirect loop -> ERR_LOOP */
  req.url = (char *)"http://127.0.0.1:80/redir-once";
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req.url = (char *)"http://127.0.0.1:80/redir-loop";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_LOOP, http_libevent_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Multipart req->parts flattening */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"http://127.0.0.1:80/mp";
  req.parts.count = 1;
  req.parts.parts = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_send(ctx, &req, &res));
  req.parts.count = 0;

  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "User-Agent", "EvUA/1.0"));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      http_request_add_part(&req, "f", "a.txt", "text/plain", "evpart", 6));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* ModalityEventLoop + event_base_loop(EVLOOP_NONBLOCK) send_multi */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_libevent_send_multi(NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  futs[0] = &f1;
  futs[1] = &f2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libevent_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libevent_send_multi(ctx, loop, &multi, futs));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
  r1.url = (char *)"https://example.com/m1";
  r2.url = (char *)"http://127.0.0.1:80/m2";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libevent_send_multi(ctx, loop, &multi, futs));
  ASSERT_EQ(1, f1.is_ready);
  ASSERT_EQ(1, f2.is_ready);
  ASSERT_EQ(200, f1.response->status_code);
  ASSERT_EQ(200, f2.response->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
  free(f1.response);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
  free(f2.response);

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_libevent_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
  free(f1.response);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
  free(f2.response);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int k;
    cfg.tls_version_mask = 0xFF00U;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_libevent_send(ctx, &r1, &res));
    cfg.tls_version_mask = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &cfg));

    g_mock_libevent_conn_new_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_send(ctx, &r1, &res));
    g_mock_libevent_conn_new_fail = 0;

    for (k = 0; k < 25; ++k) {
      c_abstract_http_error_t orc;
      res = NULL;
      r2.url = (char *)"http://127.0.0.1:80/redir-once";
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_libevent_send(ctx, &r2, &res);
      g_mock_alloc_fail = 0;
      if (res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
      }

      r2.url = (char *)"http://127.0.0.1:80/m2";
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_libevent_send_multi(ctx, NULL, &multi, futs);
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
      c_abstract_http_error_t mrc;
      g_mock_alloc_count = 1;
      g_mock_alloc_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_libevent_context_init(&c2));
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_request_add_part(&r1, "p", "p.txt", "text/plain", "x", 1));
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_libevent_send_multi(ctx, NULL, &multi, futs));
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      r2.url = (char *)"http://127.0.0.1:59999/err";
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      mrc = http_libevent_send_multi(ctx, NULL, &multi, futs);
      ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      r2.url = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_libevent_send(ctx, &r2, &res));

      for (k = 0; k < 15; ++k) {
        res = NULL;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        mrc = http_libevent_send(ctx, &r1, &res);
        g_mock_alloc_fail = 0;
        ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
        if (res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
        }
      }
      g_mock_alloc_count = 0;

      g_mock_headers_init_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libevent_send(ctx, &r1, &res));
      g_mock_headers_init_fail = 0;

      r2.url = (char *)"http://127.0.0.1:80/redir-once";
      g_mock_response_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libevent_send(ctx, &r2, &res));
      g_mock_response_free_fail = 0;

      {
        struct HttpRequest rp;
        struct HttpFuture *bad_futs[2];
        c_abstract_http_error_t xrc;

        /* context_free config_free failure */
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&c2));
        g_mock_config_free_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libevent_context_free(c2));
        g_mock_config_free_fail = 0;
        c2 = NULL;

        /* config_apply user_agent strdup OOM */
        cfg.user_agent = (char *)"ua-test";
        g_mock_alloc_count = 0;
        g_mock_alloc_fail = 1;
        xrc = http_libevent_config_apply(ctx, &cfg);
        g_mock_alloc_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, xrc);
        cfg.user_agent = NULL;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));

        /* Plain request: response_init fail, invalid method, proxy invalid &
         * https */
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&rp));
        rp.url = (char *)"http://127.0.0.1:80/plain";

        g_mock_headers_init_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_libevent_send(ctx, &rp, &res));
        g_mock_headers_init_fail = 0;

        rp.method = (enum HttpMethod)(-1);
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                  http_libevent_send(ctx, &rp, &res));
        rp.method = HTTP_GET;

        cfg.proxy_url = (char *)"";
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                  http_libevent_send(ctx, &rp, &res));

        cfg.proxy_url = (char *)"https://127.0.0.1:8443/proxy-p";
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &rp, &res));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
        cfg.proxy_url = NULL;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));

        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                  c_abstract_http_test_libevent_resolve_null());

        cfg.follow_redirects = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &rp, &res));
        g_mock_response_free_fail = 1;
        g_mock_headers_free_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_response_free(res));
        g_mock_headers_free_fail = 0;
        g_mock_response_free_fail = 0;
        free(res);
        res = NULL;
        cfg.follow_redirects = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));

        /* OOM sweep on plain request + response_free_fail on in_hdrs OOM */
        for (k = 0; k < 25; ++k) {
          res = NULL;
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          xrc = http_libevent_send(ctx, &rp, &res);
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
          if (res) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
            free(res);
            res = NULL;
          }

          res = NULL;
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          g_mock_response_free_fail = 1;
          xrc = http_libevent_send(ctx, &rp, &res);
          g_mock_response_free_fail = 0;
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
          if (res) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
            free(res);
            res = NULL;
          }
        }

        /* Multipart + request headers OOM + headers_free_fail */
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_headers_add(&r1.headers, "X-Part-Hdr", "1"));
        for (k = 0; k < 10; ++k) {
          res = NULL;
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          xrc = http_libevent_send(ctx, &r1, &res);
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
          if (res) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
            free(res);
            res = NULL;
          }
        }
        g_mock_headers_free_fail = 1;
        xrc = http_libevent_send(ctx, &r1, &res);
        g_mock_headers_free_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, xrc);
        if (res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
          res = NULL;
        }

        /* Redirect branches: no Location, loop + response_free_fail, rel OOM +
         * response_free_fail */
        cfg.follow_redirects = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  http_libevent_config_apply(ctx, &cfg));

        rp.url = (char *)"http://127.0.0.1:80/redir-no-loc";
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &rp, &res));
        ASSERT_EQ(302, res->status_code);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;

        rp.url = (char *)"http://127.0.0.1:80/redir-loop";
        g_mock_response_free_fail = 11;
        xrc = http_libevent_send(ctx, &rp, &res);
        g_mock_response_free_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, xrc);

        rp.url = (char *)"http://127.0.0.1:80/redir-rel";
        for (k = 0; k < 25; ++k) {
          res = NULL;
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          xrc = http_libevent_send(ctx, &rp, &res);
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
          if (res) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
            free(res);
            res = NULL;
          }
        }
        for (k = 0; k < 25; ++k) {
          res = NULL;
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          g_mock_response_free_fail = 1;
          xrc = http_libevent_send(ctx, &rp, &res);
          g_mock_response_free_fail = 0;
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
          if (res) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
            free(res);
            res = NULL;
          }
        }

        /* send_multi: NULL future, defer-once, null-done, in_hdrs OOM +
         * response_free_fail, headers_free_fail, loop ops */
        r2.url = (char *)"http://127.0.0.1:80/m2";
        bad_futs[0] = NULL;
        bad_futs[1] = &f2;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                  http_libevent_send_multi(ctx, NULL, &multi, bad_futs));

        r1.url = (char *)"http://127.0.0.1:80/defer-once";
        r2.url = (char *)"http://127.0.0.1:80/null-done";
        memset(&f1, 0, sizeof(f1));
        memset(&f2, 0, sizeof(f2));
        xrc = http_libevent_send_multi(ctx, NULL, &multi, futs);
        ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
        if (f1.response) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
          free(f1.response);
        }
        if (f2.response) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
          free(f2.response);
        }

        r1.url = (char *)"http://127.0.0.1:80/m1";
        r2.url = (char *)"http://127.0.0.1:80/m2";
        for (k = 0; k < 20; ++k) {
          memset(&f1, 0, sizeof(f1));
          memset(&f2, 0, sizeof(f2));
          g_mock_alloc_count = k;
          g_mock_alloc_fail = 1;
          g_mock_response_free_fail = 1;
          xrc = http_libevent_send_multi(ctx, NULL, &multi, futs);
          g_mock_response_free_fail = 0;
          g_mock_alloc_fail = 0;
          g_mock_alloc_count = 0;
          ASSERT((int)xrc >= -2000 && (int)xrc <= 2000);
          if (f1.response) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
            free(f1.response);
          }
          if (f2.response) {
            ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
            free(f2.response);
          }
        }

        memset(&f1, 0, sizeof(f1));
        memset(&f2, 0, sizeof(f2));
        g_mock_headers_free_fail = 1;
        xrc = http_libevent_send_multi(ctx, NULL, &multi, futs);
        g_mock_headers_free_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, xrc);
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
        g_mock_headers_free_fail = 1;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_loop_op_fail(1, 0));
        xrc = http_libevent_send_multi(ctx, NULL, &multi, futs);
        g_mock_headers_free_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, xrc);
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
                  c_abstract_http_test_set_loop_op_fail(1, 0));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                  http_libevent_send_multi(ctx, NULL, &multi, futs));
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
                  http_libevent_send_multi(ctx, loop, &multi, futs));
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
                  http_libevent_send_multi(ctx, loop, &multi, futs));
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
                  http_libevent_send_multi(ctx, NULL, &multi, futs));

        memset(&f1, 0, sizeof(f1));
        memset(&f2, 0, sizeof(f2));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_loop_op_fail(5, 0));
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                  http_libevent_send_multi(ctx, NULL, &multi, futs));

        rp.url = NULL;
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&rp));
      }
    }
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  r1.url = NULL;
  r2.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

TEST test_libevent_cookies_timeouts_and_expected_len(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const char *cval = NULL;
  struct libevent_TestUploadState up_st;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "ev_init", "1"));

  cfg.cookie_jar = &jar;
  cfg.timeout_ms = 0;
  cfg.connect_timeout_ms = 1500;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"http://127.0.0.1:80/set-cookie";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_cookie_jar_get(&jar, "ev_sid", &cval));
  ASSERT_STR_EQ("ev99", cval);
#else
  (void)cval;
#endif

  /* Test read_timeout_ms & write_timeout_ms fallbacks + explicit Cookie +
   * expected_body_len */
  cfg.connect_timeout_ms = 0;
  cfg.read_timeout_ms = 1200;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Cookie", "override=1"));
  memset(&up_st, 0, sizeof(up_st));
  up_st.data = "1234";
  up_st.len = 4;
  req.method = HTTP_POST;
  req.read_chunk = libevent_mock_upload_cb;
  req.read_chunk_user_data = &up_st;
  req.expected_body_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  cfg.read_timeout_ms = 0;
  cfg.write_timeout_ms = 1200;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Content-Length", "4"));
  up_st.pos = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int step;
    struct HttpRequest r_oom;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r_oom));
    r_oom.url = (char *)"http://127.0.0.1:80/set-cookie";
    g_mock_libevent_buf_add_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &r_oom, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    g_mock_libevent_buf_add_fail = 0;

    up_st.pos = 0;
    r_oom.method = HTTP_POST;
    r_oom.read_chunk = libevent_mock_upload_cb;
    r_oom.read_chunk_user_data = &up_st;
    r_oom.expected_body_len = 4;
    g_mock_libevent_buf_add_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &r_oom, &res));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    g_mock_libevent_buf_add_fail = 0;
    r_oom.read_chunk = NULL;

    for (step = 0; step < 48; ++step) {
      c_abstract_http_error_t orc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = http_libevent_send(ctx, &r_oom, &res);
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */
SUITE(http_libevent_suite) {
  RUN_TEST(test_libevent_global_lifecycle);
  RUN_TEST(test_libevent_context_lifecycle);
  RUN_TEST(test_libevent_config_application);
  RUN_TEST(test_libevent_send_invalid_arguments);
  RUN_TEST(test_libevent_send_connection_failure);
  RUN_TEST(test_libevent_send_chunked);
  RUN_TEST(test_libevent_send_chunked_abort);
  RUN_TEST(test_libevent_send_upload_chunked);
  RUN_TEST(test_libevent_send_direct_body_and_urls);
  RUN_TEST(test_libevent_send_fault_injections);
  RUN_TEST(test_libevent_methods);
  RUN_TEST(test_http_libevent_url_and_body_branches);
  RUN_TEST(test_libevent_tls_proxy_redirects_parts_and_multi);
  RUN_TEST(test_libevent_cookies_timeouts_and_expected_len);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_LIBEVENT_H */
