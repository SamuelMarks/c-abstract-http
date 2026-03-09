/* clang-format off */
#ifndef TEST_TLS_H
#define TEST_TLS_H

#include <c_abstract_http/cdd_tls.h>
#include <c_abstract_http/thread_pool.h>
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#else
#include <unistd.h>
#endif
/* clang-format on */

static struct CddTlsKey *tls_key = NULL;

static void test_tls_task_cb(void *arg) {
  int *result = (int *)arg;
  int thread_local_val = *result; /* use arg as init val */

  cdd_tls_set(tls_key, &thread_local_val);

/* simulate some context switch */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(5);
#else
  usleep(5 * 1000);
#endif

  *result = *(int *)cdd_tls_get(tls_key);
}

TEST test_tls_isolation(void) {
  struct CddThreadPool *pool = NULL;
  int results[4] = {10, 20, 30, 40};
  int i;

  ASSERT_EQ(0, cdd_tls_key_create(&tls_key, NULL));
  ASSERT_EQ(0, cdd_thread_pool_init(&pool, 4));

  for (i = 0; i < 4; ++i) {
    ASSERT_EQ(0, cdd_thread_pool_push(pool, test_tls_task_cb, &results[i]));
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

SUITE(tls_suite) { RUN_TEST(test_tls_isolation); }

#endif
