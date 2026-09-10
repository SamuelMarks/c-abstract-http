
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

static void test_co_cb(void *arg) {
  struct CoroutineTestState *state = (struct CoroutineTestState *)arg;
  state->counter++;
  (void)!abstract_http_coroutine_yield();
  state->counter++;
  (void)!abstract_http_coroutine_yield();
  state->counter++;
}

TEST test_coroutine_execution(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpCoroutine *co = NULL;
  struct CoroutineTestState state;
  state.counter = 0;

  rc = abstract_http_coroutine_init(&co, 0, test_co_cb, &state);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  ASSERT_EQ(0, state.counter);
  {
    int is_done = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_coroutine_is_done(co, &is_done));
    ASSERT_EQ(0, is_done);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_resume(co));
  ASSERT_EQ(1, state.counter);
  {
    int is_done = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_coroutine_is_done(co, &is_done));
    ASSERT_EQ(0, is_done);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_resume(co));
  ASSERT_EQ(2, state.counter);
  {
    int is_done = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_coroutine_is_done(co, &is_done));
    ASSERT_EQ(0, is_done);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_resume(co));
  ASSERT_EQ(3, state.counter);
  {
    int is_done = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_coroutine_is_done(co, &is_done));
    ASSERT_EQ(1, is_done);
  }

  /* Calling resume on a finished coroutine should return an error */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_coroutine_resume(co));

  abstract_http_coroutine_free(co);
  PASS();
}

static void dummy_coroutine_cb(void *arg) { (void)arg; }

TEST test_coroutine_errors(void) {
  struct AbstractHttpCoroutine *co = NULL;
  enum c_abstract_http_error rc =
      abstract_http_coroutine_init(&co, 0, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_coroutine_init(NULL, 1024, dummy_coroutine_cb, NULL));

  /* Test stack_size == 0 (use 65536 to avoid Wine CreateFiber(0) bug) */
  rc = abstract_http_coroutine_init(&co, 65536, dummy_coroutine_cb, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  abstract_http_coroutine_free(co);
  co = NULL;

  /* Test stack_size != 0 */
  rc = abstract_http_coroutine_init(&co, 2048, dummy_coroutine_cb, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  abstract_http_coroutine_free(co);
  co = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_coroutine_resume(co));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_coroutine_yield());
  {
    int is_done = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              abstract_http_coroutine_is_done(co, &is_done));
  }
  abstract_http_coroutine_free(co);

  dummy_coroutine_cb(NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_set_hooks(NULL));

  PASS();
}

static int mock_co_init(struct AbstractHttpCoroutine **co, size_t stack_size,
                        abstract_http_coroutine_cb cb, void *arg) {
  static int dummy = 0;
  (void)stack_size;
  (void)cb;
  (void)arg;
  if (co)
    *co = (struct AbstractHttpCoroutine *)&dummy;
  return 0;
}
static void mock_co_free(struct AbstractHttpCoroutine *co) { (void)co; }
static int

mock_co_resume(struct AbstractHttpCoroutine *co) {
  (void)co;
  return 0;
}
static int mock_co_yield(void) { return 0; }
static int mock_co_is_done(const struct AbstractHttpCoroutine *co,
                           int *out_is_done) {
  printf("mock_co_is_done CALLED\n");
  if (!co || !out_is_done)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *out_is_done = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

TEST test_coroutine_hooks(void) {
  struct AbstractHttpCoroutineHooks hooks;
  struct AbstractHttpCoroutine *co = NULL;

  hooks.init = mock_co_init;
  hooks.free = mock_co_free;
  hooks.resume = mock_co_resume;
  hooks.yield = mock_co_yield;
  hooks.is_done = mock_co_is_done;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_set_hooks(&hooks));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_init(&co, 0, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_resume(co));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_yield());
  {
    int is_done = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_coroutine_is_done(co, &is_done));
    ASSERT_EQ(1, is_done);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              abstract_http_coroutine_is_done(NULL, &is_done));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              abstract_http_coroutine_is_done(co, NULL));
  }
  abstract_http_coroutine_free(co);

  {
    struct AbstractHttpCoroutineHooks z;
    memset(&z, 0, sizeof(z));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_set_hooks(&z));
  }

  {
    struct AbstractHttpCoroutine *test_co = NULL;
    int is_done = 0;
    ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS,
        abstract_http_coroutine_init(&test_co, 0, dummy_coroutine_cb, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              abstract_http_coroutine_is_done(test_co, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              abstract_http_coroutine_is_done(NULL, &is_done));
    abstract_http_coroutine_free(test_co);
  }

  PASS();
}

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
TEST test_coroutine_pthread_create_fail(void) {
  struct AbstractHttpCoroutine *co = NULL;
  enum c_abstract_http_error rc;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_coroutine_init(&co, 0, dummy_coroutine_cb, NULL));

  g_mock_pthread_fail = 1;
  rc = abstract_http_coroutine_resume(co);
  g_mock_pthread_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  abstract_http_coroutine_free(co);
  PASS();
}
#endif

TEST test_coroutine_fallback_paths(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpCoroutine *co = NULL;

  struct CoroutineTestState state;
  state.counter = 0;

  /* coverage for C_ABSTRACT_HTTP_ERR_NOMEM */
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  rc = abstract_http_coroutine_init(&co, 0, test_co_cb, &state);
  g_mock_alloc_fail = 0;
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");

  /* coverage for free while running */
  rc = abstract_http_coroutine_init(&co, 0, test_co_cb, &state);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* We start it, let it yield, then free it */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_coroutine_resume(co));
  abstract_http_coroutine_free(co);

  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_coroutine_edge_cases(void) {
  struct AbstractHttpCoroutine *co = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  /* Need a valid callback so we don't hit C_ABSTRACT_HTTP_ERR_INVAL at line 267
   */
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            abstract_http_coroutine_init(
                &co, 0, (abstract_http_coroutine_cb)(size_t)1, NULL));
#else
  {
    int edge_rc =

        abstract_http_coroutine_init(
            &co, 65536, (abstract_http_coroutine_cb)(size_t)1, NULL);
    if (edge_rc == 0)
      abstract_http_coroutine_free(co);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, edge_rc);
  }
#endif
  g_mock_alloc_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_coroutine_yield());

  PASS();
}
#endif

SUITE(coroutine_suite) {

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_coroutine_edge_cases);
#endif

  RUN_TEST(test_coroutine_errors);
  RUN_TEST(test_coroutine_execution);
  RUN_TEST(test_coroutine_hooks);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_coroutine_fallback_paths);
#if !defined(_WIN32) && !defined(__WIN32__) && !defined(__WINDOWS__) &&        \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS) &&               \
    defined(ABSTRACT_HTTP_NO_UCONTEXT)
  RUN_TEST(test_coroutine_pthread_create_fail);
#endif

#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
