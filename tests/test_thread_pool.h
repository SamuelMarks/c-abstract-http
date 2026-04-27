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
#if !defined(_MSC_VER)
#if !defined(_MSC_VER)
#if !defined(_MSC_VER)
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
#endif
#endif
#endif
#endif

#include <c_abstract_http/thread_pool.h>
/* clang-format on */

static void sleep_ms(int ms) {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(ms);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  delay(ms);
#else
  usleep(ms * 1000);
#endif
}

struct TestTaskData {
  struct CddMutex *lock;
  int *counter;
};

static void test_task_cb(void *arg) {
  struct TestTaskData *data = (struct TestTaskData *)arg;
  sleep_ms(5); /* Simulate work */
  cdd_mutex_lock(data->lock);
  (*data->counter)++;
  cdd_mutex_unlock(data->lock);
  free(data);
}

TEST test_thread_pool_execution(void) {
  struct CddThreadPool *pool = NULL;
  struct CddMutex *lock = NULL;
  int counter = 0;
  int i;

  ASSERT_EQ(0, cdd_mutex_init(&lock));
  ASSERT_EQ(0, cdd_thread_pool_init(&pool, 4));

  for (i = 0; i < 50; ++i) {
    struct TestTaskData *data =
        (struct TestTaskData *)malloc(sizeof(struct TestTaskData));
    data->lock = lock;
    data->counter = &counter;
    ASSERT_EQ(0, cdd_thread_pool_push(pool, test_task_cb, data));
  }

  cdd_thread_pool_free(pool); /* Blocks until all tasks complete */

  ASSERT_EQ(50, counter);

  cdd_mutex_free(lock);
  PASS();
}

TEST test_mutex_lock_unlock(void) {
  struct CddMutex *lock = NULL;
  ASSERT_EQ(0, cdd_mutex_init(&lock));
  ASSERT_EQ(0, cdd_mutex_lock(lock));
  ASSERT_EQ(0, cdd_mutex_unlock(lock));
  cdd_mutex_free(lock);
  PASS();
}

TEST test_thread_pool_errors(void) {
  struct CddMutex *lock = NULL;
  struct CddCond *cond = NULL;
  struct CddThreadPool *pool = NULL;

  ASSERT_EQ(EINVAL, cdd_mutex_init(NULL));
  ASSERT_EQ(EINVAL, cdd_mutex_lock(NULL));
  ASSERT_EQ(EINVAL, cdd_mutex_unlock(NULL));
  cdd_mutex_free(NULL);

  ASSERT_EQ(EINVAL, cdd_cond_init(NULL));
  ASSERT_EQ(EINVAL, cdd_cond_wait(NULL, NULL));
  ASSERT_EQ(0, cdd_mutex_init(&lock));
  ASSERT_EQ(EINVAL, cdd_cond_wait(NULL, lock));
  ASSERT_EQ(EINVAL, cdd_cond_signal(NULL));
  ASSERT_EQ(EINVAL, cdd_cond_broadcast(NULL));
  cdd_cond_free(NULL);

  ASSERT_EQ(0, cdd_cond_init(&cond));
  ASSERT_EQ(0, cdd_cond_signal(cond));
  ASSERT_EQ(0, cdd_cond_broadcast(cond));

  cdd_cond_free(cond);
  cdd_mutex_free(lock);

  ASSERT_EQ(EINVAL, cdd_thread_pool_init(NULL, 1));
  ASSERT_EQ(EINVAL, cdd_thread_pool_init(&pool, 0));
  ASSERT_EQ(EINVAL, cdd_thread_pool_push(NULL, NULL, NULL));
  cdd_thread_pool_free(NULL);

  PASS();
}

static void dummy_cb_thread(void *arg) { (void)arg; }
TEST test_thread_pool_external(void) {
  struct CddThreadPool *pool;
  struct CddThreadPoolHooks hooks;
  memset(&hooks, 0, sizeof(hooks));
  ASSERT_EQ(0, cdd_thread_pool_init_external(&pool, &hooks));
  ASSERT_EQ(ENOTSUP, cdd_thread_pool_push(pool, dummy_cb_thread, NULL));
  cdd_thread_pool_free(pool);
  PASS();
}

SUITE(thread_pool_suite) {
  RUN_TEST(test_thread_pool_external);
  RUN_TEST(test_thread_pool_errors);
  RUN_TEST(test_mutex_lock_unlock);
  RUN_TEST(test_thread_pool_execution);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
