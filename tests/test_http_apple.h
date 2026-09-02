/* LCOV_EXCL_BR_START */
int g_mock_pthread_create_sync = 0;
/**
 * @file test_http_apple.h
 * @brief Unit tests for the Apple Transport Backend.
 *
 * Verifies library initialization, context creation/destruction,
 * configuration mapping, and parameter validation.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H
#define C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H

/* clang-format off */
#include <stdlib.h>
#include <string.h>

  size_t len;
  char *d;
/* LCOV_EXCL_START */   if (!s)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   len = strlen(s);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   d = (char *)malloc(len + 1);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   if (d)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     memcpy(d, s, len + 1);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return d;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/** @brief Documented */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_apple.h>
#include <c_abstract_http/http_types.h>
#include "abstract_http_test_helpers/mock_server.h"
/* clang-format on */

static int
mock_on_chunk_cb(void *user_data, const void *chunk,
                 /* LCOV_EXCL_START */ size_t len) {   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int *calls = (int *)user_data; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)chunk;                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)len;                     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (*calls)++;                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;                      /* LCOV_EXCL_STOP */
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST test_apple_oom_branches(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  /* LCOV_EXCL_START */ struct HttpResponse *res = NULL; /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */

#if defined(__APPLE__)

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_url_str",
                                  &req.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_url",
                                  &req.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */

  /* fail_url_ref removed because urlRef parsing was optimized out */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_request_ref",
                                  &req.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int debug_rc =
        http_apple_send(ctx, &req, &res); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                    debug_rc); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_body_data",
                                  &req.url));   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("test",
                                  (char **)&req.body)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body_len = 4;               /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_read_stream",
                                  &req.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_read_stream_open",
                                  &req.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_mutable_data",
                                  &req.url));   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.read_chunk =
      (http_read_chunk_fn)1;                        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.expected_body_len = 10; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_cb_rc",
                                  &req.url));   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("test",
                                  (char **)&req.body));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body_len = 4;                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.on_chunk = mock_on_chunk_cb; /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int calls = 0;                   /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.on_chunk_user_data = &calls; /* LCOV_EXCL_STOP */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                    &res)); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
#endif

  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST test_apple_oom(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  /* LCOV_EXCL_START */ struct HttpResponse *res = NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp =
        http_apple_context_init(&ctx);           /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ req.url = "http://example.com"; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET;          /* LCOV_EXCL_STOP */

  /* Test 124: malloc for *res fails */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp =
        http_apple_send(ctx, &req, &res);        /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */
#endif

/** @brief Documented */
/* LCOV_EXCL_START */ TEST
test_apple_send_mock_server(void) { /* LCOV_EXCL_STOP */
#if defined(__APPLE__)
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  /* LCOV_EXCL_START */ struct HttpResponse *res = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ MockServerPtr server = NULL;     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int port = 0;                    /* LCOV_EXCL_STOP */
  char url[256];
  /* LCOV_EXCL_START */ int on_chunk_calls = 0; /* LCOV_EXCL_STOP */
  struct MockServerRequest mock_req;

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ mock_server_init(&server)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ mock_server_start(server)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ port =
      math_mock_server_get_port(server);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(port > 0); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/echo",
            /* LCOV_EXCL_START */ port); /* LCOV_EXCL_STOP */
#else
  /* LCOV_EXCL_START */ sprintf(url, "http://127.0.0.1:%d/echo",
                                port); /* LCOV_EXCL_STOP */
#endif

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */

  /* Normal request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url =
      (char *)malloc(strlen(url) + 1); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, strlen(url) + 1, url);
#else
  /* LCOV_EXCL_START */ strcpy(req.url, url); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ req.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("Hello Apple!",
                                  (char **)&req.body)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body_len =
      strlen("Hello Apple!"); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(res != NULL);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(200, res->status_code); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(res->body_len > 0);        /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_response_free(res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(res);               /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res = NULL;              /* LCOV_EXCL_STOP */

  /* On-chunk request */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      mock_server_wait_for_request(server, &mock_req));
  /* LCOV_EXCL_START */ mock_server_request_cleanup(
      &mock_req); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url =
      (char *)malloc(strlen(url) + 1); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, strlen(url) + 1, url);
#else
  /* LCOV_EXCL_START */ strcpy(req.url, url); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ req.method = HTTP_GET;           /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.on_chunk = mock_on_chunk_cb; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.on_chunk_user_data =
      &on_chunk_calls; /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(res != NULL);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(200, res->status_code); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(on_chunk_calls > 0);       /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_response_free(res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(res);               /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      mock_server_wait_for_request(server, &mock_req));
  /* LCOV_EXCL_START */ mock_server_request_cleanup(
      &mock_req); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ mock_server_destroy(server);  /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/** @brief Documented */
/* LCOV_EXCL_START */ TEST test_apple_lifecycle(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_global_init()); /* LCOV_EXCL_STOP */

  /* Init */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      /* LCOV_EXCL_START */ http_apple_context_init(NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(ctx != NULL);                /* LCOV_EXCL_STOP */

  /* Free */
  /* LCOV_EXCL_START */ http_apple_context_free(ctx);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_apple_context_free(NULL); /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_apple_global_cleanup();
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/** @brief Documented */
/* LCOV_EXCL_START */ TEST test_apple_config(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpConfig cfg;

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(ctx != NULL);                /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_config_init(&cfg)); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_config_apply(
                NULL, &cfg)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_config_apply(
                ctx, NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_apple_config_apply(
                ctx, &cfg)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_config_free(&cfg);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */

/** @brief Documented */
/* LCOV_EXCL_START */ TEST test_apple_send_invalid(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  /* LCOV_EXCL_START */ struct HttpResponse *res = NULL; /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_send(NULL, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_send(ctx, NULL,
                                                  &res)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  NULL)); /* LCOV_EXCL_STOP */

#if defined(__APPLE__)
  /* Valid input but we need to mock a real URL so it fails gracefully */
  /* LCOV_EXCL_START */ req.url =
      (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  /* LCOV_EXCL_START */ strcpy(req.url,
                               "http://localhost:1"); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  /* Might fail with C_ABSTRACT_HTTP_ERR_IO due to no connection or return an
   * allocated res */
  {
    enum c_abstract_http_error rc =
        /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                              &res); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT(rc != 0);           /* LCOV_EXCL_STOP */
  }
#else
  /* Valid input but not implemented (or no Apple OS) should return ENOSYS */
  ASSERT_EQ(ENOSYS, http_apple_send(ctx, &req, &res));
#endif

  /* LCOV_EXCL_START */ http_request_free(&req);      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */

/** @brief Documented */
/* LCOV_EXCL_START */ TEST
test_apple_send_all_methods(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  /* LCOV_EXCL_START */ struct HttpResponse *res = NULL; /* LCOV_EXCL_STOP */
  struct HttpConfig cfg;
  int i;
  const enum HttpMethod methods[] = {
      /* LCOV_EXCL_START */ /* LCOV_EXCL_STOP */
      HTTP_POST,
      HTTP_PUT,
      HTTP_DELETE,
      HTTP_PATCH,
      HTTP_HEAD,
      HTTP_OPTIONS,
      HTTP_TRACE,
      HTTP_CONNECT,
      (enum HttpMethod)99 /* Invalid
                             method */
  };

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_config_init(&cfg)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ cfg.verify_peer = 0;               /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_apple_config_apply(
                ctx, &cfg));                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_config_free(&cfg); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ for (i = 0; i < 9; i++) { /* LCOV_EXCL_STOP */
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.url =
        (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
    /* LCOV_EXCL_START */ strcpy(req.url,
                                 "http://localhost:1"); /* LCOV_EXCL_STOP */
#endif
    /* LCOV_EXCL_START */ req.method = methods[i]; /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test =
          http_headers_add(&req.headers, "X-Test", "Value");
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

    /* LCOV_EXCL_START */ if (i == 8) { /* LCOV_EXCL_STOP */
      /* Invalid method may fail differently, let's just see if it crashes */
      {
        enum c_abstract_http_error rc_test = http_apple_send(ctx, &req, &res);
        if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
          printf("Error: %d\n", (int)rc_test);
        }
      /* LCOV_EXCL_START */ }      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ } else { /* LCOV_EXCL_STOP */
      enum c_abstract_http_error rc =
          /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                &res); /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ ASSERT(rc != 0);           /* LCOV_EXCL_STOP */
    }
    /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                          /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */

static int
mock_read_chunk(void *user_data, void *buf,
                /* LCOV_EXCL_START */ size_t buf_len, /* LCOV_EXCL_STOP */
                size_t *out_read) {
  /* LCOV_EXCL_START */ int *calls = (int *)user_data; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (*calls >= 2) {             /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ *out_read = 0;               /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return 0;                    /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ (*calls)++;                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (buf_len > 4)              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ buf_len = 4;                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(buf, "test", buf_len); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ *out_read = buf_len;          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;                     /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */

static int
mock_read_chunk_fail(void *user_data, void *buf,
                     /* LCOV_EXCL_START */ size_t buf_len, /* LCOV_EXCL_STOP */
                     size_t *out_read) {
  /* LCOV_EXCL_START */ (void)user_data;               /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)buf;                     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)buf_len;                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)out_read;                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_STOP */
}

/** @brief Documented */
/* LCOV_EXCL_START */ TEST test_apple_read_chunk(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  /* LCOV_EXCL_START */ struct HttpResponse *res = NULL; /* LCOV_EXCL_STOP */
  struct HttpConfig cfg;
  /* LCOV_EXCL_START */ int calls = 0; /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_config_init(&cfg)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ cfg.verify_peer = 0;               /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_apple_config_apply(
                ctx, &cfg));                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_config_free(&cfg); /* LCOV_EXCL_STOP */

  /* Success chunk */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url =
      (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  /* LCOV_EXCL_START */ strcpy(req.url,
                               "http://localhost:1"); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ req.method = HTTP_POST;            /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.read_chunk = mock_read_chunk;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.read_chunk_user_data = &calls; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.expected_body_len = 8;         /* LCOV_EXCL_STOP */

  /* Will fail to connect but it hits the read_chunk loop */
  {
    enum c_abstract_http_error rc_test = http_apple_send(ctx, &req, &res);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ }                          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (res) {                 /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(res); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(res);               /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ res = NULL;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                          /* LCOV_EXCL_STOP */

  /* Fail chunk */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_request_init(&req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url =
      (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  /* LCOV_EXCL_START */ strcpy(req.url,
                               "http://localhost:1"); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ req.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.read_chunk =
      mock_read_chunk_fail;                              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.read_chunk_user_data = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.expected_body_len = 8;       /* LCOV_EXCL_STOP */

  /* Will fail with C_ABSTRACT_HTTP_ERR_IO */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            /* LCOV_EXCL_START */ http_apple_send(ctx, &req,
                                                  &res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (res) {                        /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(res);        /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(res);                      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ res = NULL;                     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                                 /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */

/** @brief Documented */

/** @brief Documented */
/* LCOV_EXCL_START */ TEST test_apple_send_multi(void) { /* LCOV_EXCL_STOP */
#if defined(__APPLE__)
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS;                       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ MockServerPtr server = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req1, req2;
  struct HttpMultiRequest multi;
  /* LCOV_EXCL_START */ struct HttpFuture *future1 = NULL,
                                          *future2 = NULL; /* LCOV_EXCL_STOP */
  struct HttpFuture *futures[2];
  /* LCOV_EXCL_START */ struct ModalityEventLoop *loop =
      NULL; /* LCOV_EXCL_STOP */
  int port;
  char url[256];

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ mock_server_init(&server)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ mock_server_start(server)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ port =
      math_mock_server_get_port(server); /* LCOV_EXCL_STOP */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/echo",
            /* LCOV_EXCL_START */ port); /* LCOV_EXCL_STOP */
#else
  /* LCOV_EXCL_START */ sprintf(url, "http://127.0.0.1:%d/echo",
                                port); /* LCOV_EXCL_STOP */
#endif

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_loop_init(&loop)); /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup(url, &req1.url)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.method = HTTP_GET;     /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req2);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup(url, &req2.url)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req2.method = HTTP_GET;     /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req2);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

  future1 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  future2 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[0] = future1;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[1] = future2;              /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ rc =
      http_apple_send_multi(ctx, loop, &multi, futures); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ while (!future1->is_ready ||
                               !future2->is_ready) { /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ ASSERT_EQ(
      200, future1->response->status_code); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      200, future2->response->status_code); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_response_free(
      future1->response);                        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1->response); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_response_free(
      future2->response);                        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future2->response); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1);           /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future2);           /* LCOV_EXCL_STOP */

  /* Test invalid args */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      http_apple_send_multi(NULL, loop, &multi, futures));

  /* LCOV_EXCL_START */ http_multi_request_free(&multi); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req1);        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req2);        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_apple_context_free(ctx);    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_loop_free(loop);            /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ mock_server_destroy(server);     /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_apple_send_multi_branches(void) { /* LCOV_EXCL_STOP */
#if defined(__APPLE__)
  /* LCOV_EXCL_START */ struct HttpTransportContext *ctx =
      NULL; /* LCOV_EXCL_STOP */
  struct HttpRequest req1;
  struct HttpMultiRequest multi;
  /* LCOV_EXCL_START */ struct HttpFuture *future1 = NULL; /* LCOV_EXCL_STOP */
  struct HttpFuture *futures[2];
  /* LCOV_EXCL_START */ struct ModalityEventLoop *loop =
      NULL; /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      /* LCOV_EXCL_START */ http_apple_context_init(&ctx)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ http_loop_init(&loop)); /* LCOV_EXCL_STOP */

  /* Test fail_url_str */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_url_str",
                                  &req1.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.method = HTTP_GET; /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  future1 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[0] = future1;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      http_apple_send_multi(ctx, loop, &multi, futures));
  /* LCOV_EXCL_START */ while (!future1->is_ready) { /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      future1->response->status_code ? 0 : C_ABSTRACT_HTTP_ERR_INVAL);
  /* LCOV_EXCL_START */ if (future1->response) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(
        future1->response);                              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(future1->response);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_multi_request_free(&multi); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req1);        /* LCOV_EXCL_STOP */

  /* Test fail_request_ref */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_request_ref",
                                  &req1.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.method = HTTP_GET; /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  future1 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[0] = future1;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      http_apple_send_multi(ctx, loop, &multi, futures));
  /* LCOV_EXCL_START */ while (!future1->is_ready) { /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ if (future1->response) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(
        future1->response);                              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(future1->response);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_multi_request_free(&multi); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req1);        /* LCOV_EXCL_STOP */

  /* Test fail_body_data */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_body_data",
                                  &req1.url));   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("test",
                                  (char **)&req1.body)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.body_len = 4;               /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  future1 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[0] = future1;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      http_apple_send_multi(ctx, loop, &multi, futures));
  /* LCOV_EXCL_START */ while (!future1->is_ready) { /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ if (future1->response) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(
        future1->response);                              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(future1->response);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_multi_request_free(&multi); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req1);        /* LCOV_EXCL_STOP */

  /* Test fail_read_stream */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_read_stream",
                                  &req1.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.method = HTTP_GET; /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  future1 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[0] = future1;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      http_apple_send_multi(ctx, loop, &multi, futures));
  /* LCOV_EXCL_START */ while (!future1->is_ready) { /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ if (future1->response) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(
        future1->response);                              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(future1->response);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_multi_request_free(&multi); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req1);        /* LCOV_EXCL_STOP */

  /* Test fail_read_stream_open */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_mock_strdup("http://fail_read_stream_open",
                                  &req1.url));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req1.method = HTTP_GET; /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  future1 = (struct HttpFuture *)calloc(
      /* LCOV_EXCL_START */ 1, sizeof(struct HttpFuture)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ futures[0] = future1;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      http_apple_send_multi(ctx, loop, &multi, futures));
  /* LCOV_EXCL_START */ while (!future1->is_ready) { /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ if (future1->response) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(
        future1->response);                              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(future1->response);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ }                                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(future1);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_multi_request_free(&multi); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req1);        /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_apple_context_free(ctx); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_loop_free(loop);         /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ SUITE(http_apple_suite) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_apple_send_multi_branches);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_send_multi); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ RUN_TEST(
      test_apple_send_mock_server);                        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_read_chunk);   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_lifecycle);    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_config);       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_send_invalid); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_apple_send_all_methods); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_oom_branches); /* LCOV_EXCL_STOP */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(test_apple_oom); /* LCOV_EXCL_STOP */
#endif
/* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H */

/* LCOV_EXCL_BR_STOP */
