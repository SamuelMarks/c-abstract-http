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
/* clang-format on */

/** @brief Documented */
struct CoroutineTestState {
  /** @brief Documented */
  int counter;
};

static void test_co_cb(void *arg) {
  struct CoroutineTestState *state = (struct CoroutineTestState *)arg;
  state->counter++;
  cdd_coroutine_yield();
  state->counter++;
  cdd_coroutine_yield();
  state->counter++;
}

TEST test_coroutine_execution(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct CddCoroutine *co = NULL;
  struct CoroutineTestState state;
  state.counter = 0;

  rc = cdd_coroutine_init(&co, 0, test_co_cb, &state);
  ASSERT_EQ(0, rc);

  ASSERT_EQ(0, state.counter);
  ASSERT_EQ(0, math_cdd_coroutine_is_done(co));

  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(1, state.counter);
  ASSERT_EQ(0, math_cdd_coroutine_is_done(co));

  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(2, state.counter);
  ASSERT_EQ(0, math_cdd_coroutine_is_done(co));

  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(3, state.counter);
  ASSERT_EQ(1, math_cdd_coroutine_is_done(co));

  /* Calling resume on a finished coroutine should return an error */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_coroutine_resume(co));

  cdd_coroutine_free(co);
  PASS();
}

static void dummy_coroutine_cb(void *arg) { (void)arg; }

TEST test_coroutine_errors(void) {
  struct CddCoroutine *co = NULL;
  enum c_abstract_http_error rc = cdd_coroutine_init(&co, 0, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_coroutine_init(NULL, 1024, dummy_coroutine_cb, NULL));

  /* Test stack_size == 0 (use 65536 to avoid Wine CreateFiber(0) bug) */
  rc = cdd_coroutine_init(&co, 65536, dummy_coroutine_cb, NULL);
  ASSERT_EQ(0, rc);
  cdd_coroutine_free(co);
  co = NULL;

  /* Test stack_size != 0 */
  rc = cdd_coroutine_init(&co, 2048, dummy_coroutine_cb, NULL);
  ASSERT_EQ(0, rc);
  cdd_coroutine_free(co);
  co = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_coroutine_resume(co));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_coroutine_yield());
  ASSERT_EQ(1, math_cdd_coroutine_is_done(co));
  cdd_coroutine_free(co);

  cdd_coroutine_set_hooks(NULL);

  PASS();
}

static int mock_co_init(struct CddCoroutine **co, size_t stack_size,
                        cdd_coroutine_cb cb, void *arg) {
  (void)co;
  (void)stack_size;
  (void)cb;
  (void)arg;
  return 0;
}
static void mock_co_free(struct CddCoroutine *co) { (void)co; }
static int mock_co_resume(struct CddCoroutine *co) {
  (void)co;
  return 0;
}
static int mock_co_yield(void) { return 0; }
static int mock_co_is_done(const struct CddCoroutine *co) {
  (void)co;
  return 1;
}

TEST test_coroutine_hooks(void) {
  struct CddCoroutineHooks hooks;
  struct CddCoroutine *co = NULL;

  hooks.init = mock_co_init;
  hooks.free = mock_co_free;
  hooks.resume = mock_co_resume;
  hooks.yield = mock_co_yield;
  hooks.is_done = mock_co_is_done;

  cdd_coroutine_set_hooks(&hooks);

  ASSERT_EQ(0, cdd_coroutine_init(&co, 0, NULL, NULL));
  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(0, cdd_coroutine_yield());
  ASSERT_EQ(1, math_cdd_coroutine_is_done(co));
  cdd_coroutine_free(co);

  {
    struct CddCoroutineHooks z;
    memset(&z, 0, sizeof(z));
    cdd_coroutine_set_hooks(&z);
  }

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_coroutine_fallback_paths(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct CddCoroutine *co = NULL;

  struct CoroutineTestState state;
  state.counter = 0;

  /* coverage for C_ABSTRACT_HTTP_ERR_NOMEM */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_coroutine_init(&co, 0, test_co_cb, &state);
  printf("cdd_coroutine_init returned %d\n", rc);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  /* coverage for free while running */
  rc = cdd_coroutine_init(&co, 0, test_co_cb, &state);
  ASSERT_EQ(0, rc);

  /* We start it, let it yield, then free it */
  ASSERT_EQ(0, cdd_coroutine_resume(co));
  cdd_coroutine_free(co);

  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_coroutine_edge_cases(void) {
  struct CddCoroutine *co = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  /* Need a valid callback so we don't hit C_ABSTRACT_HTTP_ERR_INVAL at line 267
   */
#if !defined(_WIN32) && !defined(__APPLE__)
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            cdd_coroutine_init(&co, 0, (cdd_coroutine_cb)1, NULL));
#else
  {
    int edge_rc = cdd_coroutine_init(&co, 65536, (cdd_coroutine_cb)1, NULL);
    if (edge_rc == 0)
      cdd_coroutine_free(co);
    ASSERT_EQ(0, edge_rc);
  }
#endif
  g_mock_alloc_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_coroutine_yield());

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
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
