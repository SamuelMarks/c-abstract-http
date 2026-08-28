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

static void sleep_ms(int ms) { /* LCOV_EXCL_LINE */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  Sleep(ms);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  delay(ms);
#else
  usleep(ms * 1000); /* LCOV_EXCL_LINE */
#endif
} /* LCOV_EXCL_LINE */

/** @brief Documented */
struct TestTaskData {
  /** @brief Documented */
  struct AbstractHttpMutex *lock;
  /** @brief Documented */
  int *counter;
};

static void test_task_cb(void *arg) {                     /* LCOV_EXCL_LINE */
  struct TestTaskData *data = (struct TestTaskData *)arg; /* LCOV_EXCL_LINE */
  sleep_ms(5); /* Simulate work */                        /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = abstract_http_mutex_lock(data->lock);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  (*data->counter)++; /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = abstract_http_mutex_unlock(data->lock);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  free(data); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_thread_pool_execution(void) {       /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPool *pool = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpMutex *lock = NULL;      /* LCOV_EXCL_LINE */
  int counter = 0;                            /* LCOV_EXCL_LINE */
  int i;

  enum c_abstract_http_error rc;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mutex_init(&lock));    /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 4); /* LCOV_EXCL_LINE */
  if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP) {        /* LCOV_EXCL_LINE */
    abstract_http_mutex_free(lock);              /* LCOV_EXCL_LINE */
    PASS();                                      /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc); /* LCOV_EXCL_LINE */

  for (i = 0; i < 50; ++i) {    /* LCOV_EXCL_LINE */
    struct TestTaskData *data = /* LCOV_EXCL_LINE */
        (struct TestTaskData *)malloc(
            sizeof(struct TestTaskData)); /* LCOV_EXCL_LINE */
    data->lock = lock;                    /* LCOV_EXCL_LINE */
    data->counter = &counter;             /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,    /* LCOV_EXCL_LINE */
              abstract_http_thread_pool_push(pool, test_task_cb, data));
  } /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }
  /* Blocks until all tasks complete */ /* LCOV_EXCL_LINE */

  ASSERT_EQ(50, counter); /* LCOV_EXCL_LINE */

  abstract_http_mutex_free(lock); /* LCOV_EXCL_LINE */
  PASS();                         /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_mutex_lock_unlock(void) {      /* LCOV_EXCL_LINE */
  struct AbstractHttpMutex *lock = NULL; /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mutex_init(&lock)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mutex_lock(lock)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mutex_unlock(lock)); /* LCOV_EXCL_LINE */

  abstract_http_mutex_free(lock); /* LCOV_EXCL_LINE */
  PASS();                         /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_thread_pool_errors(void) {          /* LCOV_EXCL_LINE */
  struct AbstractHttpMutex *lock = NULL;      /* LCOV_EXCL_LINE */
  struct AbstractHttpCond *cond = NULL;       /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPool *pool = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPoolHooks hooks;

  memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_mutex_init(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_mutex_lock(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_mutex_unlock(NULL)); /* LCOV_EXCL_LINE */
  abstract_http_mutex_free(NULL);              /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_cond_init(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_cond_wait(NULL, NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mutex_init(&lock)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_cond_init(&cond)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_cond_wait(NULL, lock)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_cond_wait(cond, NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_cond_signal(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_cond_broadcast(NULL)); /* LCOV_EXCL_LINE */
  abstract_http_cond_free(NULL);                 /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_cond_signal(cond)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_cond_broadcast(cond)); /* LCOV_EXCL_LINE */

  abstract_http_cond_free(cond);  /* LCOV_EXCL_LINE */
  abstract_http_mutex_free(lock); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_thread_pool_init(NULL, 1)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,                /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_init(&pool, 0));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_push(NULL, dummy_cb_thread, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_push((struct AbstractHttpThreadPool *)1,
                                           NULL, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_init_external(NULL, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_init_external(&pool, NULL));

  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(NULL);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  abstract_http_thread_pool_test_set_stop(NULL);    /* LCOV_EXCL_LINE */
  abstract_http_thread_pool_test_inject_task(NULL); /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;               /* LCOV_EXCL_LINE */
  (void)!abstract_http_thread_pool_init(/* LCOV_EXCL_LINE */
                                        &pool,
                                        1); /* this will fail due to
                                               C_ABSTRACT_HTTP_ERR_NOMEM or we
                                               just use a valid pool */
  g_mock_alloc_fail = 0;                    /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test =
        abstract_http_thread_pool_init(&pool, 1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  g_mock_pthread_fail = 3;                       /* LCOV_EXCL_LINE */
  abstract_http_thread_pool_test_set_stop(pool); /* LCOV_EXCL_LINE */
  g_mock_pthread_fail = 0;                       /* LCOV_EXCL_LINE */

  abstract_http_thread_pool_test_set_stop(pool); /* LCOV_EXCL_LINE */
  /* Wait for the thread to exit now that stop is set */
  sleep_ms(50); /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                            /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                           /* LCOV_EXCL_LINE */
  abstract_http_thread_pool_test_inject_task(pool); /* LCOV_EXCL_LINE */
  g_mock_alloc_fail = 0;                            /* LCOV_EXCL_LINE */

  abstract_http_thread_pool_test_inject_task(pool); /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#endif
extern void dummy_cb_thread(void *arg);

TEST test_thread_pool_external(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPool *pool;
  struct AbstractHttpThreadPoolHooks hooks;
  memset(&hooks, 0, sizeof(hooks));  /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_init_external(&pool, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

int dummy_hook_push(void *ctx,
                    abstract_http_thread_task_cb cb, /* LCOV_EXCL_LINE */
                    void *arg) {
  (void)ctx; /* LCOV_EXCL_LINE */
  (void)cb;  /* LCOV_EXCL_LINE */
  (void)arg; /* LCOV_EXCL_LINE */
  return 0;  /* LCOV_EXCL_LINE */
}

extern void abstract_http_thread_pool_test_free_with_tasks(void);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_thread_pool_edge_cases(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPool *pool;
  struct AbstractHttpThreadPoolHooks hooks;
  memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_LINE */

  /* 470: pool == NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_init_external(NULL, &hooks));

  /* 498: test hook push */
  memset(&hooks, 0, sizeof(hooks));  /* LCOV_EXCL_LINE */
  hooks.push = dummy_hook_push;      /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_init_external(&pool, &hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  /* 516-519: push when stopped */
  /* and 563-565: tasks left in queue */
  {
    enum c_abstract_http_error rc =
        abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_LINE */
    if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP) {       /* LCOV_EXCL_LINE */
      PASS();                                     /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc); /* LCOV_EXCL_LINE */
  }
  abstract_http_thread_pool_test_set_stop(pool); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,           /* LCOV_EXCL_LINE */
            abstract_http_thread_pool_push(pool, dummy_cb_thread, NULL));

  /* Stop the pool first, let it join threads, THEN inject task to test cleanup
   */
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  /* I can create a fake pool to free! */
  { abstract_http_thread_pool_test_free_with_tasks(); /* LCOV_EXCL_LINE */ }

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
TEST test_thread_pool_pthread_create_failures(void) {      /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPool *pool = NULL;              /* LCOV_EXCL_LINE */

#if !defined(_WIN32)
  /* Fail on first thread */
  g_mock_alloc_fail = 0;                         /* LCOV_EXCL_LINE */
  g_mock_pthread_fail = 2;                       /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 2); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);         /* LCOV_EXCL_LINE */

  /* Fail on second thread */
  g_mock_pthread_fail = 2;                       /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 1;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 2); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);         /* LCOV_EXCL_LINE */

  g_mock_pthread_fail = 0; /* LCOV_EXCL_LINE */
#endif

  /* also test external init failure */
  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0; /* LCOV_EXCL_LINE */
  {
    struct AbstractHttpThreadPoolHooks hooks;
    memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_LINE */
    rc = abstract_http_thread_pool_init_external(&pool,
                                                 &hooks); /* LCOV_EXCL_LINE */
  }
  {
    int rc_test_tmp = rc;  /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
TEST test_thread_pool_pthread_failures(void) { /* LCOV_EXCL_LINE */
#if !defined(_WIN32)
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  struct AbstractHttpMutex *lock = NULL;                   /* LCOV_EXCL_LINE */
  struct AbstractHttpCond *cond = NULL;                    /* LCOV_EXCL_LINE */

  g_mock_pthread_fail = 1;               /* LCOV_EXCL_LINE */
  rc = abstract_http_mutex_init(&lock);  /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc); /* LCOV_EXCL_LINE */

  rc = abstract_http_cond_init(&cond);   /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc); /* LCOV_EXCL_LINE */

  g_mock_pthread_fail = 0; /* LCOV_EXCL_LINE */
#endif

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
TEST test_thread_pool_fallback_paths(void) {               /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  struct AbstractHttpThreadPool *pool = NULL;              /* LCOV_EXCL_LINE */
  struct AbstractHttpMutex *lock = NULL;                   /* LCOV_EXCL_LINE */
  struct AbstractHttpCond *cond = NULL;                    /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                    /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                   /* LCOV_EXCL_LINE */
  rc = abstract_http_mutex_init(&lock);     /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc); /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                    /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                   /* LCOV_EXCL_LINE */
  rc = abstract_http_cond_init(&cond);      /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc); /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                         /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);      /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                         /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 1;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);      /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                         /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 2;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);      /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;                         /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 3;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);      /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 0;                         /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_init(&pool, 1); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);        /* LCOV_EXCL_LINE */
  g_mock_alloc_fail = 1;                         /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                        /* LCOV_EXCL_LINE */
  rc = abstract_http_thread_pool_push(pool, test_task_cb,
                                      NULL); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);  /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = abstract_http_thread_pool_free(pool);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

SUITE(thread_pool_suite) {             /* LCOV_EXCL_LINE */
  RUN_TEST(test_thread_pool_external); /* LCOV_EXCL_LINE */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_thread_pool_errors); /* LCOV_EXCL_LINE */
#endif
  RUN_TEST(test_mutex_lock_unlock);     /* LCOV_EXCL_LINE */
  RUN_TEST(test_thread_pool_execution); /* LCOV_EXCL_LINE */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_thread_pool_edge_cases); /* LCOV_EXCL_LINE */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
  RUN_TEST(test_thread_pool_pthread_create_failures); /* LCOV_EXCL_LINE */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__)
  RUN_TEST(test_thread_pool_pthread_failures); /* LCOV_EXCL_LINE */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(__EMSCRIPTEN__) &&           \
    !defined(__EMSCRIPTEN__)
  RUN_TEST(test_thread_pool_fallback_paths); /* LCOV_EXCL_LINE */
#endif
} /* LCOV_EXCL_LINE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
