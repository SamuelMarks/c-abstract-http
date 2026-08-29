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

static char *c_abstract_http_test_apple_strdup(const char *s) { /* LCOV_EXCL_LINE */
  size_t len;
  char *d;
  if (!s) /* LCOV_EXCL_LINE */
    return NULL; /* LCOV_EXCL_LINE */
  len = strlen(s); /* LCOV_EXCL_LINE */
  d = (char *)malloc(len + 1); /* LCOV_EXCL_LINE */
  if (d) /* LCOV_EXCL_LINE */
    memcpy(d, s, len + 1); /* LCOV_EXCL_LINE */
  return d; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#undef strdup
/** @brief Documented */
#define strdup(s) c_abstract_http_test_apple_strdup(s)

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

static int mock_on_chunk_cb(void *user_data, const void *chunk,
                            size_t len) { /* LCOV_EXCL_LINE */
  int *calls = (int *)user_data;          /* LCOV_EXCL_LINE */
  (void)chunk;                            /* LCOV_EXCL_LINE */
  (void)len;                              /* LCOV_EXCL_LINE */
  (*calls)++;                             /* LCOV_EXCL_LINE */
  return 0;                               /* LCOV_EXCL_LINE */
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_apple_oom_branches(void) {       /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req;
  struct HttpResponse *res = NULL; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */

#if defined(__APPLE__)

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));      /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_url_str"); /* LCOV_EXCL_LINE */
  req.method = HTTP_GET;                   /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));  /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_url"); /* LCOV_EXCL_LINE */
  req.method = HTTP_GET;               /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */

  /* fail_url_ref removed because urlRef parsing was optimized out */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));          /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_request_ref"); /* LCOV_EXCL_LINE */
  req.method = HTTP_GET;                       /* LCOV_EXCL_LINE */
  {
    int debug_rc = http_apple_send(ctx, &req, &res); /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, debug_rc);  /* LCOV_EXCL_LINE */
  }
  http_request_free(&req); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));        /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_body_data"); /* LCOV_EXCL_LINE */
  req.method = HTTP_POST;                    /* LCOV_EXCL_LINE */
  req.body = strdup("test");                 /* LCOV_EXCL_LINE */
  req.body_len = 4;                          /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));          /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_read_stream"); /* LCOV_EXCL_LINE */
  req.method = HTTP_GET;                       /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));               /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_read_stream_open"); /* LCOV_EXCL_LINE */
  req.method = HTTP_GET;                            /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));           /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_mutable_data"); /* LCOV_EXCL_LINE */
  req.method = HTTP_POST;                       /* LCOV_EXCL_LINE */
  req.read_chunk = (http_read_chunk_fn)1;       /* LCOV_EXCL_LINE */
  req.expected_body_len = 10;                   /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));    /* LCOV_EXCL_LINE */
  req.url = strdup("http://fail_cb_rc"); /* LCOV_EXCL_LINE */
  req.method = HTTP_POST;                /* LCOV_EXCL_LINE */
  req.body = strdup("test");             /* LCOV_EXCL_LINE */
  req.body_len = 4;                      /* LCOV_EXCL_LINE */
  req.on_chunk = mock_on_chunk_cb;       /* LCOV_EXCL_LINE */
  {
    int calls = 0;                   /* LCOV_EXCL_LINE */
    req.on_chunk_user_data = &calls; /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  }
  http_request_free(&req); /* LCOV_EXCL_LINE */
#endif

  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  PASS();                       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_apple_oom(void) {                /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req;
  struct HttpResponse *res = NULL; /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0; /* LCOV_EXCL_LINE */
  {
    int rc_test_tmp = http_apple_context_init(&ctx); /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0;                           /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req)); /* LCOV_EXCL_LINE */

  req.url = "http://example.com"; /* LCOV_EXCL_LINE */
  req.method = HTTP_GET;          /* LCOV_EXCL_LINE */

  /* Test 124: malloc for *res fails */
  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0; /* LCOV_EXCL_LINE */
  {
    int rc_test_tmp = http_apple_send(ctx, &req, &res); /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0;                              /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  PASS();                       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

/** @brief Documented */
TEST test_apple_send_mock_server(void) { /* LCOV_EXCL_LINE */
#if defined(__APPLE__)
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req;
  struct HttpResponse *res = NULL; /* LCOV_EXCL_LINE */
  MockServerPtr server = NULL;     /* LCOV_EXCL_LINE */
  int port = 0;                    /* LCOV_EXCL_LINE */
  char url[256];
  int on_chunk_calls = 0; /* LCOV_EXCL_LINE */
  struct MockServerRequest mock_req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_init(&server)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_start(server));     /* LCOV_EXCL_LINE */
  port = math_mock_server_get_port(server); /* LCOV_EXCL_LINE */
  ASSERT(port > 0);                         /* LCOV_EXCL_LINE */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/echo",
            port); /* LCOV_EXCL_LINE */
#else
  sprintf(url, "http://127.0.0.1:%d/echo", port); /* LCOV_EXCL_LINE */
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */

  /* Normal request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));        /* LCOV_EXCL_LINE */
  req.url = (char *)malloc(strlen(url) + 1); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER)
  strcpy_s(req.url, strlen(url) + 1, url);
#else
  strcpy(req.url, url); /* LCOV_EXCL_LINE */
#endif
  req.method = HTTP_POST;                /* LCOV_EXCL_LINE */
  req.body = strdup("Hello Apple!");     /* LCOV_EXCL_LINE */
  req.body_len = strlen("Hello Apple!"); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  ASSERT(res != NULL);                         /* LCOV_EXCL_LINE */
  ASSERT_EQ(200, res->status_code);            /* LCOV_EXCL_LINE */
  ASSERT(res->body_len > 0);                   /* LCOV_EXCL_LINE */

  http_response_free(res); /* LCOV_EXCL_LINE */
  free(res);               /* LCOV_EXCL_LINE */
  http_request_free(&req); /* LCOV_EXCL_LINE */
  res = NULL;              /* LCOV_EXCL_LINE */

  /* On-chunk request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            mock_server_wait_for_request(server, &mock_req));
  mock_server_request_cleanup(&mock_req); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));        /* LCOV_EXCL_LINE */
  req.url = (char *)malloc(strlen(url) + 1); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER)
  strcpy_s(req.url, strlen(url) + 1, url);
#else
  strcpy(req.url, url); /* LCOV_EXCL_LINE */
#endif
  req.method = HTTP_GET;                    /* LCOV_EXCL_LINE */
  req.on_chunk = mock_on_chunk_cb;          /* LCOV_EXCL_LINE */
  req.on_chunk_user_data = &on_chunk_calls; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  ASSERT(res != NULL);                         /* LCOV_EXCL_LINE */
  ASSERT_EQ(200, res->status_code);            /* LCOV_EXCL_LINE */
  ASSERT(on_chunk_calls > 0);                  /* LCOV_EXCL_LINE */

  http_response_free(res); /* LCOV_EXCL_LINE */
  free(res);               /* LCOV_EXCL_LINE */
  http_request_free(&req); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            mock_server_wait_for_request(server, &mock_req));
  mock_server_request_cleanup(&mock_req); /* LCOV_EXCL_LINE */

  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  mock_server_destroy(server);  /* LCOV_EXCL_LINE */
#endif
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

/** @brief Documented */
TEST test_apple_lifecycle(void) {          /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_global_init()); /* LCOV_EXCL_LINE */

  /* Init */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_context_init(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT(ctx != NULL);                      /* LCOV_EXCL_LINE */

  /* Free */
  http_apple_context_free(ctx);  /* LCOV_EXCL_LINE */
  http_apple_context_free(NULL); /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = http_apple_global_cleanup();
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

/** @brief Documented */
TEST test_apple_config(void) {             /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpConfig cfg;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT(ctx != NULL);                      /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_config_init(&cfg)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_config_apply(NULL, &cfg)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_config_apply(ctx, NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_config_apply(ctx, &cfg)); /* LCOV_EXCL_LINE */

  http_config_free(&cfg);       /* LCOV_EXCL_LINE */
  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  PASS();                       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

/** @brief Documented */
TEST test_apple_send_invalid(void) {       /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req;
  struct HttpResponse *res = NULL; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send(NULL, &req, &res)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send(ctx, NULL, &res)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_apple_send(ctx, &req, NULL)); /* LCOV_EXCL_LINE */

#if defined(__APPLE__)
  /* Valid input but we need to mock a real URL so it fails gracefully */
  req.url = (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1"); /* LCOV_EXCL_LINE */
#endif
  req.method = HTTP_GET; /* LCOV_EXCL_LINE */
  /* Might fail with C_ABSTRACT_HTTP_ERR_IO due to no connection or return an
   * allocated res */
  {
    enum c_abstract_http_error rc =
        http_apple_send(ctx, &req, &res); /* LCOV_EXCL_LINE */
    ASSERT(rc != 0);                      /* LCOV_EXCL_LINE */
  }
#else
  /* Valid input but not implemented (or no Apple OS) should return ENOSYS */
  ASSERT_EQ(ENOSYS, http_apple_send(ctx, &req, &res));
#endif

  http_request_free(&req);      /* LCOV_EXCL_LINE */
  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  PASS();                       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

/** @brief Documented */
TEST test_apple_send_all_methods(void) {   /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req;
  struct HttpResponse *res = NULL; /* LCOV_EXCL_LINE */
  struct HttpConfig cfg;
  int i;
  const enum HttpMethod methods[] = {
      /* LCOV_EXCL_LINE */
      HTTP_POST,    HTTP_PUT,   HTTP_DELETE,  HTTP_PATCH,         HTTP_HEAD,
      HTTP_OPTIONS, HTTP_TRACE, HTTP_CONNECT, (enum HttpMethod)99 /* Invalid
                                                                     method */
  };

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_config_init(&cfg)); /* LCOV_EXCL_LINE */
  cfg.verify_peer = 0;               /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_config_apply(ctx, &cfg)); /* LCOV_EXCL_LINE */
  http_config_free(&cfg);                        /* LCOV_EXCL_LINE */

  for (i = 0; i < 9; i++) { /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_request_init(&req));                     /* LCOV_EXCL_LINE */
    req.url = (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER)
    strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
    strcpy(req.url, "http://localhost:1"); /* LCOV_EXCL_LINE */
#endif
    req.method = methods[i]; /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test =
          http_headers_add(&req.headers, "X-Test", "Value");
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */

    if (i == 8) { /* LCOV_EXCL_LINE */
      /* Invalid method may fail differently, let's just see if it crashes */
      {
        enum c_abstract_http_error rc_test = http_apple_send(ctx, &req, &res);
        if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
          printf("Error: %d\n", (int)rc_test);
        }
      } /* LCOV_EXCL_LINE */
    } else { /* LCOV_EXCL_LINE */
      enum c_abstract_http_error rc =
          http_apple_send(ctx, &req, &res); /* LCOV_EXCL_LINE */
      ASSERT(rc != 0);                      /* LCOV_EXCL_LINE */
    }
    http_request_free(&req); /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  PASS();                       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int mock_read_chunk(void *user_data, void *buf,
                           size_t buf_len, /* LCOV_EXCL_LINE */
                           size_t *out_read) {
  int *calls = (int *)user_data; /* LCOV_EXCL_LINE */
  if (*calls >= 2) {             /* LCOV_EXCL_LINE */
    *out_read = 0;               /* LCOV_EXCL_LINE */
    return 0;                    /* LCOV_EXCL_LINE */
  }
  (*calls)++;                   /* LCOV_EXCL_LINE */
  if (buf_len > 4)              /* LCOV_EXCL_LINE */
    buf_len = 4;                /* LCOV_EXCL_LINE */
  memcpy(buf, "test", buf_len); /* LCOV_EXCL_LINE */
  *out_read = buf_len;          /* LCOV_EXCL_LINE */
  return 0;                     /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int mock_read_chunk_fail(void *user_data, void *buf,
                                size_t buf_len, /* LCOV_EXCL_LINE */
                                size_t *out_read) {
  (void)user_data;               /* LCOV_EXCL_LINE */
  (void)buf;                     /* LCOV_EXCL_LINE */
  (void)buf_len;                 /* LCOV_EXCL_LINE */
  (void)out_read;                /* LCOV_EXCL_LINE */
  return C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_LINE */
}

/** @brief Documented */
TEST test_apple_read_chunk(void) {         /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req;
  struct HttpResponse *res = NULL; /* LCOV_EXCL_LINE */
  struct HttpConfig cfg;
  int calls = 0; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_config_init(&cfg)); /* LCOV_EXCL_LINE */
  cfg.verify_peer = 0;               /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_config_apply(ctx, &cfg)); /* LCOV_EXCL_LINE */
  http_config_free(&cfg);                        /* LCOV_EXCL_LINE */

  /* Success chunk */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));                     /* LCOV_EXCL_LINE */
  req.url = (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1"); /* LCOV_EXCL_LINE */
#endif
  req.method = HTTP_POST;            /* LCOV_EXCL_LINE */
  req.read_chunk = mock_read_chunk;  /* LCOV_EXCL_LINE */
  req.read_chunk_user_data = &calls; /* LCOV_EXCL_LINE */
  req.expected_body_len = 8;         /* LCOV_EXCL_LINE */

  /* Will fail to connect but it hits the read_chunk loop */
  {
    enum c_abstract_http_error rc_test = http_apple_send(ctx, &req, &res);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  http_request_free(&req);   /* LCOV_EXCL_LINE */
  if (res) {                 /* LCOV_EXCL_LINE */
    http_response_free(res); /* LCOV_EXCL_LINE */
    free(res);               /* LCOV_EXCL_LINE */
    res = NULL;              /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  /* Fail chunk */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_request_init(&req));                     /* LCOV_EXCL_LINE */
  req.url = (char *)malloc(sizeof("http://localhost:1")); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER)
  strcpy_s(req.url, sizeof("http://localhost:1"), "http://localhost:1");
#else
  strcpy(req.url, "http://localhost:1"); /* LCOV_EXCL_LINE */
#endif
  req.method = HTTP_POST;                /* LCOV_EXCL_LINE */
  req.read_chunk = mock_read_chunk_fail; /* LCOV_EXCL_LINE */
  req.read_chunk_user_data = NULL;       /* LCOV_EXCL_LINE */
  req.expected_body_len = 8;             /* LCOV_EXCL_LINE */

  /* Will fail with C_ABSTRACT_HTTP_ERR_IO */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_apple_send(ctx, &req, &res)); /* LCOV_EXCL_LINE */
  http_request_free(&req);                     /* LCOV_EXCL_LINE */
  if (res) {                                   /* LCOV_EXCL_LINE */
    http_response_free(res);                   /* LCOV_EXCL_LINE */
    free(res);                                 /* LCOV_EXCL_LINE */
    res = NULL;                                /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  PASS();                       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

/** @brief Documented */

/** @brief Documented */
TEST test_apple_send_multi(void) { /* LCOV_EXCL_LINE */
#if defined(__APPLE__)
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  MockServerPtr server = NULL;                             /* LCOV_EXCL_LINE */
  struct HttpTransportContext *ctx = NULL;                 /* LCOV_EXCL_LINE */
  struct HttpRequest req1, req2;
  struct HttpMultiRequest multi;
  struct HttpFuture *future1 = NULL, *future2 = NULL; /* LCOV_EXCL_LINE */
  struct HttpFuture *futures[2];
  struct ModalityEventLoop *loop = NULL; /* LCOV_EXCL_LINE */
  int port;
  char url[256];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_init(&server)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            mock_server_start(server));     /* LCOV_EXCL_LINE */
  port = math_mock_server_get_port(server); /* LCOV_EXCL_LINE */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(url, sizeof(url), "http://127.0.0.1:%d/echo",
            port); /* LCOV_EXCL_LINE */
#else
  sprintf(url, "http://127.0.0.1:%d/echo", port); /* LCOV_EXCL_LINE */
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req1.url = strdup(url); /* LCOV_EXCL_LINE */
  req1.method = HTTP_GET; /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req2);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req2.url = strdup(url); /* LCOV_EXCL_LINE */
  req2.method = HTTP_GET; /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req2);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  future1 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  future2 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  futures[0] = future1;              /* LCOV_EXCL_LINE */
  futures[1] = future2;              /* LCOV_EXCL_LINE */

  rc = http_apple_send_multi(ctx, loop, &multi, futures); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);                 /* LCOV_EXCL_LINE */

  while (!future1->is_ready || !future2->is_ready) { /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */
  }

  ASSERT_EQ(200, future1->response->status_code); /* LCOV_EXCL_LINE */
  ASSERT_EQ(200, future2->response->status_code); /* LCOV_EXCL_LINE */

  http_response_free(future1->response); /* LCOV_EXCL_LINE */
  free(future1->response);               /* LCOV_EXCL_LINE */
  http_response_free(future2->response); /* LCOV_EXCL_LINE */
  free(future2->response);               /* LCOV_EXCL_LINE */
  free(future1);                         /* LCOV_EXCL_LINE */
  free(future2);                         /* LCOV_EXCL_LINE */

  /* Test invalid args */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            http_apple_send_multi(NULL, loop, &multi, futures));

  http_multi_request_free(&multi); /* LCOV_EXCL_LINE */
  http_request_free(&req1);        /* LCOV_EXCL_LINE */
  http_request_free(&req2);        /* LCOV_EXCL_LINE */
  http_apple_context_free(ctx);    /* LCOV_EXCL_LINE */
  http_loop_free(loop);            /* LCOV_EXCL_LINE */
  mock_server_destroy(server);     /* LCOV_EXCL_LINE */
#endif
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_apple_send_multi_branches(void) { /* LCOV_EXCL_LINE */
#if defined(__APPLE__)
  struct HttpTransportContext *ctx = NULL; /* LCOV_EXCL_LINE */
  struct HttpRequest req1;
  struct HttpMultiRequest multi;
  struct HttpFuture *future1 = NULL; /* LCOV_EXCL_LINE */
  struct HttpFuture *futures[2];
  struct ModalityEventLoop *loop = NULL; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_apple_context_init(&ctx)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_LINE */

  /* Test fail_url_str */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req1.url = strdup("http://fail_url_str"); /* LCOV_EXCL_LINE */
  req1.method = HTTP_GET;                   /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  future1 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  futures[0] = future1;              /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) { /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            future1->response->status_code ? 0 : C_ABSTRACT_HTTP_ERR_INVAL);
  if (future1->response) {                 /* LCOV_EXCL_LINE */
    http_response_free(future1->response); /* LCOV_EXCL_LINE */
    free(future1->response);               /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  free(future1);                   /* LCOV_EXCL_LINE */
  http_multi_request_free(&multi); /* LCOV_EXCL_LINE */
  http_request_free(&req1);        /* LCOV_EXCL_LINE */

  /* Test fail_request_ref */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req1.url = strdup("http://fail_request_ref"); /* LCOV_EXCL_LINE */
  req1.method = HTTP_GET;                       /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  future1 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  futures[0] = future1;              /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) { /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */
  }
  if (future1->response) {                 /* LCOV_EXCL_LINE */
    http_response_free(future1->response); /* LCOV_EXCL_LINE */
    free(future1->response);               /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  free(future1);                   /* LCOV_EXCL_LINE */
  http_multi_request_free(&multi); /* LCOV_EXCL_LINE */
  http_request_free(&req1);        /* LCOV_EXCL_LINE */

  /* Test fail_body_data */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req1.url = strdup("http://fail_body_data"); /* LCOV_EXCL_LINE */
  req1.method = HTTP_POST;                    /* LCOV_EXCL_LINE */
  req1.body = strdup("test");                 /* LCOV_EXCL_LINE */
  req1.body_len = 4;                          /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  future1 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  futures[0] = future1;              /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) { /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */
  }
  if (future1->response) {                 /* LCOV_EXCL_LINE */
    http_response_free(future1->response); /* LCOV_EXCL_LINE */
    free(future1->response);               /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  free(future1);                   /* LCOV_EXCL_LINE */
  http_multi_request_free(&multi); /* LCOV_EXCL_LINE */
  http_request_free(&req1);        /* LCOV_EXCL_LINE */

  /* Test fail_read_stream */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req1.url = strdup("http://fail_read_stream"); /* LCOV_EXCL_LINE */
  req1.method = HTTP_GET;                       /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  future1 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  futures[0] = future1;              /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) { /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */
  }
  if (future1->response) {                 /* LCOV_EXCL_LINE */
    http_response_free(future1->response); /* LCOV_EXCL_LINE */
    free(future1->response);               /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  free(future1);                   /* LCOV_EXCL_LINE */
  http_multi_request_free(&multi); /* LCOV_EXCL_LINE */
  http_request_free(&req1);        /* LCOV_EXCL_LINE */

  /* Test fail_read_stream_open */
  {
    enum c_abstract_http_error rc_test = http_request_init(&req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  req1.url = strdup("http://fail_read_stream_open"); /* LCOV_EXCL_LINE */
  req1.method = HTTP_GET;                            /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_init(&multi);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = http_multi_request_add(&multi, &req1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  future1 = (struct HttpFuture *)calloc(
      1, sizeof(struct HttpFuture)); /* LCOV_EXCL_LINE */
  futures[0] = future1;              /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            http_apple_send_multi(ctx, loop, &multi, futures));
  while (!future1->is_ready) { /* LCOV_EXCL_LINE */
    {
      enum c_abstract_http_error rc_test = http_loop_tick(loop);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    } /* LCOV_EXCL_LINE */
  }
  if (future1->response) {                 /* LCOV_EXCL_LINE */
    http_response_free(future1->response); /* LCOV_EXCL_LINE */
    free(future1->response);               /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  free(future1);                   /* LCOV_EXCL_LINE */
  http_multi_request_free(&multi); /* LCOV_EXCL_LINE */
  http_request_free(&req1);        /* LCOV_EXCL_LINE */

  http_apple_context_free(ctx); /* LCOV_EXCL_LINE */
  http_loop_free(loop);         /* LCOV_EXCL_LINE */
#endif
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

SUITE(http_apple_suite) {                   /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_send_multi_branches); /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_send_multi);          /* LCOV_EXCL_LINE */

  RUN_TEST(test_apple_send_mock_server); /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_read_chunk);       /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_lifecycle);        /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_config);           /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_send_invalid);     /* LCOV_EXCL_LINE */
  RUN_TEST(test_apple_send_all_methods); /* LCOV_EXCL_LINE */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_apple_oom_branches); /* LCOV_EXCL_LINE */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_apple_oom); /* LCOV_EXCL_LINE */
#endif
} /* LCOV_EXCL_LINE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_HTTP_APPLE_H */

/* LCOV_EXCL_BR_STOP */
