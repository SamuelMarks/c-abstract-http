/**
 * @file test_http_curl.h
 * @brief Integration tests for Libcurl Backend.
 *
 * Verifies that the Curl wrapper correctly initializes, handles configuration,
 * sends requests, and maps failures to errno.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_CURL_H
#define TEST_HTTP_CURL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <string.h>

#include <c_abstract_http/http_curl.h>
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include <curl/curl.h>
#endif
#include <c_abstract_http/http_types.h>
#include "functions/parse/str.h"

/* Helper: Build a request to localhost on a port likely to be closed */
#include "cdd_test_helpers/mock_server.h"
/* clang-format on */

static int setup_request(struct HttpRequest *req, int port) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *_ast_strdup_0 = NULL;
  char url[64];

  rc = http_request_init(req);
  if (rc != 0)
    return rc;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/test", port);
#else
  sprintf(url, "http://127.0.0.1:%d/test", port);
#endif

  req->url =
      (c_abstract_http_mock_cdd_strdup(url, &_ast_strdup_0), _ast_strdup_0);
  return (enum greatest_test_res)0;
}

/** @brief Documented */
TEST test_curl_global_lifecycle(void) {
  /* Should succeed and track ref count internally */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_curl_global_init()); /* Re-entrant check */

  http_curl_global_cleanup();
  http_curl_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_curl_context_lifecycle(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpTransportContext *ctx = NULL;

  http_curl_global_init();

  rc = http_curl_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  http_curl_context_free(ctx);
  /* Double free safety check (should be safe with NULL) */
  http_curl_context_free(NULL);

  http_curl_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_curl_config_application(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *_ast_strdup_proxy = NULL;
  char *_ast_strdup_user = NULL;
  char *_ast_strdup_pass = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);

  /* Set some values */
  config.timeout_ms = 500;
  config.verify_peer = 0; /* Insecure for testing logic */
  config.follow_redirects = 0;
  config.proxy_url = (c_abstract_http_mock_cdd_strdup("http://proxy.local:8080",
                                                      &_ast_strdup_proxy),
                      _ast_strdup_proxy);
  config.proxy_username =
      (c_abstract_http_mock_cdd_strdup("admin", &_ast_strdup_user),
       _ast_strdup_user);
  config.proxy_password =
      (c_abstract_http_mock_cdd_strdup("secret", &_ast_strdup_pass),
       _ast_strdup_pass);

  rc = http_curl_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  http_config_free(&config);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_curl_send_connection_failure(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  /* Expect mapped error (ECONNREFUSED or C_ABSTRACT_HTTP_ERR_TIMEOUT or
   * EHOSTUNREACH) */
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);

  /* Fast timeout for test speed */
  config.timeout_ms = 50;
  http_curl_config_apply(ctx, &config);

  /* Use an invalid port */
  setup_request(&req, 59999);

  rc = http_curl_send(ctx, &req, &res);

  /* Verify mapping logic.
     Note: On some systems connection refused happens instanly (ECONNREFUSED),
     on others it times out (C_ABSTRACT_HTTP_ERR_TIMEOUT). Both are valid error
     mappings for this test. */
  if (rc != ECONNREFUSED && rc != C_ABSTRACT_HTTP_ERR_TIMEOUT &&
      rc != EHOSTUNREACH && rc != C_ABSTRACT_HTTP_ERR_IO) {
    fprintf(stderr, "Unexpected return code: %d (%s)\n", rc, strerror(rc));
    FAIL();
  }

  ASSERT(res == NULL); /* Should not be allocated on failure */

  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_curl_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_request_init(&req);

  /* NULL ctx */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(NULL, &req, &res));

  /* NULL req */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, NULL, &res));

  /* NULL res pointer */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, &req, NULL));

  /* Invalid parts */
  req.parts.count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, &req, &res));
  req.parts.count = 0;

  /* NULL internal config application */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_config_apply(ctx, NULL));

  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

/*
 * NOTE: Testing a successful request requires a running server.
 * We skip strictly specific success tests here (mocking libcurl
 * internals requires complex LD_PRELOAD or weak symbols which is beyond
 * standard C89 unit test scope without heavy frameworks).
 * The failure cases prove the logic integration.
 */

/** @brief Documented */
struct curl_TestChunkState {
  /** @brief Documented */
  int call_count;
  /** @brief Documented */
  size_t total_bytes;
  /** @brief Documented */
  int abort_on_call;
};

static int curl_mock_chunk_cb(void *user_data, const void *chunk,
                              size_t chunk_len) {
  struct curl_TestChunkState *state = (struct curl_TestChunkState *)user_data;
  (void)chunk;
  state->call_count++;
  state->total_bytes += chunk_len;
  if (state->abort_on_call > 0 && state->call_count >= state->abort_on_call) {
    return ECANCELED; /* Abort */
  }
  return 0;
}

/** @brief Documented */
TEST test_curl_send_chunked(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct curl_TestChunkState state;

  /* Start mock server */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  setup_request(&req, math_mock_server_get_port(server));

  /* Setup chunk callback */
  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 0;
  req.on_chunk = curl_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_curl_send(ctx, &req, &res);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);
  /* Body should not be populated when on_chunk is used */
  ASSERT_EQ(NULL, res->body);
  ASSERT_EQ(0, res->body_len);

  /* Verify chunk callback was called and read data ("OK" from mock server) */
  ASSERT(state.call_count > 0);
  ASSERT(state.total_bytes > 0);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

/** @brief Documented */
TEST test_curl_send_chunked_abort(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct curl_TestChunkState state;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  setup_request(&req, math_mock_server_get_port(server));

  state.call_count = 0;
  state.total_bytes = 0;
  state.abort_on_call = 1; /* Abort immediately on first chunk */
  req.on_chunk = curl_mock_chunk_cb;
  req.on_chunk_user_data = &state;

  rc = http_curl_send(ctx, &req, &res);

  /* Should return the error code returned by our callback (ECANCELED) */
  ASSERT_EQ(ECANCELED, rc);
  ASSERT(res == NULL);

  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

/** @brief Documented */
struct curl_TestUploadState {
  /** @brief Documented */
  const char *data;
  /** @brief Documented */
  size_t len;
  /** @brief Documented */
  size_t pos;
};

static int curl_mock_upload_cb(void *user_data, void *buf, size_t buf_len,
                               size_t *out_read) {
  struct curl_TestUploadState *state = (struct curl_TestUploadState *)user_data;
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
TEST test_curl_send_upload_chunked(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct HttpConfig config;
  struct curl_TestUploadState up_state;
  const char *payload = "UPLOAD_TEST_DATA";

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  setup_request(&req, math_mock_server_get_port(server));
  req.method = HTTP_POST;

  /* Disable Expect: 100-continue to prevent curl from waiting or aborting early
   */
  http_headers_add(&req.headers, "Expect", "");

  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = up_state.len;

  rc = http_curl_send(ctx, &req, &res);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(res != NULL);
  ASSERT_EQ(200, res->status_code);

  /* Read the payload back to verify? Our mock server doesn't echo it, but it
     reads it. We just check that it completed successfully and our pos
     advanced. */
  ASSERT_EQ(up_state.len, up_state.pos);

  http_response_free(res);
  free(res);
  http_config_free(&config);
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

/** @brief Documented */
TEST test_curl_http3_config(void) {
  struct HttpConfig config;
  struct HttpTransportContext *ctx = NULL;
  int ret;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_global_init());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.version_mask = HTTP_VERSION_3;
  config.http3_fallback = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_context_init(&ctx));

  ret = http_curl_config_apply(ctx, &config);
  ASSERT(ret == 0 || ret == C_ABSTRACT_HTTP_ERR_IO);

  http_config_free(&config);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  PASS();
}

/** @brief Documented */
TEST test_curl_edge_cases(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  int i;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_context_init(&ctx));

  /* Missing parameters */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, &req, NULL));

  /* Bad URL */
  http_request_init(&req);
  req.url = "badurl://";
  /* ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, &req, &res)); */

  /* OOM loop for http_curl_send */
  for (i = 0; i < 25; i++) {
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = i;
    {
      int rc_test_tmp = http_curl_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (rc_test_tmp == 0) {
        if (res) {
          http_response_free(res);
          free(res);
          res = NULL;
        }
        continue;
      }
      /* ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp); */
    }
  }

  /* Force header allocation failure */
  http_request_init(&req);
  req.url = "badurl://";
  http_headers_add(&req.headers, "X", "Y");
  http_headers_add(&req.headers, "X2", "Y2");
  for (i = 0; i < 25; i++) {
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = i;
    {
      int rc_test_tmp = http_curl_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (res) {
        http_response_free(res);
        free(res);
        res = NULL;
      }
      if (rc_test_tmp == 0)
        break;
    }
  }
  g_mock_alloc_fail = 0;

  req.url = NULL;
  http_request_free(&req);
  http_curl_context_free(ctx);
  PASS();
}

/** @brief Documented */
TEST test_curl_send_write_oom(void) {
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char url_buf[128];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  sprintf(url_buf, "http://127.0.0.1:%d", math_mock_server_get_port(server));

  http_curl_global_init();
  http_curl_context_init(&ctx);

  http_request_init(&req);
  req.url = url_buf;
  req.method = HTTP_GET;

  {
    int i;
    for (i = 0; i < 20; i++) {
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = i;
      if (http_curl_send(ctx, &req, &res) == 0) {
        http_response_free(res);
        free(res);
        res = NULL;
        g_mock_alloc_fail = 0;
        break;
      }
      g_mock_alloc_fail = 0;
      if (res) {
        http_response_free(res);
        free(res);
        res = NULL;
      }
    }
  }

  req.url = NULL;
  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();

  mock_server_destroy(server);

  PASS();
}

/** @brief Documented */
TEST test_curl_send_unsupported_protocol(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_context_init(&ctx));
  http_request_init(&req);
  req.url = "badprotocol://localhost/";
  req.method = HTTP_GET;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, &req, &res));

  req.url = NULL;
  http_request_free(&req);
  http_curl_context_free(ctx);
  PASS();
}

/** @brief Documented */
TEST test_curl_send_resolve_error(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_context_init(&ctx));
  http_request_init(&req);
  req.url = "http://this.domain.does.not.exist.at.all.test/";
  req.method = HTTP_GET;

  rc = http_curl_send(ctx, &req, &res);
  ASSERT(rc == EHOSTUNREACH || rc == ECONNREFUSED ||
         rc == C_ABSTRACT_HTTP_ERR_IO);

  req.url = NULL;
  http_request_free(&req);
  http_curl_context_free(ctx);
  PASS();
}

/** @brief Documented */
TEST test_curl_unsupported_methods(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_context_init(&ctx));
  http_request_init(&req);
  req.url = "badprotocol://localhost/";
  req.method = HTTP_GET;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_DELETE;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_HEAD;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_PATCH;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_PUT;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_POST;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.url = NULL;
  http_request_free(&req);
  http_curl_context_free(ctx);
  PASS();
}

/** @brief Documented */
TEST test_curl_payload_methods(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_context_init(&ctx));
  http_request_init(&req);
  req.url = NULL;
  CDD_STRDUP("http://localhost/", &req.url);
  req.body = (unsigned char *)malloc(4);
  memcpy(req.body, "data", 4);
  req.body_len = 4;

  req.method = HTTP_PATCH;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_QUERY;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_PUT;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  req.method = HTTP_POST;
  http_curl_send(ctx, &req, &res);
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  http_request_free(&req);
  http_curl_context_free(ctx);
  PASS();
}

/** @brief Documented */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
struct curl_slist *g_mock_curl_cookies = NULL;
extern int g_mock_curl_init_fail;
extern void cdd_test_multi_socket_cb(struct ModalityEventLoop *loop, int fd,
                                     int events, void *user_data);
extern int g_mock_curl_setopt_fail;
extern int g_mock_curl_setopt_count;
extern CURLcode g_mock_curl_perform_res;
#endif

TEST test_curl_send_cookies(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr server = NULL;
  int port;
  struct HttpConfig config;
  struct HttpCookieJar jar;
  struct curl_slist *cookies = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);

  http_curl_global_init();
  http_curl_context_init(&ctx);

  http_config_init(&config);
  http_cookie_jar_init(&jar);
  config.cookie_jar = &jar;
  http_curl_config_apply(ctx, &config);

  http_request_init(&req);
  setup_request(&req, port);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  cookies =
      curl_slist_append(NULL, "example.com\tFALSE\t/\tFALSE\t0\tname\tvalue");
  g_mock_curl_cookies = cookies;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }

  {
    int i;
    for (i = 0; i < 10; i++) {
      int rc_test_tmp;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_curl_cookies) {
        curl_slist_free_all(g_mock_curl_cookies);
      }
      cookies = curl_slist_append(
          NULL, "example.com\tFALSE\t/\tFALSE\t0\tname\tvalue");
      g_mock_curl_cookies = cookies;
#endif
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = i;
      rc_test_tmp = http_curl_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (res) {
        http_response_free(res);
        free(res);
        res = NULL;
      }
      if (rc_test_tmp == 0)
        break;
    }
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  cdd_test_multi_socket_cb(NULL, 0, HTTP_LOOP_ERROR, ctx);
  if (g_mock_curl_cookies) {
    curl_slist_free_all(g_mock_curl_cookies);
  }
  g_mock_curl_cookies = NULL;
#endif

  http_request_free(&req);
  http_config_free(&config);
  http_cookie_jar_free(&jar);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

static int curl_mock_upload_cb_abort(void *user_data, void *buffer,
                                     size_t max_bytes, size_t *out_bytes_read) {
  (void)user_data;
  (void)buffer;
  (void)max_bytes;
  (void)out_bytes_read;
  return -1;
}

TEST test_curl_send_upload_chunked_abort(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr server = NULL;
  int port;
  struct curl_TestUploadState up_state;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);

  http_curl_global_init();
  http_curl_context_init(&ctx);

  http_request_init(&req);
  setup_request(&req, port);
  req.method = HTTP_POST;
  req.read_chunk = curl_mock_upload_cb_abort;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

TEST test_curl_init_fails(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;

  g_mock_curl_init_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_curl_global_init());
  g_mock_curl_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_curl_context_init(&ctx));
  g_mock_curl_init_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_curl_context_init(&ctx));
  g_mock_curl_init_fail = 0;
#else
  SKIP();
#endif
  PASS();
}

TEST test_curl_send_oom(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr server = NULL;
  int port;
  int i;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);

  http_curl_global_init();
  http_curl_context_init(&ctx);

  http_request_init(&req);
  setup_request(&req, port);

  for (i = 0; i < 15; i++) {
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = i;
    {
      int rc_test_tmp = http_curl_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (rc_test_tmp == 0) {
        if (res) {
          http_response_free(res);
          free(res);
          res = NULL;
        }
        break;
      }
    }
    g_mock_alloc_fail = 0;
  }

  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

TEST test_curl_send_chunked_methods(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  MockServerPtr server = NULL;
  int port;
  int rc;
  struct curl_TestUploadState up_state;
  const char *payload = "data";
  up_state.data = payload;
  up_state.len = strlen(payload);
  up_state.pos = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);

  http_curl_global_init();
  http_curl_context_init(&ctx);

  /* PUT chunked */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = HTTP_PUT;
  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = 4;
  up_state.pos = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  /* PATCH chunked */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = HTTP_PATCH;
  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = 4;
  up_state.pos = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  /* QUERY chunked */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = HTTP_QUERY;
  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = 4;
  up_state.pos = 0;
  rc = http_curl_send(ctx, &req, &res);
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_SUCCESS, rc, "%d");
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  /* GET chunked (default fallback) */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = HTTP_GET;
  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = 4;
  up_state.pos = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  /* QUERY payload */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = HTTP_QUERY;
  req.body = (unsigned char *)"data";
  req.body_len = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  req.body = NULL;
  http_request_free(&req);

  /* Unknown method fallback */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = 999;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  /* Unknown method chunked fallback */
  http_request_init(&req);
  setup_request(&req, port);
  req.method = 999;
  req.read_chunk = curl_mock_upload_cb;
  req.read_chunk_user_data = &up_state;
  req.expected_body_len = 4;
  up_state.pos = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_curl_send(ctx, &req, &res));
  if (res) {
    http_response_free(res);
    free(res);
    res = NULL;
  }
  http_request_free(&req);

  http_curl_context_free(ctx);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

TEST test_curl_send_multi(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  MockServerPtr server = NULL;
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req1, req2;
  struct HttpMultiRequest multi;
  struct HttpFuture *future1 = NULL, *future2 = NULL;
  struct HttpFuture *futures[2];
  struct ModalityEventLoop *loop = NULL;
  struct HttpConfig config;
  int port;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&server));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(server));
  port = math_mock_server_get_port(server);

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_config_init(&config);
  http_curl_config_apply(ctx, &config);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));

  http_request_init(&req1);
  setup_request(&req1, port);
  http_request_init(&req2);
  setup_request(&req2, port);

  http_multi_request_init(&multi);
  http_multi_request_add(&multi, &req1);
  http_multi_request_add(&multi, &req2);

  future1 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  future2 = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future1;
  futures[1] = future2;

  rc = http_curl_send_multi(ctx, loop, &multi, futures);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  while (!future1->is_ready || !future2->is_ready) {
    http_loop_tick(loop);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, future1->error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, future2->error_code);
  ASSERT(future1->response != NULL);
  ASSERT(future2->response != NULL);
  ASSERT_EQ(200, future1->response->status_code);
  ASSERT_EQ(200, future2->response->status_code);

  if (future1->response) {
    http_response_free(future1->response);
    free(future1->response);
  }
  if (future2->response) {
    http_response_free(future2->response);
    free(future2->response);
  }
  free(future1);
  free(future2);

  /* Test invalid args */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_curl_send_multi(NULL, loop, &multi, futures));

  http_config_free(&config);
  http_multi_request_free(&multi);
  http_request_free(&req1);
  http_request_free(&req2);
  http_curl_context_free(ctx);
  http_loop_free(loop);
  http_curl_global_cleanup();
  mock_server_destroy(server);
  PASS();
}

TEST test_curl_send_multi_oom(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpMultiRequest multi;
  struct HttpFuture *future = NULL;
  struct HttpFuture *futures[1];
  struct ModalityEventLoop *loop = NULL;
  int i;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
  http_request_init(&req);
  setup_request(&req, 80);

  http_multi_request_init(&multi);
  http_multi_request_add(&multi, &req);

  future = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future;

  for (i = 0; i < 45; i++) {
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = i;
    {
      int rc_test_tmp = http_curl_send_multi(ctx, loop, &multi, futures);
      g_mock_alloc_fail = 0;
      if (rc_test_tmp == 0) {
        /* Cleanup success task */
        while (!futures[0]->is_ready) {
          http_loop_tick(loop);
        }
        if (futures[0]->response) {
          http_response_free(futures[0]->response);
          free(futures[0]->response);
          futures[0]->response = NULL;
        }
        futures[0]->is_ready = 0;
        break;
      }
    }
  }

  http_curl_context_free(ctx);
  http_loop_free(loop);
  free(future);
  future = NULL;
  http_multi_request_free(&multi);
  http_request_free(&req);
  http_curl_global_cleanup();
  PASS();
}

TEST test_curl_send_multi_setopt_fail(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpMultiRequest multi;
  struct HttpFuture *future = NULL;
  struct HttpFuture *futures[1];
  struct ModalityEventLoop *loop = NULL;
  int i;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_loop_init(&loop);
  http_request_init(&req);
  CDD_STRDUP("http://localhost", &req.url);
  http_headers_add(&req.headers, "X", "Y");

  http_multi_request_init(&multi);
  http_multi_request_add(&multi, &req);

  future = (struct HttpFuture *)calloc(1, sizeof(struct HttpFuture));
  futures[0] = future;

  for (i = 0; i < 45; i++) {
    g_mock_curl_setopt_fail = 1;
    g_mock_curl_setopt_count = i;
    if (http_curl_send_multi(ctx, loop, &multi, futures) == 0) {
      g_mock_curl_setopt_fail = 0;
      /* If it succeeded because an ignored setopt failed, we must still clean
       * it up! */
      while (!futures[0]->is_ready) {
        http_loop_tick(loop);
      }
      if (futures[0]->response) {
        http_response_free(futures[0]->response);
        free(futures[0]->response);
        futures[0]->response = NULL;
      }
      futures[0]->is_ready = 0;
    }
    g_mock_curl_setopt_fail = 0;
  }

  http_curl_context_free(ctx);
  http_loop_free(loop);
  free(future);
  future = NULL;
  http_multi_request_free(&multi);
  http_request_free(&req);
  http_curl_global_cleanup();
#else
  SKIP();
#endif
  PASS();
}

TEST test_curl_send_setopt_fail(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  int i;
  int j;

  http_curl_global_init();
  http_curl_context_init(&ctx);
  for (j = 0; j < 9; j++) {
    http_config_init(&config);
    if (j == 0) {
      config.version_mask = HTTP_VERSION_3;
      config.http3_fallback = 1;
    } else if (j == 1) {
      config.version_mask = HTTP_VERSION_3;
      config.http3_fallback = 0;
    } else if (j == 2) {
      config.version_mask = HTTP_VERSION_2;
    } else if (j == 3) {
      config.version_mask = HTTP_VERSION_1_1;
    } else if (j == 4) {
      config.version_mask = HTTP_VERSION_1_0;
    } else {
      config.version_mask = 0;
      if (j == 5)
        config.tls_version_mask = HTTP_TLS_VERSION_1_3;
      if (j == 6)
        config.tls_version_mask = HTTP_TLS_VERSION_1_2;
      if (j == 7)
        config.tls_version_mask = HTTP_TLS_VERSION_1_1;
      if (j == 8)
        config.tls_version_mask = HTTP_TLS_VERSION_1_0;
      config.timeout_ms = 1000;
      config.verify_peer = 1;
      config.verify_host = 1;
      config.follow_redirects = 1;
      CDD_STRDUP("http://proxy", &config.proxy_url);
      CDD_STRDUP("user", &config.proxy_username);
      CDD_STRDUP("password", &config.proxy_password);
      config.cookie_jar =
          (struct HttpCookieJar *)calloc(1, sizeof(struct HttpCookieJar));
    }

    for (i = 0; i < 55; i++) {
      g_mock_curl_setopt_fail = 1;
      g_mock_curl_setopt_count = i;
      if (http_curl_config_apply(ctx, &config) == 0) {
        g_mock_curl_setopt_fail = 0;
      }
      g_mock_curl_setopt_fail = 0;
    }
    http_config_free(&config);
    if (config.cookie_jar) {
      c_abstract_http_mock_free(config.cookie_jar);
      config.cookie_jar = NULL;
    }
  }

  {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    http_request_init(&req);
    CDD_STRDUP("http://localhost", &req.url);
    http_headers_add(&req.headers, "A", "B");
    http_headers_add(&req.headers, "C", "D");
    for (i = 0; i < 15; i++) {
      g_mock_curl_setopt_fail = 1;
      g_mock_curl_setopt_count = i;
      if (http_curl_send(ctx, &req, &res) == 0) {
        g_mock_curl_setopt_fail = 0;
        if (res) {
          http_response_free(res);
          free(res);
          res = NULL;
        }
      }
      g_mock_curl_setopt_fail = 0;
    }
    http_request_free(&req);
  }

  http_curl_context_free(ctx);
  http_curl_global_cleanup();
#else
  SKIP();
#endif
  PASS();
}

TEST test_curl_send_perform_errors(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  struct HttpTransportContext *ctx = NULL;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  int i;
  CURLcode codes[] = {
      CURLE_OPERATION_TIMEDOUT, CURLE_SSL_CONNECT_ERROR, CURLE_OUT_OF_MEMORY,
      CURLE_TOO_MANY_REDIRECTS, CURLE_SEND_ERROR,        CURLE_RECV_ERROR,
      CURLE_FAILED_INIT /* default */
  };

  http_curl_global_init();
  http_curl_context_init(&ctx);
  http_request_init(&req);
  CDD_STRDUP("http://localhost", &req.url);

  for (i = 0; i < 7; i++) {
    g_mock_curl_perform_res = codes[i];
    http_curl_send(ctx, &req, &res);
  }
  g_mock_curl_perform_res = CURLE_OK;

  http_request_free(&req);
  http_curl_context_free(ctx);
  http_curl_global_cleanup();
#else
  SKIP();
#endif
  PASS();
}

SUITE(http_curl_suite) {
  RUN_TEST(test_curl_send_cookies);
  RUN_TEST(test_curl_send_upload_chunked_abort);
  RUN_TEST(test_curl_init_fails);
  RUN_TEST(test_curl_send_oom);
  RUN_TEST(test_curl_send_chunked_methods);
  RUN_TEST(test_curl_send_multi);
  RUN_TEST(test_curl_send_multi_oom);
  RUN_TEST(test_curl_send_multi_setopt_fail);
  RUN_TEST(test_curl_send_setopt_fail);
  RUN_TEST(test_curl_send_perform_errors);
  RUN_TEST(test_curl_payload_methods);

  RUN_TEST(test_curl_unsupported_methods);

  RUN_TEST(test_curl_send_unsupported_protocol);
  RUN_TEST(test_curl_send_resolve_error);

  RUN_TEST(test_curl_send_write_oom);

  RUN_TEST(test_curl_edge_cases);

  RUN_TEST(test_curl_edge_cases);

  RUN_TEST(test_curl_global_lifecycle);
  RUN_TEST(test_curl_context_lifecycle);
  RUN_TEST(test_curl_config_application);
  RUN_TEST(test_curl_http3_config);
  RUN_TEST(test_curl_send_connection_failure);
  RUN_TEST(test_curl_send_invalid_arguments);
  RUN_TEST(test_curl_send_chunked);
  RUN_TEST(test_curl_send_chunked_abort);
  RUN_TEST(test_curl_send_upload_chunked);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_CURL_H */
