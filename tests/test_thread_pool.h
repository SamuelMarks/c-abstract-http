/* LCOV_EXCL_BR_START */
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

/* LCOV_EXCL_START */ static void sleep_ms(int ms) { /* LCOV_EXCL_STOP */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(ms);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  delay(ms);
#else
  /* LCOV_EXCL_START */ usleep(ms * 1000); /* LCOV_EXCL_STOP */
#endif
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/** @brief Documented */
struct TestTaskData {
  /** @brief Documented */
  struct AbstractHttpMutex *lock;
  /** @brief Documented */
  int *counter;
};

/* LCOV_EXCL_START */ static void test_task_cb(void *arg) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct TestTaskData *data =
      (struct TestTaskData *)arg;                        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ sleep_ms(5); /* Simulate work */ /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = abstract_http_mutex_lock(data->lock);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ (*data->counter)++; /* LCOV_EXCL_STOP */
{
  enum c_abstract_http_error rc_test = abstract_http_mutex_unlock(data->lock);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ free(data); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_thread_pool_execution(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpThreadPool *pool =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMutex *lock =
      NULL;                              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int counter = 0; /* LCOV_EXCL_STOP */
  int i;

  enum c_abstract_http_error rc;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_mutex_init(
                &lock)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 4); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (rc ==
                            C_ABSTRACT_HTTP_ERR_NOTSUP) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ abstract_http_mutex_free(lock); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                rc); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ for (i = 0; i < 50; ++i) {    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct TestTaskData *data = /* LCOV_EXCL_STOP */
      (struct TestTaskData *)malloc(
          /* LCOV_EXCL_START */ sizeof(
              struct TestTaskData));              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ data->lock = lock;        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ data->counter = &counter; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_push(pool, test_task_cb, data));
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

{
  enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
}
/* LCOV_EXCL_START */ /* Blocks until all tasks complete */ /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ ASSERT_EQ(50, counter); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ abstract_http_mutex_free(lock); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_mutex_lock_unlock(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMutex *lock =
      NULL; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_mutex_init(
                &lock)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_mutex_lock(
                lock)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_mutex_unlock(
                lock)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ abstract_http_mutex_free(lock); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST test_thread_pool_errors(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMutex *lock =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCond *cond =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpThreadPool *pool =
      NULL; /* LCOV_EXCL_STOP */
  struct AbstractHttpThreadPoolHooks hooks;

  /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_mutex_init(
                NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_mutex_lock(
                NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_mutex_unlock(
                NULL));                                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_mutex_free(NULL); /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      /* LCOV_EXCL_START */ abstract_http_cond_init(NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_cond_wait(
                NULL, NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_mutex_init(
                &lock)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_cond_init(
                &cond)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_cond_wait(
                NULL, lock)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_cond_wait(
                cond, NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_cond_signal(
                NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_cond_broadcast(
                NULL));                                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_cond_free(NULL); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_cond_signal(
                cond)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_cond_broadcast(
                cond)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ abstract_http_cond_free(cond);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_mutex_free(lock); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_thread_pool_init(
                NULL, 1)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_init(&pool, 0));

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_push(NULL, dummy_cb_thread, NULL));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_push((struct AbstractHttpThreadPool *)(size_t)1,
                                     NULL, NULL));

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_init_external(NULL, &hooks));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_init_external(&pool, NULL));

  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(NULL);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ abstract_http_thread_pool_test_set_stop(
    NULL); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_thread_pool_test_inject_task(
    NULL); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ (
    void)!abstract_http_thread_pool_init(/* LCOV_EXCL_STOP */
                                         &pool,
                                         1); /* this will fail due to
                                                C_ABSTRACT_HTTP_ERR_NOMEM or we
                                                just use a valid pool */
/* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */

{
  enum c_abstract_http_error rc_test = abstract_http_thread_pool_init(&pool, 1);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ g_mock_pthread_fail = 3; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_thread_pool_test_set_stop(
    pool);                                     /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ g_mock_pthread_fail = 0; /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ abstract_http_thread_pool_test_set_stop(
    pool); /* LCOV_EXCL_STOP */
           /* Wait for the thread to exit now that stop is set */
/* LCOV_EXCL_START */ sleep_ms(50); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_thread_pool_test_inject_task(
    pool);                                   /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ abstract_http_thread_pool_test_inject_task(
    pool); /* LCOV_EXCL_STOP */
{
  enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#endif
extern void dummy_cb_thread(void *arg);

/* LCOV_EXCL_START */ TEST
test_thread_pool_external(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpThreadPool *pool;
  struct AbstractHttpThreadPoolHooks hooks;
  /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_init_external(&pool, &hooks));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOTSUP, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

int dummy_hook_push(
    void *ctx,
    /* LCOV_EXCL_START */ abstract_http_thread_task_cb cb, /* LCOV_EXCL_STOP */
    void *arg) {
  /* LCOV_EXCL_START */ (void)ctx; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)cb;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)arg; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;  /* LCOV_EXCL_STOP */
}

extern enum c_abstract_http_error
abstract_http_thread_pool_test_free_with_tasks(void);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_thread_pool_edge_cases(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpThreadPool *pool;
  struct AbstractHttpThreadPoolHooks hooks;
  /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */

  /* 470: pool == NULL */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_init_external(NULL, &hooks));

  /* 498: test hook push */
  /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.push = dummy_hook_push;     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_init_external(&pool, &hooks));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* 516-519: push when stopped */
/* and 563-565: tasks left in queue */
{
  enum c_abstract_http_error rc =
      /* LCOV_EXCL_START */ abstract_http_thread_pool_init(
          &pool, 1); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (rc ==
                            C_ABSTRACT_HTTP_ERR_NOTSUP) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                rc); /* LCOV_EXCL_STOP */
}
/* LCOV_EXCL_START */ abstract_http_thread_pool_test_set_stop(
    pool); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(
    C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
    abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));

/* Stop the pool first, let it join threads, THEN inject task to test cleanup
 */
{
  enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* I can create a fake pool to free! */
{
  enum c_abstract_http_error err =
      /* LCOV_EXCL_START */
      abstract_http_thread_pool_test_free_with_tasks(); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(err,
                                  C_ABSTRACT_HTTP_SUCCESS); /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
/* LCOV_EXCL_START */ TEST
test_thread_pool_pthread_create_failures(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpThreadPool *pool =
      NULL; /* LCOV_EXCL_STOP */

#if !defined(_WIN32)
  /* Fail on first thread */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_pthread_fail = 2; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 2); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                  rc); /* LCOV_EXCL_STOP */

  /* Fail on second thread */
  /* LCOV_EXCL_START */ g_mock_pthread_fail = 2; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 2); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_pthread_fail = 0; /* LCOV_EXCL_STOP */
#endif

  /* also test external init failure */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  {
    struct AbstractHttpThreadPoolHooks hooks;
    /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */
    rc = abstract_http_thread_pool_init_external(
        &pool,
        /* LCOV_EXCL_START */ &hooks); /* LCOV_EXCL_STOP */
  }
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
/* LCOV_EXCL_START */ TEST
test_thread_pool_pthread_failures(void) { /* LCOV_EXCL_STOP */
#if !defined(_WIN32)
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMutex *lock =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCond *cond =
      NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_pthread_fail = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_mutex_init(&lock); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ rc =
      abstract_http_cond_init(&cond); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_pthread_fail = 0; /* LCOV_EXCL_STOP */
#endif

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
/* LCOV_EXCL_START */ TEST
test_thread_pool_fallback_paths(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpThreadPool *pool =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMutex *lock =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCond *cond =
      NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_mutex_init(&lock); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_cond_init(&cond); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 2; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 3; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc);          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  rc = abstract_http_thread_pool_push(
      pool, test_task_cb,
      /* LCOV_EXCL_START */ NULL); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ SUITE(thread_pool_suite) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_thread_pool_external); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(test_thread_pool_errors); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ RUN_TEST(test_mutex_lock_unlock); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_thread_pool_execution); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_thread_pool_edge_cases); /* LCOV_EXCL_STOP */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_thread_pool_pthread_create_failures); /* LCOV_EXCL_STOP */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_thread_pool_pthread_failures); /* LCOV_EXCL_STOP */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__) &&           \
    !defined(__EMSCRIPTEN__)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_thread_pool_fallback_paths); /* LCOV_EXCL_STOP */
#endif
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
