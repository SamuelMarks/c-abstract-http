#ifndef TEST_THREAD_POOL_H
#define TEST_THREAD_POOL_H

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <c_abstract_http/thread_pool.h>
/* clang-format on */

static void sleep_ms(int ms) {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(ms);
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

SUITE(thread_pool_suite) {
  RUN_TEST(test_mutex_lock_unlock);
  RUN_TEST(test_thread_pool_execution);
}

#endif
