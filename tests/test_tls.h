#ifndef TEST_TLS_H
#define TEST_TLS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/cdd_tls.h>
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

static struct CddTlsKey *tls_key = NULL;

static void test_tls_task_cb(void *arg) {
  int *result = (int *)arg;
  int thread_local_val = *result; /* use arg as init val */
  void *out_val = NULL;

  cdd_tls_set(tls_key, &thread_local_val);

/* simulate some context switch */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(5);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  delay(5);
#else
  usleep(5 * 1000);
#endif

  if (cdd_tls_get(tls_key, &out_val) == 0 && out_val != NULL) {
    *result = *(int *)out_val;
  }
}

TEST test_tls_isolation(void) {
  struct CddThreadPool *pool = NULL;
  int results[4] = {10, 20, 30, 40};
  int i;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cdd_tls_key_create(&tls_key, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cdd_thread_pool_init(&pool, 4));
  for (i = 0; i < 4; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cdd_thread_pool_push(pool, test_tls_task_cb, &results[i]));
  }

  cdd_thread_pool_free(pool);

  ASSERT_EQ(10, results[0]);
  ASSERT_EQ(20, results[1]);
  ASSERT_EQ(30, results[2]);
  ASSERT_EQ(40, results[3]);

  cdd_tls_key_delete(tls_key);
  tls_key = NULL;
  PASS();
}

#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_tls_oom(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct CddTlsKey *key = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_tls_key_create(&key, NULL);
  printf("tls_key_create returned %d\n", rc);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

#if !defined(_WIN32)
  g_mock_pthread_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, cdd_tls_key_create(&key, NULL));
  g_mock_pthread_fail = 0;
#endif

  cdd_tls_key_create(&key, NULL);

#if !defined(_WIN32)
  g_mock_pthread_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, cdd_tls_set(key, NULL));

  g_mock_pthread_fail = 0;
#endif

  cdd_tls_key_delete(key);

  PASS();
}
#endif

TEST test_tls_errors(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_tls_key_create(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_tls_set(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_tls_get(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_tls_get((struct CddTlsKey *)1, NULL));
  cdd_tls_key_delete(NULL);
  PASS();
}

SUITE(tls_suite) {
  RUN_TEST(test_tls_errors);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_tls_oom);
#endif
  RUN_TEST(test_tls_isolation);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
