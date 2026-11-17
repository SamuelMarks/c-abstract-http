/**
 * @file test_http_picoquic.h
 * @brief Exhaustive unit and HTTP/3 loopback tests for the picoquic backend.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_PICOQUIC_H
#define TEST_HTTP_PICOQUIC_H

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
#include <c_abstract_http/http_picoquic.h>
#include <c_abstract_http/http_types.h>
#include "mock_alloc.h"
/* clang-format on */

struct pq_chunk_state {
  int calls;
  size_t total;
  int abort_call;
};

static enum c_abstract_http_error
pq_test_read_cb(void *user_data, void *buf, size_t max_len, size_t *out_read) {
  struct pq_chunk_state *st = (struct pq_chunk_state *)user_data;
  if (st->abort_call > 0 && st->calls + 1 >= st->abort_call) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  if (st->calls >= 2) {
    *out_read = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  st->calls++;
  if (max_len > 5) {
    max_len = 5;
  }
  memcpy(buf, "chunk", max_len);
  *out_read = max_len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
pq_test_on_chunk_cb(void *user_data, const void *chunk, size_t len) {
  struct pq_chunk_state *st = (struct pq_chunk_state *)user_data;
  (void)chunk;
  st->calls++;
  st->total += len;
  if (st->abort_call > 0 && st->calls >= st->abort_call) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error pq_mock_wakeup_fail(void *ctx) {
  (void)ctx;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_picoquic_global_lifecycle(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

TEST test_picoquic_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT(ctx != NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_picoquic_quic_null_on_free = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(ctx));
  g_mock_picoquic_quic_null_on_free = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  g_mock_config_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_context_free(ctx));
  g_mock_config_free_fail = 0;
  free(ctx);
  ctx = NULL;

  g_mock_picoquic_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_context_init(&ctx));
  g_mock_picoquic_config_init_fail = 0;

  g_mock_picoquic_create_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_context_init(&ctx));
  g_mock_config_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_context_init(&ctx));
  g_mock_config_free_fail = 0;
  g_mock_picoquic_create_fail = 0;

  {
    int i;
    for (i = 0; i < 4; ++i) {
      enum c_abstract_http_error rc;
      ctx = NULL;
      g_mock_alloc_count = i;
      g_mock_alloc_fail = 1;
      rc = http_picoquic_context_init(&ctx);
      g_mock_alloc_fail = 0;
      if (rc == C_ABSTRACT_HTTP_SUCCESS && ctx) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(ctx));
      }
    }
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

TEST test_picoquic_config_application(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_picoquic_config_apply(NULL, &config));

  config.version_mask = HTTP_VERSION_3;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));

  config.version_mask = HTTP_VERSION_1_1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

TEST test_picoquic_h3_loopback_and_branches(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig config;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const enum HttpMethod methods[9] = {HTTP_GET,     HTTP_POST,  HTTP_PUT,
                                      HTTP_DELETE,  HTTP_PATCH, HTTP_HEAD,
                                      HTTP_OPTIONS, HTTP_TRACE, HTTP_CONNECT};
  const char *bad_urls[] = {NULL,
                            "not-a-url",
                            "ftp://example.com/a",
                            "httpz://example.com/a",
                            "httz://example.com/a",
                            "https:///nohost",
                            "https://example.com:0/a",
                            "https://example.com:99999/a"};
  const char *err_paths[] = {
      "https://example.com/net-close",      "https://example.com/app-close",
      "https://example.com/trunc-flen",     "https://example.com/trunc-kl",
      "https://example.com/trunc-vl",       "https://example.com/trunc-frame",
      "https://example.com/varint8",        "https://example.com/bad-qpack",
      "https://example.com/bad-hdr-marker", "https://example.com/trunc-hdr",
      "https://example.com/trunc-val",      "https://example.com/trunc-varint2",
      "https://example.com/trunc-varint4"};
  size_t m;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));

  /* Test NULL cookie_jar and empty cookie_jar first */
  config.cookie_jar = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com/empty";
  req.body = "unused";
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  config.cookie_jar = &jar;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(ctx));
  ctx = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "c1", "v1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "c2", "v2"));
  config.cookie_jar = &jar;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, &req, NULL));
  /* Not configured yet */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, &req, &res));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    struct HttpTransportContext *null_quic_ctx = NULL;
    g_mock_picoquic_quic_null_on_free = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_picoquic_context_init(&null_quic_ctx));
    g_mock_picoquic_quic_null_on_free = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_picoquic_config_apply(null_quic_ctx, &config));
    req.url = (char *)"https://example.com";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_picoquic_send(null_quic_ctx, &req, &res));
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_picoquic_context_free(null_quic_ctx));
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_config_apply(ctx, &config));

  /* URL validation branches */
  for (m = 0; m < sizeof(bad_urls) / sizeof(bad_urls[0]); ++m) {
    req.url = (char *)(size_t)bad_urls[m];
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, &req, &res));
  }
  req.url = "https://example.com";
  req.method = (enum HttpMethod)99;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_picoquic_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* All 9 HTTP methods + http:// scheme + port + no-path */
  for (m = 0; m < 9; ++m) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (m % 2 == 0) ? (char *)"https://example.com"
                           : (char *)"http://127.0.0.1:8443/echo";
    req.method = methods[m];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "x-custom", "val"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "x-empty", ""));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
    ASSERT(res != NULL);
    ASSERT_EQ(200, res->status_code);
    ASSERT(res->body != NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Verify Set-Cookie jar extraction */
  {
    const char *cval = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_cookie_jar_get(&jar, "pq_sid", &cval));
    ASSERT_STR_EQ("abc", cval);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_cookie_jar_get(&jar, "pq_plain", &cval));
    ASSERT_STR_EQ("xyz", cval);
  }

  /* Large (>16384 byte) POST body to exercise 4-byte varint + buffer growth */
  {
    char *big_body = (char *)malloc(17000);
    ASSERT(big_body != NULL);
    memset(big_body, 'Q', 17000);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)"https://example.com/grease4";
    req.method = HTTP_POST;
    req.body = big_body;
    req.body_len = 17000;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
    ASSERT(res != NULL);
    ASSERT_EQ(17000U, res->body_len);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    req.body = NULL;
    req.body_len = 0;
    free(big_body);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Empty body response + streaming read_chunk + streaming on_chunk + aborts */
  {
    struct pq_chunk_state up_st;
    struct pq_chunk_state dn_st;
    memset(&up_st, 0, sizeof(up_st));
    memset(&dn_st, 0, sizeof(dn_st));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)"https://example.com/empty";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
    ASSERT_EQ(0U, res->body_len);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    req.url = (char *)"https://example.com/stream";
    req.method = HTTP_POST;
    req.read_chunk = pq_test_read_cb;
    req.read_chunk_user_data = &up_st;
    req.on_chunk = pq_test_on_chunk_cb;
    req.on_chunk_user_data = &dn_st;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_send(ctx, &req, &res));
    ASSERT_EQ(10U, dn_st.total);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    memset(&up_st, 0, sizeof(up_st));
    up_st.abort_call = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
              http_picoquic_send(ctx, &req, &res));

    memset(&up_st, 0, sizeof(up_st));
    memset(&dn_st, 0, sizeof(dn_st));
    dn_st.abort_call = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
              http_picoquic_send(ctx, &req, &res));

    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Connection / stream / malformed frame error paths */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com:59999/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CONNREFUSED,
            http_picoquic_send(ctx, &req, &res));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
#endif
  req.url = (char *)"https://example.com:59998/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_send(ctx, &req, &res));
  req.url = (char *)"https://example.com:59997/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_send(ctx, &req, &res));

  for (m = 0; m < sizeof(err_paths) / sizeof(err_paths[0]); ++m) {
    req.url = (char *)(size_t)err_paths[m];
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_send(ctx, &req, &res));
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  req.url = (char *)"https://example.com/trunc-frame";
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
#endif
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Exhaustive OOM sweep across URL parse, H3 serialize, stream cb, decode */
  {
    int i;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)"https://example.com/oom";
    req.method = HTTP_POST;
    req.body = "payload-data";
    req.body_len = 12;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "x-hdr", "1"));

    g_mock_picoquic_response_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_picoquic_send(ctx, &req, &res));
    g_mock_picoquic_response_init_fail = 0;

    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_picoquic_send(ctx, &req, &res));
    g_mock_headers_init_fail = 0;

    for (i = 0; i < 130; ++i) {
      enum c_abstract_http_error rc;
      res = NULL;
      g_mock_alloc_count = i;
      g_mock_alloc_fail = 1;
      rc = http_picoquic_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (rc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
    }

    /* Also sweep OOM with read_chunk */
    {
      struct pq_chunk_state up_st;
      req.body = NULL;
      req.body_len = 0;
      req.read_chunk = pq_test_read_cb;
      req.read_chunk_user_data = &up_st;
      for (i = 0; i < 60; ++i) {
        enum c_abstract_http_error rc;
        memset(&up_st, 0, sizeof(up_st));
        res = NULL;
        g_mock_alloc_count = i;
        g_mock_alloc_fail = 1;
        rc = http_picoquic_send(ctx, &req, &res);
        g_mock_alloc_fail = 0;
        if (rc == C_ABSTRACT_HTTP_SUCCESS && res) {
          ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
          free(res);
        }
      }
    }

    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }
#endif

  /* Multi-send coverage */
  {
    struct HttpMultiRequest multi;
    struct HttpRequest r1, r2;
    struct HttpFuture f1, f2;
    struct HttpFuture *futs[2];
    struct ModalityEventLoop *loop = NULL;
    struct ModalityEventLoop *ext_loop = NULL;
    struct HttpLoopHooks hooks;

    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_picoquic_send_multi(NULL, NULL, NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_picoquic_send_multi(ctx, NULL, NULL, NULL));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_picoquic_send_multi(ctx, NULL, &multi, NULL));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
    r1.url = (char *)"https://example.com/one";
    r2.url = (char *)"https://example.com/two";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));

    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    futs[0] = &f1;
    futs[1] = &f2;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_picoquic_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(1, f1.is_ready);
    ASSERT_EQ(1, f2.is_ready);
    ASSERT_EQ(200, f1.response->status_code);
    ASSERT_EQ(200, f2.response->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));

    /* Succeeding multi-send with NULL loop */
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_picoquic_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

    /* Loop wakeup failure */
    memset(&hooks, 0, sizeof(hooks));
    hooks.wakeup = pq_mock_wakeup_fail;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_init_external(&ext_loop, &hooks));
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_picoquic_send_multi(ctx, ext_loop, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_loop));

    /* Partial failure in multi */
    r2.url = (char *)"invalid-url";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_picoquic_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);

    r1.url = NULL;
    r2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&r2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&config));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_picoquic_global_cleanup());
  PASS();
}

/** @brief Suite definition for picoquic backend */
SUITE(http_picoquic_suite) {
  RUN_TEST(test_picoquic_global_lifecycle);
  RUN_TEST(test_picoquic_context_lifecycle);
  RUN_TEST(test_picoquic_config_application);
  RUN_TEST(test_picoquic_h3_loopback_and_branches);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_PICOQUIC_H */
