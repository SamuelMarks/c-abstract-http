
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

#include <c_abstract_http/thread_pool.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#ifndef ENOTSUP
#define ENOTSUP EINVAL
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct CddMutex */
struct CddMutex {
  CRITICAL_SECTION cs;
};

/** @brief Internal struct CddCond */
struct CddCond {
#if defined(_MSC_VER) && _MSC_VER < 1600
  HANDLE semaphore;
  int waiters;
#else
  CONDITION_VARIABLE cv;
#endif
};

enum c_abstract_http_error cdd_mutex_init(struct CddMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct CddMutex *)malloc(sizeof(struct CddMutex));
  if (!*mutex)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  InitializeCriticalSection(&(*mutex)->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_mutex_lock(struct CddMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  EnterCriticalSection(&mutex->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_mutex_unlock(struct CddMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  LeaveCriticalSection(&mutex->cs);
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_mutex_free(struct CddMutex *mutex) {
  if (mutex) {
    DeleteCriticalSection(&mutex->cs);
    free(mutex);
  }
}

enum c_abstract_http_error cdd_cond_init(struct CddCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct CddCond *)malloc(sizeof(struct CddCond));
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

enum c_abstract_http_error cdd_cond_wait(struct CddCond *cond,
                                         struct CddMutex *mutex) {
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

enum c_abstract_http_error cdd_cond_signal(struct CddCond *cond) {
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

enum c_abstract_http_error cdd_cond_broadcast(struct CddCond *cond) {
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

void cdd_cond_free(struct CddCond *cond) {
  if (cond) {
#if defined(_MSC_VER) && _MSC_VER < 1600
    if (cond->semaphore) {
      CloseHandle(cond->semaphore);
    }
#endif
    free(cond);
  }
}

typedef HANDLE cdd_thread_t;
#define CDD_THREAD_FUNC DWORD
typedef LPVOID cdd_thread_arg_t;

static int
thread_create(cdd_thread_t *thread,
              CDD_THREAD_FUNC(WINAPI *start_routine)(cdd_thread_arg_t),
              cdd_thread_arg_t arg) {
  *thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
  return (*thread == NULL) ? EIO : 0;
}

static void thread_join(cdd_thread_t thread) {
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
}

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)

/** @brief Internal struct CddMutex */
struct CddMutex {
  int dummy;
};
/** @brief Internal struct CddCond */
struct CddCond {
  int dummy;
};

enum c_abstract_http_error cdd_mutex_init(struct CddMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct CddMutex *)malloc(sizeof(struct CddMutex));
  return *mutex ? 0 : ENOMEM;
}
enum c_abstract_http_error cdd_mutex_lock(struct CddMutex *mutex) {
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error cdd_mutex_unlock(struct CddMutex *mutex) {
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
void cdd_mutex_free(struct CddMutex *mutex) { free(mutex); }

enum c_abstract_http_error cdd_cond_init(struct CddCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct CddCond *)malloc(sizeof(struct CddCond));
  return *cond ? 0 : ENOMEM;
}
enum c_abstract_http_error cdd_cond_wait(struct CddCond *cond,
                                         struct CddMutex *mutex) {
  (void)cond;
  (void)mutex;
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error cdd_cond_signal(struct CddCond *cond) {
  (void)cond;
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error cdd_cond_broadcast(struct CddCond *cond) {
  (void)cond;
  return C_ABSTRACT_HTTP_SUCCESS;
}
void cdd_cond_free(struct CddCond *cond) { free(cond); }

typedef int cdd_thread_t;
#define CDD_THREAD_FUNC void *
typedef void *cdd_thread_arg_t;

static int thread_create(cdd_thread_t *thread,
                         CDD_THREAD_FUNC (*start_routine)(cdd_thread_arg_t),
                         cdd_thread_arg_t arg) {
  (void)thread;
  (void)start_routine;
  (void)arg;
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
}
static void thread_join(cdd_thread_t thread) { (void)thread; }

#else /* POSIX */

/** @brief Internal struct CddMutex */
struct CddMutex {
  /** @brief mtx (variable) of struct CddMutex */
  pthread_mutex_t mtx;
};

/** @brief Internal struct CddCond */
struct CddCond {
  /** @brief cond (variable) of struct CddCond */
  pthread_cond_t cond;
};

enum c_abstract_http_error cdd_mutex_init(struct CddMutex **mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *mutex = (struct CddMutex *)malloc(sizeof(struct CddMutex));
  if (!*mutex)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  if (pthread_mutex_init(&(*mutex)->mtx, NULL) != 0) {
    free(*mutex);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_mutex_lock(struct CddMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_mutex_lock(&mutex->mtx);
}

enum c_abstract_http_error cdd_mutex_unlock(struct CddMutex *mutex) {
  if (!mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_mutex_unlock(&mutex->mtx);
}

void cdd_mutex_free(struct CddMutex *mutex) {
  if (mutex) {
    pthread_mutex_destroy(&mutex->mtx);
    free(mutex);
  }
}

enum c_abstract_http_error cdd_cond_init(struct CddCond **cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *cond = (struct CddCond *)malloc(sizeof(struct CddCond));
  if (!*cond)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  if (pthread_cond_init(&(*cond)->cond, NULL) != 0) {
    free(*cond);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_cond_wait(struct CddCond *cond,
                                         struct CddMutex *mutex) {
  if (!cond || !mutex)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_cond_wait(&cond->cond, &mutex->mtx);
}

enum c_abstract_http_error cdd_cond_signal(struct CddCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_cond_signal(&cond->cond);
}

enum c_abstract_http_error cdd_cond_broadcast(struct CddCond *cond) {
  if (!cond)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return pthread_cond_broadcast(&cond->cond);
}

void cdd_cond_free(struct CddCond *cond) {
  if (cond) {
    pthread_cond_destroy(&cond->cond);
    free(cond);
  }
}

typedef pthread_t cdd_thread_t;
#define CDD_THREAD_FUNC void *
typedef void *cdd_thread_arg_t;

static int thread_create(cdd_thread_t *thread,
                         CDD_THREAD_FUNC (*start_routine)(cdd_thread_arg_t),
                         cdd_thread_arg_t arg) {
  return (pthread_create(thread, NULL, start_routine, arg) == 0) ? 0 : EIO;
}

static void thread_join(cdd_thread_t thread) { pthread_join(thread, NULL); }

#endif /* POSIX vs WIN32 */

/* --- Thread Pool Implementation --- */

/** @brief Internal struct TaskNode */
struct TaskNode {
  /** @brief cb (variable) of struct TaskNode */
  cdd_thread_task_cb cb;
  /** @brief arg (variable) of struct TaskNode */
  void *arg;
  /** @brief next (variable) of struct TaskNode */
  struct TaskNode *next;
};

/** @brief Internal struct CddThreadPool */
struct CddThreadPool {
  /** @brief is_external (variable) of struct CddThreadPool */
  int is_external;
  /** @brief hooks (variable) of struct CddThreadPool */
  struct CddThreadPoolHooks hooks;
  /** @brief threads (variable) of struct CddThreadPool */
  cdd_thread_t *threads;
  /** @brief num_threads (variable) of struct CddThreadPool */
  size_t num_threads;
  /** @brief head (variable) of struct CddThreadPool */
  struct TaskNode *head;
  /** @brief tail (variable) of struct CddThreadPool */
  struct TaskNode *tail;
  /** @brief lock (variable) of struct CddThreadPool */
  struct CddMutex *lock;
  /** @brief cond (variable) of struct CddThreadPool */
  struct CddCond *cond;
  /** @brief stop (variable) of struct CddThreadPool */
  int stop;
};

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
static CDD_THREAD_FUNC WINAPI worker_thread(cdd_thread_arg_t arg) {
#else
static CDD_THREAD_FUNC worker_thread(cdd_thread_arg_t arg) {
#endif
  struct CddThreadPool *pool = (struct CddThreadPool *)arg;

  while (1) {
    struct TaskNode *task = NULL;

    cdd_mutex_lock(pool->lock);

    while (!pool->stop && !pool->head) {
      cdd_cond_wait(pool->cond, pool->lock);
    }

    if (pool->stop && !pool->head) {
      cdd_mutex_unlock(pool->lock);
      break;
    }

    task = pool->head;
    pool->head = task->next;
    if (!pool->head) {
      pool->tail = NULL;
    }

    cdd_mutex_unlock(pool->lock);

    task->cb(task->arg);
    free(task);
  }

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  return NULL;
#endif
}

enum c_abstract_http_error cdd_thread_pool_init(struct CddThreadPool **pool,
                                                size_t num_threads) {
  struct CddThreadPool *p;
  size_t i;

  if (!pool || num_threads == 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = (struct CddThreadPool *)malloc(sizeof(struct CddThreadPool));
  if (!p)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memset(p, 0, sizeof(struct CddThreadPool));

  p->num_threads = num_threads;
  p->threads = (cdd_thread_t *)malloc(num_threads * sizeof(cdd_thread_t));
  if (!p->threads) {
    free(p);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (cdd_mutex_init(&p->lock) != 0) {
    free(p->threads);
    free(p);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (cdd_cond_init(&p->cond) != 0) {
    cdd_mutex_free(p->lock);
    free(p->threads);
    free(p);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  p->stop = 0;

  for (i = 0; i < num_threads; ++i) {
    if (thread_create(&p->threads[i], worker_thread, p) != 0) {
      /* If we fail partway, trigger stop and join what we have */
      p->stop = 1;
      cdd_cond_broadcast(p->cond);
      while (i > 0) {
        printf("JOINING THREAD %lu\n", (unsigned long)i);
        i--;
        thread_join(p->threads[i]);
      }
      cdd_cond_free(p->cond);
      cdd_mutex_free(p->lock);
      free(p->threads);
      free(p);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  *pool = p;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
cdd_thread_pool_init_external(struct CddThreadPool **pool,
                              const struct CddThreadPoolHooks *hooks) {
  struct CddThreadPool *p;

  if (!pool || !hooks)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = (struct CddThreadPool *)malloc(sizeof(struct CddThreadPool));
  if (!p)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memset(p, 0, sizeof(struct CddThreadPool));

  p->is_external = 1;
  p->hooks = *hooks;

  *pool = p;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_thread_pool_push(struct CddThreadPool *pool,
                                                cdd_thread_task_cb cb,
                                                void *arg) {
  struct TaskNode *task;

  LOG_DEBUG("cdd_thread_pool_push: Entering");
  if (!pool || !cb) {
    LOG_DEBUG("cdd_thread_pool_push: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (pool->is_external) {
    if (pool->hooks.push) {
      LOG_DEBUG("cdd_thread_pool_push: Hooking");
      return pool->hooks.push(pool->hooks.external_context, cb, arg);
    }
    LOG_DEBUG("cdd_thread_pool_push: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  task = (struct TaskNode *)malloc(sizeof(struct TaskNode));
  if (!task) {
    LOG_DEBUG("cdd_thread_pool_push: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  task->cb = cb;
  task->arg = arg;
  task->next = NULL;

  cdd_mutex_lock(pool->lock);
  if (pool->stop) {
    cdd_mutex_unlock(pool->lock);
    free(task);
    LOG_DEBUG("cdd_thread_pool_push: Error EINVAL (pool stopped)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (!pool->head) {
    pool->head = task;
    pool->tail = task;
  } else {
    pool->tail->next = task;
    pool->tail = task;
  }

  cdd_cond_signal(pool->cond);
  cdd_mutex_unlock(pool->lock);

  LOG_DEBUG("cdd_thread_pool_push: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_thread_pool_free(struct CddThreadPool *pool) {
  size_t i;
  LOG_DEBUG("cdd_thread_pool_free: Entering");
  if (!pool) {
    LOG_DEBUG("cdd_thread_pool_free: Exiting early (pool NULL)");
    return;
  }

  if (pool->is_external) {
    free(pool);
    LOG_DEBUG("cdd_thread_pool_free: Exiting (external pool freed)");
    return;
  }

  cdd_mutex_lock(pool->lock);
  pool->stop = 1;
  cdd_cond_broadcast(pool->cond);
  cdd_mutex_unlock(pool->lock);

  for (i = 0; i < pool->num_threads; ++i) {
    thread_join(pool->threads[i]);
  }

  /* Free any remaining tasks in queue */
  while (pool->head) {
    struct TaskNode *next = pool->head->next;
    free(pool->head);
    pool->head = next;
  }

  cdd_cond_free(pool->cond);
  cdd_mutex_free(pool->lock);
  free(pool->threads);
  free(pool);
  LOG_DEBUG("cdd_thread_pool_free: Exiting");
}

#if 1
void cdd_thread_pool_test_set_stop(struct CddThreadPool *pool);
void cdd_thread_pool_test_set_stop(struct CddThreadPool *pool) {
  if (pool) {
    cdd_mutex_lock(pool->lock);
    pool->stop = 1;
    cdd_mutex_unlock(pool->lock);
  }
}
#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
void dummy_cb_thread(void *arg);
void dummy_cb_thread(void *arg) { (void)arg; }
#else
extern void dummy_cb_thread(void *arg);
#endif
void cdd_thread_pool_test_inject_task(struct CddThreadPool *pool);
void cdd_thread_pool_test_inject_task(struct CddThreadPool *pool) {
  if (pool) {
    struct TaskNode *t = (struct TaskNode *)malloc(sizeof(struct TaskNode));
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
void cdd_thread_pool_test_free_with_tasks(void);
void cdd_thread_pool_test_free_with_tasks(void) {
  struct CddThreadPool *fake_pool =
      (struct CddThreadPool *)malloc(sizeof(struct CddThreadPool));
  memset(fake_pool, 0, sizeof(struct CddThreadPool));
  fake_pool->num_threads = 0;
  cdd_thread_pool_test_inject_task(fake_pool);
  cdd_thread_pool_free(fake_pool);
}
#endif
