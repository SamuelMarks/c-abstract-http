/**
 * @file thread_pool.c
 * @brief Implementation of the cross-platform Thread Pool API.
 * @author Samuel Marks
 */

/* clang-format off */
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
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <c_abstract_http/thread_pool.h>
/* clang-format on */

#ifndef ENOTSUP
#define ENOTSUP EINVAL
#endif

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

struct CddMutex {
  CRITICAL_SECTION cs;
};

struct CddCond {
#if defined(_MSC_VER) && _MSC_VER < 1600
  HANDLE semaphore;
  int waiters;
#else
  CONDITION_VARIABLE cv;
#endif
};

int cdd_mutex_init(struct CddMutex **mutex) {
  if (!mutex)
    return EINVAL;
  *mutex = (struct CddMutex *)malloc(sizeof(struct CddMutex));
  if (!*mutex)
    return ENOMEM;
  InitializeCriticalSection(&(*mutex)->cs);
  return 0;
}

int cdd_mutex_lock(struct CddMutex *mutex) {
  if (!mutex)
    return EINVAL;
  EnterCriticalSection(&mutex->cs);
  return 0;
}

int cdd_mutex_unlock(struct CddMutex *mutex) {
  if (!mutex)
    return EINVAL;
  LeaveCriticalSection(&mutex->cs);
  return 0;
}

void cdd_mutex_free(struct CddMutex *mutex) {
  if (mutex) {
    DeleteCriticalSection(&mutex->cs);
    free(mutex);
  }
}

int cdd_cond_init(struct CddCond **cond) {
  if (!cond)
    return EINVAL;
  *cond = (struct CddCond *)malloc(sizeof(struct CddCond));
  if (!*cond)
    return ENOMEM;
#if defined(_MSC_VER) && _MSC_VER < 1600
  (*cond)->semaphore = CreateSemaphore(NULL, 0, MAXLONG, NULL);
  (*cond)->waiters = 0;
#else
  InitializeConditionVariable(&(*cond)->cv);
#endif
  return 0;
}

int cdd_cond_wait(struct CddCond *cond, struct CddMutex *mutex) {
  if (!cond || !mutex)
    return EINVAL;
#if defined(_MSC_VER) && _MSC_VER < 1600
  cond->waiters++;
  LeaveCriticalSection(&mutex->cs);
  WaitForSingleObject(cond->semaphore, INFINITE);
  EnterCriticalSection(&mutex->cs);
#else
  SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
#endif
  return 0;
}

int cdd_cond_signal(struct CddCond *cond) {
  if (!cond)
    return EINVAL;
#if defined(_MSC_VER) && _MSC_VER < 1600
  if (cond->waiters > 0) {
    cond->waiters--;
    ReleaseSemaphore(cond->semaphore, 1, NULL);
  }
#else
  WakeConditionVariable(&cond->cv);
#endif
  return 0;
}

int cdd_cond_broadcast(struct CddCond *cond) {
  if (!cond)
    return EINVAL;
#if defined(_MSC_VER) && _MSC_VER < 1600
  if (cond->waiters > 0) {
    ReleaseSemaphore(cond->semaphore, cond->waiters, NULL);
    cond->waiters = 0;
  }
#else
  WakeAllConditionVariable(&cond->cv);
#endif
  return 0;
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

#else /* POSIX */

struct CddMutex {
  pthread_mutex_t mtx;
};

struct CddCond {
  pthread_cond_t cond;
};

int cdd_mutex_init(struct CddMutex **mutex) {
  if (!mutex)
    return EINVAL;
  *mutex = (struct CddMutex *)malloc(sizeof(struct CddMutex));
  if (!*mutex)
    return ENOMEM;
  if (pthread_mutex_init(&(*mutex)->mtx, NULL) != 0) {
    free(*mutex);
    return EIO;
  }
  return 0;
}

int cdd_mutex_lock(struct CddMutex *mutex) {
  if (!mutex)
    return EINVAL;
  return (pthread_mutex_lock(&mutex->mtx) == 0) ? 0 : EIO;
}

int cdd_mutex_unlock(struct CddMutex *mutex) {
  if (!mutex)
    return EINVAL;
  return (pthread_mutex_unlock(&mutex->mtx) == 0) ? 0 : EIO;
}

void cdd_mutex_free(struct CddMutex *mutex) {
  if (mutex) {
    pthread_mutex_destroy(&mutex->mtx);
    free(mutex);
  }
}

int cdd_cond_init(struct CddCond **cond) {
  if (!cond)
    return EINVAL;
  *cond = (struct CddCond *)malloc(sizeof(struct CddCond));
  if (!*cond)
    return ENOMEM;
  if (pthread_cond_init(&(*cond)->cond, NULL) != 0) {
    free(*cond);
    return EIO;
  }
  return 0;
}

int cdd_cond_wait(struct CddCond *cond, struct CddMutex *mutex) {
  if (!cond || !mutex)
    return EINVAL;
  return (pthread_cond_wait(&cond->cond, &mutex->mtx) == 0) ? 0 : EIO;
}

int cdd_cond_signal(struct CddCond *cond) {
  if (!cond)
    return EINVAL;
  return (pthread_cond_signal(&cond->cond) == 0) ? 0 : EIO;
}

int cdd_cond_broadcast(struct CddCond *cond) {
  if (!cond)
    return EINVAL;
  return (pthread_cond_broadcast(&cond->cond) == 0) ? 0 : EIO;
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

struct TaskNode {
  cdd_thread_task_cb cb;
  void *arg;
  struct TaskNode *next;
};

struct CddThreadPool {
  int is_external;
  struct CddThreadPoolHooks hooks;

  cdd_thread_t *threads;
  size_t num_threads;

  struct TaskNode *head;
  struct TaskNode *tail;

  struct CddMutex *lock;
  struct CddCond *cond;

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
    if (task) {
      pool->head = task->next;
      if (!pool->head) {
        pool->tail = NULL;
      }
    }

    cdd_mutex_unlock(pool->lock);

    if (task) {
      task->cb(task->arg);
      free(task);
    }
  }

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
  return 0;
#else
  return NULL;
#endif
}

int cdd_thread_pool_init(struct CddThreadPool **pool, size_t num_threads) {
  struct CddThreadPool *p;
  size_t i;

  if (!pool || num_threads == 0)
    return EINVAL;

  p = (struct CddThreadPool *)malloc(sizeof(struct CddThreadPool));
  if (!p)
    return ENOMEM;
  memset(p, 0, sizeof(struct CddThreadPool));

  p->num_threads = num_threads;
  p->threads = (cdd_thread_t *)malloc(num_threads * sizeof(cdd_thread_t));
  if (!p->threads) {
    free(p);
    return ENOMEM;
  }

  if (cdd_mutex_init(&p->lock) != 0) {
    free(p->threads);
    free(p);
    return EIO;
  }

  if (cdd_cond_init(&p->cond) != 0) {
    cdd_mutex_free(p->lock);
    free(p->threads);
    free(p);
    return EIO;
  }

  p->stop = 0;

  for (i = 0; i < num_threads; ++i) {
    if (thread_create(&p->threads[i], worker_thread, p) != 0) {
      /* If we fail partway, trigger stop and join what we have */
      p->stop = 1;
      cdd_cond_broadcast(p->cond);
      while (i > 0) {
        i--;
        thread_join(p->threads[i]);
      }
      cdd_cond_free(p->cond);
      cdd_mutex_free(p->lock);
      free(p->threads);
      free(p);
      return EIO;
    }
  }

  *pool = p;
  return 0;
}

int cdd_thread_pool_init_external(struct CddThreadPool **pool,
                                  const struct CddThreadPoolHooks *hooks) {
  struct CddThreadPool *p;

  if (!pool || !hooks)
    return EINVAL;

  p = (struct CddThreadPool *)malloc(sizeof(struct CddThreadPool));
  if (!p)
    return ENOMEM;
  memset(p, 0, sizeof(struct CddThreadPool));

  p->is_external = 1;
  p->hooks = *hooks;

  *pool = p;
  return 0;
}

int cdd_thread_pool_push(struct CddThreadPool *pool, cdd_thread_task_cb cb,
                         void *arg) {
  struct TaskNode *task;

  if (!pool || !cb)
    return EINVAL;

  if (pool->is_external) {
    if (pool->hooks.push) {
      return pool->hooks.push(pool->hooks.external_context, cb, arg);
    }
    return ENOTSUP;
  }

  task = (struct TaskNode *)malloc(sizeof(struct TaskNode));
  if (!task)
    return ENOMEM;

  task->cb = cb;
  task->arg = arg;
  task->next = NULL;

  cdd_mutex_lock(pool->lock);
  if (pool->stop) {
    cdd_mutex_unlock(pool->lock);
    free(task);
    return EINVAL;
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

  return 0;
}

void cdd_thread_pool_free(struct CddThreadPool *pool) {
  size_t i;
  if (!pool)
    return;

  if (pool->is_external) {
    free(pool);
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
}
