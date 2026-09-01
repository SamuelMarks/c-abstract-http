/* LCOV_EXCL_BR_START */
#ifndef TEST_TLS_H
#define TEST_TLS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/c_abstract_http_tls.h>
#include <c_abstract_http/thread_pool.h>
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#else
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif

static struct AbstractHttpTlsKey *tls_key = NULL;

/* LCOV_EXCL_START */ static void test_tls_task_cb(void *arg) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   int *result = (int *)arg;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   int thread_local_val = *result; /* use arg as init val */  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   void *out_val = NULL;  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_tls_set(tls_key, &thread_local_val); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */

/* simulate some context switch */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(5);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  delay(5);
#else
/* LCOV_EXCL_START */   usleep(5 * 1000);  /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */   if (abstract_http_tls_get(tls_key, &out_val) == 0 && out_val != NULL) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     *result = *(int *)out_val;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_tls_isolation(void) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpThreadPool *pool = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   int results[4] = {10, 20, 30, 40};  /* LCOV_EXCL_STOP */
  int i;

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_key_create(&tls_key, NULL));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 4));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   for (i = 0; i < 4; i++) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_push(pool, test_tls_task_cb, &results[i]));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(10, results[0]);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(20, results[1]);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(30, results[2]);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(40, results[3]);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_tls_key_delete(tls_key); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   tls_key = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   PASS();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST test_tls_oom(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpTlsKey *key =
      NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_tls_key_create(&key, NULL); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ printf("tls_key_create returned %d\n",
                               rc); /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

#if !defined(_WIN32)
  /* LCOV_EXCL_START */ g_mock_pthread_fail = 1; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            /* LCOV_EXCL_START */ abstract_http_tls_key_create(
                &key, NULL));                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_pthread_fail = 0; /* LCOV_EXCL_STOP */
#endif

  {
    enum c_abstract_http_error rc_test =
        abstract_http_tls_key_create(&key, NULL);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

#if !defined(_WIN32)
/* LCOV_EXCL_START */ g_mock_pthread_fail = 1; /* LCOV_EXCL_STOP */
ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
          /* LCOV_EXCL_START */ abstract_http_tls_set(
              key, NULL)); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ g_mock_pthread_fail = 0; /* LCOV_EXCL_STOP */
#endif

{
  enum c_abstract_http_error rc_test = abstract_http_tls_key_delete(key);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST test_tls_errors(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_tls_key_create(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_tls_set(
                NULL, NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_tls_get(
                NULL, NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      /* LCOV_EXCL_START */ C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_tls_get((struct AbstractHttpTlsKey *)(size_t)1, NULL));
  {
    enum c_abstract_http_error rc_test = abstract_http_tls_key_delete(NULL);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ SUITE(tls_suite) {           /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_tls_errors); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(test_tls_oom); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ RUN_TEST(test_tls_isolation); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
