#ifndef TEST_COROUTINE_H
#define TEST_COROUTINE_H

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
  state.counter = 0;

  ASSERT_EQ(0, cdd_coroutine_init(&co, 0, test_co_cb, &state));

  ASSERT_EQ(0, state.counter);
  ASSERT_EQ(0, cdd_coroutine_is_done(co));

  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(1, state.counter);
  ASSERT_EQ(0, cdd_coroutine_is_done(co));

  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(2, state.counter);
  ASSERT_EQ(0, cdd_coroutine_is_done(co));

  ASSERT_EQ(0, cdd_coroutine_resume(co));
  ASSERT_EQ(3, state.counter);
  ASSERT_EQ(1, cdd_coroutine_is_done(co));

  /* Calling resume on a finished coroutine should return an error */
  ASSERT_EQ(EINVAL, cdd_coroutine_resume(co));

  cdd_coroutine_free(co);
  PASS();
}

SUITE(coroutine_suite) { RUN_TEST(test_coroutine_execution); }

#endif
