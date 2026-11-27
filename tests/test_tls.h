/**
 * @file test_tls.h
 * @brief Unit and concurrency tests for cross-platform Thread-Local Storage.
 */

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
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <time.h>
#else
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
#include "mock_alloc.h"
/* clang-format on */

static struct AbstractHttpTlsKey *tls_key = NULL;
static struct AbstractHttpTlsKey *tls_dtor_key = NULL;
static int g_tls_dtor_count = 0;

static void test_tls_dtor_cb(void *ptr) {
  int *flag = (int *)ptr;
  if (flag) {
    *flag += 1;
  }
  g_tls_dtor_count++;
}

NO_DISCARD static c_abstract_http_error_t test_tls_dtor_task(void *arg) {
  void *got = NULL;
  c_abstract_http_error_t rc;
  rc = abstract_http_tls_set(tls_dtor_key, arg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = abstract_http_tls_get(tls_dtor_key, &got);
  if (rc != C_ABSTRACT_HTTP_SUCCESS || got != arg) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  /* Overwrite with same pointer to exercise slot update path */
  rc = abstract_http_tls_set(tls_dtor_key, arg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t test_tls_task_cb(void *arg) {
  int *result = (int *)arg;
  int thread_local_val;
  void *out_val = NULL;
  c_abstract_http_error_t rc;

  if (!result)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  thread_local_val = *result;

  rc = abstract_http_tls_set(tls_key, &thread_local_val);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  /* simulate some context switch */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(5);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  {
    clock_t start_t = clock();
    clock_t wait_t =
        (clock_t)((5UL * (unsigned long)CLOCKS_PER_SEC + 999UL) / 1000UL);
    while ((clock_t)(clock() - start_t) < wait_t) {
    }
  }
#else
  usleep(5 * 1000);
#endif

  rc = abstract_http_tls_get(tls_key, &out_val);
  if (rc == C_ABSTRACT_HTTP_SUCCESS && out_val != NULL) {
    *result = *(int *)out_val;
  }
  return rc;
}

TEST test_tls_isolation(void) {
  struct AbstractHttpThreadPool *pool = NULL;
  int results[4] = {10, 20, 30, 40};
  int i;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_tls_key_create(&tls_key, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 4));
  for (i = 0; i < 4; i++) {
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        abstract_http_thread_pool_push(pool, test_tls_task_cb, &results[i]));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

  ASSERT_EQ(10, results[0]);
  ASSERT_EQ(20, results[1]);
  ASSERT_EQ(30, results[2]);
  ASSERT_EQ(40, results[3]);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_key_delete(tls_key));
  tls_key = NULL;
  PASS();
}

TEST test_tls_destructor(void) {
  struct AbstractHttpThreadPool *pool = NULL;
  int local_flag = 0;
  int main_flag = 0;
  void *got = NULL;

  g_tls_dtor_count = 0;
  tls_dtor_key = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_tls_key_create(&tls_dtor_key, test_tls_dtor_cb));

  /* Setting and clearing on main thread */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_tls_set(tls_dtor_key, &main_flag));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_get(tls_dtor_key, &got));
  ASSERT_EQ(&main_flag, got);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_set(tls_dtor_key, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_get(tls_dtor_key, &got));
  ASSERT_EQ(NULL, got);

  /* Worker thread exit triggers destructor on POSIX & Win32 FLS */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 1));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      abstract_http_thread_pool_push(pool, test_tls_dtor_task, &local_flag));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

#if !defined(__EMSCRIPTEN__) && !defined(__MSDOS__) && !defined(__DOS__) &&    \
    !defined(DOS) && !defined(C_ABSTRACT_HTTP_SINGLE_THREADED)
  ASSERT_EQ(1, local_flag);
  ASSERT_EQ(1, g_tls_dtor_count);
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_tls_key_delete(tls_dtor_key));
  tls_dtor_key = NULL;
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_tls_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpTlsKey *key = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_tls_key_create(&key, NULL);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

#if !defined(_WIN32)
  g_mock_pthread_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, abstract_http_tls_key_create(&key, NULL));
  g_mock_pthread_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_key_create(&key, NULL));

#if !defined(_WIN32)
  g_mock_pthread_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, abstract_http_tls_set(key, NULL));

  g_mock_pthread_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_key_delete(key));

  PASS();
}
#endif

TEST test_tls_errors(void) {
  int val = 42;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, test_tls_task_cb(NULL));
  tls_key = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, test_tls_task_cb(&val));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_tls_key_create(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_tls_set(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_tls_get(NULL, NULL));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      abstract_http_tls_get((struct AbstractHttpTlsKey *)(size_t)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_tls_key_delete(NULL));
  PASS();
}

SUITE(tls_suite) {
  RUN_TEST(test_tls_errors);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_tls_oom);
#endif
  RUN_TEST(test_tls_isolation);
  RUN_TEST(test_tls_destructor);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_TLS_H */
