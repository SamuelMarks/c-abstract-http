
/**
 * @file thread_pool.c
 * @brief Cross-platform mutex, condition variable, and worker thread pool
 * implementation.
 */

/* clang-format off */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef _WIN32_WINNT
/** @brief Target Windows Vista+ API level */
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No threading support on DOS */
#else
#include <pthread.h>
#endif
/** @brief Broadcast condition variable alias */
#define ABSTRACT_HTTP_COND_BROADCAST abstract_http_cond_broadcast

#include <c_abstract_http/thread_pool.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#ifndef ENOTSUP
/** @brief Fallback ENOTSUP value when unavailable in errno.h */
#define ENOTSUP EINVAL
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection mode for mutex init/lock/unlock failure tests */
extern int g_mock_mutex_fail;
/** @brief Fault-injection mode for condition variable failure tests */
extern int g_mock_cond_fail;
/** @brief Fault-injection flag for mutex destruction failure tests */
extern int g_mock_mutex_free_fail;
/** @brief Fault-injection flag for condition variable destruction failure tests
 */
extern int g_mock_cond_free_fail;
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct AbstractHttpMutex */
struct AbstractHttpMutex {
  CRITICAL_SECTION cs; /**< Win32 critical section */
};

/** @brief Internal struct AbstractHttpCond */
struct AbstractHttpCond {
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
  HANDLE semaphore; /**< Pre-Vista condition semaphore */
  int waiters;      /**< Number of waiting threads */
#else
  CONDITION_VARIABLE cv; /**< Win32 Vista+ condition variable */
#endif
};

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_init(struct AbstractHttpMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct AbstractHttpMutex *)malloc(sizeof(struct AbstractHttpMutex));
  if (!*mutex)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  InitializeCriticalSection(&(*mutex)->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_fail == 1)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (g_mock_mutex_fail == 3)
    g_mock_mutex_fail = 1;
#endif
  EnterCriticalSection(&mutex->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_fail == 2) {
    LeaveCriticalSection(&mutex->cs);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (g_mock_mutex_fail == 4)
    g_mock_mutex_fail = 2;
#endif
  LeaveCriticalSection(&mutex->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_free(struct AbstractHttpMutex *mutex) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (mutex) {
    DeleteCriticalSection(&mutex->cs);
    free(mutex);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_init(struct AbstractHttpCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct AbstractHttpCond *)malloc(sizeof(struct AbstractHttpCond));
  if (!*cond)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
  (*cond)->semaphore = CreateSemaphore(NULL, 0, MAXLONG, NULL);
  (*cond)->waiters = 0;
#else
  InitializeConditionVariable(&(*cond)->cv);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t abstract_http_cond_wait(
    struct AbstractHttpCond *cond, struct AbstractHttpMutex *mutex) {
  if (!cond || !mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_fail == 1)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
  cond->waiters++;
  LeaveCriticalSection(&mutex->cs);
  WaitForSingleObject(cond->semaphore, INFINITE);
  EnterCriticalSection(&mutex->cs);
#else
  SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_signal(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (g_mock_cond_fail == 4)
    g_mock_cond_fail = 2;
#endif
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
  if (cond->waiters > 0) {
    cond->waiters--;
    ReleaseSemaphore(cond->semaphore, 1, NULL);
  }
#else
  WakeConditionVariable(&cond->cv);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
ABSTRACT_HTTP_COND_BROADCAST(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_fail == 3)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
  if (cond->waiters > 0) {
    ReleaseSemaphore(cond->semaphore, cond->waiters, NULL);
    cond->waiters = 0;
  }
#else
  WakeAllConditionVariable(&cond->cv);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_free(struct AbstractHttpCond *cond) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (cond) {
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
    if (cond->semaphore) {
      CloseHandle(cond->semaphore);
    }
#endif
    free(cond);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Win32 thread handle alias */
typedef HANDLE abstract_http_thread_t;
/** @brief Win32 thread return type */
#define ABSTRACT_HTTP_THREAD_FUNC DWORD
/** @brief Win32 thread argument type */
typedef LPVOID abstract_http_thread_arg_t;
/** @brief Win32 thread start routine function pointer */
typedef DWORD(WINAPI *abstract_http_win32_thread_fn)(LPVOID);

/**
 * @brief Spawn a Win32 worker thread.
 * @param thread Output thread handle.
 * @param start_routine Thread entry point.
 * @param arg Thread argument pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t
thread_create(abstract_http_thread_t *thread,
              abstract_http_win32_thread_fn start_routine,
              abstract_http_thread_arg_t arg) {
  *thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
  return (*thread == NULL) ? C_ABSTRACT_HTTP_ERR_IO : C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Join and close a Win32 worker thread handle.
 * @param thread Thread handle to wait on and close.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
thread_join(abstract_http_thread_t thread) {
  if (!thread)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (WaitForSingleObject(thread, 0xFFFFFFFFUL) == 0xFFFFFFFFUL) {
    CloseHandle(thread);
    return C_ABSTRACT_HTTP_ERR_SYSCALL;
  }
  if (!CloseHandle(thread)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)

/** @brief Internal struct AbstractHttpMutex */
struct AbstractHttpMutex {
  int dummy; /**< Non-empty C89 struct placeholder */
};
/** @brief Internal struct AbstractHttpCond */
struct AbstractHttpCond {
  int dummy; /**< Non-empty C89 struct placeholder */
};

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_init(struct AbstractHttpMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct AbstractHttpMutex *)malloc(sizeof(struct AbstractHttpMutex));
  return *mutex ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_NOMEM;
}
NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex) {
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex) {
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_free(struct AbstractHttpMutex *mutex) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  free(mutex);
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_init(struct AbstractHttpCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct AbstractHttpCond *)malloc(sizeof(struct AbstractHttpCond));
  return *cond ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_NOMEM;
}
NO_DISCARD c_abstract_http_error_t abstract_http_cond_wait(
    struct AbstractHttpCond *cond, struct AbstractHttpMutex *mutex) {
  (void)cond;
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
NO_DISCARD c_abstract_http_error_t
abstract_http_cond_signal(struct AbstractHttpCond *cond) {
  (void)cond;
  return C_ABSTRACT_HTTP_SUCCESS;
}
NO_DISCARD c_abstract_http_error_t
ABSTRACT_HTTP_COND_BROADCAST(struct AbstractHttpCond *cond) {
  (void)cond;
  return C_ABSTRACT_HTTP_SUCCESS;
}
NO_DISCARD c_abstract_http_error_t
abstract_http_cond_free(struct AbstractHttpCond *cond) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  free(cond);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Cooperative MS-DOS thread identifier */
typedef int abstract_http_thread_t;
/** @brief MS-DOS thread start routine return type */
#define ABSTRACT_HTTP_THREAD_FUNC void *
/** @brief MS-DOS thread start routine argument type */
typedef void *abstract_http_thread_arg_t;

/**
 * @brief Stub thread creation helper for cooperative MS-DOS targets.
 * @param thread Receives synthetic thread handle.
 * @param start_routine Thread entry callback (unused on DOS).
 * @param arg Thread argument pointer (unused on DOS).
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t thread_create(
    abstract_http_thread_t *thread,
    ABSTRACT_HTTP_THREAD_FUNC (*start_routine)(abstract_http_thread_arg_t),
    abstract_http_thread_arg_t arg) {
  (void)start_routine;
  (void)arg;
  if (!thread)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *thread = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}
/**
 * @brief Stub thread join helper for cooperative MS-DOS targets.
 * @param thread Synthetic thread handle.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
thread_join(abstract_http_thread_t thread) {
  if (!thread)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#else /* POSIX */

/** @brief Internal struct AbstractHttpMutex */
struct AbstractHttpMutex {
  /** @brief mtx (variable) of struct AbstractHttpMutex */
  pthread_mutex_t mtx;
};

/** @brief Internal struct AbstractHttpCond */
struct AbstractHttpCond {
  /** @brief cond (variable) of struct AbstractHttpCond */
  pthread_cond_t cond;
};

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_init(struct AbstractHttpMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct AbstractHttpMutex *)malloc(sizeof(struct AbstractHttpMutex));
  if (!*mutex)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  if (pthread_mutex_init(&(*mutex)->mtx, NULL) != 0) {
    free(*mutex);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_fail == 1)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (g_mock_mutex_fail == 3)
    g_mock_mutex_fail = 1;
#endif
  return pthread_mutex_lock(&mutex->mtx) == 0 ? C_ABSTRACT_HTTP_SUCCESS
                                              : C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_fail == 2) {
    int unlock_rc = pthread_mutex_unlock(&mutex->mtx);
    return (c_abstract_http_error_t)(unlock_rc * 0 + C_ABSTRACT_HTTP_ERR_IO);
  }
  if (g_mock_mutex_fail == 4)
    g_mock_mutex_fail = 2;
#endif
  return pthread_mutex_unlock(&mutex->mtx) == 0 ? C_ABSTRACT_HTTP_SUCCESS
                                                : C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_mutex_free(struct AbstractHttpMutex *mutex) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mutex_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (mutex) {
    pthread_mutex_destroy(&mutex->mtx);
    free(mutex);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_init(struct AbstractHttpCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct AbstractHttpCond *)malloc(sizeof(struct AbstractHttpCond));
  if (!*cond)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  if (pthread_cond_init(&(*cond)->cond, NULL) != 0) {
    free(*cond);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t abstract_http_cond_wait(
    struct AbstractHttpCond *cond, struct AbstractHttpMutex *mutex) {
  if (!cond || !mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_fail == 1)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  return pthread_cond_wait(&cond->cond, &mutex->mtx) == 0
             ? C_ABSTRACT_HTTP_SUCCESS
             : C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_signal(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (g_mock_cond_fail == 4)
    g_mock_cond_fail = 2;
#endif
  return pthread_cond_signal(&cond->cond) == 0 ? C_ABSTRACT_HTTP_SUCCESS
                                               : C_ABSTRACT_HTTP_ERR_IO;
}

/* Wake all threads waiting on a condition variable. */
NO_DISCARD c_abstract_http_error_t
ABSTRACT_HTTP_COND_BROADCAST(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_fail == 3)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  return pthread_cond_broadcast(&cond->cond) == 0 ? C_ABSTRACT_HTTP_SUCCESS
                                                  : C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_cond_free(struct AbstractHttpCond *cond) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (cond) {
    pthread_cond_destroy(&cond->cond);
    free(cond);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief POSIX thread handle type */
typedef pthread_t abstract_http_thread_t;
/** @brief POSIX thread return type */
#define ABSTRACT_HTTP_THREAD_FUNC void *
/** @brief POSIX thread argument type */
typedef void *abstract_http_thread_arg_t;

/**
 * @brief Spawn a POSIX worker thread.
 * @param thread Output thread handle.
 * @param start_routine Thread entry point.
 * @param arg Thread argument pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t thread_create(
    abstract_http_thread_t *thread,
    ABSTRACT_HTTP_THREAD_FUNC (*start_routine)(abstract_http_thread_arg_t),
    abstract_http_thread_arg_t arg) {
  return (pthread_create(thread, NULL, start_routine, arg) == 0)
             ? C_ABSTRACT_HTTP_SUCCESS
             : C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Join a POSIX worker thread.
 * @param thread Thread handle to wait on.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t
thread_join(abstract_http_thread_t thread) {
  if (pthread_join(thread, NULL) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* POSIX vs WIN32 */

/* --- Thread Pool Implementation --- */

/** @brief Internal struct TaskNode */
struct TaskNode {
  /** @brief cb (variable) of struct TaskNode */
  abstract_http_thread_task_cb cb;
  /** @brief arg (variable) of struct TaskNode */
  void *arg;
  /** @brief next (variable) of struct TaskNode */
  struct TaskNode *next;
};

/** @brief Internal struct AbstractHttpThreadPool */
struct AbstractHttpThreadPool {
  /** @brief is_external (variable) of struct AbstractHttpThreadPool */
  int is_external;
  /** @brief hooks (variable) of struct AbstractHttpThreadPool */
  struct AbstractHttpThreadPoolHooks hooks;
  /** @brief threads (variable) of struct AbstractHttpThreadPool */
  abstract_http_thread_t *threads;
  /** @brief num_threads (variable) of struct AbstractHttpThreadPool */
  size_t num_threads;
  /** @brief head (variable) of struct AbstractHttpThreadPool */
  struct TaskNode *head;
  /** @brief tail (variable) of struct AbstractHttpThreadPool */
  struct TaskNode *tail;
  /** @brief lock (variable) of struct AbstractHttpThreadPool */
  struct AbstractHttpMutex *lock;
  /** @brief cond (variable) of struct AbstractHttpThreadPool */
  struct AbstractHttpCond *cond;
  /** @brief stop (variable) of struct AbstractHttpThreadPool */
  int stop;
};

/**
 * @brief Execute a queued worker task callback and log any error enum.
 * @param cb_fn Task callback function pointer.
 * @param cb_arg Task user argument pointer.
 */
#define THREAD_POOL_RUN_TASK(cb_fn, cb_arg)                                    \
  do {                                                                         \
    c_abstract_http_error_t _t_rc = (cb_fn)((cb_arg));                         \
    if (_t_rc != C_ABSTRACT_HTTP_SUCCESS) {                                    \
      LOG_DEBUG("worker_thread: task callback returned error %d", (int)_t_rc); \
    }                                                                          \
  } while (0)

/**
 * @brief Main loop dequeuing and running worker tasks until pool shutdown.
 * @param pool Active thread pool instance.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization error enum.
 */
NO_DISCARD static c_abstract_http_error_t
worker_thread_loop(struct AbstractHttpThreadPool *pool) {
  for (;;) {
    struct TaskNode *task = NULL;
    c_abstract_http_error_t err;

    err = abstract_http_mutex_lock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      return err;
    }

    while (!pool->stop && !pool->head) {
      err = abstract_http_cond_wait(pool->cond, pool->lock);
      if (err != C_ABSTRACT_HTTP_SUCCESS) {
        c_abstract_http_error_t u_err = abstract_http_mutex_unlock(pool->lock);
        if (u_err != C_ABSTRACT_HTTP_SUCCESS) {
          return u_err;
        }
        return err;
      }
    }

    if (pool->stop && !pool->head) {
      err = abstract_http_mutex_unlock(pool->lock);
      if (err != C_ABSTRACT_HTTP_SUCCESS) {
        return err;
      }
      break;
    }

    task = pool->head;
    pool->head = task->next;
    if (!pool->head) {
      pool->tail = NULL;
    }

    err = abstract_http_mutex_unlock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      THREAD_POOL_RUN_TASK(task->cb, task->arg);
      free(task);
      return err;
    }

    THREAD_POOL_RUN_TASK(task->cb, task->arg);
    free(task);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
/**
 * @brief Platform worker thread entry point on Windows.
 * @param arg Pointer to struct AbstractHttpThreadPool.
 * @return 0 on clean exit or non-zero error status.
 */
static DWORD WINAPI worker_thread(abstract_http_thread_arg_t arg) {
#else
/**
 * @brief Platform worker thread entry point on POSIX.
 * @param arg Pointer to struct AbstractHttpThreadPool.
 * @return NULL on clean exit or encoded error status.
 */
static void *worker_thread(abstract_http_thread_arg_t arg) {
#endif
  struct AbstractHttpThreadPool *pool = (struct AbstractHttpThreadPool *)arg;
  c_abstract_http_error_t rc = worker_thread_loop(pool);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return (ABSTRACT_HTTP_THREAD_FUNC)(unsigned long)rc;
  }
  return (ABSTRACT_HTTP_THREAD_FUNC)0;
}

/**
 * @brief Join a worker thread and record the first error into acc_err.
 * @param th Thread handle to join.
 * @param acc_err Lvalue enum variable accumulating the first join error.
 */
#define THREAD_POOL_JOIN_ACCUM(th, acc_err)                                    \
  do {                                                                         \
    c_abstract_http_error_t _j_rc = thread_join((th));                         \
    if (_j_rc != C_ABSTRACT_HTTP_SUCCESS) {                                    \
      LOG_DEBUG("thread_pool: thread_join failed %d", (int)_j_rc);             \
      if ((acc_err) == C_ABSTRACT_HTTP_SUCCESS) {                              \
        (acc_err) = _j_rc;                                                     \
      }                                                                        \
    }                                                                          \
  } while (0)

NO_DISCARD c_abstract_http_error_t abstract_http_thread_pool_init(
    struct AbstractHttpThreadPool **pool, size_t num_threads) {
  struct AbstractHttpThreadPool *p;
  size_t i;

  if (!pool || num_threads == 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = (struct AbstractHttpThreadPool *)malloc(
      sizeof(struct AbstractHttpThreadPool));
  if (!p)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memset(p, 0, sizeof(struct AbstractHttpThreadPool));

  p->num_threads = num_threads;
  p->threads = (abstract_http_thread_t *)malloc(num_threads *
                                                sizeof(abstract_http_thread_t));
  if (!p->threads) {
    free(p);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  {
    c_abstract_http_error_t err;
    err = abstract_http_mutex_init(&p->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      free(p->threads);
      free(p);
      return err;
    }

    err = abstract_http_cond_init(&p->cond);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      {
        c_abstract_http_error_t rc = abstract_http_mutex_free(p->lock);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(p->threads);
          free(p);
          return rc;
        }
      }
      free(p->threads);
      free(p);
      return err;
    }
  }

  p->stop = 0;

  for (i = 0; i < num_threads; ++i) {
    c_abstract_http_error_t tc_rc =
        thread_create(&p->threads[i], worker_thread, p);
    if (tc_rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t berr;
      c_abstract_http_error_t cleanup_err;
      /* If we fail partway, trigger stop and join what we have */
      cleanup_err = abstract_http_mutex_lock(p->lock);
      if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
        goto init_join_threads;
      }
      p->stop = 1;
      berr = ABSTRACT_HTTP_COND_BROADCAST(p->cond);
      if (berr != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("abstract_http_thread_pool_init: broadcast failed with %d",
                  (int)berr);
      }
      cleanup_err = abstract_http_mutex_unlock(p->lock);
      if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("abstract_http_thread_pool_init: unlock failed with %d",
                  (int)cleanup_err);
        goto init_join_threads;
      }
    init_join_threads:
      while (i > 0) {
        i--;
        THREAD_POOL_JOIN_ACCUM(p->threads[i], cleanup_err);
      }
      {
        c_abstract_http_error_t rc = abstract_http_cond_free(p->cond);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(p->threads);
          free(p);
          return rc;
        }
      }
      {
        c_abstract_http_error_t rc = abstract_http_mutex_free(p->lock);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(p->threads);
          free(p);
          return rc;
        }
      }
      free(p->threads);
      free(p);
      if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
        return cleanup_err;
      }
      return tc_rc;
    }
  }

  *pool = p;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an external hook-backed thread pool */
NO_DISCARD c_abstract_http_error_t abstract_http_thread_pool_init_external(
    struct AbstractHttpThreadPool **pool,
    const struct AbstractHttpThreadPoolHooks *hooks) {
  struct AbstractHttpThreadPool *p;

  if (!pool || !hooks)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = (struct AbstractHttpThreadPool *)malloc(
      sizeof(struct AbstractHttpThreadPool));
  if (!p)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memset(p, 0, sizeof(struct AbstractHttpThreadPool));

  p->is_external = 1;
  p->hooks = *hooks;

  *pool = p;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_push(struct AbstractHttpThreadPool *pool,
                               abstract_http_thread_task_cb cb, void *arg) {
  struct TaskNode *task;

  LOG_DEBUG("abstract_http_thread_pool_push: Entering");
  if (!pool || !cb) {
    LOG_DEBUG("abstract_http_thread_pool_push: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (pool->is_external) {
    if (pool->hooks.push) {
      LOG_DEBUG("abstract_http_thread_pool_push: Hooking");
      return pool->hooks.push(pool->hooks.external_context, cb, arg);
    }
    LOG_DEBUG("abstract_http_thread_pool_push: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  task = (struct TaskNode *)malloc(sizeof(struct TaskNode));
  if (!task) {
    LOG_DEBUG("abstract_http_thread_pool_push: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  task->cb = cb;
  task->arg = arg;
  task->next = NULL;

  {
    c_abstract_http_error_t err = abstract_http_mutex_lock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      free(task);
      return err;
    }
    if (pool->stop) {
      err = abstract_http_mutex_unlock(pool->lock);
      if (err != C_ABSTRACT_HTTP_SUCCESS) {
        free(task);
        return err;
      }
      free(task);
      LOG_DEBUG("abstract_http_thread_pool_push: Error EINVAL (pool stopped)");
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }

    if (!pool->head) {
      pool->head = task;
      pool->tail = task;
    } else {
      pool->tail->next = task;
      pool->tail = task;
    }

    err = abstract_http_cond_signal(pool->cond);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t unlock_err =
          abstract_http_mutex_unlock(pool->lock);
      if (unlock_err != C_ABSTRACT_HTTP_SUCCESS) {
        return unlock_err;
      }
      return err;
    }
    {
      c_abstract_http_error_t unlock_err =
          abstract_http_mutex_unlock(pool->lock);
      if (unlock_err != C_ABSTRACT_HTTP_SUCCESS) {
        return unlock_err;
      }
    }
  }

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  while (pool->head && !pool->stop) {
    struct TaskNode *run_node = pool->head;
    pool->head = run_node->next;
    if (!pool->head) {
      pool->tail = NULL;
    }
    THREAD_POOL_RUN_TASK(run_node->cb, run_node->arg);
    free(run_node);
  }
#endif

  LOG_DEBUG("abstract_http_thread_pool_push: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_free(struct AbstractHttpThreadPool *pool) {
  size_t i;
  c_abstract_http_error_t join_err = C_ABSTRACT_HTTP_SUCCESS;
  LOG_DEBUG("abstract_http_thread_pool_free: Entering");
  if (!pool) {
    LOG_DEBUG("abstract_http_thread_pool_free: Exiting early (pool NULL)");
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  if (pool->is_external) {
    free(pool);
    LOG_DEBUG("abstract_http_thread_pool_free: Exiting (external pool freed)");
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  {
    c_abstract_http_error_t err;
    err = abstract_http_mutex_lock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      return err;
    }
    pool->stop = 1;
    err = ABSTRACT_HTTP_COND_BROADCAST(pool->cond);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t unlock_err =
          abstract_http_mutex_unlock(pool->lock);
      if (unlock_err != C_ABSTRACT_HTTP_SUCCESS) {
        return unlock_err;
      }
      return err;
    }
    {
      c_abstract_http_error_t unlock_err =
          abstract_http_mutex_unlock(pool->lock);
      if (unlock_err != C_ABSTRACT_HTTP_SUCCESS) {
        return unlock_err;
      }
    }
  }

  for (i = 0; i < pool->num_threads; ++i) {
    THREAD_POOL_JOIN_ACCUM(pool->threads[i], join_err);
  }

  /* Free any remaining tasks in queue */
  while (pool->head) {
    struct TaskNode *next = pool->head->next;
    free(pool->head);
    pool->head = next;
  }

  {
    c_abstract_http_error_t rc = abstract_http_cond_free(pool->cond);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
  }
  {
    c_abstract_http_error_t rc = abstract_http_mutex_free(pool->lock);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
  }
  free(pool->threads);
  free(pool);
  LOG_DEBUG("abstract_http_thread_pool_free: Exiting");
  return join_err;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Set the stop flag on a thread pool under lock for testing.
 * @param pool Target thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS or lock error enum.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_set_stop(struct AbstractHttpThreadPool *pool);
/** @brief Fault-injection mode for pthread/thread pool tests */
extern int g_mock_pthread_fail;

/**
 * @brief Acquire pool lock or inject simulated lock error in OOM test builds.
 * @param lock Pool mutex handle.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
pool_lock_for_stop(struct AbstractHttpMutex *lock) {
  if (g_mock_pthread_fail == 3) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return abstract_http_mutex_lock(lock);
}

/**
 * @brief Set the stop flag on a thread pool under lock for testing.
 * @param pool Target thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS or lock error enum.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_set_stop(struct AbstractHttpThreadPool *pool) {
  if (pool) {
    c_abstract_http_error_t err = pool_lock_for_stop(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      return err;
    }
    pool->stop = 1;
    err = abstract_http_mutex_unlock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      return err;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clear the stop flag on a thread pool for testing.
 * @param pool Target thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_clear_stop(struct AbstractHttpThreadPool *pool);
/**
 * @brief Clear the stop flag on a thread pool for testing.
 * @param pool Target thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_clear_stop(struct AbstractHttpThreadPool *pool) {
  if (pool) {
    pool->stop = 0;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Dummy thread task callback.
 * @param arg Unused task argument.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t dummy_cb_thread(void *arg);
/**
 * @brief Dummy thread task callback.
 * @param arg Unused task argument.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t dummy_cb_thread(void *arg) {
  (void)arg;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#else
/**
 * @brief Dummy thread task callback provided by test harness.
 * @param arg Unused task argument.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD extern c_abstract_http_error_t dummy_cb_thread(void *arg);
#endif
/**
 * @brief Inject a task directly into the thread pool queue for testing.
 * @param pool Target thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on NULL/OOM.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_inject_task(struct AbstractHttpThreadPool *pool);
/**
 * @brief Inject a task directly into the thread pool queue for testing.
 * @param pool Target thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on NULL/OOM.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_thread_pool_test_inject_task(
    struct AbstractHttpThreadPool *pool) {
  if (!pool) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  {
    struct TaskNode *t = (struct TaskNode *)malloc(sizeof(struct TaskNode));
    if (!t) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    t->cb = dummy_cb_thread;
    t->arg = NULL;
    t->next = pool->head;
    pool->head = t;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Run worker thread routine once for testing.
 * @param pool Target thread pool.
 * @return Result from worker_thread_loop.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_worker(struct AbstractHttpThreadPool *pool);
/**
 * @brief Run worker thread routine once for testing.
 * @param pool Target thread pool.
 * @return Result from worker_thread_loop.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_thread_pool_test_worker(struct AbstractHttpThreadPool *pool) {
  return worker_thread_loop(pool);
}

/**
 * @brief Create a dummy thread pool structure without spawning worker threads.
 * @param out Output pointer receiving allocated dummy thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on NULL/OOM.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_thread_pool_test_create_dummy(
    struct AbstractHttpThreadPool **out);
/**
 * @brief Create a dummy thread pool structure without spawning worker threads.
 * @param out Output pointer receiving allocated dummy thread pool.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on NULL/OOM.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_thread_pool_test_create_dummy(
    struct AbstractHttpThreadPool **out) {
  struct AbstractHttpThreadPool *p;
  c_abstract_http_error_t err;

  if (!out) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  p = (struct AbstractHttpThreadPool *)malloc(sizeof(*p));
  if (!p) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(p, 0, sizeof(*p));
  err = abstract_http_mutex_init(&p->lock);
  if (err != C_ABSTRACT_HTTP_SUCCESS) {
    free(p);
    return err;
  }
  err = abstract_http_cond_init(&p->cond);
  if (err != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t rc = abstract_http_mutex_free(p->lock);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(p);
      return rc;
    }
    free(p);
    return err;
  }
  *out = p;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif
