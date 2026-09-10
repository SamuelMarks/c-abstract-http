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
extern enum c_abstract_http_error
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd);
extern enum c_abstract_http_error c_abstract_http_test_libevent_evbuffer(void);
extern enum c_abstract_http_error
c_abstract_http_test_libevent_add_header_edge(void);
#endif

/**
 * @brief Helper to initialize a request for testing.
 *
 * @param[out] req Request to initialize.
 * @param[in] port Server port.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
static enum c_abstract_http_error
setup_libevent_request(struct HttpRequest *req, int port) {
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
  http_libevent_context_free(ctx);
  http_libevent_context_free(NULL);

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
  c_abstract_http_strdup("agent", &_ast_strdup_agent);
  config.user_agent = _ast_strdup_agent;
  c_abstract_http_strdup("http://proxy.local:8080", &_ast_strdup_proxy);
  config.proxy_url = _ast_strdup_proxy;
  c_abstract_http_strdup("admin", &_ast_strdup_user);
  config.proxy_username = _ast_strdup_user;
  c_abstract_http_strdup("secret", &_ast_strdup_pass);
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
  http_config_free(&config);
  http_libevent_context_free(ctx);
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

  http_request_free(&req);
  http_libevent_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_connection_failure(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.timeout_ms = 50;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, setup_libevent_request(&req, 59999));

  rc = http_libevent_send(ctx, &req, &res);
  ASSERT(rc == (enum c_abstract_http_error)ECONNREFUSED ||
         rc == C_ABSTRACT_HTTP_ERR_TIMEOUT ||
         rc == (enum c_abstract_http_error)EHOSTUNREACH ||
         rc == C_ABSTRACT_HTTP_ERR_IO);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_libevent_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  PASS();
}

/** @brief Documented */

static int libevent_mock_chunk_cb(void *user_data, const void *chunk,
                                  size_t chunk_len) {
  struct libevent_TestChunkState *state;
  (void)chunk;
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
  enum c_abstract_http_error rc;

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

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_libevent_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  mock_server_destroy(server);
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
  enum c_abstract_http_error rc;

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
  ASSERT_EQ((enum c_abstract_http_error)ECANCELED, rc);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_libevent_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  mock_server_destroy(server);
  PASS();
}

static int libevent_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
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
  return 0;
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
  enum c_abstract_http_error rc;

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

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_libevent_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_global_cleanup());
  mock_server_destroy(server);
  PASS();
}

/** @brief Documented */
TEST test_libevent_send_direct_body_and_urls(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *_ast_url = NULL;
  enum c_abstract_http_error rc;
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
    c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
    req.url = _ast_url;
    req.method = HTTP_POST;
    c_abstract_http_strdup("test_body", &body_copy);
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
    http_response_free(res);
    free(res);
    res = NULL;
    http_request_free(&req);
  }

  /* Test 2: URL without http:// prefix, slash without colon */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("127.0.0.1/path", &_ast_url);
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Test 3: URL without slash or colon */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("localhost", &_ast_url);
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Test 4: Long host */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup(long_host_url, &_ast_url);
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);

  /* Test 5: Empty response body (len == 0) */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_libevent_empty_body = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  _ast_url = NULL;
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
  req.url = _ast_url;
  rc = http_libevent_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(0, res->body_len);
  http_response_free(res);
  free(res);
  res = NULL;
  http_request_free(&req);
  g_mock_libevent_empty_body = 0;
#endif

  http_libevent_context_free(ctx);
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
  c_abstract_http_strdup("http://127.0.0.1:80/path", &_ast_url);
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
  ASSERT_EQ((enum c_abstract_http_error)ECONNREFUSED,
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
    c_abstract_http_strdup("body_data", &body_copy2);
    req.body = body_copy2;
    req.body_len = 9;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "X-Custom", "Value"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_libevent_send(ctx, &req, &res));
    if (res) {
      http_response_free(res);
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
      http_response_free(res);
      free(res);
      res = NULL;
    }
    req.read_chunk = NULL;
    g_mock_libevent_buf_add_fail = 0;
  }

  /* 10: Null res in dispatch */
  g_mock_libevent_empty_body = 2;
  ASSERT_EQ((enum c_abstract_http_error)ECONNREFUSED,
            http_libevent_send(ctx, &req, &res));
  g_mock_libevent_empty_body = 0;

  http_request_free(&req);
  http_libevent_context_free(ctx);
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

    g_mock_alloc_count = 1;
    g_mock_alloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, setup_libevent_request(&req_test, 80));
    g_mock_alloc_fail = 0;
  }
#endif
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
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_LIBEVENT_H */
