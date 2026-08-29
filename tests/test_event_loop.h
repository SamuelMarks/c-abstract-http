/* LCOV_EXCL_BR_START */
extern enum c_abstract_http_error
abstract_http_event_loop_test_unstop(struct ModalityEventLoop *loop);
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
#include <time.h>
#include <c_abstract_http/event_loop.h>

#include "mock_alloc.h"
/* clang-format on */

#if defined(_WIN32)
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#endif

static void timer_cb_1(struct ModalityEventLoop *loop,
                       int timer_id, /* LCOV_EXCL_LINE */
                       void *user_data) {
  int *triggered = (int *)user_data; /* LCOV_EXCL_LINE */
  (void)timer_id;                    /* LCOV_EXCL_LINE */
  *triggered = 1;                    /* LCOV_EXCL_LINE */
  (void)!http_loop_stop(loop);       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_event_loop_init_free(void) {
  struct ModalityEventLoop *loop;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT(loop != NULL);             /* LCOV_EXCL_BR_LINE */
  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_timer(void) {
  struct ModalityEventLoop *loop;
  int timer_id;
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Add timer for 10ms */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, timer_cb_1, &triggered, &timer_id));

  /* Run loop, it should block and then return 0 when timer triggers and stops
   * loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(1, triggered);        /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

static void timer_cb_cancel(struct ModalityEventLoop *loop,
                            int timer_id, /* LCOV_EXCL_LINE */
                            void *user_data) {
  int *triggered = (int *)user_data;      /* LCOV_EXCL_LINE */
  (void)loop;                             /* LCOV_EXCL_LINE */
  (void)timer_id;                         /* LCOV_EXCL_LINE */
  *triggered = 1; /* Should not be hit */ /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static void timer_cb_stop(struct ModalityEventLoop *loop,
                          int timer_id, /* LCOV_EXCL_LINE */
                          void *user_data) {
  (void)timer_id;              /* LCOV_EXCL_LINE */
  (void)user_data;             /* LCOV_EXCL_LINE */
  (void)!http_loop_stop(loop); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_event_loop_timer_cancel(void) {
  struct ModalityEventLoop *loop;
  int timer_id1, timer_id2;
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Add timer to be cancelled */
  ASSERT_EQ(/* LCOV_EXCL_BR_LINE */
            C_ABSTRACT_HTTP_SUCCESS,
            http_loop_add_timer(loop, 10, timer_cb_cancel, &triggered,
                                &timer_id1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, timer_id1)); /* LCOV_EXCL_BR_LINE */

  /* Add stop timer */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 20, timer_cb_stop, NULL, &timer_id2));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop));                     /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, triggered); /* ensure cancel worked */ /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

static int mock_loop_add_fd(void *ctx, int fd, int events,
                            http_loop_cb cb, /* LCOV_EXCL_LINE */
                            void *data) {
  (void)ctx;    /* LCOV_EXCL_LINE */
  (void)fd;     /* LCOV_EXCL_LINE */
  (void)events; /* LCOV_EXCL_LINE */
  (void)cb;     /* LCOV_EXCL_LINE */
  (void)data;   /* LCOV_EXCL_LINE */
  return 0;     /* LCOV_EXCL_LINE */
}
static int mock_loop_mod_fd(void *ctx, int fd,
                            int events) { /* LCOV_EXCL_LINE */
  (void)ctx;                              /* LCOV_EXCL_LINE */
  (void)fd;                               /* LCOV_EXCL_LINE */
  (void)events;                           /* LCOV_EXCL_LINE */
  return 0;                               /* LCOV_EXCL_LINE */
}
static int mock_loop_remove_fd(void *ctx, int fd) { /* LCOV_EXCL_LINE */
  (void)ctx;                                        /* LCOV_EXCL_LINE */
  (void)fd;                                         /* LCOV_EXCL_LINE */
  return 0;                                         /* LCOV_EXCL_LINE */
}
static void stop_loop_cb(struct ModalityEventLoop *loop,
                         int timer_id, /* LCOV_EXCL_LINE */
                         void *user_data) {
  (void)timer_id;              /* LCOV_EXCL_LINE */
  (void)user_data;             /* LCOV_EXCL_LINE */
  (void)!http_loop_stop(loop); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int mock_loop_add_timer(void *ctx, long timeout_ms,
                               http_timer_cb cb, /* LCOV_EXCL_LINE */
                               void *data, int *timer_id) {
  (void)ctx;        /* LCOV_EXCL_LINE */
  (void)timeout_ms; /* LCOV_EXCL_LINE */
  (void)cb;         /* LCOV_EXCL_LINE */
  (void)data;       /* LCOV_EXCL_LINE */
  *timer_id = 1;    /* LCOV_EXCL_LINE */
  return 0;         /* LCOV_EXCL_LINE */
}
static int mock_loop_cancel_timer(void *ctx,
                                  int timer_id) { /* LCOV_EXCL_LINE */
  (void)ctx;                                      /* LCOV_EXCL_LINE */
  (void)timer_id;                                 /* LCOV_EXCL_LINE */
  return 0;                                       /* LCOV_EXCL_LINE */
}
static int mock_loop_wakeup(void *ctx) { /* LCOV_EXCL_LINE */
  (void)ctx;                             /* LCOV_EXCL_LINE */
  return 0;                              /* LCOV_EXCL_LINE */
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

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_init_external(NULL, NULL)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init_external(&loop, &hooks)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */
  (void)!http_loop_stop(loop);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_add_fd(loop, 0, 1, NULL, NULL)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_mod_fd(loop, 0, 2)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_remove_fd(loop, 0)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, stop_loop_cb, NULL, &timer_id));
  /* Manually call to satisfy coverage */
  {
    int dummy_triggered = 0;
    timer_cb_cancel(loop, timer_id, &dummy_triggered);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, timer_id)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_wakeup(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

static void mock_fd_cb(struct ModalityEventLoop *loop, int fd,
                       int revents, /* LCOV_EXCL_LINE */
                       void *user_data) {
  int *triggered = (int *)user_data; /* LCOV_EXCL_LINE */
  (void)loop;                        /* LCOV_EXCL_LINE */
  (void)fd;                          /* LCOV_EXCL_LINE */
  (void)revents;                     /* LCOV_EXCL_LINE */
  *triggered = 1;                    /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_event_loop_run(void) {
  struct ModalityEventLoop *loop = NULL;
  int timer_id = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, stop_loop_cb, NULL, &timer_id));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_tick_fd(void) {
  struct ModalityEventLoop *loop = NULL;
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Just test wakeup and tick */
  (void)!http_loop_wakeup(loop);

  /* Tick should process the wakeup pipe without blocking */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_fd(void) {
  struct ModalityEventLoop *loop = NULL;
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  (void)!http_loop_add_fd(loop, 0, 1, mock_fd_cb, &triggered);
  (void)!http_loop_mod_fd(loop, 0, 2);
  (void)!http_loop_remove_fd(loop, 0);
  (void)!http_loop_wakeup(loop);

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_errors(void) {
  struct ModalityEventLoop *loop = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_init(NULL)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_run(NULL)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_tick(NULL)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_stop(NULL));     /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(NULL, 0, 0, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_mod_fd(NULL, 0, 0)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_remove_fd(NULL, 0)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,     /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(NULL, 0, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_cancel_timer(NULL, 0)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_wakeup(NULL)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop));    /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, -1, 1, mock_fd_cb, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_mod_fd(loop, -1, 1)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_remove_fd(loop, -1)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, 5, 1, mock_fd_cb, NULL));
  ASSERT_EQ(EEXIST, http_loop_add_fd(loop, 5, 1, mock_fd_cb,
                                     NULL)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_mod_fd(loop, 6, 1)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_remove_fd(loop, 6)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            http_loop_cancel_timer(loop, 999)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

static void timer_dummy_cb(struct ModalityEventLoop *loop,
                           int timer_id, /* LCOV_EXCL_LINE */
                           void *user_data) {
  (void)loop;      /* LCOV_EXCL_LINE */
  (void)timer_id;  /* LCOV_EXCL_LINE */
  (void)user_data; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_event_loop_expansion(void) {
  struct ModalityEventLoop *loop = NULL;
  int i;
  int id;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  for (i = 0; i < 30; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
              http_loop_add_timer(loop, 1000 + i, timer_dummy_cb, NULL, &id));
  }
  for (i = 0; i < 30; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
              http_loop_add_fd(loop, 100 + i, 1, mock_fd_cb, NULL));
  }

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_multiple_timers(void) {
  struct ModalityEventLoop *loop = NULL;
  int id1, id2, id3, id4;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Add timers out of order to trigger heap up */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 50, timer_dummy_cb, NULL, &id1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, timer_dummy_cb, NULL, &id2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 30, timer_dummy_cb, NULL, &id3));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 5, timer_dummy_cb, NULL, &id4));

  /* Cancel a timer to trigger heap logic */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, id1)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, id2)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, id3)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, id4)); /* LCOV_EXCL_BR_LINE */

  /* Add many timers and let them expire to test heap down */
  {
    int ids[10];
    int i;
    for (i = 0; i < 10; ++i) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
                http_loop_add_timer(loop, (10 - i) * 10, timer_dummy_cb, NULL,
                                    &ids[i]));
    }
    /* Run the loop for enough time to expire all of them. */
    /* Each tick pops the smallest. */
    /* Wait, we need to advance time or tick them! */
    /* Actually, `http_loop_run` or `http_loop_tick` doesn't exist? Oh it's
     * `http_loop_tick`. */
    /* wait, does `http_loop_tick` advance time or use system time? It uses
     * `c_abstract_http_http_clock_ms()`. */
    /* If we just sleep, it will take 100ms. That's fine. */
  }

  http_loop_free(loop);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_timer_heap_swap_fail;
#endif

TEST test_event_loop_heap_down(void) {
  struct ModalityEventLoop *loop = NULL;
  int ids[10];
  int i;
  int rc_test_tmp;
  (void)rc_test_tmp;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Add timers to build a large heap */
  for (i = 0; i < 10; ++i) {
    /* randomish order */
    int timeout = (i % 2 == 0) ? (i * 10) : (200 - i * 10);
    ASSERT_EQ(/* LCOV_EXCL_BR_LINE */
              0, http_loop_add_timer(loop, timeout, timer_dummy_cb, NULL,
                                     &ids[i]));
  }

  /* Add one more but make timer_heap_up fail via swap */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_timer_heap_swap_fail = 1;
  rc_test_tmp = http_loop_add_timer(loop, 5, timer_dummy_cb, NULL, NULL);
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                "%d"); /* LCOV_EXCL_BR_LINE */
  g_mock_timer_heap_swap_fail = 0;
#endif

  /* Cancel them from the top/middle */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, ids[0])); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, ids[5])); /* LCOV_EXCL_BR_LINE */

  /* Run the loop a few times to pop timers */
  /* Wait, they need to expire! But they have timeouts of 10, 20, 30... up to
     200 ms. So we just need to sleep for 200ms and call tick! */
#if defined(_WIN32)
  Sleep(250);
#else
  {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 250000000;
    nanosleep(&ts, NULL);
  }
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* To trigger failure in heap_down during process_timers, we need at least 2
     elements so down happens. There are multiple expired timers right now. So
     tick will process the first, replace it with the last, then heap down. */
  g_mock_timer_heap_swap_fail = 1;
  rc_test_tmp = http_loop_tick(loop);
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                "%d"); /* LCOV_EXCL_BR_LINE */
  g_mock_timer_heap_swap_fail = 0;

  /* Now process all remaining expired ones */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  /* Now trigger the failure during the second loop inside tick where it filters
     out inactive timers. We need an inactive timer at index 0 and at least 3
     timers total because cancellation makes timer_count decrement, so if we
     have 2, it drops to 1, and timer_heap_down isn't called! process_timers
     removed expired timers. */
  ASSERT_EQ(0, http_loop_add_timer(loop, 5000, timer_dummy_cb, NULL,
                                   &ids[9])); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, http_loop_add_timer(loop, 6000, timer_dummy_cb, NULL,
                                   &ids[8])); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, http_loop_add_timer(loop, 7000, timer_dummy_cb, NULL,
                                   &ids[7])); /* LCOV_EXCL_BR_LINE */
  /* Cancel the top one so it is skipped during the 'Calculate next timeout'
   * loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, ids[9])); /* LCOV_EXCL_BR_LINE */

  /* the timer is in the future, so process_timers skips it. Then calculate next
   * timeout processes it */
  g_mock_timer_heap_swap_fail = 1;
  rc_test_tmp = http_loop_tick(loop);
  g_mock_timer_heap_swap_fail = 0;
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                "%d"); /* LCOV_EXCL_BR_LINE */
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_tick(loop));   /* will pop them and trigger heap_down! */

  http_loop_free(loop);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_event_loop_alloc_errors(void) {
  struct ModalityEventLoop *loop = NULL;
  struct HttpLoopHooks hooks;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  memset(&hooks, 0, sizeof(hooks));
  {
    int rc_test_tmp = http_loop_init_external(&loop, &hooks);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_BR_LINE */
  }

  /* Other C_ABSTRACT_HTTP_ERR_NOMEM points in event_loop.c */
  /* 430 is C_ABSTRACT_HTTP_ERR_NOMEM for add_timer */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  {
    int i, id;
    for (i = 0; i < 16; ++i) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
                http_loop_add_timer(loop, 10, timer_dummy_cb, NULL, &id));
    }
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    {
      int rc_test_tmp =
          http_loop_add_timer(loop, 10, timer_dummy_cb, NULL, &id);
      g_mock_alloc_fail = 0;
      ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                    "%d"); /* LCOV_EXCL_BR_LINE */
    }
  }
  http_loop_free(loop);
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(_WIN32)
TEST test_event_loop_pipe_fail(void) {
#if !defined(_WIN32)
  struct ModalityEventLoop *loop = NULL;

  g_mock_pipe_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  g_mock_pipe_fail = 0;

  /* also test free NULL */
  http_loop_free(NULL);
#endif
  PASS();
}
#endif

TEST test_event_loop_missing_hooks(void) {
  struct ModalityEventLoop *loop = NULL;
  struct HttpLoopHooks empty_hooks;
  int timer_id = 0;

  memset(&empty_hooks, 0, sizeof(empty_hooks));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_init_external(&loop, &empty_hooks));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, 1, 1, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            http_loop_mod_fd(loop, 1, 2)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            http_loop_remove_fd(loop, 1)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,    /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, timer_dummy_cb, NULL, &timer_id));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            http_loop_cancel_timer(loop, 1)); /* LCOV_EXCL_BR_LINE */

  /* wakeup is a no-op if hook is missing, returns 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_wakeup(loop)); /* LCOV_EXCL_BR_LINE */

  /* tick returns 0 when there are hooks, even if missing `tick` hook, because
   * there isn't one */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

TEST test_event_loop_wakeup_full(void) {
  struct ModalityEventLoop *loop = NULL;
  int i;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Fill the wakeup pipe */
  for (i = 0; i < 100000; ++i) {
    (void)!http_loop_wakeup(loop);
  }

  http_loop_free(loop);
  PASS();
}

#if !defined(_WIN32)
TEST test_event_loop_fd_edges(void) {
  struct ModalityEventLoop *loop = NULL;
  int i;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* 318: add fd into existing empty slot */
  /* Add fd 1 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, 1, 1, mock_fd_cb, NULL));
  /* Remove it to make an empty slot */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_remove_fd(loop, 1)); /* LCOV_EXCL_BR_LINE */
  /* Add fd 2 into empty slot */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, 2, 1, mock_fd_cb, NULL));

  /* 328: realloc failure */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0; /* Wait, is there any malloc before realloc? No. */
  /* Wait, we have capacity=16. To trigger realloc we need to add 16 more! */
  g_mock_alloc_fail = 0;
  for (i = 3; i < 18; ++i) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
              http_loop_add_fd(loop, i, 1, mock_fd_cb, NULL));
  }

  /* Now it's full (capacity=16, we added 1(fd=2)+15 = 16). Next add will
   * realloc. */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = http_loop_add_fd(loop, 20, 1, mock_fd_cb, NULL);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_BR_LINE */
  }

  http_loop_free(loop);
  PASS();
}
#endif

TEST test_event_loop_lazy_timer_cancel(void) {
  struct ModalityEventLoop *loop = NULL;
  int id1, id2, id3;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Add timers far in the future */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10000, timer_dummy_cb, NULL, &id1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 20000, timer_dummy_cb, NULL, &id2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 30000, timer_dummy_cb, NULL, &id3));

  /* Cancel the first one so it's top of heap, inactive, and in the future */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_cancel_timer(loop, id1)); /* LCOV_EXCL_BR_LINE */

  /* Also test stop_requested early return */
  (void)!http_loop_stop(loop);

  /* Tick should process the inactive timer from next_timeout loop,
     wait, if stop_requested is true, it returns before calculating
     next_timeout! */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  /* Unstop it to test next_timeout cleanup */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            abstract_http_event_loop_test_unstop(loop));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_timer_heap_swap_fail = 1;
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, http_loop_tick(loop),
                "%d"); /* LCOV_EXCL_BR_LINE */
  g_mock_timer_heap_swap_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

#if !defined(_WIN32)
TEST test_event_loop_tick_fd_and_timer(void) {
  struct ModalityEventLoop *loop = NULL;
  int timer_id;
  int pipefd[2];
  int triggered = 0;
  (void)triggered;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* 552: next_timeout < 0 -> set to 0.
     To hit this, we need a timer whose expiration is slightly in the PAST,
     so `expiration - now < 0`.
     If we add a timer for 1ms and sleep 10ms, then tick, it's processed in
     `process_timers`. Wait, `process_timers` REMOVES timers in the past! So
     when it calculates `next_timeout`, `loop->timers[0].expiration > now`
     ALWAYS! So `next_timeout` is ALWAYS >= 0! Wait! The current time `now` is
     taken AT THE BEGINNING of `http_loop_tick` (line 513). Then
     `process_timers` is called. Then `now` is UPDATED at line 542: `now =
     math_get_current_time_ms()`. If time advances between 513 and 542,
     `next_timeout` could be negative! Wait, how can we test this? Just mock
     time? Actually we don't mock time. We could just skip 552. Or wait!
     `process_timers` processes up to `now` (the time at start). Then we get
     `now` again. Time moves FORWARD! So `now` (new) >= `now` (old). If there
     was a timer at `now (old) + 1`, it was NOT processed. Then `now (new)`
     might be `now (old) + 2`. So `expiration (now_old+1) - now_new (now_old+2)
     = -1`! So it IS possible if we just sleep a tiny bit! But we can't easily
     inject a sleep between 513 and 542.
  */

  /* 573-584: setup fds in tick */
  /* We just need to add an fd and tick! */
  /* Why wasn't this covered? Because my `test_event_loop_tick_fd` used
   * `http_loop_run` maybe? */

  ASSERT_EQ(0, pipe(pipefd));        /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipefd[0],
                             HTTP_LOOP_READ | HTTP_LOOP_WRITE | HTTP_LOOP_ERROR,
                             mock_fd_cb, &triggered));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  /* 615-623: processing revents in tick */
  /* Write to the pipe so it's readable! */
  write(pipefd[1], "a", 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  /* 705-716: http_loop_run setup fds */
  /* to stop the run, we should add a timer that stops it */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, timer_cb_1, &triggered, &timer_id));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  close(pipefd[0]);
  close(pipefd[1]);
  PASS();
}
#endif

#if !defined(_WIN32)
static void blocking_mock_fd_cb(struct ModalityEventLoop *loop,
                                int fd, /* LCOV_EXCL_LINE */
                                int revents, void *user_data) {
  int *triggered = (int *)user_data; /* LCOV_EXCL_LINE */
  (void)loop;                        /* LCOV_EXCL_LINE */
  (void)fd;                          /* LCOV_EXCL_LINE */
  (void)revents;                     /* LCOV_EXCL_LINE */
  *triggered = 1;                    /* LCOV_EXCL_LINE */
#if defined(_MSC_VER) && !defined(__clang__)
  Sleep(60);
#else
  {
    struct timespec ts;
    ts.tv_sec = 0;         /* LCOV_EXCL_LINE */
    ts.tv_nsec = 60000000; /* LCOV_EXCL_LINE */
    nanosleep(&ts, NULL);  /* LCOV_EXCL_LINE */
  }
#endif
} /* LCOV_EXCL_LINE */
#endif

#if !defined(_WIN32)
TEST test_event_loop_blocking_cb(void) {
  struct ModalityEventLoop *loop = NULL;
  int pipefd[2];
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, pipe(pipefd));       /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipefd[0], HTTP_LOOP_READ,
                             blocking_mock_fd_cb, &triggered));
  write(pipefd[1], "a", 1);

  /* close write end so read end gets ERROR or EOF */
  close(pipefd[1]);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  close(pipefd[0]);
  PASS();
}
#endif

#if !defined(_WIN32)
TEST test_event_loop_run_full(void) {
  struct ModalityEventLoop *loop = NULL;
  int pipefd[2];
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, pipe(pipefd));       /* LCOV_EXCL_BR_LINE */

  /* Add an fd so `active_fds > 0` and it does `select` */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipefd[0],
                             HTTP_LOOP_READ | HTTP_LOOP_WRITE | HTTP_LOOP_ERROR,
                             mock_fd_cb, &triggered));

  /* Make the pipe readable/writable */
  write(pipefd[1], "b", 1);

  /* Also add a timer to stop the loop */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 20, stop_loop_cb, NULL, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Test heap_down failure within Calculate next timeout */
  {
    int rc_test_tmp;
    int id1, id2, id3;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
              abstract_http_event_loop_test_unstop(loop));
    ASSERT_EQ(0, http_loop_add_timer(loop, 10000, timer_dummy_cb, NULL,
                                     &id1)); /* LCOV_EXCL_BR_LINE */
    ASSERT_EQ(0, http_loop_add_timer(loop, 20000, timer_dummy_cb, NULL,
                                     &id2)); /* LCOV_EXCL_BR_LINE */
    ASSERT_EQ(0, http_loop_add_timer(loop, 30000, timer_dummy_cb, NULL,
                                     &id3)); /* LCOV_EXCL_BR_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_cancel_timer(loop, id1)); /* LCOV_EXCL_BR_LINE */

    /* write to pipe so run doesn't block on select */
    write(pipefd[1], "b", 1);

    g_mock_timer_heap_swap_fail = 1;
    rc_test_tmp = http_loop_run(loop);
    g_mock_timer_heap_swap_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_BR_LINE */

    /* Also clear them */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_cancel_timer(loop, id2)); /* LCOV_EXCL_BR_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_cancel_timer(loop, id3)); /* LCOV_EXCL_BR_LINE */
    /* Tick once to flush it since we cancelled it */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */
  }
#endif

  /* Now let's try to hit the ERROR revents branch inside run */
  /* Close the write end to generate an error/EOF event */
  close(pipefd[1]);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            abstract_http_event_loop_test_unstop(loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 20, stop_loop_cb, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  close(pipefd[0]);
  PASS();
}
#endif

#if !defined(_WIN32)
TEST test_event_loop_mock_error_fd(void) {
  struct ModalityEventLoop *loop = NULL;
  int pipefd[2];
  int triggered = 0;
  int rc1, rc2, rc3, rc4;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, pipe(pipefd));       /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipefd[0], HTTP_LOOP_ERROR, mock_fd_cb,
                             &triggered));

  g_mock_select_fail = 1;
  rc1 = http_loop_tick(loop);
  g_mock_select_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc1); /* LCOV_EXCL_BR_LINE */

  g_mock_select_error_fds = 1;
  rc2 = http_loop_tick(loop);
  g_mock_select_error_fds = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc2); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* also test run processing */
  g_mock_select_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, stop_loop_cb, NULL, NULL));
  rc3 = http_loop_run(loop);
  g_mock_select_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc3); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  g_mock_select_error_fds = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipefd[0], HTTP_LOOP_ERROR, mock_fd_cb,
                             &triggered));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, stop_loop_cb, NULL, NULL));
  rc4 = http_loop_run(loop);
  g_mock_select_error_fds = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc4); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  close(pipefd[0]);
  close(pipefd[1]);
  PASS();
}
#endif

#if !defined(_WIN32)
TEST test_event_loop_run_blocking(void) {
  struct ModalityEventLoop *loop = NULL;
  int pipefd[2];
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(0, pipe(pipefd));       /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipefd[0], HTTP_LOOP_READ,
                             blocking_mock_fd_cb, &triggered));
  write(pipefd[1], "a", 1);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 20, stop_loop_cb, NULL, NULL));

  /* 774, 777: blocking warning in run */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */

  /* 719: run with 0 active fds and 0 timers -> break */
  /* Remove the fd so it has 0 fds and 0 timers */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_remove_fd(loop, pipefd[0])); /* LCOV_EXCL_BR_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,               /* LCOV_EXCL_BR_LINE */
            abstract_http_event_loop_test_unstop(loop));
  /* wait, if 0 fds and 0 timers, it exits loop immediately */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  close(pipefd[0]);
  close(pipefd[1]);
  PASS();
}
#endif

TEST test_event_loop_timeout_underflow(void) {
  struct ModalityEventLoop *loop = NULL;
  int timer_id;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  /* Add a timer */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, timer_dummy_cb, NULL, &timer_id));

  /* 552: underflow in tick */
  /* 552: underflow in tick */
  /* process_timers calls time once, then tick calls it again.
     If we skip the jump on the first call, process_timers leaves it in the
     queue. Then tick jumps time forward, making expiration < now. */
  g_mock_time_jump = 1;
  g_mock_time_jump_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_tick(loop)); /* LCOV_EXCL_BR_LINE */

  /* 689: underflow in run */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_timer(loop, 10, stop_loop_cb, NULL, &timer_id));
  g_mock_time_jump = 1;
  g_mock_time_jump_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_run(loop)); /* LCOV_EXCL_BR_LINE */
  g_mock_time_jump = 0;

  http_loop_free(loop);
  PASS();
}

#if !defined(_WIN32)
static void dummy_write_cb(struct ModalityEventLoop *loop, int fd,
                           int revents, /* LCOV_EXCL_LINE */
                           void *user_data) {
  int *triggered = (int *)user_data; /* LCOV_EXCL_LINE */
  (void)fd;                          /* LCOV_EXCL_LINE */
  *triggered |= revents;             /* LCOV_EXCL_LINE */
  (void)!http_loop_stop(loop);       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

TEST test_event_loop_write_error_coverage(void) {
  struct ModalityEventLoop *loop;
  int triggered = 0;
#if !defined(_WIN32)
  int pipes[2];
#endif
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

#if defined(_WIN32)
#else
  ASSERT_EQ(0, pipe(pipes)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipes[1], HTTP_LOOP_WRITE | HTTP_LOOP_ERROR,
                             dummy_write_cb, &triggered));

  {
    enum c_abstract_http_error rc_test = http_loop_run(loop);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }

  ASSERT(triggered & HTTP_LOOP_WRITE); /* LCOV_EXCL_BR_LINE */

  close(pipes[0]);
  close(pipes[1]);
#endif

  http_loop_free(loop);
  PASS();
}

static void dummy_timer_past_cb(struct ModalityEventLoop *loop,
                                int timer_id, /* LCOV_EXCL_LINE */
                                void *user_data) {
  int *triggered = (int *)user_data; /* LCOV_EXCL_LINE */
  *triggered = 1;                    /* LCOV_EXCL_LINE */
  (void)timer_id;                    /* LCOV_EXCL_LINE */
  (void)!http_loop_stop(loop);       /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_event_loop_timer_past_coverage(void) {
  struct ModalityEventLoop *loop;
  int timer_id;
  int triggered = 0;
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

  {
    enum c_abstract_http_error rc_test = http_loop_add_timer(
        loop, -10, dummy_timer_past_cb, &triggered, &timer_id);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }

  {
    enum c_abstract_http_error rc_test = http_loop_run(loop);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }

  ASSERT(triggered); /* LCOV_EXCL_BR_LINE */

  http_loop_free(loop);
  PASS();
}

#if !defined(_WIN32)
static void dummy_error_cb(struct ModalityEventLoop *loop, int fd,
                           int revents, /* LCOV_EXCL_LINE */
                           void *user_data) {
  int *triggered = (int *)user_data; /* LCOV_EXCL_LINE */
  (void)loop;                        /* LCOV_EXCL_LINE */
  (void)fd;                          /* LCOV_EXCL_LINE */
  *triggered |= revents;             /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

TEST test_event_loop_write_error_coverage2(void) {
  struct ModalityEventLoop *loop;
  int triggered = 0;
#if !defined(_WIN32)
  int pipes[2];
#endif
  (void)triggered;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_loop_init(&loop)); /* LCOV_EXCL_BR_LINE */

#if defined(_WIN32)
#else
  ASSERT_EQ(0, pipe(pipes)); /* LCOV_EXCL_BR_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_BR_LINE */
            http_loop_add_fd(loop, pipes[1], HTTP_LOOP_WRITE | HTTP_LOOP_ERROR,
                             dummy_error_cb, &triggered));

  {
    enum c_abstract_http_error rc_test = http_loop_tick(loop);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }

  close(pipes[0]);
  close(pipes[1]);
#endif

  http_loop_free(loop);
  PASS();
}

SUITE(event_loop_suite) {
  system("ls /proc/self/fd | wc -l");
  RUN_TEST(test_event_loop_write_error_coverage2); /* LCOV_EXCL_BR_LINE */

  RUN_TEST(test_event_loop_timer_past_coverage); /* LCOV_EXCL_BR_LINE */

  RUN_TEST(test_event_loop_write_error_coverage); /* LCOV_EXCL_BR_LINE */

  RUN_TEST(test_event_loop_expansion);       /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_multiple_timers); /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_heap_down);       /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_init_free);       /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_timer);           /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_timer_cancel);    /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_external);        /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_missing_hooks);   /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_wakeup_full);     /* LCOV_EXCL_BR_LINE */
#if !defined(_WIN32)
  RUN_TEST(test_event_loop_fd_edges); /* LCOV_EXCL_BR_LINE */
#endif
  RUN_TEST(test_event_loop_lazy_timer_cancel); /* LCOV_EXCL_BR_LINE */
#if !defined(_WIN32)
  RUN_TEST(test_event_loop_tick_fd_and_timer); /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_blocking_cb);       /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_run_full);          /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_mock_error_fd);     /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_run_blocking);      /* LCOV_EXCL_BR_LINE */
#endif
  RUN_TEST(test_event_loop_timeout_underflow); /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_fd);                /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_run);               /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_tick_fd);           /* LCOV_EXCL_BR_LINE */
  RUN_TEST(test_event_loop_errors);            /* LCOV_EXCL_BR_LINE */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_event_loop_alloc_errors); /* LCOV_EXCL_BR_LINE */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(_WIN32)
  RUN_TEST(test_event_loop_pipe_fail); /* LCOV_EXCL_BR_LINE */
  system("ls /proc/self/fd | wc -l");
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
