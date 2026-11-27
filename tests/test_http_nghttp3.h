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
#include "../src/crypto_utils.h"
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

  free(cfg.user_agent);
  cfg.user_agent = NULL;
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
    c89stringutils_snprintf(fb_url, sizeof(fb_url),
                            "http://127.0.0.1:%d/net-err", port);
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
    c89stringutils_snprintf(fb_url, sizeof(fb_url),
                            "http://127.0.0.1:%d/net-err", port);
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

TEST test_nghttp3_tls_redirects_clen_and_multiplexing(void) {
  struct HttpTransportContext *ctx = NULL;
  struct HttpConfig cfg;
  struct HttpRequest req1;
  struct HttpRequest req2;
  struct HttpResponse *res = NULL;
  struct ngh3_chunk_state cst;
  const char *hval = NULL;
  static const unsigned char k_spki[] = {0x30, 0x0d, 0x30, 0x05, 0x06,
                                         0x03, 0x2b, 0x65, 0x70, 0x03,
                                         0x04, 0x00, 0x01, 0x02, 0x03};
  unsigned char spki_hash[32];
  char *b64_pin = NULL;
  size_t b64_len = 0;
  char pin_str[128];
  int step;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_global_init());
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_context_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));

  /* TLS config validation & SPKI SHA-256 pinning */
  cfg.tls_version_mask = HTTP_TLS_VERSION_1_2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, http_nghttp3_config_apply(ctx, &cfg));
  cfg.tls_version_mask = HTTP_TLS_VERSION_1_3;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_sha256_oneshot(k_spki, sizeof(k_spki), spki_hash));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_base64_encode(spki_hash, 32, &b64_pin, &b64_len));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(pin_str, sizeof(pin_str), "sha256//%s", b64_pin);
#else
  c89stringutils_snprintf(pin_str, sizeof(pin_str), "sha256//%s", b64_pin);
#endif
  free(b64_pin);
  cfg.cert_pin_sha256 = pin_str;
  cfg.http3_fallback = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_config_apply(ctx, &cfg));
  cfg.cert_pin_sha256 = NULL;

  /* Peer CN mismatch & bad SPKI pin rejection */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
  req1.url = (char *)"https://sub.bad-cn.invalid/check";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, http_nghttp3_send(ctx, &req1, &res));

  /* expected_body_len streaming upload + 301 & 307 redirects + redirect loop */
  memset(&cst, 0, sizeof(cst));
  req1.method = HTTP_POST;
  req1.url = (char *)"https://example.com/redir-301";
  req1.read_chunk = ngh3_test_read_cb;
  req1.read_chunk_user_data = &cst;
  req1.expected_body_len = 10;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req1, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&res->headers, "x-echo-method", &hval));
  ASSERT_STR_EQ("GET", hval);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  memset(&cst, 0, sizeof(cst));
  req1.method = HTTP_POST;
  req1.url = (char *)"https://example.com/redir-307";
  req1.read_chunk = ngh3_test_read_cb;
  req1.read_chunk_user_data = &cst;
  req1.expected_body_len = 10;
  /*Note: cst stops at calls >= 2; reset via 2-hop test using body for 307 +
   * upload for /upload */
  req1.url = (char *)"https://example.com/upload";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req1, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&res->headers, "x-echo-clen", &hval));
  ASSERT_STR_EQ("10", hval);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req1.read_chunk = NULL;
  req1.expected_body_len = 0;
  req1.body = "keep";
  req1.body_len = 4;
  req1.url = (char *)"https://example.com/redir-307";
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_nghttp3_send(ctx, &req1, &res));
  ASSERT_EQ(200, res->status_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&res->headers, "x-echo-method", &hval));
  ASSERT_STR_EQ("POST", hval);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  req1.method = HTTP_GET;
  req1.body = NULL;
  req1.body_len = 0;
  req1.url = (char *)"https://example.com/redir-loop";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_LOOP, http_nghttp3_send(ctx, &req1, &res));

  /* Concurrent bidirectional multi-stream multiplexing on ModalityEventLoop */
  {
    struct ModalityEventLoop *loop = NULL;
    struct HttpMultiRequest multi;
    struct HttpFuture f1;
    struct HttpFuture f2;
    struct HttpFuture *futs[2];

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&loop));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_init(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));
    req1.url = (char *)"https://example.com/mux-1";
    req2.url = (char *)"https://example.com/mux-2";
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_add(&multi, &req2));
    futs[0] = &f1;
    futs[1] = &f2;

    /* 1. Same-authority concurrent streams (stream_id 0 and 4 on shared conn)
     */
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(1, f1.is_ready);
    ASSERT_EQ(1, f2.is_ready);
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        http_headers_get(&f1.response->headers, "x-echo-stream-id", &hval));
    ASSERT_STR_EQ("0", hval);
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        http_headers_get(&f2.response->headers, "x-echo-stream-id", &hval));
    ASSERT_STR_EQ("4", hval);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

    /* 2. Distinct authorities + redirect inside multi + invalid 2nd URL */
    req1.url = (char *)"https://example.com/redir-301";
    req2.url = (char *)"https://other.example.com/mux-2";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(200, f1.response->status_code);
    ASSERT_EQ(200, f2.response->status_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

    /* 3. Invalid 2nd URL & error propagation with caller loop */
    req1.url = (char *)"https://example.com/mux-1";
    req2.url = (char *)"https://sub.bad-cn.invalid/mux-2";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    if (f1.response) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
    }

    /* 4. Drive loopback failure inside mux group -> fallback to single send */
    req1.url = (char *)"https://example.com/mux-1";
    req2.url = (char *)"https://example.com/net-err";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    if (f1.response) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
      free(f1.response);
    }

    /* 4b. response_free failure on submit_stream fail & fallback cleanup + stop
     * fail */
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    g_mock_response_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    req1.url = (char *)"https://example.com:59998/open-bidi-fail";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    g_mock_response_free_fail = 0;

    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(3, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, loop, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(0, 0));

    /* 4c. Multipart slot in multi + slot-1 multipart + error branches */
    req1.url = (char *)"https://example.com/mp-multi";
    req1.method = HTTP_POST;
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        http_request_add_part(&req1, "f", "a.txt", "text/plain", "hi", 2));
    req2.url = (char *)"https://example.com/mux-2";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);

    /* Slot 0 normal + Slot 1 multipart & broken Slot 0 multipart */
    req2.method = HTTP_POST;
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        http_request_add_part(&req2, "f", "b.txt", "text/plain", "ok", 2));
    req1.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req1));
    req1.url = (char *)"https://example.com/mux-1";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
    free(f1.response);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
    free(f2.response);
    req2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req2));

    /* Broken multipart on slot 0, bad URLs, CN fail on slot 0,
     * headers_init_fail */
    req1.parts.count = 1;
    req1.parts.parts = NULL;
    req2.url = (char *)"https://example.com/mux-2";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    req1.parts.count = 0;

    req1.url = (char *)"badurl";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    req1.url = (char *)"https://sub.bad-cn.invalid/mux-1";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    req1.url = (char *)"https://example.com/mux-1";
    req2.url = (char *)"badurl";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));
    req2.url = (char *)"https://example.com:59998/open-bidi-fail";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    g_mock_response_free_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    g_mock_response_free_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));

    /* Failing slot 0 inside multi when slot 1 was also queued */
    req1.url = (char *)"https://example.com/net-err";
    req2.url = (char *)"https://example.com/mux-2";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));

    req1.url = (char *)"https://example.com/mux-1";
    memset(&f1, 0, sizeof(f1));
    memset(&f2, 0, sizeof(f2));
    g_mock_headers_init_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              http_nghttp3_send_multi(ctx, NULL, &multi, futs));
    g_mock_headers_init_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));

    {
      struct ModalityEventLoop *ext_l = NULL;
      struct HttpLoopHooks ext_h;
      memset(&ext_h, 0, sizeof(ext_h));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                http_loop_init_external(&ext_l, &ext_h));
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                http_nghttp3_send_multi(ctx, ext_l, &multi, futs));
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(4, 1));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                http_nghttp3_send_multi(ctx, ext_l, &multi, futs));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(0, 0));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f1));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_future_free(&f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_l));
    }

    /* 5. Loop op failures & OOM sweep across send_multi */
    req2.url = (char *)"https://example.com/mux-2";
    for (step = 1; step <= 5; ++step) {
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(step, 0));
      ASSERT(http_nghttp3_send_multi(ctx,
                                     (step == 1 || step == 5) ? NULL : loop,
                                     &multi, futs) != C_ABSTRACT_HTTP_SUCCESS);
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_loop_op_fail(0, 0));
    }

    for (step = 0; step <= 45; ++step) {
      c_abstract_http_error_t mrc;
      memset(&f1, 0, sizeof(f1));
      memset(&f2, 0, sizeof(f2));
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      mrc = http_nghttp3_send_multi(ctx, NULL, &multi, futs);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (f1.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f1.response));
        free(f1.response);
      }
      if (f2.response) {
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(f2.response));
        free(f2.response);
      }
      if (mrc == C_ABSTRACT_HTTP_SUCCESS) {
        break;
      }
    }

    req2.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_multi_request_free(&multi));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(loop));
  }

  req1.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req1));
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
  RUN_TEST(test_nghttp3_tls_redirects_clen_and_multiplexing);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_HTTP_NGHTTP3_H */
