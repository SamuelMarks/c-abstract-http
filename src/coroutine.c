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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#if defined(__APPLE__) && defined(__MACH__)
/* ucontext is deprecated on macOS but still mostly works for simple cases,
   we define _XOPEN_SOURCE to suppress some warnings, or ignore them. */
#define _XOPEN_SOURCE 600
#endif
#include <ucontext.h>
#endif

#include <c_abstract_http/coroutine.h>
#include <c_abstract_http/cdd_tls.h>
/* clang-format on */

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

  if (!co || !cb)
    return EINVAL;

  if (dwTlsIndex == TLS_OUT_OF_INDEXES) {
    dwTlsIndex = TlsAlloc();
    if (dwTlsIndex == TLS_OUT_OF_INDEXES)
      return EIO;
  }

  c = (struct CddCoroutine *)calloc(1, sizeof(struct CddCoroutine));
  if (!c)
    return ENOMEM;

  c->cb = cb;
  c->arg = arg;
  c->is_done = 0;

  if (stack_size == 0)
    stack_size = 1024 * 1024; /* 1MB default */

  c->fiber = CreateFiber(stack_size, fiber_entry, c);
  if (!c->fiber) {
    free(c);
    return EIO;
  }

  *co = c;
  return 0;
}

void cdd_coroutine_free(struct CddCoroutine *co) {
  if (co) {
    if (co->fiber) {
      DeleteFiber(co->fiber);
    }
    free(co);
  }
}

int cdd_coroutine_resume(struct CddCoroutine *co) {
  LPVOID current_fiber;

  if (!co || co->is_done)
    return EINVAL;

  current_fiber = GetCurrentFiber();
  /* If thread is not a fiber yet, convert it */
  if (current_fiber == (LPVOID)0x1e00 || current_fiber == NULL) {
    current_fiber = ConvertThreadToFiber(NULL);
    if (!current_fiber)
      return EIO;
  }

  co->caller_fiber = current_fiber;
  SwitchToFiber(co->fiber);

  return 0;
}

int cdd_coroutine_yield(void) {
  struct CddCoroutine *co;
  if (dwTlsIndex == TLS_OUT_OF_INDEXES)
    return EINVAL;

  co = (struct CddCoroutine *)TlsGetValue(dwTlsIndex);
  if (!co || !co->caller_fiber)
    return EINVAL;

  SwitchToFiber(co->caller_fiber);
  return 0;
}

int cdd_coroutine_is_done(const struct CddCoroutine *co) {
  return co ? co->is_done : 1;
}

#else /* POSIX ucontext */

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

  if (!co || !cb)
    return EINVAL;

  init_tls_key();

  c = (struct CddCoroutine *)calloc(1, sizeof(struct CddCoroutine));
  if (!c)
    return ENOMEM;

  c->cb = cb;
  c->arg = arg;
  c->is_done = 0;

  if (stack_size == 0)
    stack_size = 1024 * 1024; /* 1MB default */

  c->stack_size = stack_size;
  c->stack = malloc(stack_size);
  if (!c->stack) {
    free(c);
    return ENOMEM;
  }

  if (getcontext(&c->ctx) != 0) {
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
  return 0;
}

void cdd_coroutine_free(struct CddCoroutine *co) {
  if (co) {
    if (co->stack)
      free(co->stack);
    free(co);
  }
}

int cdd_coroutine_resume(struct CddCoroutine *co) {
  if (!co || co->is_done)
    return EINVAL;

  init_tls_key();
  pthread_setspecific(co_tls_key, co);

  if (swapcontext(&co->caller_ctx, &co->ctx) != 0) {
    return EIO;
  }
  return 0;
}

int cdd_coroutine_yield(void) {
  struct CddCoroutine *co;

  if (!co_tls_initialized)
    return EINVAL;
  co = (struct CddCoroutine *)pthread_getspecific(co_tls_key);

  if (!co)
    return EINVAL;

  if (swapcontext(&co->ctx, &co->caller_ctx) != 0) {
    return EIO;
  }
  return 0;
}

int cdd_coroutine_is_done(const struct CddCoroutine *co) {
  return co ? co->is_done : 1;
}

#endif