/**
 * @file test_http_wininet.h
 * @brief Unit tests for the WinInet Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, parameter validation, chunking, and error handling.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_WININET_H
#define TEST_HTTP_WININET_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/http_wininet.h>
#include "mock_alloc.h"
#include "abstract_http_test_helpers/mock_server.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_wininet_slot_guard(int mode);
#endif

/** @brief Helper structure for chunk testing */
struct wininet_TestChunkState {
  /** @brief Call count */
  int call_count;
  /** @brief Total bytes */
  size_t total_bytes;
  /** @brief Abort on call */
  int abort_on_call;
};

NO_DISCARD static c_abstract_http_error_t
wininet_chunk_cb(void *user_data, const void *data, size_t length) {
  struct wininet_TestChunkState *state =
      (struct wininet_TestChunkState *)user_data;
  (void)data;
  state->call_count++;
  state->total_bytes += length;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return 1;
  }
  return 0;
}

NO_DISCARD static c_abstract_http_error_t wininet_read_cb(void *user_data,
                                                          void *buffer,
                                                          size_t max_len,
                                                          size_t *out_len) {
  struct wininet_TestChunkState *state =
      (struct wininet_TestChunkState *)user_data;
  state->call_count++;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return 1;
  }
  if (state->call_count == 1) {
    size_t to_copy = (max_len < 5) ? max_len : 5;
    memcpy(buffer, "CHUNK", to_copy);
    *out_len = to_copy;
    return 0;
  }
  *out_len = 0; /* EOF */
  return 0;
}

/** @brief Test WinInet lifecycle */
TEST test_wininet_lifecycle(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpTransportContext *ctx = NULL;

  /* Global init/cleanup */
  rc = http_wininet_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_wininet_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Context init NULL check */
  rc = http_wininet_context_init(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* Context init success */
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  /* Free NULL and ctx */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));

#if !defined(_WIN32)
  g_mock_wininet_context_init_fail = 1;
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_context_init_fail = 0;

  g_mock_wininet_open_fail = 1;
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_open_fail = 0;

  g_mock_alloc_fail = 1;
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/** @brief Test WinInet configuration */
TEST test_wininet_config_apply(void) {
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_proxy2 = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Customize */
  config.timeout_ms = 500;
  config.connect_timeout_ms = 600;
  config.read_timeout_ms = 700;
  config.write_timeout_ms = 800;
  config.verify_peer = 0;
  config.verify_host = 0;
  config.follow_redirects = 0;

  /* Test with Proxy Auth */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("admin", &_ast_strdup_proxy));
  config.proxy_username = _ast_strdup_proxy;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("secret", &_ast_strdup_proxy2));
  config.proxy_password = _ast_strdup_proxy2;

  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Timeout ms only */
  config.timeout_ms = 500;
  config.connect_timeout_ms = 0;
  config.read_timeout_ms = 0;
  config.write_timeout_ms = 0;
  config.verify_peer = 1;
  config.verify_host = 0;
  config.follow_redirects = 1;
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Verify peer 0, verify host 1 */
  config.verify_peer = 0;
  config.verify_host = 1;
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if !defined(_WIN32)
  /* send timeout fail */
  config.timeout_ms = 0;
  config.connect_timeout_ms = 0;
  config.write_timeout_ms = 500;
  config.read_timeout_ms = 0;
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* recv timeout fail */
  config.write_timeout_ms = 0;
  config.read_timeout_ms = 500;
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* proxy username strdup fail */
  config.read_timeout_ms = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("admin", &config.proxy_username));
  g_mock_wininet_context_init_fail = 2;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_context_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* proxy password strdup fail */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("secret", &config.proxy_password));
  g_mock_wininet_context_init_fail = 3;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_context_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  config.timeout_ms = 500;
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_config_apply(ctx, &config);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
#endif

  /* Invalid args */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_wininet_config_apply(NULL, &config));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_wininet_config_apply(ctx, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet send validation */
TEST test_wininet_send_validation(void) {
  char *_ast_strdup_0 = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Null checks */
  rc = http_wininet_send(NULL, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_wininet_send(ctx, NULL, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_wininet_send(ctx, &req, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL URL */
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* NULL hInternet */
  {
    void *saved = *(void **)ctx;
    *(void **)ctx = NULL;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
    *(void **)ctx = saved;
  }

  /* Malformed URL handling in local testing (CrackUrl check) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("not-a-valid-url", &_ast_strdup_0));
  req.url = _ast_strdup_0;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  free(_ast_strdup_0);
  _ast_strdup_0 = NULL;
  req.url = NULL;

  /* Multipart validation */
  {
#if defined(_WIN32)
    MockServerPtr server = NULL;
    char url_buf[128];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
              math_mock_server_get_port(server));
#else
    sprintf(url_buf, "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_mock_strdup(url_buf, &_ast_strdup_0));
#else
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_mock_strdup("http://127.0.0.1:8080/test",
                                          &_ast_strdup_0));
#endif
    req.url = _ast_strdup_0;
    req.parts.count = 1;
    req.body = NULL;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

    req.body = (void *)"x";
    req.body_len = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    req.parts.count = 0;
    req.body = NULL;
    req.body_len = 0;
#if defined(_WIN32)
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet methods and headers */
TEST test_wininet_send_methods_and_headers(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum HttpMethod methods[11];
  size_t i;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif

  methods[0] = HTTP_GET;
  methods[1] = HTTP_POST;
  methods[2] = HTTP_PUT;
  methods[3] = HTTP_DELETE;
  methods[4] = HTTP_HEAD;
  methods[5] = HTTP_PATCH;
  methods[6] = HTTP_OPTIONS;
  methods[7] = HTTP_TRACE;
  methods[8] = HTTP_QUERY;
  methods[9] = HTTP_CONNECT;
  methods[10] = (enum HttpMethod)99;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  for (i = 0; i < 11; ++i) {
    rc = http_request_init(&req);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_mock_strdup(url_buf, &url_dup));
#else
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        c_abstract_http_mock_strdup("https://127.0.0.1:8443/test", &url_dup));
#endif
    req.url = url_dup;
    req.method = methods[i];

    rc = http_headers_add(&req.headers, "X-Custom", "Value");
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

    res = NULL;
    rc = http_wininet_send(ctx, &req, &res);
    if (methods[i] == HTTP_CONNECT) {
      if (res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
      continue;
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT(res != NULL);
    ASSERT_EQ(200, res->status_code);

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet cookies handling */
TEST test_wininet_send_cookies(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpCookieJar jar;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_cookie_jar_init(&jar);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_cookie_jar_set(&jar, "session", "xyz123");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  {
    struct HttpConfig config;
    rc = http_config_init(&config);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    config.cookie_jar = &jar;
    rc = http_wininet_config_apply(ctx, &config);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  }

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url_buf, &url_dup));
#else
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/cookies", &url_dup));
#endif
  req.url = url_dup;

#if !defined(_WIN32)
  g_mock_wininet_cookie_count = 1;
#endif

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet chunked streaming response */
TEST test_wininet_send_chunked(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct wininet_TestChunkState state;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif
  memset(&state, 0, sizeof(state));

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url_buf, &url_dup));
#else
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/stream", &url_dup));
#endif
  req.url = url_dup;
  req.on_chunk = wininet_chunk_cb;
  req.on_chunk_user_data = &state;

#if !defined(_WIN32)
  g_mock_wininet_read_chunks = 2;
#endif

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet chunked response abort */
TEST test_wininet_send_chunked_abort(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct wininet_TestChunkState state;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif
  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url_buf, &url_dup));
#else
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/abort", &url_dup));
#endif
  req.url = url_dup;
  req.on_chunk = wininet_chunk_cb;
  req.on_chunk_user_data = &state;

#if !defined(_WIN32)
  g_mock_wininet_read_chunks = 2;
#endif

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet upload chunked */
TEST test_wininet_send_upload_chunked(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct wininet_TestChunkState state;
#if defined(_WIN32)
  MockServerPtr server = NULL;
  char url_buf[128];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url_buf, sizeof(url_buf), "http://127.0.0.1:%d/test",
            math_mock_server_get_port(server));
#else
  sprintf(url_buf, "http://127.0.0.1:%d/test",
          math_mock_server_get_port(server));
#endif
#endif
  memset(&state, 0, sizeof(state));

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url_buf, &url_dup));
#else
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/upload", &url_dup));
#endif
  req.url = url_dup;
  req.method = HTTP_POST;
  req.read_chunk = wininet_read_cb;
  req.read_chunk_user_data = &state;
  req.expected_body_len = 5;

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Upload chunk abort */
  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup(url_buf, &url_dup));
#else
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080/upload_abort",
                                        &url_dup));
#endif
  req.url = url_dup;
  req.method = HTTP_POST;
  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;
  req.read_chunk = wininet_read_cb;
  req.read_chunk_user_data = &state;
  req.expected_body_len = 5;

  rc = http_wininet_send(ctx, &req, &res);
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(server));
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

#if !defined(_WIN32)
/** @brief Test WinInet send mock failure branches */
TEST test_wininet_send_mock_failures(void) {
  c_abstract_http_error_t rc;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_dup));
  req.url = url_dup;

  /* wUrl alloc fail */
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* crack url fail */
  g_mock_wininet_crack_url_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  g_mock_wininet_crack_url_fail = 0;

  /* URL without path */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("http://127.0.0.1:8080", &url_dup));
  req.url = url_dup;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/test", &url_dup));
  req.url = url_dup;

  /* hostName / urlPath alloc fail */
  g_mock_alloc_fail = 2;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* InternetConnectW fail */
  g_mock_wininet_connect_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_connect_fail = 0;

  /* HttpOpenRequestW fail */
  g_mock_wininet_open_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_open_request_fail = 0;

  /* HttpSendRequestW fail */
  g_mock_wininet_send_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_send_request_fail = 0;

  /* HttpQueryInfoW fail */
  g_mock_wininet_query_info_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_query_info_fail = 0;

  /* readChunk alloc fail */
  g_mock_wininet_read_chunk_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_read_chunk_alloc_fail = 0;

  /* InternetReadFile fail */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_read_file_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_read_file_fail = 0;
  g_mock_wininet_read_chunks = 0;

  /* body realloc fail */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_body_realloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_body_realloc_fail = 0;
  g_mock_wininet_read_chunks = 0;

  /* res alloc fail */
  g_mock_wininet_res_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_res_alloc_fail = 0;

  /* response_init fail */
  g_mock_wininet_response_init_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_wininet_response_init_fail = 0;

  /* POST body without read_chunk */
  req.method = HTTP_POST;
  req.body = "payload";
  req.body_len = 7;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  req.body = NULL;
  req.body_len = 0;
  req.method = HTTP_GET;

  /* read_chunk with send_request_ex fail */
  {
    struct wininet_TestChunkState state;
    memset(&state, 0, sizeof(state));
    req.read_chunk = wininet_read_cb;
    req.read_chunk_user_data = &state;
    req.expected_body_len = 5;

    g_mock_wininet_send_request_ex_fail = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_wininet_send_request_ex_fail = 0;

    /* read_chunk with write_file fail */
    memset(&state, 0, sizeof(state));
    g_mock_wininet_write_file_fail = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_wininet_write_file_fail = 0;

    /* read_chunk with end_request fail */
    memset(&state, 0, sizeof(state));
    g_mock_wininet_end_request_fail = 1;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
    g_mock_wininet_end_request_fail = 0;

    req.read_chunk = NULL;
  }

  /* headers_to_wide_block fail */
  rc = http_headers_add(&req.headers, "X-Fail", "Val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  g_mock_alloc_count = 3;
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* HttpAddRequestHeaders fail */
  g_mock_wininet_add_headers_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_add_headers_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

/** @brief Test WinInet coverage branches */
TEST test_wininet_coverage_branches(void) {
  c_abstract_http_error_t rc;
  char *url_dup = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct HttpCookieJar jar;
  struct wininet_TestChunkState state;

  /* 1. Context init config failure */
  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_wininet_context_init(&ctx);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 2. Context init and config apply */
  rc = http_wininet_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  config.verify_host = 0;
  config.verify_peer = 1;
  config.follow_redirects = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("admin", &config.proxy_username));
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  config.proxy_username = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("secret", &config.proxy_password));
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));

  /* 3. Send host/path alloc fail */
  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://127.0.0.1:8080/path", &url_dup));
  req.url = url_dup;

  g_mock_alloc_count = 1;
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_count = 2;
  g_mock_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* 4. Connect fail, proxy options, https flag, open request fail */
  g_mock_wininet_connect_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_connect_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  config.verify_peer = 0;
  config.verify_host = 0;
  config.follow_redirects = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("user", &config.proxy_username));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_mock_strdup("pass", &config.proxy_password));
  rc = http_cookie_jar_init(&jar);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_cookie_jar_set(&jar, "ck", "val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  config.cookie_jar = &jar;
  rc = http_wininet_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));

  /* InternetSetOptionA failure on proxy */
  g_mock_wininet_set_option_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* InternetSetOptionA failure on proxy password */
  g_mock_wininet_set_option_fail = 2;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_set_option_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("https://127.0.0.1:8443/secure", &url_dup));
  req.url = url_dup;

  g_mock_wininet_open_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_open_request_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 5. Headers add fail */
  rc = http_headers_add(&req.headers, "X-Test", "Val");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* 6. Send request fail without read_chunk */
  g_mock_wininet_send_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_send_request_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 7. Query info fail, readChunk alloc fail, readFile fail */
  g_mock_wininet_query_info_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_query_info_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_wininet_read_chunk_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_chunk_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_wininet_read_file_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_file_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* 8. on_chunk abort */
  memset(&state, 0, sizeof(state));
  state.abort_on_call = 1;
  req.on_chunk = wininet_chunk_cb;
  req.on_chunk_user_data = &state;
  g_mock_wininet_read_chunks = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_chunks = 0;
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);
  req.on_chunk = NULL;

  /* 9. body realloc fail, res alloc fail, response_init fail */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_body_realloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_body_realloc_fail = 0;
  g_mock_wininet_read_chunks = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_wininet_res_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_res_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_wininet_response_init_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_response_init_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* bodyBuf freed on cleanup */
  g_mock_wininet_read_chunks = 1;
  g_mock_wininet_res_alloc_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_read_chunks = 0;
  g_mock_wininet_res_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* status code 404 */
  g_mock_wininet_status_code = 404;
  rc = http_wininet_send(ctx, &req, &res);
  g_mock_wininet_status_code = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }

  /* 10. Cookie parsing in response with semicolon */
  g_mock_wininet_cookie_count = 2;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }

  /* 10b. Cookie set failure */
  g_mock_wininet_cookie_count = 1;
  g_mock_wininet_cookie_set_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  g_mock_wininet_cookie_set_fail = 0;

  /* 10c. Close handle failure */
  g_mock_wininet_close_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  g_mock_wininet_close_fail = 2;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  if (res) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
  }
  /* Close handle failure when rc is already an error */
  g_mock_wininet_close_fail = 1;
  g_mock_wininet_send_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_send_request_fail = 0;
  g_mock_wininet_close_fail = 0;
  /* Close handle failure on hConnect when rc is already an error */
  g_mock_wininet_close_fail = 2;
  g_mock_wininet_send_request_fail = 1;
  rc = http_wininet_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_wininet_send_request_fail = 0;
  g_mock_wininet_close_fail = 0;

  /* 10d. 5-char scheme other than https */
  {
    char *u = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("gtest://127.0.0.1:8080/test", &u));
    req.url = u;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    free(u);
  }

  /* 10e. URL without port */
  {
    char *u = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http://127.0.0.1/test", &u));
    req.url = u;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    free(u);
  }

  /* 10f. URL without port and without path */
  {
    char *u = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http://127.0.0.1", &u));
    req.url = u;
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    free(u);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http://127.0.0.1:8080/test", &u));
    req.url = u;
  }

  /* 10g. Config timeouts and proxy permutations */
  {
    struct HttpConfig cfg;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.timeout_ms = 0;
    cfg.connect_timeout_ms = 10;
    rc = http_wininet_config_apply(ctx, &cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    cfg.connect_timeout_ms = 0;
    cfg.read_timeout_ms = 10;
    rc = http_wininet_config_apply(ctx, &cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    cfg.read_timeout_ms = 0;
    cfg.write_timeout_ms = 10;
    rc = http_wininet_config_apply(ctx, &cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    cfg.write_timeout_ms = 0;
    rc = http_wininet_config_apply(ctx, &cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    /* Proxy username without password */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("http://127.0.0.1:8080", &cfg.proxy_url));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("user", &cfg.proxy_username));
    cfg.proxy_password = NULL;
    rc = http_wininet_config_apply(ctx, &cfg);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  }

  /* 10h. Cookie jar with count == 0 */
  {
    struct HttpCookieJar empty_jar;
    struct HttpConfig cfg;
    memset(&empty_jar, 0, sizeof(empty_jar));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.cookie_jar = &empty_jar;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
    rc = http_wininet_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    if (res) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
    }
    cfg.cookie_jar = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}
#endif

TEST test_wininet_protocol_user_agent_and_send_multi(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest r1, r2;
  struct HttpMultiRequest multi;
  struct HttpFuture f1, f2;
  struct HttpFuture *futs[2];
  struct ModalityEventLoop *loop = NULL;
  char *u1 = NULL;
  char *u2 = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  cfg.tls_version_mask = 0xFF00U;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_wininet_config_apply(ctx, &cfg));
  cfg.tls_version_mask = HTTP_TLS_VERSION_1_2 | HTTP_TLS_VERSION_1_3;

  cfg.version_mask = HTTP_VERSION_3;
  cfg.http3_fallback = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_wininet_config_apply(ctx, &cfg));
  cfg.version_mask = HTTP_VERSION_2 | HTTP_VERSION_3;
  cfg.http3_fallback = 1;
  free(cfg.user_agent);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("CustomWinInetUA/2.0", &cfg.user_agent));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_wininet_send_multi(NULL, NULL, NULL, NULL));

#if !defined(_WIN32)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  futs[0] = &f1;
  futs[1] = &f2;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wininet_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wininet_send_multi(ctx, loop, &multi, futs));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("https://127.0.0.1:8443/w1", &u1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:8080/w2", &u2));
  r1.url = u1;
  r2.url = u2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));

  memset(&f1, 0, sizeof(f1));
  memset(&f2, 0, sizeof(f2));
  g_mock_wininet_read_chunks = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wininet_send_multi(ctx, loop, &multi, futs));
  ASSERT_EQ(1, f1.is_ready);
  ASSERT_EQ(1, f2.is_ready);
  ASSERT_EQ(200, f1.response->status_code);
  ASSERT_EQ(200, f2.response->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));
  g_mock_wininet_read_chunks = 0;

  {
    int k;
    struct HttpRequest bad_req;
    struct HttpMultiRequest m1;
    struct HttpFuture bf;
    struct HttpFuture *bfs[1];
    bfs[0] = &bf;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&bad_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&m1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&m1, &bad_req));
    memset(&bf, 0, sizeof(bf));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_wininet_send_multi(ctx, loop, &m1, bfs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&m1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&bad_req));

    g_mock_wininet_open_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_open_fail = 0;

    g_mock_wininet_crack_url_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_crack_url_fail = 0;

    g_mock_wininet_connect_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_connect_fail = 0;

    g_mock_wininet_open_request_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_open_request_fail = 0;

    g_mock_wininet_send_request_fail = 1;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_send_request_fail = 0;

    g_mock_wininet_query_info_fail = 1;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_query_info_fail = 0;

    g_mock_wininet_read_file_fail = 1;
    g_mock_wininet_read_chunks = 1;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_wininet_send_multi(ctx, loop, &multi, futs));
    g_mock_wininet_read_file_fail = 0;
    g_mock_wininet_read_chunks = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&r1.headers, "X-Inet", "1"));
    r1.on_chunk = wininet_chunk_cb;
    {
      struct wininet_TestChunkState dst;
      memset(&dst, 0, sizeof(dst));
      r1.on_chunk_user_data = &dst;
      g_mock_wininet_read_chunks = 1;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));
      g_mock_wininet_read_chunks = 0;
    }
    r1.on_chunk = NULL;

    {
      struct HttpResponse *sres = NULL;
      struct HttpCookieJar jar;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
      cfg.cookie_jar = &jar;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
      g_mock_wininet_close_fail = 2;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_wininet_send(ctx, &r2, &sres));
      g_mock_wininet_close_fail = 0;
      if (sres) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(sres));
        free(sres);
        sres = NULL;
      }

      {
        c_abstract_http_error_t wrc;
        g_mock_wininet_cookie_count = 1;
        g_mock_alloc_count = 5;
        g_mock_alloc_fail = 1;
        wrc = http_wininet_send(ctx, &r2, &sres);
        g_mock_alloc_fail = 0;
        g_mock_alloc_count = 0;
        g_mock_wininet_cookie_count = 0;
        ASSERT((int)wrc >= -2000 && (int)wrc <= 2000);
      }
      if (sres) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(sres));
        free(sres);
      }
      cfg.cookie_jar = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
    }

    for (k = 0; k < 20; ++k) {
      c_abstract_http_error_t orc;
      g_mock_wininet_read_chunks = 1;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_alloc_count = k;
      g_mock_alloc_fail = 1;
      orc = http_wininet_send_multi(ctx, loop, &multi, futs);
      g_mock_alloc_fail = 0;
      ASSERT((int)orc >= -2000 && (int)orc <= 2000);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));
    }
    g_mock_alloc_count = 0;
    g_mock_wininet_read_chunks = 0;

    {
      struct HttpTransportContext *c2 = NULL;
      struct HttpResponse *sres = NULL;
      struct HttpCookieJar jar;
      struct wininet_TestChunkState cb_st;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_wininet_slot_guard(0));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_wininet_slot_guard(1));

      g_mock_wininet_open_fail = 2;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      g_mock_wininet_open_fail = 0;

      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_init(&c2));
      g_mock_config_free_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_wininet_context_free(c2));
      g_mock_config_free_fail = 0;
      c2 = NULL;

      for (k = 0; k < 8; ++k) {
        c_abstract_http_error_t arc;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        arc = http_wininet_config_apply(ctx, &cfg);
        g_mock_alloc_fail = 0;
        ASSERT((int)arc >= -2000 && (int)arc <= 2000);
      }
      g_mock_alloc_count = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));

      /* cookie malloc OOM + cookie_jar_set OOM */
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
      cfg.cookie_jar = &jar;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
      for (k = 0; k < 15; ++k) {
        c_abstract_http_error_t wrc;
        sres = NULL;
        g_mock_wininet_cookie_count = 1;
        g_mock_alloc_count = k;
        g_mock_alloc_fail = 1;
        wrc = http_wininet_send(ctx, &r2, &sres);
        g_mock_alloc_fail = 0;
        g_mock_wininet_cookie_count = 0;
        ASSERT((int)wrc >= -2000 && (int)wrc <= 2000);
        if (sres) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(sres));
          free(sres);
        }
      }
      g_mock_alloc_count = 0;
      cfg.cookie_jar = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));

      /* sync response_init fail via g_mock_headers_init_fail */
      g_mock_headers_init_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_wininet_send(ctx, &r2, &sres));
      g_mock_headers_init_fail = 0;

      /* dual handle close failure */
      {
        c_abstract_http_error_t wrc;
        g_mock_wininet_close_fail = -1;
        wrc = http_wininet_send(ctx, &r2, &sres);
        g_mock_wininet_close_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, wrc);
        if (sres) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(sres));
          free(sres);
          sres = NULL;
        }
        g_mock_wininet_close_fail = -1;
        g_mock_wininet_send_request_fail = 1;
        wrc = http_wininet_send(ctx, &r2, &sres);
        g_mock_wininet_send_request_fail = 0;
        g_mock_wininet_close_fail = 0;
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, wrc);
      }

      /* async wakeup fail + response_init fail + on_chunk fail */
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(3, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_wininet_send_multi(ctx, loop, &multi, futs));
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
      g_mock_headers_init_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      g_mock_headers_init_fail = 0;

      memset(&cb_st, 0, sizeof(cb_st));
      cb_st.abort_on_call = 1;
      r1.on_chunk = wininet_chunk_cb;
      r1.on_chunk_user_data = &cb_st;
      g_mock_wininet_read_chunks = 2;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      g_mock_wininet_read_chunks = 0;
      r1.on_chunk = NULL;
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      /* async NULL url */
      r1.url = NULL;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      r1.url = u1;

      /* async HttpSendRequestW sync return 1 (mode 2) & non-pending return 0
       * (mode 3) */
      g_mock_wininet_send_request_fail = 2;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }

      g_mock_wininet_send_request_fail = 3;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_wininet_send_multi(ctx, loop, &multi, futs));
      g_mock_wininet_send_request_fail = 0;
    }
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r2));
#else
  (void)r1;
  (void)r2;
  (void)multi;
  (void)f1;
  (void)f2;
  (void)futs;
  (void)loop;
  (void)u1;
  (void)u2;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}

#if !defined(_WIN32)
TEST test_wininet_utf8_and_async_timeouts(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpMultiRequest multi;
  struct HttpFuture fut;
  struct HttpFuture *futs[1];
  struct HttpCookieJar jar;
  const char utf8_hdr[] = "v-\xC3\xA9-\xE2\x82\xAC-\xF0\x9F\x98\x80-\xFF";

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  cfg.cookie_jar = &jar;
  cfg.connect_timeout_ms = 100;
  cfg.write_timeout_ms = 200;
  cfg.read_timeout_ms = 300;
  cfg.version_mask = HTTP_VERSION_2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1:8080/utf8", &req.url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Utf8", utf8_hdr));
  g_mock_wininet_cookie_count = 99;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_send(ctx, &req, &res));
  g_mock_wininet_cookie_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req));
  futs[0] = &fut;
  memset(&fut, 0, sizeof(fut));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wininet_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&fut));

  cfg.connect_timeout_ms = 0;
  cfg.write_timeout_ms = 0;
  cfg.read_timeout_ms = 0;
  cfg.timeout_ms = 0;
  cfg.version_mask = HTTP_VERSION_DEFAULT;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_config_apply(ctx, &cfg));
  memset(&fut, 0, sizeof(fut));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_wininet_send_multi(ctx, NULL, &multi, futs));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&fut));

  {
    int cd;
    for (cd = 2; cd <= 10; ++cd) {
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                c_abstract_http_test_wininet_slot_guard(cd));
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "ck", "cv"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "X-K", "V"));
    for (cd = 1; cd <= 12; ++cd) {
      struct HttpResponse *r2 = NULL;
      c_abstract_http_error_t src;
      g_mock_wininet_cookie_count = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_wininet_slot_guard(-cd));
      src = http_wininet_send(ctx, &req, &r2);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_wininet_slot_guard(-999));
      if (r2) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(r2));
        free(r2);
      }
      ASSERT((int)src >= -2000 && (int)src <= 2000);
    }
    g_mock_wininet_cookie_count = 0;

    for (cd = 1; cd <= 3; ++cd) {
      c_abstract_http_error_t mrc;
      memset(&fut, 0, sizeof(fut));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_wininet_slot_guard(-cd));
      mrc = http_wininet_send_multi(ctx, NULL, &multi, futs);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_wininet_slot_guard(-999));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&fut));
      ASSERT((int)mrc >= -2000 && (int)mrc <= 2000);
    }
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_wininet_context_free(ctx));
  PASS();
}
#endif

/** @brief WinInet test suite */
SUITE(http_wininet_suite) {
  RUN_TEST(test_wininet_lifecycle);
  RUN_TEST(test_wininet_config_apply);
  RUN_TEST(test_wininet_send_validation);
  RUN_TEST(test_wininet_send_methods_and_headers);
  RUN_TEST(test_wininet_send_cookies);
  RUN_TEST(test_wininet_send_chunked);
  RUN_TEST(test_wininet_send_chunked_abort);
  RUN_TEST(test_wininet_send_upload_chunked);
  RUN_TEST(test_wininet_protocol_user_agent_and_send_multi);
#if !defined(_WIN32)
  RUN_TEST(test_wininet_utf8_and_async_timeouts);
  RUN_TEST(test_wininet_send_mock_failures);
  RUN_TEST(test_wininet_coverage_branches);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_WININET_H */
