/**
 * @file coroutine.c
 * @brief Implementation of the minimalistic Greenthread / Coroutine API.
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
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <dos.h>
#else
#if defined(__APPLE__) && defined(__MACH__)
/* ucontext is deprecated on macOS but still mostly works for simple cases on x86_64.
   It is broken on arm64 and will cause a Bus Error. */
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm__) || defined(__aarch64)
#define CDD_NO_UCONTEXT 1
#else
#define _XOPEN_SOURCE 600
#endif
#elif defined(__linux__) && !defined(__GLIBC__)
#define CDD_NO_UCONTEXT 1
#endif
#ifndef CDD_NO_UCONTEXT
#include <ucontext.h>
#endif
#include <pthread.h>
#endif

#include <c_abstract_http/coroutine.h>
#include <c_abstract_http/cdd_tls.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#ifndef ENOTSUP
#define ENOTSUP EINVAL
#endif

static struct CddCoroutineHooks g_coroutine_hooks = {NULL, NULL, NULL, NULL,
                                                     NULL};

void cdd_coroutine_set_hooks(const struct CddCoroutineHooks *hooks) {
  if (hooks) {
    g_coroutine_hooks = *hooks;
  }
}

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

struct CddCoroutine {
  LPVOID fiber;
  LPVOID caller_fiber;
  cdd_coroutine_cb cb;
  void *arg;
  int is_done;
};

static DWORD dwTlsIndex = TLS_OUT_OF_INDEXES;

static VOID WINAPI fiber_entry(LPVOID lpParameter) {
  struct CddCoroutine *co = (struct CddCoroutine *)lpParameter;
  TlsSetValue(dwTlsIndex, co);

  if (co->cb) {
    co->cb(co->arg);
  }

  co->is_done = 1;
  SwitchToFiber(co->caller_fiber);
}

int cdd_coroutine_init(struct CddCoroutine **co, size_t stack_size,
                       cdd_coroutine_cb cb, void *arg) {
  struct CddCoroutine *c;

  LOG_DEBUG("cdd_coroutine_init: Entering");
  if (g_coroutine_hooks.init) {
    LOG_DEBUG("cdd_coroutine_init: Hooking");
    return g_coroutine_hooks.init(co, stack_size, cb, arg);
  }

  if (!co || !cb) {
    LOG_DEBUG("cdd_coroutine_init: Error EINVAL");
    return EINVAL;
  }

  if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
    dwTlsIndex = TlsAlloc();
    if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
      LOG_DEBUG("cdd_coroutine_init: Error EIO (TlsAlloc failed)");
      return EIO;
    }
  }

  c = (struct CddCoroutine *)calloc(1, sizeof(struct CddCoroutine));
  if (!c) {
    LOG_DEBUG("cdd_coroutine_init: Error ENOMEM");
    return ENOMEM;
  }

  c->cb = cb;
  c->arg = arg;
  c->is_done = 0;

  if (stack_size == 0)
    stack_size = 1024 * 1024; /* 1MB default */

  c->fiber = CreateFiber(stack_size, fiber_entry, c);
  if (!c->fiber) {
    LOG_DEBUG("cdd_coroutine_init: Error EIO (CreateFiber failed)");
    free(c);
    return EIO;
  }

  *co = c;
  LOG_DEBUG("cdd_coroutine_init: Success");
  return 0;
}

void cdd_coroutine_free(struct CddCoroutine *co) {
  LOG_DEBUG("cdd_coroutine_free: Entering");
  if (g_coroutine_hooks.free) {
    LOG_DEBUG("cdd_coroutine_free: Hooking");
    g_coroutine_hooks.free(co);
    return;
  }

  if (co) {
    if (co->fiber) {
      DeleteFiber(co->fiber);
    }
    free(co);
  }
  LOG_DEBUG("cdd_coroutine_free: Exiting");
}

int cdd_coroutine_resume(struct CddCoroutine *co) {
  LPVOID current_fiber;

  LOG_DEBUG("cdd_coroutine_resume: Entering");
  if (g_coroutine_hooks.resume) {
    LOG_DEBUG("cdd_coroutine_resume: Hooking");
    return g_coroutine_hooks.resume(co);
  }

  if (!co || co->is_done) {
    LOG_DEBUG("cdd_coroutine_resume: Error EINVAL");
    return EINVAL;
  }

  current_fiber = GetCurrentFiber();
  /* If thread is not a fiber yet, convert it */
  if (current_fiber == (LPVOID)0x1e00 || current_fiber == NULL) {
    current_fiber = ConvertThreadToFiber(NULL);
    if (!current_fiber) {
      LOG_DEBUG(
          "cdd_coroutine_resume: Error EIO (ConvertThreadToFiber failed)");
      return EIO;
    }
  }

  co->caller_fiber = current_fiber;
  SwitchToFiber(co->fiber);

  LOG_DEBUG("cdd_coroutine_resume: Success");
  return 0;
}

int cdd_coroutine_yield(void) {
  struct CddCoroutine *co;

  LOG_DEBUG("cdd_coroutine_yield: Entering");
  if (g_coroutine_hooks.yield) {
    LOG_DEBUG("cdd_coroutine_yield: Hooking");
    return g_coroutine_hooks.yield();
  }

  if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
    LOG_DEBUG("cdd_coroutine_yield: Error EINVAL (TLS uninitialized)");
    return EINVAL;
  }

  co = (struct CddCoroutine *)TlsGetValue(dwTlsIndex);
  if (!co || !co->caller_fiber) {
    LOG_DEBUG("cdd_coroutine_yield: Error EINVAL");
    return EINVAL;
  }

  SwitchToFiber(co->caller_fiber);
  LOG_DEBUG("cdd_coroutine_yield: Success");
  return 0;
}

int math_cdd_coroutine_is_done(const struct CddCoroutine *co) {
  if (g_coroutine_hooks.is_done) {
    return g_coroutine_hooks.is_done(co);
  }

  return co ? co->is_done : 1;
}

#elif !defined(CDD_NO_UCONTEXT) /* POSIX ucontext */

struct CddCoroutine {
  ucontext_t ctx;
  ucontext_t caller_ctx;
  void *stack;
  size_t stack_size;
  cdd_coroutine_cb cb;
  void *arg;
  int is_done;
};

/* We use a simple global thread-local pointer to track the current active
   coroutine for yield. Using our own cdd_tls.h or native __thread depending on
   compiler. For C89 strictness, we use pthread_key. */
static pthread_key_t co_tls_key;
static int co_tls_initialized = 0;

static void init_tls_key(void) {
  if (!co_tls_initialized) {
    pthread_key_create(&co_tls_key, NULL);
    co_tls_initialized = 1;
  }
}

static void ucontext_entry(void) {
  struct CddCoroutine *co =
      (struct CddCoroutine *)pthread_getspecific(co_tls_key);
  if (co) {
    if (co->cb) {
      co->cb(co->arg);
    }
    co->is_done = 1;
    /* Swap back to caller */
    swapcontext(&co->ctx, &co->caller_ctx);
  }
}

int cdd_coroutine_init(struct CddCoroutine **co, size_t stack_size,
                       cdd_coroutine_cb cb, void *arg) {
  struct CddCoroutine *c;

  LOG_DEBUG("cdd_coroutine_init: Entering");
  if (g_coroutine_hooks.init) {
    LOG_DEBUG("cdd_coroutine_init: Hooking");
    return g_coroutine_hooks.init(co, stack_size, cb, arg);
  }

  if (!co || !cb) {
    LOG_DEBUG("cdd_coroutine_init: Error EINVAL");
    return EINVAL;
  }

  init_tls_key();

  c = (struct CddCoroutine *)calloc(1, sizeof(struct CddCoroutine));
  if (!c) {
    LOG_DEBUG("cdd_coroutine_init: Error ENOMEM");
    return ENOMEM;
  }

  c->cb = cb;
  c->arg = arg;
  c->is_done = 0;

  if (stack_size == 0)
    stack_size = 1024 * 1024; /* 1MB default */

  c->stack_size = stack_size;
  c->stack = malloc(stack_size);
  if (!c->stack) {
    LOG_DEBUG("cdd_coroutine_init: Error ENOMEM allocating stack");
    free(c);
    return ENOMEM;
  }

  if (getcontext(&c->ctx) != 0) {
    LOG_DEBUG("cdd_coroutine_init: Error getcontext failed");
    free(c->stack);
    free(c);
    return EIO;
  }

  c->ctx.uc_stack.ss_sp = c->stack;
  c->ctx.uc_stack.ss_size = c->stack_size;
  c->ctx.uc_link = &c->caller_ctx;

  /* makecontext expects integer arguments, passing pointers requires some care
     on 64-bit systems but standard ucontext usage often passes nothing and
     relies on globals/TLS, which we do. */
  makecontext(&c->ctx, (void (*)(void))ucontext_entry, 0);

  *co = c;
  LOG_DEBUG("cdd_coroutine_init: Success");
  return 0;
}

void cdd_coroutine_free(struct CddCoroutine *co) {
  LOG_DEBUG("cdd_coroutine_free: Entering");
  if (g_coroutine_hooks.free) {
    LOG_DEBUG("cdd_coroutine_free: Hooking");
    g_coroutine_hooks.free(co);
    return;
  }

  if (co) {
    if (co->stack)
      free(co->stack);
    free(co);
  }
  LOG_DEBUG("cdd_coroutine_free: Exiting");
}

int cdd_coroutine_resume(struct CddCoroutine *co) {
  LOG_DEBUG("cdd_coroutine_resume: Entering");
  if (g_coroutine_hooks.resume) {
    LOG_DEBUG("cdd_coroutine_resume: Hooking");
    return g_coroutine_hooks.resume(co);
  }

  if (!co || co->is_done) {
    LOG_DEBUG("cdd_coroutine_resume: Error EINVAL");
    return EINVAL;
  }

  init_tls_key();
  pthread_setspecific(co_tls_key, co);

  if (swapcontext(&co->caller_ctx, &co->ctx) != 0) {
    LOG_DEBUG("cdd_coroutine_resume: Error EIO (swapcontext failed)");
    return EIO;
  }
  LOG_DEBUG("cdd_coroutine_resume: Success");
  return 0;
}

int cdd_coroutine_yield(void) {
  struct CddCoroutine *co;

  LOG_DEBUG("cdd_coroutine_yield: Entering");
  if (g_coroutine_hooks.yield) {
    LOG_DEBUG("cdd_coroutine_yield: Hooking");
    return g_coroutine_hooks.yield();
  }

  if (!co_tls_initialized) {
    LOG_DEBUG("cdd_coroutine_yield: Error EINVAL (TLS not initialized)");
    return EINVAL;
  }
  co = (struct CddCoroutine *)pthread_getspecific(co_tls_key);

  if (!co) {
    LOG_DEBUG("cdd_coroutine_yield: Error EINVAL (no coroutine in TLS)");
    return EINVAL;
  }

  if (swapcontext(&co->ctx, &co->caller_ctx) != 0) {
    LOG_DEBUG("cdd_coroutine_yield: Error EIO (swapcontext failed)");
    return EIO;
  }
  LOG_DEBUG("cdd_coroutine_yield: Success");
  return 0;
}

int math_cdd_coroutine_is_done(const struct CddCoroutine *co) {
  if (g_coroutine_hooks.is_done) {
    return g_coroutine_hooks.is_done(co);
  }

  return co ? co->is_done : 1;
}

#else

int cdd_coroutine_init(struct CddCoroutine **co, size_t stack_size,
                       cdd_coroutine_cb cb, void *arg) {
  if (g_coroutine_hooks.init) {
    return g_coroutine_hooks.init(co, stack_size, cb, arg);
  }
#ifndef ENOSYS
#define ENOSYS EINVAL
#endif
  return ENOSYS;
}

void cdd_coroutine_free(struct CddCoroutine *co) {
  LOG_DEBUG("cdd_coroutine_free (fallback): Entering");
  if (g_coroutine_hooks.free) {
    LOG_DEBUG("cdd_coroutine_free (fallback): Hooking");
    g_coroutine_hooks.free(co);
  }
}

int cdd_coroutine_resume(struct CddCoroutine *co) {
  LOG_DEBUG("cdd_coroutine_resume (fallback): Entering");
  if (g_coroutine_hooks.resume) {
    LOG_DEBUG("cdd_coroutine_resume (fallback): Hooking");
    return g_coroutine_hooks.resume(co);
  }
#ifndef ENOSYS
#define ENOSYS EINVAL
#endif
  LOG_DEBUG("cdd_coroutine_resume (fallback): Error ENOSYS");
  return ENOSYS;
}

int cdd_coroutine_yield(void) {
  LOG_DEBUG("cdd_coroutine_yield (fallback): Entering");
  if (g_coroutine_hooks.yield) {
    LOG_DEBUG("cdd_coroutine_yield (fallback): Hooking");
    return g_coroutine_hooks.yield();
  }
#ifndef ENOSYS
#define ENOSYS EINVAL
#endif
  LOG_DEBUG("cdd_coroutine_yield (fallback): Error ENOSYS");
  return ENOSYS;
}

int math_cdd_coroutine_is_done(const struct CddCoroutine *co) {
  if (g_coroutine_hooks.is_done) {
    return g_coroutine_hooks.is_done(co);
  }
  return 1;
}

#endif
