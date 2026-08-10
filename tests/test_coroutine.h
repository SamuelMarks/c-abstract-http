/* LCOV_EXCL_BR_START */
#ifndef TEST_COROUTINE_H
#define TEST_COROUTINE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/coroutine.h>
#include "mock_alloc.h"
/* clang-format on */

/** @brief Documented */
struct CoroutineTestState {
  /** @brief Documented */
  int counter;
};

static void test_co_cb(void *arg) { /* LCOV_EXCL_LINE */
  struct CoroutineTestState *state =
      (struct CoroutineTestState *)arg; /* LCOV_EXCL_LINE */
  state->counter++;                     /* LCOV_EXCL_LINE */
  abstract_http_coroutine_yield();      /* LCOV_EXCL_LINE */
  state->counter++;                     /* LCOV_EXCL_LINE */
  abstract_http_coroutine_yield();      /* LCOV_EXCL_LINE */
  state->counter++;                     /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_coroutine_execution(void) {                      /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  struct AbstractHttpCoroutine *co = NULL;                 /* LCOV_EXCL_LINE */
  struct CoroutineTestState state;
  state.counter = 0; /* LCOV_EXCL_LINE */

  rc = abstract_http_coroutine_init(&co, 0, test_co_cb,
                                    &state); /* LCOV_EXCL_LINE */
  if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP) {    /* LCOV_EXCL_LINE */
    PASS();                                  /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc); /* LCOV_EXCL_LINE */

  ASSERT_EQ(0, state.counter);                            /* LCOV_EXCL_LINE */
  ASSERT_EQ(0, math_abstract_http_coroutine_is_done(co)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_resume(co));          /* LCOV_EXCL_LINE */
  ASSERT_EQ(1, state.counter);                            /* LCOV_EXCL_LINE */
  ASSERT_EQ(0, math_abstract_http_coroutine_is_done(co)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_resume(co));          /* LCOV_EXCL_LINE */
  ASSERT_EQ(2, state.counter);                            /* LCOV_EXCL_LINE */
  ASSERT_EQ(0, math_abstract_http_coroutine_is_done(co)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_resume(co));          /* LCOV_EXCL_LINE */
  ASSERT_EQ(3, state.counter);                            /* LCOV_EXCL_LINE */
  ASSERT_EQ(1, math_abstract_http_coroutine_is_done(co)); /* LCOV_EXCL_LINE */

  /* Calling resume on a finished coroutine should return an error */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_resume(co)); /* LCOV_EXCL_LINE */

  abstract_http_coroutine_free(co); /* LCOV_EXCL_LINE */
  PASS();                           /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static void dummy_coroutine_cb(void *arg) { (void)arg; } /* LCOV_EXCL_LINE */

TEST test_coroutine_errors(void) {                      /* LCOV_EXCL_LINE */
  struct AbstractHttpCoroutine *co = NULL;              /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc =                       /* LCOV_EXCL_LINE */
      abstract_http_coroutine_init(&co, 0, NULL, NULL); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);             /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,                  /* LCOV_EXCL_LINE */
            abstract_http_coroutine_init(NULL, 1024, dummy_coroutine_cb, NULL));

  /* Test stack_size == 0 (use 65536 to avoid Wine CreateFiber(0) bug) */
  rc = abstract_http_coroutine_init(&co, 65536, dummy_coroutine_cb,
                                    NULL); /* LCOV_EXCL_LINE */
  if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP) {  /* LCOV_EXCL_LINE */
    PASS();                                /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc); /* LCOV_EXCL_LINE */
  abstract_http_coroutine_free(co);       /* LCOV_EXCL_LINE */
  co = NULL;                              /* LCOV_EXCL_LINE */

  /* Test stack_size != 0 */
  rc = abstract_http_coroutine_init(&co, 2048, dummy_coroutine_cb,
                                    NULL); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);  /* LCOV_EXCL_LINE */
  abstract_http_coroutine_free(co);        /* LCOV_EXCL_LINE */
  co = NULL;                               /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_resume(co)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_yield());             /* LCOV_EXCL_LINE */
  ASSERT_EQ(1, math_abstract_http_coroutine_is_done(co)); /* LCOV_EXCL_LINE */
  abstract_http_coroutine_free(co);                       /* LCOV_EXCL_LINE */

  abstract_http_coroutine_set_hooks(NULL); /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int mock_co_init(struct AbstractHttpCoroutine **co,
                        size_t stack_size, /* LCOV_EXCL_LINE */
                        abstract_http_coroutine_cb cb, void *arg) {
  (void)co;         /* LCOV_EXCL_LINE */
  (void)stack_size; /* LCOV_EXCL_LINE */
  (void)cb;         /* LCOV_EXCL_LINE */
  (void)arg;        /* LCOV_EXCL_LINE */
  return 0;         /* LCOV_EXCL_LINE */
}
static void mock_co_free(struct AbstractHttpCoroutine *co) {
  (void)co;
} /* LCOV_EXCL_LINE */
static int
mock_co_resume(struct AbstractHttpCoroutine *co) { /* LCOV_EXCL_LINE */
  (void)co;                                        /* LCOV_EXCL_LINE */
  return 0;                                        /* LCOV_EXCL_LINE */
}
static int mock_co_yield(void) { return 0; } /* LCOV_EXCL_LINE */
static int
mock_co_is_done(const struct AbstractHttpCoroutine *co) { /* LCOV_EXCL_LINE */
  (void)co;                                               /* LCOV_EXCL_LINE */
  return 1;                                               /* LCOV_EXCL_LINE */
}

TEST test_coroutine_hooks(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpCoroutineHooks hooks;
  struct AbstractHttpCoroutine *co = NULL; /* LCOV_EXCL_LINE */

  hooks.init = mock_co_init;       /* LCOV_EXCL_LINE */
  hooks.free = mock_co_free;       /* LCOV_EXCL_LINE */
  hooks.resume = mock_co_resume;   /* LCOV_EXCL_LINE */
  hooks.yield = mock_co_yield;     /* LCOV_EXCL_LINE */
  hooks.is_done = mock_co_is_done; /* LCOV_EXCL_LINE */

  abstract_http_coroutine_set_hooks(&hooks); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            abstract_http_coroutine_init(&co, 0, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_resume(co)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_yield());             /* LCOV_EXCL_LINE */
  ASSERT_EQ(1, math_abstract_http_coroutine_is_done(co)); /* LCOV_EXCL_LINE */
  abstract_http_coroutine_free(co);                       /* LCOV_EXCL_LINE */

  {
    struct AbstractHttpCoroutineHooks z;
    memset(&z, 0, sizeof(z));              /* LCOV_EXCL_LINE */
    abstract_http_coroutine_set_hooks(&z); /* LCOV_EXCL_LINE */
  }

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)

#if defined(EMSCRIPTEN) || defined(__EMSCRIPTEN__)
#define ABSTRACT_HTTP_NO_UCONTEXT 1
#endif
#if defined(__APPLE__) && defined(__MACH__)
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm__) ||          \
    defined(__aarch64) || defined(EMSCRIPTEN)
#define ABSTRACT_HTTP_NO_UCONTEXT 1
#endif
#elif defined(__linux__) && !defined(__GLIBC__)
#define ABSTRACT_HTTP_NO_UCONTEXT 1
#endif

#if !defined(_WIN32) && !defined(__WIN32__) && !defined(__WINDOWS__) &&        \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS) &&               \
    defined(ABSTRACT_HTTP_NO_UCONTEXT)
TEST test_coroutine_pthread_create_fail(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpCoroutine *co = NULL;      /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_LINE */
            abstract_http_coroutine_init(&co, 0, dummy_coroutine_cb, NULL));

  g_mock_pthread_fail = 1;                 /* LCOV_EXCL_LINE */
  rc = abstract_http_coroutine_resume(co); /* LCOV_EXCL_LINE */
  g_mock_pthread_fail = 0;                 /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc); /* LCOV_EXCL_LINE */

  abstract_http_coroutine_free(co); /* LCOV_EXCL_LINE */
  PASS();                           /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

TEST test_coroutine_fallback_paths(void) {                 /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  struct AbstractHttpCoroutine *co = NULL;                 /* LCOV_EXCL_LINE */

  struct CoroutineTestState state;
  state.counter = 0; /* LCOV_EXCL_LINE */

  /* coverage for C_ABSTRACT_HTTP_ERR_NOMEM */
  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0; /* LCOV_EXCL_LINE */
  rc = abstract_http_coroutine_init(&co, 0, test_co_cb,
                                    &state);                /* LCOV_EXCL_LINE */
  printf("abstract_http_coroutine_init returned %d\n", rc); /* LCOV_EXCL_LINE */
  {
    int rc_test_tmp = rc;  /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  /* coverage for free while running */
  rc = abstract_http_coroutine_init(&co, 0, test_co_cb,
                                    &state); /* LCOV_EXCL_LINE */
  if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP) {    /* LCOV_EXCL_LINE */
    PASS();                                  /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc); /* LCOV_EXCL_LINE */

  /* We start it, let it yield, then free it */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_resume(co)); /* LCOV_EXCL_LINE */
  abstract_http_coroutine_free(co);              /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_coroutine_edge_cases(void) {     /* LCOV_EXCL_LINE */
  struct AbstractHttpCoroutine *co = NULL; /* LCOV_EXCL_LINE */

  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 1; /* LCOV_EXCL_LINE */
  /* Need a valid callback so we don't hit C_ABSTRACT_HTTP_ERR_INVAL at line 267
   */
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            abstract_http_coroutine_init(&co, 0, (abstract_http_coroutine_cb)1,
                                         NULL));
#else
  {
    int edge_rc =
        abstract_http_coroutine_init(/* LCOV_EXCL_LINE */
                                     &co, 65536, (abstract_http_coroutine_cb)1,
                                     NULL);
    if (edge_rc == 0)                            /* LCOV_EXCL_LINE */
      abstract_http_coroutine_free(co);          /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, edge_rc); /* LCOV_EXCL_LINE */
  }
#endif
  g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_yield()); /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

SUITE(coroutine_suite) { /* LCOV_EXCL_LINE */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_coroutine_edge_cases); /* LCOV_EXCL_LINE */
#endif

  RUN_TEST(test_coroutine_errors);    /* LCOV_EXCL_LINE */
  RUN_TEST(test_coroutine_execution); /* LCOV_EXCL_LINE */
  RUN_TEST(test_coroutine_hooks);     /* LCOV_EXCL_LINE */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_coroutine_fallback_paths); /* LCOV_EXCL_LINE */
#if !defined(_WIN32) && !defined(__WIN32__) && !defined(__WINDOWS__) &&        \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS) &&               \
    defined(ABSTRACT_HTTP_NO_UCONTEXT)
  RUN_TEST(test_coroutine_pthread_create_fail); /* LCOV_EXCL_LINE */
#endif

#endif
} /* LCOV_EXCL_LINE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
