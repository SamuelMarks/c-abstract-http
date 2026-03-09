#ifndef TEST_EVENT_LOOP_H
#define TEST_EVENT_LOOP_H

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
/* clang-format on */

static void timer_cb_1(struct ModalityEventLoop *loop, int timer_id,
                       void *user_data) {
  int *triggered = (int *)user_data;
  *triggered = 1;
  http_loop_stop(loop);
}

TEST test_event_loop_init_free(void) {
  struct ModalityEventLoop *loop;
  ASSERT_EQ(0, http_loop_init(&loop));
  ASSERT(loop != NULL);
  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_timer(void) {
  struct ModalityEventLoop *loop;
  int timer_id;
  int triggered = 0;

  ASSERT_EQ(0, http_loop_init(&loop));

  /* Add timer for 10ms */
  ASSERT_EQ(0,
            http_loop_add_timer(loop, 10, timer_cb_1, &triggered, &timer_id));

  /* Run loop, it should block and then return 0 when timer triggers and stops
   * loop */
  ASSERT_EQ(0, http_loop_run(loop));
  ASSERT_EQ(1, triggered);

  http_loop_free(loop);
  PASS();
}

static void timer_cb_cancel(struct ModalityEventLoop *loop, int timer_id,
                            void *user_data) {
  int *triggered = (int *)user_data;
  *triggered = 1; /* Should not be hit */
}

static void timer_cb_stop(struct ModalityEventLoop *loop, int timer_id,
                          void *user_data) {
  http_loop_stop(loop);
}

TEST test_event_loop_timer_cancel(void) {
  struct ModalityEventLoop *loop;
  int timer_id1, timer_id2;
  int triggered = 0;

  ASSERT_EQ(0, http_loop_init(&loop));

  /* Add timer to be cancelled */
  ASSERT_EQ(0, http_loop_add_timer(loop, 10, timer_cb_cancel, &triggered,
                                   &timer_id1));
  ASSERT_EQ(0, http_loop_cancel_timer(loop, timer_id1));

  /* Add stop timer */
  ASSERT_EQ(0, http_loop_add_timer(loop, 20, timer_cb_stop, NULL, &timer_id2));

  ASSERT_EQ(0, http_loop_run(loop));
  ASSERT_EQ(0, triggered); /* ensure cancel worked */

  http_loop_free(loop);
  PASS();
}

SUITE(event_loop_suite) {
  RUN_TEST(test_event_loop_init_free);
  RUN_TEST(test_event_loop_timer);
  RUN_TEST(test_event_loop_timer_cancel);
}

#endif
