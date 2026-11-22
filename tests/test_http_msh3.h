/**
 * @file test_http_msh3.h
 * @brief Exhaustive unit and HTTP/3 loopback tests for the MsH3 Transport
 * Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, user_agent/cookie_jar headers, multipart flattening,
 * streaming upload/download callbacks, concurrent multi-request multiplexing,
 * and OOM/fault-injection error paths.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_MSH3_H
#define TEST_HTTP_MSH3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_msh3.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

/** @brief Streaming test callback state for MsH3 */
struct msh3_chunk_test_state {
  int calls;
  size_t total;
  int abort_call;
};

static enum c_abstract_http_error msh3_test_read_cb(void *user_data, void *buf,
                                                    size_t max_len,
                                                    size_t *out_read) {
  struct msh3_chunk_test_state *st = (struct msh3_chunk_test_state *)user_data;
  if (st->abort_call > 0 && st->calls + 1 >= st->abort_call) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  if (st->calls >= 2) {
    *out_read = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  st->calls++;
  if (max_len > 4) {
    max_len = 4;
  }
  memcpy(buf, "part", max_len);
  *out_read = max_len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
msh3_test_on_chunk_cb(void *user_data, const void *chunk, size_t len) {
  struct msh3_chunk_test_state *st = (struct msh3_chunk_test_state *)user_data;
  (void)chunk;
  st->calls++;
  st->total += len;
  if (st->abort_call > 0 && st->calls >= st->abort_call) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error msh3_mock_wakeup_fail(void *ctx) {
  (void)ctx;
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Setup request helper returning error enum.
 *
 * @param[out] req Pointer to HttpRequest to initialize.
 * @param[in] url_str URL string to copy into request.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error
test_msh3_setup_request(struct HttpRequest *req, const char *url_str) {
  char *u = NULL;
  enum c_abstract_http_error rc;

  rc = http_request_init(req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = c_abstract_http_strdup(url_str, &u);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    enum c_abstract_http_error free_rc = http_request_free(req);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return free_rc;
    }
    return rc;
  }
  req->url = u;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Test MsH3 helper error returns */
TEST test_msh3_setup_request_coverage(void) {
  struct HttpRequest req;
  enum c_abstract_http_error rc;
  (void)req;

  rc = test_msh3_setup_request(NULL, "http://localhost/");
  ASSERT(rc != C_ABSTRACT_HTTP_SUCCESS);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_alloc_fail = 1;
  rc = test_msh3_setup_request(&req, "http://localhost/");
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

/** @brief Test MsH3 global lifecycle and reference counting */
TEST test_msh3_global_lifecycle(void) {
  enum c_abstract_http_error rc;

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_msh3_mutex_init_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_mutex_init_fail = 0;

  /* Real mutex_init failure */
  g_mock_alloc_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  g_mock_msh3_global_lock_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_msh3_global_lock_fail = 0;

  /* Lock failure on initialized mutex in global_init */
  g_mock_mutex_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_mutex_fail = 0;

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  g_mock_msh3_global_lock_fail = 1;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_msh3_global_lock_fail = 0;

  g_mock_mutex_fail = 1;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_mutex_fail = 0;

  /* Unlock failure in global_cleanup */
  g_mock_mutex_fail = 2;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_mutex_fail = 0;

  /* Mutex free failure in global_cleanup */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_global_init());
  g_mock_mutex_free_fail = 1;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_mutex_free_fail = 0;

  g_mock_msh3_api_open_fail = 1;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_api_open_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_global_cleanup());

  g_mock_mutex_fail = 2;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_mutex_fail = 0;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  g_mock_msh3_api_open_fail = 1;
  g_mock_mutex_fail = 2;
  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_api_open_fail = 0;
  g_mock_mutex_fail = 0;
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#endif

  PASS();
}

/** @brief Test MsH3 context creation and destruction */
TEST test_msh3_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  enum c_abstract_http_error rc;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT(ctx != NULL);

  rc = http_config_init(&cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = http_msh3_config_apply(ctx, &cfg);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ctx = NULL;
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  ctx = NULL;
  g_mock_msh3_config_init_fail = 1;
  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_config_init_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_init(&ctx));
  g_mock_config_free_fail = 1;
  rc = http_msh3_context_free(ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_config_free_fail = 0;
#endif

  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 configuration application */
TEST test_msh3_config_application(void) {
  struct HttpConfig config;
  struct HttpTransportContext *ctx = NULL;
  enum c_abstract_http_error rc;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_config_init(&config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_config_apply(NULL, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_config_apply(ctx, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  config.verify_peer = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("MsH3Agent/1.0", &config.user_agent));
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  config.verify_peer = 1;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_msh3_extra_events = 1;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  g_mock_msh3_extra_events = 0;

  g_mock_msh3_config_open_fail = 1;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_msh3_config_open_fail = 0;

  /* OOM on user_agent strdup (2nd allocation: 1st is MsH3ConfigurationOpen) */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = http_msh3_config_apply(ctx, &config);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(ctx));
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

/** @brief Test MsH3 send parameter validation and URL parse errors */
TEST test_msh3_send_invalid_arguments(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpResponse *res = NULL;
  struct HttpRequest req;
  enum c_abstract_http_error rc;
  size_t i;

  rc = http_msh3_global_init();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_msh3_context_init(&ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = http_request_init(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  (void)i;

  rc = http_msh3_send(NULL, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_send(ctx, NULL, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_send(ctx, &req, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  rc = http_msh3_send(ctx, &req, &res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  {
    char *u = NULL;
    rc = c_abstract_http_strdup("invalid_url_without_scheme", &u);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    req.url = u;
    rc = http_msh3_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  for (i = 1; i <= 7; ++i) {
    g_mock_msh3_parse_url_alloc_fail = (int)i;
    res = NULL;
    if (i == 4) {
      rc = test_msh3_setup_request(&req, "https://127.0.0.1");
    } else if (i == 3) {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080");
    } else if (i == 5) {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1");
    } else if (i == 7) {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1:8080");
    } else {
      rc = test_msh3_setup_request(&req, "http://127.0.0.1/path");
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    rc = http_msh3_send(ctx, &req, &res);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
    ASSERT(res == NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    g_mock_msh3_parse_url_alloc_fail = 0;
  }

  /* Real malloc OOM inside msh3_url_alloc */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1:8443/hello"));
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = http_msh3_send(ctx, &req, &res);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(ctx));
  rc = http_msh3_global_cleanup();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Test MsH3 HTTP/3 methods, headers, cookies, multipart, and streams */
TEST test_msh3_features_and_streaming(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct msh3_chunk_test_state st;
  const char *val = NULL;
  enum HttpMethod methods[8];
  size_t m;

  methods[0] = HTTP_GET;
  methods[1] = HTTP_POST;
  methods[2] = HTTP_PUT;
  methods[3] = HTTP_DELETE;
  methods[4] = HTTP_PATCH;
  methods[5] = HTTP_HEAD;
  methods[6] = HTTP_OPTIONS;
  methods[7] = (enum HttpMethod)99;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_init(&ctx));

  /* Auto-config on first send when ctx->config is NULL (also no-slash URL) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1/notfound"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(404, res->status_code);
  ASSERT_EQ(7U, res->body_len);
  ASSERT_STR_EQ("msh3-ok", (const char *)res->body);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Configure User-Agent and CookieJar */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "k1", "v1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "k2", "v2"));
  cfg.cookie_jar = &jar;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("CAH-MsH3/2.0", &cfg.user_agent));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_config_apply(ctx, &cfg));

  /* Exercise all HTTP methods + Set-Cookie parsing */
  for (m = 0; m < 8; ++m) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              test_msh3_setup_request(
                  &req, (m == 0) ? "https://127.0.0.1/setcookie"
                                 : "https://127.0.0.1/setcookie_bad"));
    req.method = methods[m];
    if (m == 1) {
      req.body = (void *)"payload";
      req.body_len = 7;
    }
    if (m == 2) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req.headers, "User-Agent", "CustomUA/1.0"));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_add(&req.headers, "Cookie", "override=1"));
    }
    g_mock_msh3_extra_events = (m == 0) ? 1 : 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
    g_mock_msh3_extra_events = 0;
    if (m == 1) {
      ASSERT_EQ(201, res->status_code);
      ASSERT_STR_EQ("payload", (const char *)res->body);
      req.body = NULL;
      req.body_len = 0;
    } else {
      ASSERT_EQ(200, res->status_code);
    }
    if (m == 0) {
      val = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_get(&res->headers, "x-echo-ua", &val));
      ASSERT_STR_EQ("CAH-MsH3/2.0", val);
      val = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_headers_get(&res->headers, "x-echo-cookie", &val));
      ASSERT_STR_EQ("k1=v1; k2=v2", val);
      val = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_cookie_jar_get(&jar, "msh3", &val));
      ASSERT(val != NULL);
      ASSERT_STR_EQ("1", val);
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Negative HttpMethod validation */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1/neg"));
  req.method = (enum HttpMethod)(-1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Multipart req->parts flattening */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1:4433/upload"));
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Custom", "1"));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      http_request_add_part(&req, "field1", NULL, NULL, "hello-parts", 11));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(201, res->status_code);
  ASSERT(strstr((const char *)res->body, "hello-parts") != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Corrupt multipart parts (count > 0 && parts == NULL) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1/corrupt"));
  req.parts.count = 1;
  req.parts.parts = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_msh3_send(ctx, &req, &res));
  req.parts.count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Streaming upload (read_chunk) + streaming download (on_chunk) */
  memset(&st, 0, sizeof(st));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1/stream"));
  req.method = HTTP_POST;
  req.read_chunk = msh3_test_read_cb;
  req.read_chunk_user_data = &st;
  {
    struct msh3_chunk_test_state rx_st;
    memset(&rx_st, 0, sizeof(rx_st));
    req.on_chunk = msh3_test_on_chunk_cb;
    req.on_chunk_user_data = &rx_st;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
    ASSERT_EQ(201, res->status_code);
    ASSERT_EQ(8U, rx_st.total);
    ASSERT(res->body == NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    /* Streaming upload abort */
    memset(&st, 0, sizeof(st));
    st.abort_call = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, http_msh3_send(ctx, &req, &res));
    st.abort_call = 0;

    /* Streaming download abort */
    memset(&st, 0, sizeof(st));
    memset(&rx_st, 0, sizeof(rx_st));
    rx_st.abort_call = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, http_msh3_send(ctx, &req, &res));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_global_cleanup());
  PASS();
}

/** @brief Test MsH3 fault injection, DNS resolution, and multi multiplexing */
TEST test_msh3_fault_injection_and_multi(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  int step;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_global_init());

  /* Auto-config failure paths when ctx->config is NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1/ok"));

  g_mock_msh3_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_config_init_fail = 0;

  g_mock_msh3_config_open_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_config_open_fail = 0;

  g_mock_config_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_config_free_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(ctx));

  /* Re-create configured context with cookie jar */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "c", "1"));
  cfg.cookie_jar = &jar;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_config_apply(ctx, &cfg));

  /* Real getaddrinfo path (IP literal not matching 127.0.0.1/localhost) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://0.0.0.0:4433/ip"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Unresolvable host via real getaddrinfo */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      test_msh3_setup_request(&req, "https://nonexistent.invalid.:4433/x"));
  ASSERT(http_msh3_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);

  /* Switch back to 127.0.0.1/setcookie for OOM & mock toggles */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            test_msh3_setup_request(&req, "https://127.0.0.1/setcookie"));

  g_mock_msh3_res_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_res_alloc_fail = 0;

  g_mock_msh3_res_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_res_init_fail = 0;

  g_mock_headers_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_headers_init_fail = 0;

  g_mock_msh3_mutex_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_mutex_init_fail = 0;

  g_mock_msh3_cond_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_cond_init_fail = 0;

  g_mock_msh3_getaddrinfo_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_HOSTUNREACH, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_getaddrinfo_fail = 0;

  g_mock_msh3_getaddrinfo_null_result = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_msh3_getaddrinfo_null_result = 0;

  g_mock_msh3_zero_addrlen = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  g_mock_msh3_zero_addrlen = 0;

  g_mock_msh3_conn_open_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_conn_open_fail = 0;

  g_mock_msh3_request_open_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_request_open_fail = 0;

  g_mock_msh3_send_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_send_fail = 0;

  /* Streaming MsH3RequestSend failures (initial headers, chunk, FIN) */
  {
    struct msh3_chunk_test_state st;
    req.read_chunk = msh3_test_read_cb;
    req.read_chunk_user_data = &st;
    for (step = 1; step <= 3; ++step) {
      memset(&st, 0, sizeof(st));
      g_mock_msh3_send_fail = step;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
    }
    g_mock_msh3_send_fail = 0;
    req.read_chunk = NULL;
  }

  g_mock_msh3_header_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_header_alloc_fail = 0;

  g_mock_msh3_header_alloc_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_header_alloc_fail = 0;

  g_mock_msh3_header_add_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_header_add_fail = 0;

  g_mock_msh3_body_realloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_body_realloc_fail = 0;

  g_mock_msh3_cb_mutex_lock_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_cb_mutex_lock_fail = 0;

  g_mock_msh3_shutdown_error = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_shutdown_error = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_shutdown_error = 0;

  g_mock_msh3_send_lock_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_send_lock_fail = 0;

  g_mock_msh3_cond_wait_loop = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  g_mock_msh3_cond_wait_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_msh3_cond_wait_fail = 0;
  g_mock_msh3_cond_wait_loop = 0;

  /* Cleanup primitive failures */
  g_mock_cond_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_cond_free_fail = 0;

  g_mock_mutex_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_free_fail = 0;

  g_mock_msh3_send_fail = 1;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
  g_mock_msh3_send_fail = 0;

  /* Systematic OOM sweep across send + multipart send */
  for (step = 0; step <= 40; ++step) {
    enum c_abstract_http_error oom_rc;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = step;
    oom_rc = http_msh3_send(ctx, &req, &res);
    g_mock_alloc_fail = 0;
    if (oom_rc == C_ABSTRACT_HTTP_SUCCESS) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      break;
    }
  }

  /* Multipart OOM sweep */
  {
    struct HttpRequest mp_req;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              test_msh3_setup_request(&mp_req, "https://127.0.0.1/mp"));
    mp_req.method = HTTP_POST;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&mp_req.headers, "X-A", "B"));
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        http_request_add_part(&mp_req, "f", "a.txt", "text/plain", "data", 4));
    for (step = 0; step <= 30; ++step) {
      enum c_abstract_http_error oom_rc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      oom_rc = http_msh3_send(ctx, &mp_req, &res);
      g_mock_alloc_fail = 0;
      if (oom_rc == C_ABSTRACT_HTTP_SUCCESS) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
        break;
      }
    }
    /* Header copy failure + headers_free failure inside prepare_effective */
    g_mock_headers_free_fail = 1;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 5;
    ASSERT(http_msh3_send(ctx, &mp_req, &res) != C_ABSTRACT_HTTP_SUCCESS);
    g_mock_alloc_fail = 0;
    g_mock_headers_free_fail = 0;

    /* Flatten failure + headers_free failure inside prepare_effective */
    g_mock_headers_free_fail = 1;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 7;
    ASSERT(http_msh3_send(ctx, &mp_req, &res) != C_ABSTRACT_HTTP_SUCCESS);
    g_mock_alloc_fail = 0;
    g_mock_headers_free_fail = 0;

    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &mp_req, &res));
    g_mock_headers_init_fail = 0;

    g_mock_headers_free_fail = 1;
    g_mock_msh3_res_alloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_msh3_send(ctx, &mp_req, &res));
    g_mock_msh3_res_alloc_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &mp_req, &res));
    g_mock_headers_free_fail = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&mp_req));
  }

  /* Mutex/cond lock/signal/unlock failures inside callback & await */
  g_mock_mutex_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;

  /* Lock fail only on 2nd lock (msh3_await_slot lock after callback succeeds)
   */
  g_mock_mutex_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;

  g_mock_cond_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_cond_fail = 2;
  g_mock_mutex_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;
  g_mock_cond_fail = 0;

  g_mock_mutex_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;

  /* Unlock fail only on 2nd unlock (msh3_await_slot unlock) */
  g_mock_mutex_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;

  /* Wait loop mock cond_wait_fail + unlock error */
  g_mock_msh3_cond_wait_loop = 1;
  g_mock_msh3_cond_wait_fail = 1;
  g_mock_mutex_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;
  g_mock_msh3_cond_wait_fail = 0;
  g_mock_msh3_cond_wait_loop = 0;

  /* Real abstract_http_cond_wait failure (both clean unlock and failed unlock)
   */
  g_mock_msh3_cond_wait_loop = 2;
  g_mock_cond_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_send(ctx, &req, &res));
  g_mock_mutex_fail = 0;
  g_mock_cond_fail = 0;
  g_mock_msh3_cond_wait_loop = 0;

  /* Concurrent http_msh3_send_multi multiplexing tests */
  {
    struct HttpMultiRequest multi;
    struct HttpRequest req2;
    struct HttpFuture fut1;
    struct HttpFuture fut2;
    struct HttpFuture *futs[2];
    struct ModalityEventLoop *loop = NULL;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_init(&fut1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_init(&fut2));
    futs[0] = &fut1;
    futs[1] = &fut2;

    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_msh3_send_multi(NULL, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_msh3_send_multi(ctx, NULL, NULL, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_msh3_send_multi(ctx, NULL, &multi, NULL));

    /* Empty multi without and with loop */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_msh3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_msh3_send_multi(ctx, loop, &multi, futs));

    /* Concurrent 2-stream batch */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              test_msh3_setup_request(&req2, "https://127.0.0.1/two"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req2));

    /* OOM on slots array */
    g_mock_alloc_count = 0;
    g_mock_alloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              http_msh3_send_multi(ctx, loop, &multi, futs));
    g_mock_alloc_fail = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_msh3_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(1, fut1.is_ready);
    ASSERT_EQ(1, fut2.is_ready);
    ASSERT_EQ(200, fut1.response->status_code);
    ASSERT_EQ(200, fut2.response->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(fut1.response));
    free(fut1.response);
    fut1.response = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(fut2.response));
    free(fut2.response);
    fut2.response = NULL;

    /* Non-empty multi with loop == NULL */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_msh3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(fut1.response));
    free(fut1.response);
    fut1.response = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(fut2.response));
    free(fut2.response);
    fut2.response = NULL;

    /* Failure on 2nd stream in Phase 1 while 1st stream is already active */
    free(req2.url);
    req2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_msh3_send_multi(ctx, loop, &multi, futs));

    /* Restore req2.url and test Phase 2 await failure */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("https://127.0.0.1/two", &req2.url));
    g_mock_msh3_send_lock_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_msh3_send_multi(ctx, loop, &multi, futs));
    g_mock_msh3_send_lock_fail = 0;

    /* Loop wakeup failure */
    {
      struct ModalityEventLoop *ext_loop = NULL;
      struct HttpLoopHooks hooks;
      memset(&hooks, 0, sizeof(hooks));
      hooks.wakeup = msh3_mock_wakeup_fail;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_loop_init_external(&ext_loop, &hooks));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_msh3_send_multi(ctx, ext_loop, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(fut1.response));
      free(fut1.response);
      fut1.response = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(fut2.response));
      free(fut2.response);
      fut2.response = NULL;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_loop));
    }

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&fut1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&fut2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_msh3_context_free(ctx));
  g_mock_mutex_fail = 2;
  g_mock_mutex_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_msh3_global_cleanup());
  g_mock_mutex_fail = 0;
  g_mock_mutex_free_fail = 0;
  PASS();
}
#endif

/** @brief Suite definition for MsH3 backend */
SUITE(http_msh3_suite) {
  RUN_TEST(test_msh3_setup_request_coverage);
  RUN_TEST(test_msh3_global_lifecycle);
  RUN_TEST(test_msh3_context_lifecycle);
  RUN_TEST(test_msh3_config_application);
  RUN_TEST(test_msh3_send_invalid_arguments);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_msh3_features_and_streaming);
  RUN_TEST(test_msh3_fault_injection_and_multi);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_MSH3_H */
