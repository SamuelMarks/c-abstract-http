
/* clang-format off */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No threading support on DOS */
#else
#include <pthread.h>
#endif
#define ABSTRACT_HTTP_COND_BROADCAST abstract_http_cond_broadcast

#include <c_abstract_http/thread_pool.h>
extern void *c_abstract_http_mock_malloc(size_t size);
#include "c_abstract_http/log.h"
/* clang-format on */

#ifndef ENOTSUP
#define ENOTSUP EINVAL
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct AbstractHttpMutex */
struct AbstractHttpMutex {
  CRITICAL_SECTION cs;
};

/** @brief Internal struct AbstractHttpCond */
struct AbstractHttpCond {
#if defined(_MSC_VER) && _MSC_VER < 1600
  HANDLE semaphore;
  int waiters;
#else
  CONDITION_VARIABLE cv;
#endif
};

enum c_abstract_http_error
abstract_http_mutex_init(struct AbstractHttpMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct AbstractHttpMutex *)malloc(sizeof(struct AbstractHttpMutex));
  if (!*mutex)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  InitializeCriticalSection(&(*mutex)->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  EnterCriticalSection(&mutex->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  LeaveCriticalSection(&mutex->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

void abstract_http_mutex_free(struct AbstractHttpMutex *mutex) {
  if (mutex) {
    DeleteCriticalSection(&mutex->cs);
    free(mutex);
  }
}

enum c_abstract_http_error
abstract_http_cond_init(struct AbstractHttpCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct AbstractHttpCond *)malloc(sizeof(struct AbstractHttpCond));
  if (!*cond)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
#if defined(_MSC_VER) && _MSC_VER < 1600
  (*cond)->semaphore = CreateSemaphore(NULL, 0, MAXLONG, NULL);
  (*cond)->waiters = 0;
#else
  InitializeConditionVariable(&(*cond)->cv);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_cond_wait(struct AbstractHttpCond *cond,
                        struct AbstractHttpMutex *mutex) {
  if (!cond || !mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(_MSC_VER) && _MSC_VER < 1600
  cond->waiters++;
  LeaveCriticalSection(&mutex->cs);
  WaitForSingleObject(cond->semaphore, INFINITE);
  EnterCriticalSection(&mutex->cs);
#else
  SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_cond_signal(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(_MSC_VER) && _MSC_VER < 1600
  if (cond->waiters > 0) {
    cond->waiters--;
    ReleaseSemaphore(cond->semaphore, 1, NULL);
  }
#else
  WakeConditionVariable(&cond->cv);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
ABSTRACT_HTTP_COND_BROADCAST(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(_MSC_VER) && _MSC_VER < 1600
  if (cond->waiters > 0) {
    ReleaseSemaphore(cond->semaphore, cond->waiters, NULL);
    cond->waiters = 0;
  }
#else
  WakeAllConditionVariable(&cond->cv);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

void abstract_http_cond_free(struct AbstractHttpCond *cond) {
  if (cond) {
#if defined(_MSC_VER) && _MSC_VER < 1600
    if (cond->semaphore) {
      CloseHandle(cond->semaphore);
    }
#endif
    free(cond);
  }
}

typedef HANDLE abstract_http_thread_t;
#define ABSTRACT_HTTP_THREAD_FUNC DWORD
typedef LPVOID abstract_http_thread_arg_t;

static int thread_create(abstract_http_thread_t *thread,
                         ABSTRACT_HTTP_THREAD_FUNC(WINAPI *start_routine)(
                             abstract_http_thread_arg_t),
                         abstract_http_thread_arg_t arg) {
  *thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
  return (*thread == NULL) ? EIO : 0;
}

static void thread_join(abstract_http_thread_t thread) {
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
}

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)

/** @brief Internal struct AbstractHttpMutex */
struct AbstractHttpMutex {
  int dummy;
};
/** @brief Internal struct AbstractHttpCond */
struct AbstractHttpCond {
  int dummy;
};

enum c_abstract_http_error
abstract_http_mutex_init(struct AbstractHttpMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct AbstractHttpMutex *)malloc(sizeof(struct AbstractHttpMutex));
  return *mutex ? 0 : ENOMEM;
}
enum c_abstract_http_error
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex) {
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex) {
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
void abstract_http_mutex_free(struct AbstractHttpMutex *mutex) { free(mutex); }

enum c_abstract_http_error
abstract_http_cond_init(struct AbstractHttpCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct AbstractHttpCond *)malloc(sizeof(struct AbstractHttpCond));
  return *cond ? 0 : ENOMEM;
}
enum c_abstract_http_error
abstract_http_cond_wait(struct AbstractHttpCond *cond,
                        struct AbstractHttpMutex *mutex) {
  (void)cond;
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error
abstract_http_cond_signal(struct AbstractHttpCond *cond) {
  (void)cond;
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error
ABSTRACT_HTTP_COND_BROADCAST(struct AbstractHttpCond *cond) {
  (void)cond;
  return C_ABSTRACT_HTTP_SUCCESS;
}
void abstract_http_cond_free(struct AbstractHttpCond *cond) { free(cond); }

typedef int abstract_http_thread_t;
#define ABSTRACT_HTTP_THREAD_FUNC void *
typedef void *abstract_http_thread_arg_t;

static int thread_create(
    abstract_http_thread_t *thread,
    ABSTRACT_HTTP_THREAD_FUNC (*start_routine)(abstract_http_thread_arg_t),
    abstract_http_thread_arg_t arg) {
  (void)thread;
  (void)start_routine;
  (void)arg;
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
}
static void thread_join(abstract_http_thread_t thread) { (void)thread; }

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

enum c_abstract_http_error
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

enum c_abstract_http_error
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_mutex_lock(&mutex->mtx);
}

enum c_abstract_http_error
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_mutex_unlock(&mutex->mtx);
}

void abstract_http_mutex_free(struct AbstractHttpMutex *mutex) {
  if (mutex) {
    pthread_mutex_destroy(&mutex->mtx);
    free(mutex);
  }
}

enum c_abstract_http_error
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

enum c_abstract_http_error
abstract_http_cond_wait(struct AbstractHttpCond *cond,
                        struct AbstractHttpMutex *mutex) {
  if (!cond || !mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_cond_wait(&cond->cond, &mutex->mtx);
}

enum c_abstract_http_error
abstract_http_cond_signal(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_cond_signal(&cond->cond);
}

enum c_abstract_http_error
ABSTRACT_HTTP_COND_BROADCAST(struct AbstractHttpCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_cond_broadcast(&cond->cond);
}

void abstract_http_cond_free(struct AbstractHttpCond *cond) {
  if (cond) {
    pthread_cond_destroy(&cond->cond);
    free(cond);
  }
}

typedef pthread_t abstract_http_thread_t;
#define ABSTRACT_HTTP_THREAD_FUNC void *
typedef void *abstract_http_thread_arg_t;

static int thread_create(
    abstract_http_thread_t *thread,
    ABSTRACT_HTTP_THREAD_FUNC (*start_routine)(abstract_http_thread_arg_t),
    abstract_http_thread_arg_t arg) {
  return (pthread_create(thread, NULL, start_routine, arg) == 0) ? 0 : EIO;
}

static void thread_join(abstract_http_thread_t thread) {
  pthread_join(thread, NULL);
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

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
static ABSTRACT_HTTP_THREAD_FUNC WINAPI
worker_thread(abstract_http_thread_arg_t arg) {
#else
static ABSTRACT_HTTP_THREAD_FUNC worker_thread(abstract_http_thread_arg_t arg) {
#endif
  struct AbstractHttpThreadPool *pool = (struct AbstractHttpThreadPool *)arg;

  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  while (1) {
    struct TaskNode *task = NULL;
    enum c_abstract_http_error err;

    err = abstract_http_mutex_lock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
      rc = err;                           /* LCOV_EXCL_LINE */
      break;                              /* LCOV_EXCL_LINE */
    }

    while (!pool->stop && !pool->head) {
      err = abstract_http_cond_wait(pool->cond, pool->lock);
      if (err != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
        rc = err;                           /* LCOV_EXCL_LINE */
        break;                              /* LCOV_EXCL_LINE */
      }
    }

    if (rc != C_ABSTRACT_HTTP_SUCCESS) {            /* LCOV_EXCL_BR_LINE */
      err = abstract_http_mutex_unlock(pool->lock); /* LCOV_EXCL_LINE */
      (void)err;                                    /* LCOV_EXCL_LINE */
      break;                                        /* LCOV_EXCL_LINE */
    }

    if (pool->stop && !pool->head) {
      err = abstract_http_mutex_unlock(pool->lock);
      (void)err;
      break;
    }

    task = pool->head;
    pool->head = task->next;
    if (!pool->head) {
      pool->tail = NULL;
    }

    err = abstract_http_mutex_unlock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
      rc = err;                           /* LCOV_EXCL_LINE */
      /* We should probably execute the task anyway since we popped it */
    }

    if (task) {
      task->cb(task->arg);
      free(task);
    }
  }
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  return (ABSTRACT_HTTP_THREAD_FUNC)(unsigned long)rc;
#else
  return (ABSTRACT_HTTP_THREAD_FUNC)(unsigned long)rc;
#endif
}

enum c_abstract_http_error
abstract_http_thread_pool_init(struct AbstractHttpThreadPool **pool,
                               size_t num_threads) {
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
    enum c_abstract_http_error err;
    err = abstract_http_mutex_init(&p->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      free(p->threads);
      free(p);
      return err;
    }

    err = abstract_http_cond_init(&p->cond);
    if (err != C_ABSTRACT_HTTP_SUCCESS) {
      abstract_http_mutex_free(p->lock);
      free(p->threads);
      free(p);
      return err;
    }
  }

  p->stop = 0;

  for (i = 0; i < num_threads; ++i) {
    if (thread_create(&p->threads[i], worker_thread, p) != 0) {
      enum c_abstract_http_error berr;
      /* If we fail partway, trigger stop and join what we have */
      p->stop = 1;
      berr = ABSTRACT_HTTP_COND_BROADCAST(p->cond);
      (void)berr;
      while (i > 0) {
        printf("JOINING THREAD %lu\n", (unsigned long)i);
        i--;
        thread_join(p->threads[i]);
      }
      abstract_http_cond_free(p->cond);
      abstract_http_mutex_free(p->lock);
      free(p->threads);
      free(p);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  *pool = p;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error abstract_http_thread_pool_init_external(
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

enum c_abstract_http_error
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
    enum c_abstract_http_error err = abstract_http_mutex_lock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
      free(task);                         /* LCOV_EXCL_LINE */
      return err;                         /* LCOV_EXCL_LINE */
    }
    if (pool->stop) {
      err = abstract_http_mutex_unlock(pool->lock);
      (void)err;
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
    (void)err;
    err = abstract_http_mutex_unlock(pool->lock);
    (void)err;
  }

  LOG_DEBUG("abstract_http_thread_pool_push: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_thread_pool_free(struct AbstractHttpThreadPool *pool) {
  size_t i;
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
    enum c_abstract_http_error err;
    err = abstract_http_mutex_lock(pool->lock);
    if (err != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
      return err;                         /* LCOV_EXCL_LINE */
    }
    pool->stop = 1;
    err = ABSTRACT_HTTP_COND_BROADCAST(pool->cond);
    (void)err;
    err = abstract_http_mutex_unlock(pool->lock);
    (void)err;
  }

  for (i = 0; i < pool->num_threads; ++i) {
    thread_join(pool->threads[i]);
  }

  /* Free any remaining tasks in queue */
  while (pool->head) {
    struct TaskNode *next = pool->head->next;
    free(pool->head);
    pool->head = next;
  }

  abstract_http_cond_free(pool->cond);
  abstract_http_mutex_free(pool->lock);
  free(pool->threads);
  free(pool);
  LOG_DEBUG("abstract_http_thread_pool_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if 1
enum c_abstract_http_error
abstract_http_thread_pool_test_set_stop(struct AbstractHttpThreadPool *pool);
extern int g_mock_pthread_fail;
enum c_abstract_http_error
abstract_http_thread_pool_test_set_stop(struct AbstractHttpThreadPool *pool) {
  if (pool) {
    enum c_abstract_http_error err;
    if (g_mock_pthread_fail == 3) {
      err = C_ABSTRACT_HTTP_ERR_INVAL;
    } else {
      err = abstract_http_mutex_lock(pool->lock);
    }
    if (err != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
      return err;                         /* LCOV_EXCL_LINE */
    }
    pool->stop = 1;
    err = abstract_http_mutex_unlock(pool->lock);
    (void)err;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
void dummy_cb_thread(void *arg);
void dummy_cb_thread(void *arg) { (void)arg; }
#else
extern void dummy_cb_thread(void *arg);
#endif
void abstract_http_thread_pool_test_inject_task(
    struct AbstractHttpThreadPool *pool);
void abstract_http_thread_pool_test_inject_task(
    struct AbstractHttpThreadPool *pool) {
  if (pool) {
    struct TaskNode *t =
        (struct TaskNode *)c_abstract_http_mock_malloc(sizeof(struct TaskNode));
    if (t) {
      t->cb = dummy_cb_thread;
      t->arg = NULL;
      t->next = pool->head;
      pool->head = t;
    }
  }
}
#endif

#if 1
void abstract_http_thread_pool_test_free_with_tasks(void);
void abstract_http_thread_pool_test_free_with_tasks(void) {
  struct AbstractHttpThreadPool *fake_pool =
      (struct AbstractHttpThreadPool *)c_abstract_http_mock_malloc(
          sizeof(struct AbstractHttpThreadPool));
  if (fake_pool) {
    enum c_abstract_http_error err;
    memset(fake_pool, 0, sizeof(struct AbstractHttpThreadPool));
    fake_pool->num_threads = 0;
    err = abstract_http_mutex_init(&fake_pool->lock);
    (void)err;
    err = abstract_http_cond_init(&fake_pool->cond);
    (void)err;
    abstract_http_thread_pool_test_inject_task(fake_pool);
    err = abstract_http_thread_pool_free(fake_pool);
    (void)err;
  }
}
#endif
