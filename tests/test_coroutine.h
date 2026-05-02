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

struct CoroutineTestState {
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
  struct CddCoroutine *co = NULL;
  struct CoroutineTestState state;
  int rc;
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
  ASSERT_EQ(EINVAL, cdd_coroutine_resume(co));

  cdd_coroutine_free(co);
  PASS();
}

TEST test_coroutine_errors(void) {
  struct CddCoroutine *co = NULL;
  int rc = cdd_coroutine_init(&co, 0, NULL, NULL);
  ASSERT_EQ(EINVAL, rc);
  ASSERT_EQ(EINVAL, cdd_coroutine_resume(co));
  ASSERT_EQ(EINVAL, cdd_coroutine_yield());
  ASSERT_EQ(1, math_cdd_coroutine_is_done(co));
  cdd_coroutine_free(co);
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
  struct CddCoroutine *co = NULL;
  int rc;

  struct CoroutineTestState state;
  state.counter = 0;

  /* coverage for ENOMEM */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_coroutine_init(&co, 0, test_co_cb, &state);
  printf("cdd_coroutine_init returned %d\n", rc);
  ASSERT_EQ(ENOMEM, rc);
  g_mock_alloc_fail = 0;

  /* coverage for free while running */
  rc = cdd_coroutine_init(&co, 0, test_co_cb, &state);
  ASSERT_EQ(0, rc);

  /* We start it, let it yield, then free it */
  ASSERT_EQ(0, cdd_coroutine_resume(co));
  cdd_coroutine_free(co);

  PASS();
}
#endif

SUITE(coroutine_suite) {
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
