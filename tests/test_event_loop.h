#ifndef TEST_EVENT_LOOP_H
#define TEST_EVENT_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
  (void)timer_id;
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
  (void)loop;
  (void)timer_id;
  *triggered = 1; /* Should not be hit */
}

static void timer_cb_stop(struct ModalityEventLoop *loop, int timer_id,
                          void *user_data) {
  (void)timer_id;
  (void)user_data;
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

static int mock_loop_add_fd(void *ctx, int fd, int events, http_loop_cb cb,
                            void *data) {
  (void)ctx;
  (void)fd;
  (void)events;
  (void)cb;
  (void)data;
  return 0;
}
static int mock_loop_mod_fd(void *ctx, int fd, int events) {
  (void)ctx;
  (void)fd;
  (void)events;
  return 0;
}
static int mock_loop_remove_fd(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return 0;
}
static void stop_loop_cb(struct ModalityEventLoop *loop, int timer_id,
                         void *user_data) {
  (void)timer_id;
  (void)user_data;
  http_loop_stop(loop);
}

static int mock_loop_add_timer(void *ctx, long timeout_ms, http_timer_cb cb,
                               void *data, int *timer_id) {
  (void)ctx;
  (void)timeout_ms;
  (void)cb;
  (void)data;
  *timer_id = 1;
  return 0;
}
static int mock_loop_cancel_timer(void *ctx, int timer_id) {
  (void)ctx;
  (void)timer_id;
  return 0;
}
static int mock_loop_wakeup(void *ctx) {
  (void)ctx;
  return 0;
}

TEST test_event_loop_external(void) {
  struct ModalityEventLoop *loop = NULL;
  struct HttpLoopHooks hooks;
  int timer_id = 0;

  memset(&hooks, 0, sizeof(hooks));
  hooks.add_fd = mock_loop_add_fd;
  hooks.mod_fd = mock_loop_mod_fd;
  hooks.remove_fd = mock_loop_remove_fd;
  hooks.add_timer = mock_loop_add_timer;
  hooks.cancel_timer = mock_loop_cancel_timer;
  hooks.wakeup = mock_loop_wakeup;

  ASSERT_EQ(EINVAL, http_loop_init_external(NULL, NULL));
  ASSERT_EQ(0, http_loop_init_external(&loop, &hooks));

  ASSERT_EQ(ENOTSUP, http_loop_run(loop));
  ASSERT_EQ(0, http_loop_tick(loop));
  http_loop_stop(loop);

  ASSERT_EQ(0, http_loop_add_fd(loop, 0, 1, NULL, NULL));
  ASSERT_EQ(0, http_loop_mod_fd(loop, 0, 2));
  ASSERT_EQ(0, http_loop_remove_fd(loop, 0));

  ASSERT_EQ(0, http_loop_add_timer(loop, 10, stop_loop_cb, NULL, &timer_id));
  ASSERT_EQ(0, http_loop_cancel_timer(loop, timer_id));

  ASSERT_EQ(0, http_loop_wakeup(loop));

  http_loop_free(loop);
  PASS();
}

static void mock_fd_cb(struct ModalityEventLoop *loop, int fd, int revents,
                       void *user_data) {
  int *triggered = (int *)user_data;
  (void)loop;
  (void)fd;
  (void)revents;
  *triggered = 1;
}

TEST test_event_loop_run(void) {
  struct ModalityEventLoop *loop = NULL;
  int timer_id = 0;
  ASSERT_EQ(0, http_loop_init(&loop));

  ASSERT_EQ(0, http_loop_add_timer(loop, 10, stop_loop_cb, NULL, &timer_id));
  ASSERT_EQ(0, http_loop_run(loop));

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_tick_fd(void) {
  struct ModalityEventLoop *loop = NULL;
  int triggered = 0;

  ASSERT_EQ(0, http_loop_init(&loop));

  /* Just test wakeup and tick */
  http_loop_wakeup(loop);

  /* Tick should process the wakeup pipe without blocking */
  ASSERT_EQ(0, http_loop_tick(loop));

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_fd(void) {
  struct ModalityEventLoop *loop = NULL;
  int triggered = 0;

  ASSERT_EQ(0, http_loop_init(&loop));

  http_loop_add_fd(loop, 0, 1, mock_fd_cb, &triggered);
  http_loop_mod_fd(loop, 0, 2);
  http_loop_remove_fd(loop, 0);
  http_loop_wakeup(loop);

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_errors(void) {
  struct ModalityEventLoop *loop = NULL;
  ASSERT_EQ(EINVAL, http_loop_init(NULL));
  ASSERT_EQ(EINVAL, http_loop_run(NULL));
  ASSERT_EQ(EINVAL, http_loop_tick(NULL));
  ASSERT_EQ(EINVAL, http_loop_add_fd(NULL, 0, 0, NULL, NULL));
  ASSERT_EQ(EINVAL, http_loop_mod_fd(NULL, 0, 0));
  ASSERT_EQ(EINVAL, http_loop_remove_fd(NULL, 0));
  ASSERT_EQ(EINVAL, http_loop_add_timer(NULL, 0, NULL, NULL, NULL));
  ASSERT_EQ(EINVAL, http_loop_cancel_timer(NULL, 0));
  ASSERT_EQ(EINVAL, http_loop_wakeup(NULL));

  ASSERT_EQ(0, http_loop_init(&loop));
  ASSERT_EQ(EINVAL, http_loop_add_timer(loop, 10, NULL, NULL, NULL));
  ASSERT_EQ(EINVAL, http_loop_add_fd(loop, -1, 1, mock_fd_cb, NULL));
  ASSERT_EQ(EINVAL, http_loop_remove_fd(loop, -1));
  http_loop_free(loop);
  PASS();
}

static void timer_dummy_cb(struct ModalityEventLoop *loop, int timer_id,
                           void *user_data) {
  (void)loop;
  (void)timer_id;
  (void)user_data;
}

TEST test_event_loop_multiple_timers(void) {
  struct ModalityEventLoop *loop = NULL;
  int id1, id2, id3, id4;

  ASSERT_EQ(0, http_loop_init(&loop));

  /* Add timers out of order to trigger heap up */
  ASSERT_EQ(0, http_loop_add_timer(loop, 50, timer_dummy_cb, NULL, &id1));
  ASSERT_EQ(0, http_loop_add_timer(loop, 10, timer_dummy_cb, NULL, &id2));
  ASSERT_EQ(0, http_loop_add_timer(loop, 30, timer_dummy_cb, NULL, &id3));
  ASSERT_EQ(0, http_loop_add_timer(loop, 5, timer_dummy_cb, NULL, &id4));

  /* Cancel a timer to trigger heap logic */
  ASSERT_EQ(0, http_loop_cancel_timer(loop, id1));
  ASSERT_EQ(0, http_loop_cancel_timer(loop, id2));
  ASSERT_EQ(0, http_loop_cancel_timer(loop, id3));
  ASSERT_EQ(0, http_loop_cancel_timer(loop, id4));

  http_loop_free(loop);
  PASS();
}

SUITE(event_loop_suite) {
  RUN_TEST(test_event_loop_multiple_timers);
  RUN_TEST(test_event_loop_init_free);
  RUN_TEST(test_event_loop_timer);
  RUN_TEST(test_event_loop_timer_cancel);
  RUN_TEST(test_event_loop_external);
  RUN_TEST(test_event_loop_fd);
  RUN_TEST(test_event_loop_run);
  RUN_TEST(test_event_loop_tick_fd);
  RUN_TEST(test_event_loop_errors);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
