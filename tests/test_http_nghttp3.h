/**
 * @file test_http_nghttp3.h
 * @brief Exhaustive unit and HTTP/3 loopback tests for the `nghttp3` + `ngtcp2`
 * backend.
 *
 * @author Samuel Marks
 */

#ifndef TEST_HTTP_NGHTTP3_H
#define TEST_HTTP_NGHTTP3_H

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
#include <c_abstract_http/http_nghttp3.h>
#include <c_abstract_http/http_types.h>
#include "abstract_http_test_helpers/mock_server.h"
#include "mock_alloc.h"
/* clang-format on */

struct ngh3_chunk_state {
  int calls;
  size_t total;
  int abort_call;
};

NO_DISCARD static c_abstract_http_error_t ngh3_test_read_cb(void *user_data,
                                                            void *buf,
                                                            size_t max_len,
                                                            size_t *out_read) {
  struct ngh3_chunk_state *st = (struct ngh3_chunk_state *)user_data;
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

NO_DISCARD static c_abstract_http_error_t
ngh3_test_on_chunk_cb(void *user_data, const void *chunk, size_t len) {
  struct ngh3_chunk_state *st = (struct ngh3_chunk_state *)user_data;
  (void)chunk;
  st->calls++;
  st->total += len;
  if (st->abort_call > 0 && st->calls >= st->abort_call) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t ngh3_mock_wakeup_fail(void *ctx) {
  (void)ctx;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_nghttp3_global_and_context_lifecycle(void) {
  struct HttpTransportContext *ctx = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_nghttp3_global_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_global_init());
  g_mock_nghttp3_global_init_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_context_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT(ctx != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_free(NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_nghttp3_context_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_context_init(&ctx));
  g_mock_nghttp3_context_init_fail = 0;

  g_mock_nghttp3_config_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_context_init(&ctx));
  g_mock_nghttp3_config_init_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  g_mock_config_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_context_free(ctx));
  g_mock_config_free_fail = 0;
  free(ctx);
  ctx = NULL;

  {
    int i;
    for (i = 0; i < 4; ++i) {
      c_abstract_http_error_t rc;
      ctx = NULL;
      g_mock_alloc_count = i;
      g_mock_alloc_fail = 1;
      rc = http_nghttp3_context_init(&ctx);
      g_mock_alloc_fail = 0;
      if (rc == C_ABSTRACT_HTTP_SUCCESS && ctx) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_free(ctx));
      }
    }
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

TEST test_nghttp3_h3_loopback_and_branches(void) {
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
  size_t m;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&config));
  config.http3_fallback = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_nghttp3_config_apply(NULL, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(NULL, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, NULL, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  config.timeout_ms = 0;
  config.connect_timeout_ms = 0;
  config.cookie_jar = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  config.timeout_ms = 5000;
  config.connect_timeout_ms = 2000;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  config.cookie_jar = &jar;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &config));

  /* Empty cookie jar + empty body */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com/empty";
  req.body = "unused";
  req.body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  req.url = NULL;
  req.body = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "c1", "v1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "c2", "v2"));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  for (m = 0; m < sizeof(bad_urls) / sizeof(bad_urls[0]); ++m) {
    req.url = (char *)(size_t)bad_urls[m];
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, &res));
  }
  req.url = (char *)"https://example.com";
  req.method = (enum HttpMethod)99;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, &res));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  for (m = 0; m < 9; ++m) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (m % 2 == 0) ? (char *)"https://example.com"
                           : (char *)"http://127.0.0.1:8443/echo";
    req.method = methods[m];
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "x-custom", "val"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "x-empty", ""));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
    ASSERT_EQ(200, res->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Short body (<=4 bytes) and static POST body + streaming read/on_chunk */
  {
    struct ngh3_chunk_state up_st;
    struct ngh3_chunk_state dn_st;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)"https://example.com/short";
    req.method = HTTP_POST;
    req.body = "hi";
    req.body_len = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
    ASSERT_EQ(2U, res->body_len);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    memset(&up_st, 0, sizeof(up_st));
    memset(&dn_st, 0, sizeof(dn_st));
    req.body = NULL;
    req.body_len = 0;
    req.url = (char *)"https://example.com/stream";
    req.read_chunk = ngh3_test_read_cb;
    req.read_chunk_user_data = &up_st;
    req.on_chunk = ngh3_test_on_chunk_cb;
    req.on_chunk_user_data = &dn_st;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
    ASSERT_EQ(10U, dn_st.total);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
    free(res);
    res = NULL;

    memset(&up_st, 0, sizeof(up_st));
    up_st.abort_call = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, http_nghttp3_send(ctx, &req, &res));

    memset(&up_st, 0, sizeof(up_st));
    memset(&dn_st, 0, sizeof(dn_st));
    dn_st.abort_call = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, http_nghttp3_send(ctx, &req, &res));

    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

  /* Connection / stream / engine failure branches */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = (char *)"https://example.com:59999/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CONNREFUSED,
            http_nghttp3_send(ctx, &req, &res));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_response_free_fail = 0;
#endif
  req.url = (char *)"https://example.com:59998/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  req.url = (char *)"https://example.com:59997/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  req.url = (char *)"https://example.com:59996/fail";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  req.url = (char *)"https://example.com/net-err";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  req.url = (char *)"https://example.com/ok";
  g_mock_nghttp3_conn_new_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_conn_new_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_conn_new_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_conn_new_fail = 0;

  g_mock_nghttp3_send_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_send_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_send_fail = 0;

  g_mock_nghttp3_response_alloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_response_alloc_fail = 0;

  g_mock_nghttp3_response_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_nghttp3_send(ctx, &req, &res));
  g_mock_nghttp3_response_init_fail = 0;

  g_mock_headers_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_headers_init_fail = 0;
#endif
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int i;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = (char *)"https://example.com/oom";
    req.method = HTTP_POST;
    req.body = "payload-data";
    req.body_len = 12;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_headers_add(&req.headers, "x-hdr", "1"));

    for (i = 0; i < 110; ++i) {
      c_abstract_http_error_t rc;
      res = NULL;
      g_mock_alloc_count = i;
      g_mock_alloc_fail = 1;
      rc = http_nghttp3_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (rc == C_ABSTRACT_HTTP_SUCCESS && res) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
      }
    }
    req.url = NULL;
    req.body = NULL;
    req.body_len = 0;
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
              http_nghttp3_send_multi(NULL, NULL, NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_nghttp3_send_multi(ctx, NULL, NULL, NULL));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_nghttp3_send_multi(ctx, NULL, &multi, NULL));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&r2));
    r1.url = (char *)"https://example.com/one";
    r2.url = (char *)"https://example.com/two";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &r2));

    futs[0] = &f1;
    futs[1] = &f2;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    g_mock_nghttp3_loop_wakeup_fail = 1;
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);
    g_mock_nghttp3_loop_wakeup_fail = 0;
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));

    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

    memset(&hooks, 0, sizeof(hooks));
    hooks.wakeup = ngh3_mock_wakeup_fail;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_init_external(&ext_loop, &hooks));
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, ext_loop, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_loop));

    r2.url = (char *)"invalid-url";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_nghttp3_fallback_proxy_and_parts(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpCookieJar jar;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const char *val = NULL;
  int step;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_init(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_set(&jar, "k1", "v1"));
  cfg.cookie_jar = &jar;

  cfg.proxy_url = (char *)"invalid-proxy";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"socks5://127.0.0.1:1080";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_nghttp3_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"masque://:443";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"http://127.0.0.1:8080";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
  cfg.proxy_url = (char *)"https://127.0.0.1:8443";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
  cfg.proxy_url = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("Ng3Agent/3.0", &cfg.user_agent));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("masque://127.0.0.1:4433", &cfg.proxy_url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("alice", &cfg.proxy_username));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("secret", &cfg.proxy_password));

  for (step = 0; step <= 20; ++step) {
    c_abstract_http_error_t apply_rc;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = step;
    apply_rc = http_nghttp3_config_apply(ctx, &cfg);
    g_mock_alloc_fail = 0;
    if (apply_rc == C_ABSTRACT_HTTP_SUCCESS) {
      break;
    }
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("https://origin.example/test", &req.url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&res->headers, "x-echo-ua", &val));
  ASSERT_STR_EQ("Ng3Agent/3.0", val);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  for (step = 0; step <= 65; ++step) {
    c_abstract_http_error_t oom_rc;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = step;
    oom_rc = http_nghttp3_send(ctx, &req, &res);
    g_mock_alloc_fail = 0;
    if (oom_rc == C_ABSTRACT_HTTP_SUCCESS) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      break;
    }
  }

  free(cfg.proxy_password);
  cfg.proxy_password = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "user-agent", "Custom/1.0"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Also test MASQUE proxy with cookie_jar + port 59998 open_bidi failure */
  cfg.http3_fallback = 0;
  free(req.url);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("https://origin.example/fail", &req.url));
  free(cfg.proxy_url);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("masque://127.0.0.1:59998", &cfg.proxy_url));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  free(cfg.proxy_url);
  cfg.proxy_url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("https://example.com/mp", &req.url));
  req.parts.count = 1;
  req.parts.parts = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, http_nghttp3_send(ctx, &req, &res));
  req.parts.count = 0;

  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Part-Hdr", "1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_add_part(&req, "doc", "a.txt", "text/plain",
                                  "ng3-part-body", 13));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT(strstr((const char *)res->body, "ng3-part-body") != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  for (step = 0; step <= 25; ++step) {
    c_abstract_http_error_t oom_rc;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = step;
    oom_rc = http_nghttp3_send(ctx, &req, &res);
    g_mock_alloc_fail = 0;
    if (oom_rc == C_ABSTRACT_HTTP_SUCCESS) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
      free(res);
      res = NULL;
      break;
    }
  }
  g_mock_headers_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_headers_init_fail = 0;

  g_mock_headers_free_fail = 1;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT(http_nghttp3_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
  g_mock_alloc_count = 3;
  ASSERT(http_nghttp3_send(ctx, &req, &res) != C_ABSTRACT_HTTP_SUCCESS);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
  g_mock_headers_free_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if !defined(__EMSCRIPTEN__) && !defined(C_ABSTRACT_HTTP_SINGLE_THREADED)
  {
    MockServerPtr srv = NULL;
    struct MockServerRequest srv_req;
    char fb_url[128];
    int port;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(srv));
    port = math_mock_server_get_port(srv);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(fb_url, sizeof(fb_url), "http://127.0.0.1:%d/net-err", port);
#else
    sprintf(fb_url, "http://127.0.0.1:%d/net-err", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(fb_url, &req.url));

    cfg.http3_fallback = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));

    cfg.http3_fallback = 1;
    free(cfg.proxy_url);
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        c_abstract_http_strdup("masque://127.0.0.1:443/", &cfg.proxy_url));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_nghttp3_send(ctx, &req, &res));
    g_mock_config_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
    g_mock_config_free_fail = 0;
    free(cfg.proxy_url);
    cfg.proxy_url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));

    for (step = 0; step <= 80; ++step) {
      c_abstract_http_error_t frc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      frc = http_nghttp3_send(ctx, &req, &res);
      g_mock_alloc_fail = 0;
      if (frc == C_ABSTRACT_HTTP_SUCCESS) {
        ASSERT_EQ(200, res->status_code);
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
        free(res);
        res = NULL;
        memset(&srv_req, 0, sizeof(srv_req));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  mock_server_wait_for_request(srv, &srv_req));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  mock_server_request_cleanup(&srv_req));
        break;
      }
    }

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
    srv = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(&srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start(srv));
    port = math_mock_server_get_port(srv);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(fb_url, sizeof(fb_url), "http://127.0.0.1:%d/net-err", port);
#else
    sprintf(fb_url, "http://127.0.0.1:%d/net-err", port);
#endif
    free(req.url);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(fb_url, &req.url));
    g_mock_config_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_nghttp3_send(ctx, &req, &res));
    g_mock_config_free_fail = 0;
    memset(&srv_req, 0, sizeof(srv_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_wait_for_request(srv, &srv_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(&srv_req));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(srv));
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_cookie_jar_free(&jar));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_free(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_cleanup());
  PASS();
}
#endif

/** @brief Suite definition for nghttp3 + ngtcp2 backend */
SUITE(http_nghttp3_suite) {
  RUN_TEST(test_nghttp3_global_and_context_lifecycle);
  RUN_TEST(test_nghttp3_h3_loopback_and_branches);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_nghttp3_fallback_proxy_and_parts);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_NGHTTP3_H */
