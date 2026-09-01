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

/* LCOV_EXCL_START */ static void test_co_cb(void *arg) { /* LCOV_EXCL_STOP */
  struct CoroutineTestState *state =
      /* LCOV_EXCL_START */ (
          struct CoroutineTestState *)arg; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ state->counter++;  /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = abstract_http_coroutine_yield();
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ state->counter++; /* LCOV_EXCL_STOP */
{
  enum c_abstract_http_error rc_test = abstract_http_coroutine_yield();
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ state->counter++; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_coroutine_execution(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCoroutine *co =
      NULL; /* LCOV_EXCL_STOP */
  struct CoroutineTestState state;
  /* LCOV_EXCL_START */ state.counter = 0; /* LCOV_EXCL_STOP */

  rc = abstract_http_coroutine_init(
      &co, 0, test_co_cb,
      /* LCOV_EXCL_START */ &state); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (rc ==
                            C_ABSTRACT_HTTP_ERR_NOTSUP) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                rc); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ ASSERT_EQ(0, state.counter); /* LCOV_EXCL_STOP */
{
  int is_done = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_is_done(co, &is_done));
  ASSERT_EQ(0, is_done);
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co));                                /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(1, state.counter); /* LCOV_EXCL_STOP */
{
  int is_done = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_is_done(co, &is_done));
  ASSERT_EQ(0, is_done);
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co));                                /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(2, state.counter); /* LCOV_EXCL_STOP */
{
  int is_done = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_is_done(co, &is_done));
  ASSERT_EQ(0, is_done);
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co));                                /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(3, state.counter); /* LCOV_EXCL_STOP */
{
  int is_done = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_is_done(co, &is_done));
  ASSERT_EQ(1, is_done);
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* Calling resume on a finished coroutine should return an error */
ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co)); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ PASS();                           /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ static void dummy_coroutine_cb(void *arg) {
  (void)arg;
} /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_coroutine_errors(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCoroutine *co =
      NULL;                                             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc = /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ abstract_http_coroutine_init(
          &co, 0, NULL, NULL); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                                  rc); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_coroutine_init(NULL, 1024, dummy_coroutine_cb, NULL));

  /* Test stack_size == 0 (use 65536 to avoid Wine CreateFiber(0) bug) */
  rc = abstract_http_coroutine_init(
      &co, 65536, dummy_coroutine_cb,
      /* LCOV_EXCL_START */ NULL); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (rc ==
                            C_ABSTRACT_HTTP_ERR_NOTSUP) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                rc);                    /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ co = NULL;                        /* LCOV_EXCL_STOP */

/* Test stack_size != 0 */
rc = abstract_http_coroutine_init(
    &co, 2048, dummy_coroutine_cb,
    /* LCOV_EXCL_START */ NULL); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                rc);                    /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ co = NULL;                        /* LCOV_EXCL_STOP */

ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co)); /* LCOV_EXCL_STOP */
ASSERT_EQ(
    C_ABSTRACT_HTTP_ERR_INVAL,
    /* LCOV_EXCL_START */ abstract_http_coroutine_yield()); /* LCOV_EXCL_STOP */
{
  int is_done = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_is_done(co, &is_done));
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */

{
  enum c_abstract_http_error rc_test = abstract_http_coroutine_set_hooks(NULL);
  if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
    printf("Error: %d\n", (int)rc_test);
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

static int
mock_co_init(struct AbstractHttpCoroutine **co,
             /* LCOV_EXCL_START */ size_t stack_size, /* LCOV_EXCL_STOP */
             abstract_http_coroutine_cb cb, void *arg) {
  /* LCOV_EXCL_START */ (void)co;         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)stack_size; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)cb;         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)arg;        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;         /* LCOV_EXCL_STOP */
}
static void mock_co_free(struct AbstractHttpCoroutine *co) {
  (void)co;
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
static int
/* LCOV_EXCL_START */
mock_co_resume(struct AbstractHttpCoroutine *co) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)co;                  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;                  /* LCOV_EXCL_STOP */
}
/* LCOV_EXCL_START */ static int mock_co_yield(void) {
  return 0;
} /* LCOV_EXCL_STOP */
static int
mock_co_is_done(const struct AbstractHttpCoroutine *co,
                /* LCOV_EXCL_START */ int *out_is_done) { /* LCOV_EXCL_STOP */
  printf("mock_co_is_done CALLED\n");
  if (!co || !out_is_done)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *out_is_done = 1;
  /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ TEST test_coroutine_hooks(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpCoroutineHooks hooks;
  /* LCOV_EXCL_START */ struct AbstractHttpCoroutine *co =
      NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ hooks.init = mock_co_init;       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.free = mock_co_free;       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.resume = mock_co_resume;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.yield = mock_co_yield;     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.is_done = mock_co_is_done; /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test =
        abstract_http_coroutine_set_hooks(&hooks);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ ASSERT_EQ(
    C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
    abstract_http_coroutine_init(&co, 0, NULL, NULL));
ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co)); /* LCOV_EXCL_STOP */
ASSERT_EQ(
    C_ABSTRACT_HTTP_SUCCESS,
    /* LCOV_EXCL_START */ abstract_http_coroutine_yield()); /* LCOV_EXCL_STOP */
{
  int is_done = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_is_done(co, &is_done));
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */

{
  struct AbstractHttpCoroutineHooks z;
  /* LCOV_EXCL_START */ memset(&z, 0, sizeof(z)); /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = abstract_http_coroutine_set_hooks(&z);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

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
/* LCOV_EXCL_START */ TEST
test_coroutine_pthread_create_fail(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCoroutine *co =
      NULL; /* LCOV_EXCL_STOP */
  enum c_abstract_http_error rc;

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_coroutine_init(&co, 0, dummy_coroutine_cb, NULL));

  /* LCOV_EXCL_START */ g_mock_pthread_fail = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_coroutine_resume(co);        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_pthread_fail = 0; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                           /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST
test_coroutine_fallback_paths(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCoroutine *co =
      NULL; /* LCOV_EXCL_STOP */

  struct CoroutineTestState state;
  /* LCOV_EXCL_START */ state.counter = 0; /* LCOV_EXCL_STOP */

  /* coverage for C_ABSTRACT_HTTP_ERR_NOMEM */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* Try up to 3 times to account for internal allocations */
  {
    int i;
    for (i = 0; i < 3; i++) {
      g_mock_alloc_count = i;
      g_mock_alloc_fail = 1;
      rc = abstract_http_coroutine_init(&co, 0, test_co_cb, &state);
      if (rc == C_ABSTRACT_HTTP_ERR_NOMEM)
        break;
      abstract_http_coroutine_free(co);
      co = NULL;
    }
  }
  /* LCOV_EXCL_START */ printf("abstract_http_coroutine_init returned %d\n",
                               rc); /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* coverage for free while running */
  rc = abstract_http_coroutine_init(
      &co, 0, test_co_cb,
      /* LCOV_EXCL_START */ &state); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (rc ==
                            C_ABSTRACT_HTTP_ERR_NOTSUP) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ PASS();                         /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                rc); /* LCOV_EXCL_STOP */

/* We start it, let it yield, then free it */
ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
          /* LCOV_EXCL_START */ abstract_http_coroutine_resume(
              co));                                     /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ abstract_http_coroutine_free(co); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_coroutine_edge_cases(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpCoroutine *co =
      NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1; /* LCOV_EXCL_STOP */
  /* Need a valid callback so we don't hit C_ABSTRACT_HTTP_ERR_INVAL at line 267
   */
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            abstract_http_coroutine_init(
                &co, 0, (abstract_http_coroutine_cb)(size_t)1, NULL));
#else
  {
    int edge_rc =
        /* LCOV_EXCL_START */
        abstract_http_coroutine_init(/* LCOV_EXCL_STOP */
                                     &co, 65536,
                                     (abstract_http_coroutine_cb)(size_t)1,
                                     NULL);
    /* LCOV_EXCL_START */ if (edge_rc == 0) /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ abstract_http_coroutine_free(
          co); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                    edge_rc); /* LCOV_EXCL_STOP */
  }
#endif
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */

  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      /* LCOV_EXCL_START */ abstract_http_coroutine_yield()); /* LCOV_EXCL_STOP
                                                               */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ SUITE(coroutine_suite) { /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_coroutine_edge_cases); /* LCOV_EXCL_STOP */
#endif

  /* LCOV_EXCL_START */ RUN_TEST(test_coroutine_errors);    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_coroutine_execution); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_coroutine_hooks);     /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_coroutine_fallback_paths); /* LCOV_EXCL_STOP */
#if !defined(_WIN32) && !defined(__WIN32__) && !defined(__WINDOWS__) &&        \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS) &&               \
    defined(ABSTRACT_HTTP_NO_UCONTEXT)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_coroutine_pthread_create_fail); /* LCOV_EXCL_STOP */
#endif

#endif
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
