
/**
 * @file coroutine.c
 * @brief Stackful coroutine / green-thread implementation across Win32 Fibers,
 * POSIX ucontext, and pthreads.
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
#include <dos.h>
#else
#if defined(EMSCRIPTEN) || defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN__)
/** @brief Disable ucontext backend when unavailable */
#define ABSTRACT_HTTP_NO_UCONTEXT 1
#endif
#if defined(__APPLE__) && defined(__MACH__)
/* ucontext is deprecated on macOS but still mostly works for simple cases on x86_64.
   It is broken on arm64 and will cause a Bus Error. */
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm__) || defined(__aarch64) || defined(EMSCRIPTEN)
/** @brief Disable ucontext backend on Apple Silicon and Emscripten */
#define ABSTRACT_HTTP_NO_UCONTEXT 1
#else
/** @brief Enable X/Open ucontext declarations on x86_64 macOS */
#define _XOPEN_SOURCE 600
#endif
#elif defined(__linux__) && !defined(__GLIBC__)
/** @brief Disable ucontext backend on non-glibc Linux (e.g. musl) */
#define ABSTRACT_HTTP_NO_UCONTEXT 1
#endif
#ifndef ABSTRACT_HTTP_NO_UCONTEXT
#include <ucontext.h>
#endif
#include <pthread.h>
#endif

#include <c_abstract_http/coroutine.h>
#include <c_abstract_http/c_abstract_http_tls.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#ifndef ENOTSUP
/** @brief Fallback ENOTSUP value when unavailable in errno.h */
#define ENOTSUP EINVAL
#endif

/** @brief Optional global coroutine lifecycle override hooks */
static struct AbstractHttpCoroutineHooks g_coroutine_hooks = {NULL, NULL, NULL,
                                                              NULL, NULL};

/** @brief Set custom coroutine hooks */
enum c_abstract_http_error abstract_http_coroutine_set_hooks(
    const struct AbstractHttpCoroutineHooks *hooks) {
  if (hooks) {
    g_coroutine_hooks = *hooks;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct AbstractHttpCoroutine */
struct AbstractHttpCoroutine {
  /** @brief fiber (variable) of struct AbstractHttpCoroutine */
  LPVOID fiber;
  /** @brief caller_fiber (variable) of struct AbstractHttpCoroutine */
  LPVOID caller_fiber;
  /** @brief cb (variable) of struct AbstractHttpCoroutine */
  abstract_http_coroutine_cb cb;
  /** @brief arg (variable) of struct AbstractHttpCoroutine */
  void *arg;
  /** @brief body_rc (variable) of struct AbstractHttpCoroutine */
  enum c_abstract_http_error body_rc;
  /** @brief is_done (variable) of struct AbstractHttpCoroutine */
  int is_done;
};

/** @brief Win32 TLS slot index storing active fiber coroutine pointer */
static DWORD dwTlsIndex = TLS_OUT_OF_INDEXES;

/**
 * @brief Win32 Fiber start trampoline executing the coroutine body.
 * @param lpParameter Pointer to struct AbstractHttpCoroutine.
 */
static void WINAPI fiber_entry(LPVOID lpParameter) {
  struct AbstractHttpCoroutine *co =
      (struct AbstractHttpCoroutine *)lpParameter;
  TlsSetValue(dwTlsIndex, co);

  co->body_rc = co->cb(co->arg);

  co->is_done = 1;
  SwitchToFiber(co->caller_fiber);
}

enum c_abstract_http_error
abstract_http_coroutine_init(struct AbstractHttpCoroutine **co,
                             size_t stack_size, abstract_http_coroutine_cb cb,
                             void *arg) {
  struct AbstractHttpCoroutine *c;

  LOG_DEBUG("abstract_http_coroutine_init: Entering");
  if (g_coroutine_hooks.init) {
    LOG_DEBUG("abstract_http_coroutine_init: Hooking");
    return g_coroutine_hooks.init(co, stack_size, cb, arg);
  }

  if (!co || !cb) {
    LOG_DEBUG("abstract_http_coroutine_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
    dwTlsIndex = TlsAlloc();
    if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
      LOG_DEBUG("abstract_http_coroutine_init: Error EIO (TlsAlloc failed)");
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  c = (struct AbstractHttpCoroutine *)calloc(
      1, sizeof(struct AbstractHttpCoroutine));
  if (!c) {
    LOG_DEBUG("abstract_http_coroutine_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  c->cb = cb;
  c->arg = arg;
  c->body_rc = C_ABSTRACT_HTTP_SUCCESS;
  c->is_done = 0;

  if (stack_size == 0)
    stack_size = 1024 * 1024; /* 1MB default */

  c->fiber = CreateFiber(stack_size, fiber_entry, c);
  if (!c->fiber) {
    LOG_DEBUG("abstract_http_coroutine_init: Error EIO (CreateFiber failed)");
    free(c);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  *co = c;
  LOG_DEBUG("abstract_http_coroutine_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_coroutine_free(struct AbstractHttpCoroutine *co) {
  LOG_DEBUG("abstract_http_coroutine_free: Entering");
  if (g_coroutine_hooks.free) {
    LOG_DEBUG("abstract_http_coroutine_free: Hooking");
    return g_coroutine_hooks.free(co);
  }

  if (co) {
    if (co->fiber) {
      DeleteFiber(co->fiber);
    }
    free(co);
  }
  LOG_DEBUG("abstract_http_coroutine_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_coroutine_resume(struct AbstractHttpCoroutine *co) {
  LPVOID current_fiber;

  LOG_DEBUG("abstract_http_coroutine_resume: Entering");
  if (g_coroutine_hooks.resume) {
    LOG_DEBUG("abstract_http_coroutine_resume: Hooking");
    return g_coroutine_hooks.resume(co);
  }

  if (!co || co->is_done) {
    LOG_DEBUG("abstract_http_coroutine_resume: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  current_fiber = GetCurrentFiber();
  /* If thread is not a fiber yet, convert it */
  if (current_fiber == ((LPVOID)(uintptr_t)0x1e00) || current_fiber == NULL) {
    current_fiber = ConvertThreadToFiber(NULL);
    if (!current_fiber) {
      LOG_DEBUG("abstract_http_coroutine_resume: Error EIO "
                "(ConvertThreadToFiber failed)");
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  co->caller_fiber = current_fiber;
  SwitchToFiber(co->fiber);

  if (co->body_rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("abstract_http_coroutine_resume: Body failed with %d",
              (int)co->body_rc);
    return co->body_rc;
  }

  LOG_DEBUG("abstract_http_coroutine_resume: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Yield execution from current coroutine */
enum c_abstract_http_error abstract_http_coroutine_yield(void) {
  struct AbstractHttpCoroutine *co;

  LOG_DEBUG("abstract_http_coroutine_yield: Entering");
  if (g_coroutine_hooks.yield) {
    LOG_DEBUG("abstract_http_coroutine_yield: Hooking");
    return g_coroutine_hooks.yield();
  }

  if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
    LOG_DEBUG(
        "abstract_http_coroutine_yield: Error EINVAL (TLS uninitialized)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  co = (struct AbstractHttpCoroutine *)TlsGetValue(dwTlsIndex);
  if (!co || !co->caller_fiber) {
    LOG_DEBUG("abstract_http_coroutine_yield: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  SwitchToFiber(co->caller_fiber);
  LOG_DEBUG("abstract_http_coroutine_yield: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error
abstract_http_coroutine_is_done(const struct AbstractHttpCoroutine *co,
                                int *out_is_done) {
  if (g_coroutine_hooks.is_done) {
    return g_coroutine_hooks.is_done(co, out_is_done);
  }
  if (!co || !out_is_done)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  *out_is_done = co->is_done;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Resets fallback initialization state for testing.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_coroutine_reset_fallback(void);

enum c_abstract_http_error c_abstract_http_test_coroutine_reset_fallback(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

#elif !defined(ABSTRACT_HTTP_NO_UCONTEXT) /* POSIX ucontext */

/** @brief Internal struct AbstractHttpCoroutine */
struct AbstractHttpCoroutine {
  /** @brief ctx (variable) of struct AbstractHttpCoroutine */
  ucontext_t ctx;
  /** @brief caller_ctx (variable) of struct AbstractHttpCoroutine */
  ucontext_t caller_ctx;
  /** @brief stack (variable) of struct AbstractHttpCoroutine */
  void *stack;
  /** @brief stack_size (variable) of struct AbstractHttpCoroutine */
  size_t stack_size;
  /** @brief cb (variable) of struct AbstractHttpCoroutine */
  abstract_http_coroutine_cb cb;
  /** @brief arg (variable) of struct AbstractHttpCoroutine */
  void *arg;
  /** @brief body_rc (variable) of struct AbstractHttpCoroutine */
  enum c_abstract_http_error body_rc;
  /** @brief is_done (variable) of struct AbstractHttpCoroutine */
  int is_done;
};

/* We use a simple global thread-local pointer to track the current active
 coroutine for yield. Using our own c_abstract_http_tls.h or native __thread
 depending on compiler. For C89 strictness, we use pthread_key. */
/** @brief Thread-specific key tracking active ucontext coroutine */
static pthread_key_t co_tls_key;
/** @brief Non-zero once co_tls_key has been initialized */
static int co_tls_initialized = 0;

/**
 * @brief Lazily initialize the ucontext thread-local key.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_SYSCALL.
 */
static enum c_abstract_http_error init_tls_key(void) {
  int rc;
  if (!co_tls_initialized) {
    rc = pthread_key_create(&co_tls_key, NULL);
    if (rc != 0) {
      return C_ABSTRACT_HTTP_ERR_SYSCALL;
    }
    co_tls_initialized = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief makecontext entry trampoline executing the active ucontext coroutine
 */
static void ucontext_entry(void) {
  struct AbstractHttpCoroutine *co =
      (struct AbstractHttpCoroutine *)pthread_getspecific(co_tls_key);
  co->body_rc = co->cb(co->arg);
  co->is_done = 1;
}

enum c_abstract_http_error
abstract_http_coroutine_init(struct AbstractHttpCoroutine **co,
                             size_t stack_size, abstract_http_coroutine_cb cb,
                             void *arg) {
  struct AbstractHttpCoroutine *c;

  LOG_DEBUG("abstract_http_coroutine_init: Entering");
  if (g_coroutine_hooks.init) {
    LOG_DEBUG("abstract_http_coroutine_init: Hooking");
    return g_coroutine_hooks.init(co, stack_size, cb, arg);
  }

  if (!co || !cb) {
    LOG_DEBUG("abstract_http_coroutine_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  {
    enum c_abstract_http_error tls_rc = init_tls_key();
    if (tls_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("abstract_http_coroutine_init: Error init_tls_key failed");
      return tls_rc;
    }
  }

  c = (struct AbstractHttpCoroutine *)calloc(
      1, sizeof(struct AbstractHttpCoroutine));
  if (!c) {
    LOG_DEBUG("abstract_http_coroutine_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  c->cb = cb;
  c->arg = arg;
  c->body_rc = C_ABSTRACT_HTTP_SUCCESS;
  c->is_done = 0;

  if (stack_size == 0)
    stack_size = 1024 * 1024; /* 1MB default */

  c->stack_size = stack_size;
  c->stack = malloc(stack_size);
  if (!c->stack) {
    LOG_DEBUG("abstract_http_coroutine_init: Error ENOMEM allocating stack");
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  getcontext(&c->ctx);

  c->ctx.uc_stack.ss_sp = c->stack;
  c->ctx.uc_stack.ss_size = c->stack_size;
  c->ctx.uc_link = &c->caller_ctx;

  /* makecontext expects integer arguments, passing pointers requires some care
   * on 64-bit systems but standard ucontext usage often
   * passes nothing and relies on globals/TLS,
   * which we do. */
  makecontext(&c->ctx, (void (*)(void))ucontext_entry, 0);

  *co = c;
  LOG_DEBUG("abstract_http_coroutine_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_coroutine_free(struct AbstractHttpCoroutine *co) {
  LOG_DEBUG("abstract_http_coroutine_free: Entering");
  if (g_coroutine_hooks.free) {
    LOG_DEBUG("abstract_http_coroutine_free: Hooking");
    return g_coroutine_hooks.free(co);
  }

  if (co) {
    free(co->stack);
    free(co);
  }
  LOG_DEBUG("abstract_http_coroutine_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_coroutine_resume(struct AbstractHttpCoroutine *co) {
  LOG_DEBUG("abstract_http_coroutine_resume: Entering");
  if (g_coroutine_hooks.resume) {
    LOG_DEBUG("abstract_http_coroutine_resume: Hooking");
    return g_coroutine_hooks.resume(co);
  }

  if (!co || co->is_done) {
    LOG_DEBUG("abstract_http_coroutine_resume: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  {
    enum c_abstract_http_error tls_rc = init_tls_key();
    if (tls_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("abstract_http_coroutine_resume: Error init_tls_key failed");
      return tls_rc;
    }
  }
  pthread_setspecific(co_tls_key, co);

  swapcontext(&co->caller_ctx, &co->ctx);
  if (co->body_rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("abstract_http_coroutine_resume: Body failed with %d",
              (int)co->body_rc);
    return co->body_rc;
  }
  LOG_DEBUG("abstract_http_coroutine_resume: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Yield execution from current coroutine */
enum c_abstract_http_error abstract_http_coroutine_yield(void) {
  struct AbstractHttpCoroutine *co;

  LOG_DEBUG("abstract_http_coroutine_yield: Entering");
  if (g_coroutine_hooks.yield) {
    LOG_DEBUG("abstract_http_coroutine_yield: Hooking");
    return g_coroutine_hooks.yield();
  }

  {
    enum c_abstract_http_error tls_rc = init_tls_key();
    if (tls_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("abstract_http_coroutine_yield: Error init_tls_key failed");
      return tls_rc;
    }
  }
  co = (struct AbstractHttpCoroutine *)pthread_getspecific(co_tls_key);

  if (!co) {
    LOG_DEBUG(
        "abstract_http_coroutine_yield: Error EINVAL (no coroutine in TLS)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  swapcontext(&co->ctx, &co->caller_ctx);
  LOG_DEBUG("abstract_http_coroutine_yield: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error
abstract_http_coroutine_is_done(const struct AbstractHttpCoroutine *co,
                                int *out_is_done) {
  if (g_coroutine_hooks.is_done) {
    return g_coroutine_hooks.is_done(co, out_is_done);
  }
  if (!co || !out_is_done)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  *out_is_done = co->is_done;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Resets fallback initialization state for testing.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_coroutine_reset_fallback(void);

enum c_abstract_http_error c_abstract_http_test_coroutine_reset_fallback(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif
#else

/** @brief Internal struct AbstractHttpCoroutine */
struct AbstractHttpCoroutine {
  /** @brief thread (variable) of struct AbstractHttpCoroutine */
  pthread_t thread;
  /** @brief mutex (variable) of struct AbstractHttpCoroutine */
  pthread_mutex_t mutex;
  /** @brief cond_resume (variable) of struct AbstractHttpCoroutine */
  pthread_cond_t cond_resume;
  /** @brief cond_yield (variable) of struct AbstractHttpCoroutine */
  pthread_cond_t cond_yield;
  /** @brief cb (variable) of struct AbstractHttpCoroutine */
  abstract_http_coroutine_cb cb;
  /** @brief arg (variable) of struct AbstractHttpCoroutine */
  void *arg;
  /** @brief body_rc (variable) of struct AbstractHttpCoroutine */
  enum c_abstract_http_error body_rc;
  /** @brief is_done (variable) of struct AbstractHttpCoroutine */
  int is_done;
  /** @brief should_run (variable) of struct AbstractHttpCoroutine */
  int should_run;
  /** @brief is_started (variable) of struct AbstractHttpCoroutine */
  int is_started;
  /** @brief is_exiting (variable) of struct AbstractHttpCoroutine */
  int is_exiting;
  /** @brief is_ready (variable) of struct AbstractHttpCoroutine */
  int is_ready;
};

/** @brief Thread-specific key tracking active pthread-fallback coroutine */
static pthread_key_t co_fallback_key;
/** @brief Non-zero once co_fallback_key has been initialized */
static int co_fallback_initialized = 0;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Resets fallback initialization state for testing.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_coroutine_reset_fallback(void);

enum c_abstract_http_error c_abstract_http_test_coroutine_reset_fallback(void) {
  co_fallback_initialized = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Lazily initialize the pthread-fallback thread-local key.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error init_fallback_key(void) {
  if (!co_fallback_initialized) {
    if (pthread_key_create(&co_fallback_key, NULL) != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    co_fallback_initialized = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Execute coroutine callback and percolate its error enum.
 * @param co Active coroutine instance.
 * @return Callback result enum.
 */
static enum c_abstract_http_error
co_thread_run_body(struct AbstractHttpCoroutine *co) {
  enum c_abstract_http_error cb_rc = co->cb(co->arg);
  if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return cb_rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief POSIX thread entry routine emulating symmetric coroutine resume/yield.
 * @param arg Pointer to struct AbstractHttpCoroutine.
 * @return NULL upon completion.
 */
static void *co_thread_func(void *arg) {
  struct AbstractHttpCoroutine *co = (struct AbstractHttpCoroutine *)arg;
  pthread_setspecific(co_fallback_key, co);

  pthread_mutex_lock(&co->mutex);
  co->is_ready = 1;
  pthread_cond_signal(&co->cond_yield);
  while (!co->should_run) {
    pthread_cond_wait(&co->cond_resume, &co->mutex);
  }
  pthread_mutex_unlock(&co->mutex);

  {
    enum c_abstract_http_error b_rc = co_thread_run_body(co);
    if (b_rc != C_ABSTRACT_HTTP_SUCCESS) {
      pthread_mutex_lock(&co->mutex);
      co->body_rc = b_rc;
      co->is_done = 1;
      co->should_run = 0;
      pthread_cond_signal(&co->cond_yield);
      pthread_mutex_unlock(&co->mutex);
      return (void *)((size_t)(int)b_rc * 0);
    }
    pthread_mutex_lock(&co->mutex);
    co->is_done = 1;
    co->should_run = 0;
    pthread_cond_signal(&co->cond_yield);
    pthread_mutex_unlock(&co->mutex);
  }

  return NULL;
}

enum c_abstract_http_error
abstract_http_coroutine_init(struct AbstractHttpCoroutine **co,
                             size_t stack_size, abstract_http_coroutine_cb cb,
                             void *arg) {
  struct AbstractHttpCoroutine *c;
  enum c_abstract_http_error k_rc;

  (void)stack_size;
  k_rc = init_fallback_key();
  if (k_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return k_rc;
  }

  if (g_coroutine_hooks.init) {
    return g_coroutine_hooks.init(co, stack_size, cb, arg);
  }

  if (!co || !cb) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct AbstractHttpCoroutine *)malloc(sizeof(*c));
  if (!c) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  c->cb = cb;
  c->arg = arg;
  c->body_rc = C_ABSTRACT_HTTP_SUCCESS;
  c->is_done = 0;
  c->should_run = 0;
  c->is_started = 0;
  c->is_exiting = 0;
  c->is_ready = 0;

  pthread_mutex_init(&c->mutex, NULL);
  pthread_cond_init(&c->cond_resume, NULL);
  pthread_cond_init(&c->cond_yield, NULL);

  *co = c;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_coroutine_free(struct AbstractHttpCoroutine *co) {
  LOG_DEBUG("abstract_http_coroutine_free (fallback): Entering");
  if (g_coroutine_hooks.free) {
    LOG_DEBUG("abstract_http_coroutine_free (fallback): Hooking");
    return g_coroutine_hooks.free(co);
  }
  if (!co)
    return C_ABSTRACT_HTTP_SUCCESS;

  if (co->is_started && !co->is_done) {
    pthread_mutex_lock(&co->mutex);
    co->should_run = 1;
    co->is_exiting = 1;
    pthread_cond_signal(&co->cond_resume);
    pthread_mutex_unlock(&co->mutex);
    pthread_join(co->thread, NULL);
  } else if (co->is_started) {
    pthread_join(co->thread, NULL);
  }

  pthread_mutex_destroy(&co->mutex);
  pthread_cond_destroy(&co->cond_resume);
  pthread_cond_destroy(&co->cond_yield);
  free(co);
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection flag for pthread_create failure tests */
extern int g_mock_pthread_fail;
/**
 * @brief Fault-injectable wrapper around pthread_create.
 * @param a Thread handle pointer.
 * @param b Thread attributes pointer.
 * @param c Thread start routine.
 * @param d Thread argument pointer.
 * @return 0 on success or non-zero on failure.
 */
#define PTHREAD_CREATE_MOCK(a, b, c, d)                                        \
  (g_mock_pthread_fail ? 1 : pthread_create(a, b, c, d))
#else
/** @brief Direct alias to pthread_create in production builds */
#define PTHREAD_CREATE_MOCK pthread_create
#endif

enum c_abstract_http_error
abstract_http_coroutine_resume(struct AbstractHttpCoroutine *co) {
  enum c_abstract_http_error body_rc;
  LOG_DEBUG("abstract_http_coroutine_resume (fallback): Entering");
  if (g_coroutine_hooks.resume) {
    LOG_DEBUG("abstract_http_coroutine_resume (fallback): Hooking");
    return g_coroutine_hooks.resume(co);
  }
  if (!co)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (co->is_done)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  pthread_mutex_lock(&co->mutex);
  if (!co->is_started) {
    if (PTHREAD_CREATE_MOCK(&co->thread, NULL, co_thread_func, co) != 0) {
      pthread_mutex_unlock(&co->mutex);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    co->is_started = 1;
    while (!co->is_ready) {
      pthread_cond_wait(&co->cond_yield, &co->mutex);
    }
  }

  co->should_run = 1;
  pthread_cond_signal(&co->cond_resume);

  while (co->should_run && !co->is_done) {
    pthread_cond_wait(&co->cond_yield, &co->mutex);
  }
  body_rc = co->body_rc;
  pthread_mutex_unlock(&co->mutex);
  if (body_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return body_rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Yield execution from current coroutine */
enum c_abstract_http_error abstract_http_coroutine_yield(void) {
  struct AbstractHttpCoroutine *co;
  enum c_abstract_http_error k_rc;
  LOG_DEBUG("abstract_http_coroutine_yield (fallback): Entering");
  if (g_coroutine_hooks.yield) {
    LOG_DEBUG("abstract_http_coroutine_yield (fallback): Hooking");
    return g_coroutine_hooks.yield();
  }

  k_rc = init_fallback_key();
  if (k_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return k_rc;
  }
  co = (struct AbstractHttpCoroutine *)pthread_getspecific(co_fallback_key);
  if (!co)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  pthread_mutex_lock(&co->mutex);
  co->should_run = 0;
  pthread_cond_signal(&co->cond_yield);

  while (!co->should_run && !co->is_exiting) {
    pthread_cond_wait(&co->cond_resume, &co->mutex);
  }
  if (co->is_exiting) {
    pthread_mutex_unlock(&co->mutex);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&co->mutex);
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error
abstract_http_coroutine_is_done(const struct AbstractHttpCoroutine *co,
                                int *out_is_done) {
  if (g_coroutine_hooks.is_done) {
    return g_coroutine_hooks.is_done(co, out_is_done);
  }
  if (!co || !out_is_done)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  *out_is_done = co->is_done;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif
