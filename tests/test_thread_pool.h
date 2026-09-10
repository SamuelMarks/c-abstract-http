
#ifndef TEST_THREAD_POOL_H
#define TEST_THREAD_POOL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <dos.h>
#else
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif

#include <c_abstract_http/thread_pool.h>

#include "mock_alloc.h"
/* clang-format on */

extern enum c_abstract_http_error
abstract_http_thread_pool_test_set_stop(struct AbstractHttpThreadPool *pool);
extern void
abstract_http_thread_pool_test_inject_task(struct AbstractHttpThreadPool *pool);

static void sleep_ms(int ms) {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep((unsigned long)ms);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  delay(ms);
#else
  usleep((unsigned int)ms * 1000U);
#endif
}

/** @brief Documented */
struct TestTaskData {
  /** @brief Documented */
  struct AbstractHttpMutex *lock;
  /** @brief Documented */
  int *counter;
};

static void test_task_cb(void *arg) {
  struct TestTaskData *data = (struct TestTaskData *)arg;
  sleep_ms(5); /* Simulate work */
  (void)!abstract_http_mutex_lock(data->lock);
  (*data->counter)++;
  (void)!abstract_http_mutex_unlock(data->lock);
  free(data);
}

TEST test_thread_pool_execution(void) {
  struct AbstractHttpThreadPool *pool = NULL;
  struct AbstractHttpMutex *lock = NULL;
  int counter = 0;
  int i;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mutex_init(&lock));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 4));

  for (i = 0; i < 50; ++i) {
    struct TestTaskData *data =
        (struct TestTaskData *)malloc(sizeof(struct TestTaskData));
    data->lock = lock;
    data->counter = &counter;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, test_task_cb, data));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
  /* Blocks until all tasks complete */

  ASSERT_EQ(50, counter);

  abstract_http_mutex_free(lock);
  PASS();
}

TEST test_mutex_lock_unlock(void) {
  struct AbstractHttpMutex *lock = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mutex_init(&lock));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mutex_lock(lock));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mutex_unlock(lock));

  abstract_http_mutex_free(lock);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_thread_pool_errors(void) {
  struct AbstractHttpMutex *lock = NULL;
  struct AbstractHttpCond *cond = NULL;
  struct AbstractHttpThreadPool *pool = NULL;
  struct AbstractHttpThreadPoolHooks hooks;

  memset(&hooks, 0, sizeof(hooks));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_mutex_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_mutex_lock(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_mutex_unlock(NULL));
  abstract_http_mutex_free(NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_cond_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_cond_wait(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mutex_init(&lock));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_cond_init(&cond));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_cond_wait(NULL, lock));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_cond_wait(cond, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_cond_signal(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_cond_broadcast(NULL));
  abstract_http_cond_free(NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_cond_signal(cond));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_cond_broadcast(cond));

  abstract_http_cond_free(cond);
  abstract_http_mutex_free(lock);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_thread_pool_init(NULL, 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_init(&pool, 0));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_push(NULL, dummy_cb_thread, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_push(
                (struct AbstractHttpThreadPool *)(size_t)1, NULL, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_init_external(NULL, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_init_external(&pool, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(NULL));

  abstract_http_thread_pool_test_set_stop(NULL);
  abstract_http_thread_pool_test_inject_task(NULL);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  (void)!abstract_http_thread_pool_init(&pool,
                                        1); /* this will fail due to
                                               C_ABSTRACT_HTTP_ERR_NOMEM or
                                               we just use a valid pool */
  g_mock_alloc_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 1));

  g_mock_pthread_fail = 3;
  abstract_http_thread_pool_test_set_stop(pool);
  g_mock_pthread_fail = 0;

  abstract_http_thread_pool_test_set_stop(pool);
  /* Wait for the thread to exit now that stop is set */
  sleep_ms(50);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  abstract_http_thread_pool_test_inject_task(pool);
  g_mock_alloc_fail = 0;

  abstract_http_thread_pool_test_inject_task(pool);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

  PASS();
}

#endif
extern void dummy_cb_thread(void *arg);

TEST test_thread_pool_external(void) {
  struct AbstractHttpThreadPool *pool;
  struct AbstractHttpThreadPoolHooks hooks;
  memset(&hooks, 0, sizeof(hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_init_external(&pool, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error
abstract_http_thread_pool_test_worker(struct AbstractHttpThreadPool *pool);
extern enum c_abstract_http_error
abstract_http_thread_pool_test_clear_stop(struct AbstractHttpThreadPool *pool);
extern enum c_abstract_http_error abstract_http_thread_pool_test_create_dummy(
    struct AbstractHttpThreadPool **out);

static int dummy_hook_push(void *ctx, abstract_http_thread_task_cb cb,
                           void *arg) {
  (void)ctx;
  (void)cb;
  (void)arg;
  return 0;
}

TEST test_thread_pool_edge_cases(void) {
  struct AbstractHttpThreadPool *pool;
  struct AbstractHttpThreadPoolHooks hooks;
  memset(&hooks, 0, sizeof(hooks));

  /* 470: pool == NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_init_external(NULL, &hooks));

  /* 498: test hook push */
  memset(&hooks, 0, sizeof(hooks));
  hooks.push = dummy_hook_push;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_init_external(&pool, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

  /* 516-519: push when stopped */
  /* and 563-565: tasks left in queue */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 1));
  abstract_http_thread_pool_test_set_stop(pool);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));

  /* Stop the pool first, let it join threads, THEN inject task to test cleanup
   */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

  /* Test push and free failure branches using g_mock_mutex_fail and
   * g_mock_cond_fail */
  {
    struct AbstractHttpThreadPool *test_pool = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_test_create_dummy(&test_pool));

    /* push: mutex_lock fail */
    g_mock_mutex_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_push(test_pool, dummy_cb_thread, NULL));
    g_mock_mutex_fail = 0;

    /* push: cond_signal fail */
    g_mock_cond_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_push(test_pool, dummy_cb_thread, NULL));
    g_mock_cond_fail = 0;

    /* push: unlock fail after signal */
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_push(test_pool, dummy_cb_thread, NULL));
    g_mock_mutex_fail = 0;

    /* test_set_stop: unlock fail */
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_set_stop(test_pool));
    g_mock_mutex_fail = 0;

    /* stop the pool */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_test_set_stop(test_pool));

    /* push when stopped: unlock fail */
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_push(test_pool, dummy_cb_thread, NULL));
    g_mock_mutex_fail = 0;

    /* free: mutex_lock fail */
    g_mock_mutex_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_free(test_pool));
    g_mock_mutex_fail = 0;

    /* free: cond_broadcast fail */
    g_mock_cond_fail = 3;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_free(test_pool));
    g_mock_cond_fail = 0;

    /* free: unlock fail after broadcast */
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_free(test_pool));
    g_mock_mutex_fail = 0;

    /* free successfully */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_free(test_pool));
  }

  /* Test worker_thread branches via dummy pool */
  {
    struct AbstractHttpThreadPool *dummy_pool = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              abstract_http_thread_pool_test_create_dummy(NULL));

    /* dummy create alloc fail */
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              abstract_http_thread_pool_test_create_dummy(&dummy_pool));
    g_mock_alloc_fail = 0;

#if !defined(_WIN32)
    /* dummy create mutex_init fail */
    g_mock_pthread_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_create_dummy(&dummy_pool));
    g_mock_pthread_fail = 0;
#endif

    /* dummy create cond_init fail */
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              abstract_http_thread_pool_test_create_dummy(&dummy_pool));
    g_mock_alloc_fail = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_test_clear_stop(NULL));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_test_create_dummy(&dummy_pool));

    /* Case 1: worker_thread mutex_lock fail */
    g_mock_mutex_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_worker(dummy_pool));
    g_mock_mutex_fail = 0;

    /* Case 2: worker_thread cond_wait fail */
    g_mock_cond_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_worker(dummy_pool));
    g_mock_cond_fail = 0;

    /* Case 3: worker_thread cond_wait fail + unlock fail */
    g_mock_cond_fail = 1;
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_worker(dummy_pool));
    g_mock_cond_fail = 0;
    g_mock_mutex_fail = 0;

    /* Case 4: pool->stop == 1 + unlock fail */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_test_set_stop(dummy_pool));
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_worker(dummy_pool));
    g_mock_mutex_fail = 0;

    /* Case 5: task popped + unlock fail */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_test_clear_stop(dummy_pool));
    abstract_http_thread_pool_test_inject_task(dummy_pool);
    g_mock_mutex_fail = 2;
    g_mock_cond_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              abstract_http_thread_pool_test_worker(dummy_pool));
    g_mock_mutex_fail = 0;
    g_mock_cond_fail = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_free(dummy_pool));
  }

  PASS();
}

#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
TEST test_thread_pool_pthread_create_failures(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpThreadPool *pool = NULL;

#if !defined(_WIN32)
  /* Fail on first thread */
  g_mock_alloc_fail = 0;
  g_mock_pthread_fail = 2;
  g_mock_alloc_count = 0;
  rc = abstract_http_thread_pool_init(&pool, 2);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* Fail on second thread */
  g_mock_pthread_fail = 2;
  g_mock_alloc_count = 1;
  rc = abstract_http_thread_pool_init(&pool, 2);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_pthread_fail = 0;
#endif

  /* also test external init failure */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    struct AbstractHttpThreadPoolHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    rc = abstract_http_thread_pool_init_external(&pool, &hooks);
  }
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
TEST test_thread_pool_pthread_failures(void) {
#if !defined(_WIN32)
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpMutex *lock = NULL;
  struct AbstractHttpCond *cond = NULL;

  g_mock_pthread_fail = 1;
  rc = abstract_http_mutex_init(&lock);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  rc = abstract_http_cond_init(&cond);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  g_mock_pthread_fail = 0;
#endif

  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
TEST test_thread_pool_fallback_paths(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpThreadPool *pool = NULL;
  struct AbstractHttpMutex *lock = NULL;
  struct AbstractHttpCond *cond = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_mutex_init(&lock);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_cond_init(&cond);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_thread_pool_init(&pool, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = abstract_http_thread_pool_init(&pool, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2;
  rc = abstract_http_thread_pool_init(&pool, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 3;
  rc = abstract_http_thread_pool_init(&pool, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  g_mock_alloc_fail = 0;
  rc = abstract_http_thread_pool_init(&pool, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_thread_pool_push(pool, test_task_cb, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

  PASS();
}
#endif

SUITE(thread_pool_suite) {
  RUN_TEST(test_thread_pool_external);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_thread_pool_errors);
#endif
  RUN_TEST(test_mutex_lock_unlock);
  RUN_TEST(test_thread_pool_execution);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_thread_pool_edge_cases);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
  RUN_TEST(test_thread_pool_pthread_create_failures);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
  RUN_TEST(test_thread_pool_pthread_failures);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__) &&           \
    !defined(__EMSCRIPTEN__)
  RUN_TEST(test_thread_pool_fallback_paths);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
