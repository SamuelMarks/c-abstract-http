
/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <dos.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <fcntl.h>
#endif

#include <c_abstract_http/event_loop.h>
#include "c_abstract_http/log.h"
/* clang-format on */

/** @brief Internal struct TimerNode */
struct TimerNode {
  /** @brief expiration (variable) of struct TimerNode */
  cdd_int64_t expiration;
  /** @brief id (variable) of struct TimerNode */
  int id;
  /** @brief cb (variable) of struct TimerNode */
  http_timer_cb cb;
  /** @brief user_data (variable) of struct TimerNode */
  void *user_data;
  /** @brief active (variable) of struct TimerNode */
  int active;
};

/** @brief Internal struct FdNode */
struct FdNode {
  /** @brief fd (variable) of struct FdNode */
  int fd;
  /** @brief events (variable) of struct FdNode */
  int events;
  /** @brief cb (variable) of struct FdNode */
  http_loop_cb cb;
  /** @brief user_data (variable) of struct FdNode */
  void *user_data;
  /** @brief active (variable) of struct FdNode */
  int active;
};

/** @brief Internal struct ModalityEventLoop */
struct ModalityEventLoop {
  /** @brief running (variable) of struct ModalityEventLoop */
  int running;
  /** @brief stop_requested (variable) of struct ModalityEventLoop */
  int stop_requested;
  /* External Hooks (if any) */
  /** @brief has_hooks (variable) of struct ModalityEventLoop */
  int has_hooks;
  /** @brief hooks (variable) of struct ModalityEventLoop */
  struct HttpLoopHooks hooks;
  /* Timer Min-Heap */
  /** @brief timers (variable) of struct ModalityEventLoop */
  struct TimerNode *timers;
  /** @brief timer_count (variable) of struct ModalityEventLoop */
  size_t timer_count;
  /** @brief timer_capacity (variable) of struct ModalityEventLoop */
  size_t timer_capacity;
  /** @brief next_timer_id (variable) of struct ModalityEventLoop */
  int next_timer_id;
  /* FD Registry */
  /** @brief fds (variable) of struct ModalityEventLoop */
  struct FdNode *fds;
  /** @brief fd_count (variable) of struct ModalityEventLoop */
  size_t fd_count;
  /** @brief fd_capacity (variable) of struct ModalityEventLoop */
  size_t fd_capacity;
  /* Wakeup mechanism (Self-pipe trick or Windows Event) */
#if defined(_WIN32)
  HANDLE wakeup_event;
#else
  /** @brief wakeup_pipe[2] (variable) of struct ModalityEventLoop */
  int wakeup_pipe[2];
#endif
};

cdd_int64_t real_math_get_current_time_ms(void);
cdd_int64_t real_math_get_current_time_ms(void) {

#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER < 1600
  return (cdd_int64_t)GetTickCount();
#else
  return (cdd_int64_t)GetTickCount64();
#endif
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (cdd_int64_t)tv.tv_sec * 1000 + (cdd_int64_t)tv.tv_usec / 1000;
#endif
}

static void timer_heap_swap(struct TimerNode *a, struct TimerNode *b) {
  struct TimerNode temp = *a;
  *a = *b;
  *b = temp;
}

static void timer_heap_up(struct ModalityEventLoop *loop, size_t idx) {
  while (idx > 0) {
    size_t parent = (idx - 1) / 2;
    if (loop->timers[idx].expiration >= loop->timers[parent].expiration) {
      break;
    }
    timer_heap_swap(&loop->timers[idx], &loop->timers[parent]);
    idx = parent;
  }
}

static void timer_heap_down(struct ModalityEventLoop *loop, size_t idx) {
  while (1) {
    size_t left = 2 * idx + 1;
    size_t right = 2 * idx + 2;
    size_t smallest = idx;

    if (left < loop->timer_count &&
        loop->timers[left].expiration < loop->timers[smallest].expiration) {
      smallest = left;
    }
    if (right < loop->timer_count &&
        loop->timers[right].expiration < loop->timers[smallest].expiration) {
      smallest = right;
    }

    if (smallest == idx) {
      break;
    }

    timer_heap_swap(&loop->timers[idx], &loop->timers[smallest]);
    idx = smallest;
  }
}

int http_loop_init_external(struct ModalityEventLoop **loop,
                            const struct HttpLoopHooks *hooks) {
  struct ModalityEventLoop *l;
  if (!loop || !hooks) {
    return EINVAL;
  }

  l = (struct ModalityEventLoop *)malloc(sizeof(struct ModalityEventLoop));
  if (!l) {
    return ENOMEM;
  }
  memset(l, 0, sizeof(struct ModalityEventLoop));

  l->has_hooks = 1;
  l->hooks = *hooks;

  *loop = l;
  return 0;
}

int http_loop_init(struct ModalityEventLoop **loop) {
  struct ModalityEventLoop *l;
  LOG_DEBUG("http_loop_init: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_init: Error EINVAL");
    return EINVAL;
  }

  l = (struct ModalityEventLoop *)malloc(sizeof(struct ModalityEventLoop));
  if (!l) {
    LOG_DEBUG("http_loop_init: Error ENOMEM");
    return ENOMEM;
  }
  memset(l, 0, sizeof(struct ModalityEventLoop));

  l->timer_capacity = 16;
  l->timers =
      (struct TimerNode *)malloc(l->timer_capacity * sizeof(struct TimerNode));
  if (!l->timers) {
    LOG_DEBUG("http_loop_init: Error ENOMEM (timers array)");
    free(l);
    return ENOMEM;
  }

  l->fd_capacity = 16;
  l->fds = (struct FdNode *)malloc(l->fd_capacity * sizeof(struct FdNode));
  if (!l->fds) {
    LOG_DEBUG("http_loop_init: Error ENOMEM (fds array)");
    free(l->timers);
    free(l);
    return ENOMEM;
  }

#if defined(_WIN32)
  l->wakeup_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!l->wakeup_event) {
    LOG_DEBUG("http_loop_init: Error EIO (CreateEvent failed)");
    free(l->fds);
    free(l->timers);
    free(l);
    return EIO;
  }
#else
  if (pipe(l->wakeup_pipe) < 0) {
    LOG_DEBUG("http_loop_init: Error EIO (pipe failed)");
    free(l->fds);
    free(l->timers);
    free(l);
    return EIO;
  }
  fcntl(l->wakeup_pipe[0], F_SETFL, O_NONBLOCK);
  fcntl(l->wakeup_pipe[1], F_SETFL, O_NONBLOCK);
#endif

  *loop = l;
  LOG_DEBUG("http_loop_init: Success");
  return 0;
}

void http_loop_free(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_free: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_free: Exiting early (loop is NULL)");
    return;
  }

  if (!loop->has_hooks) {
#if defined(_WIN32)
    if (loop->wakeup_event) {
      CloseHandle(loop->wakeup_event);
    }
#else
    if (loop->wakeup_pipe[0] > 0)
      close(loop->wakeup_pipe[0]);
    if (loop->wakeup_pipe[1] > 0)
      close(loop->wakeup_pipe[1]);
#endif

    if (loop->timers)
      free(loop->timers);
    if (loop->fds)
      free(loop->fds);
  }
  free(loop);
  LOG_DEBUG("http_loop_free: Exiting");
}

int http_loop_wakeup(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_wakeup: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_wakeup: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.wakeup) {
      LOG_DEBUG("http_loop_wakeup: Hooking");
      return loop->hooks.wakeup(loop->hooks.external_context);
    }
    LOG_DEBUG("http_loop_wakeup: Hooking (No-op)");
    return 0; /* No-op if not provided */
  }

#if defined(_WIN32)
  SetEvent(loop->wakeup_event);
  LOG_DEBUG("http_loop_wakeup: Success (Windows)");
  return 0;
#else
  {
    char c = 'w';
    if (write(loop->wakeup_pipe[1], &c, 1) < 0) {
      /* Ignore EAGAIN, pipe is full and wakeup is already pending */
    }
  }
  LOG_DEBUG("http_loop_wakeup: Success (POSIX)");
  return 0;
#endif
}

int http_loop_add_fd(struct ModalityEventLoop *loop, int fd, int events,
                     http_loop_cb cb, void *user_data) {
  size_t i;
  LOG_DEBUG("http_loop_add_fd: Entering");
  if (!loop || fd < 0) {
    LOG_DEBUG("http_loop_add_fd: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.add_fd) {
      LOG_DEBUG("http_loop_add_fd: Hooking");
      return loop->hooks.add_fd(loop->hooks.external_context, fd, events, cb,
                                user_data);
    }
    LOG_DEBUG("http_loop_add_fd: Error ENOTSUP (hook missing)");
    return ENOTSUP;
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].fd == fd) {
      LOG_DEBUG("http_loop_add_fd: Error EEXIST");
      return EEXIST;
    }
  }

  /* Find empty slot or expand */
  for (i = 0; i < loop->fd_count; ++i) {
    if (!loop->fds[i].active)
      break;
  }

  if (i == loop->fd_count) {
    if (loop->fd_count >= loop->fd_capacity) {
      size_t new_cap = loop->fd_capacity * 2;
      struct FdNode *new_arr =
          (struct FdNode *)realloc(loop->fds, new_cap * sizeof(struct FdNode));
      if (!new_arr) {
        LOG_DEBUG("http_loop_add_fd: Error ENOMEM");
        return ENOMEM;
      }
      loop->fds = new_arr;
      loop->fd_capacity = new_cap;
    }
    loop->fd_count++;
  }

  loop->fds[i].fd = fd;
  loop->fds[i].events = events;
  loop->fds[i].cb = cb;
  loop->fds[i].user_data = user_data;
  loop->fds[i].active = 1;

  LOG_DEBUG("http_loop_add_fd: Success");
  return 0;
}

int http_loop_mod_fd(struct ModalityEventLoop *loop, int fd, int events) {
  size_t i;
  LOG_DEBUG("http_loop_mod_fd: Entering");
  if (!loop || fd < 0) {
    LOG_DEBUG("http_loop_mod_fd: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.mod_fd) {
      LOG_DEBUG("http_loop_mod_fd: Hooking");
      return loop->hooks.mod_fd(loop->hooks.external_context, fd, events);
    }
    LOG_DEBUG("http_loop_mod_fd: Error ENOTSUP (hook missing)");
    return ENOTSUP;
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].fd == fd) {
      loop->fds[i].events = events;
      LOG_DEBUG("http_loop_mod_fd: Success");
      return 0;
    }
  }
  LOG_DEBUG("http_loop_mod_fd: Error ENOENT");
  return ENOENT;
}

int http_loop_remove_fd(struct ModalityEventLoop *loop, int fd) {
  size_t i;
  LOG_DEBUG("http_loop_remove_fd: Entering");
  if (!loop || fd < 0) {
    LOG_DEBUG("http_loop_remove_fd: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.remove_fd) {
      LOG_DEBUG("http_loop_remove_fd: Hooking");
      return loop->hooks.remove_fd(loop->hooks.external_context, fd);
    }
    LOG_DEBUG("http_loop_remove_fd: Error ENOTSUP (hook missing)");
    return ENOTSUP;
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].fd == fd) {
      loop->fds[i].active = 0;
      LOG_DEBUG("http_loop_remove_fd: Success");
      return 0;
    }
  }
  LOG_DEBUG("http_loop_remove_fd: Error ENOENT");
  return ENOENT;
}

int http_loop_add_timer(struct ModalityEventLoop *loop, long timeout_ms,
                        http_timer_cb cb, void *user_data, int *out_timer_id) {
  int id;
  LOG_DEBUG("http_loop_add_timer: Entering");
  if (!loop || !cb) {
    LOG_DEBUG("http_loop_add_timer: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.add_timer) {
      LOG_DEBUG("http_loop_add_timer: Hooking");
      return loop->hooks.add_timer(loop->hooks.external_context, timeout_ms, cb,
                                   user_data, out_timer_id);
    }
    LOG_DEBUG("http_loop_add_timer: Error ENOTSUP (hook missing)");
    return ENOTSUP;
  }

  if (loop->timer_count >= loop->timer_capacity) {
    size_t new_cap = loop->timer_capacity * 2;
    struct TimerNode *new_arr = (struct TimerNode *)realloc(
        loop->timers, new_cap * sizeof(struct TimerNode));
    if (!new_arr) {
      LOG_DEBUG("http_loop_add_timer: Error ENOMEM");
      return ENOMEM;
    }
    loop->timers = new_arr;
    loop->timer_capacity = new_cap;
  }

  id = ++loop->next_timer_id;
  if (out_timer_id)
    *out_timer_id = id;

  loop->timers[loop->timer_count].expiration =
      real_math_get_current_time_ms() + timeout_ms;
  loop->timers[loop->timer_count].id = id;
  loop->timers[loop->timer_count].cb = cb;
  loop->timers[loop->timer_count].user_data = user_data;
  loop->timers[loop->timer_count].active = 1;

  timer_heap_up(loop, loop->timer_count);
  loop->timer_count++;

  LOG_DEBUG("http_loop_add_timer: Success");
  return 0;
}

int http_loop_cancel_timer(struct ModalityEventLoop *loop, int timer_id) {
  size_t i;
  LOG_DEBUG("http_loop_cancel_timer: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_cancel_timer: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.cancel_timer) {
      LOG_DEBUG("http_loop_cancel_timer: Hooking");
      return loop->hooks.cancel_timer(loop->hooks.external_context, timer_id);
    }
    LOG_DEBUG("http_loop_cancel_timer: Error ENOTSUP (hook missing)");
    return ENOTSUP;
  }

  for (i = 0; i < loop->timer_count; ++i) {
    if (loop->timers[i].id == timer_id && loop->timers[i].active) {
      loop->timers[i].active = 0;
      /* We lazily remove it when it reaches the top of the heap */
      LOG_DEBUG("http_loop_cancel_timer: Success");
      return 0;
    }
  }
  LOG_DEBUG("http_loop_cancel_timer: Error ENOENT");
  return ENOENT;
}

static void process_timers(struct ModalityEventLoop *loop) {
  cdd_int64_t now;

  now = real_math_get_current_time_ms();

  while (loop->timer_count > 0) {
    if (loop->timers[0].expiration > now) {
      break; /* Min element is in the future */
    }

    if (loop->timers[0].active) {
      loop->timers[0].active = 0;
      loop->timers[0].cb(loop, loop->timers[0].id, loop->timers[0].user_data);
    }

    /* Remove min element */
    loop->timers[0] = loop->timers[loop->timer_count - 1];
    loop->timer_count--;
    if (loop->timer_count > 0) {
      timer_heap_down(loop, 0);
    }
  }
}

int http_loop_tick(struct ModalityEventLoop *loop) {
  cdd_int64_t now;
  cdd_int64_t next_timeout = -1;
  size_t i;
  int active_fds = 0;
  int max_fd = -1;
  fd_set read_fds, write_fds, error_fds;
  struct timeval tv;
  struct timeval *ptv = NULL;
  int ret;

  LOG_DEBUG("http_loop_tick: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_tick: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    /* If hooked externally, ticketing should ideally be handled by the
     * framework */
    LOG_DEBUG("http_loop_tick: Exiting (external loop)");
    return 0;
  }

  /* Process expired timers first */
  process_timers(loop);

  if (loop->stop_requested) {
    return 0;
  }

  /* Calculate next timeout */
  if (loop->timer_count > 0) {
    now = real_math_get_current_time_ms();
    while (loop->timer_count > 0 && !loop->timers[0].active) {
      loop->timers[0] = loop->timers[loop->timer_count - 1];
      loop->timer_count--;
      if (loop->timer_count > 0)
        timer_heap_down(loop, 0);
    }
    if (loop->timer_count > 0) {
      next_timeout = loop->timers[0].expiration - now;
      if (next_timeout < 0)
        next_timeout = 0;
    }
  }

  /* tick should be non-blocking or very short blocking */
  tv.tv_sec = 0;
  tv.tv_usec = 0; /* Fully non-blocking for tick */
  ptv = &tv;

  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&error_fds);

  /* Setup wakeup pipe */
#if !defined(_WIN32)
  FD_SET(loop->wakeup_pipe[0], &read_fds);
  max_fd = loop->wakeup_pipe[0];
#endif

  /* Setup sockets */
  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active) {
      active_fds++;
      if (loop->fds[i].events & HTTP_LOOP_READ)
        FD_SET((unsigned)loop->fds[i].fd, &read_fds);
      if (loop->fds[i].events & HTTP_LOOP_WRITE)
        FD_SET((unsigned)loop->fds[i].fd, &write_fds);
      if (loop->fds[i].events & HTTP_LOOP_ERROR)
        FD_SET((unsigned)loop->fds[i].fd, &error_fds);
      if (loop->fds[i].fd > max_fd)
        max_fd = loop->fds[i].fd;
    }
  }

  if (active_fds == 0 && loop->timer_count == 0) {
    return 0; /* Nothing to do */
  }

#if defined(_WIN32)
  if (active_fds == 0) {
    if (WaitForSingleObject(loop->wakeup_event, 0) == WAIT_OBJECT_0) {
      ResetEvent(loop->wakeup_event);
    }
    return 0;
  }
#endif

  ret = select(max_fd + 1, &read_fds, &write_fds, &error_fds, ptv);

#if defined(_WIN32)
  if (WaitForSingleObject(loop->wakeup_event, 0) == WAIT_OBJECT_0) {
    ResetEvent(loop->wakeup_event);
  }
#else
  if (ret > 0 && FD_ISSET(loop->wakeup_pipe[0], &read_fds)) {
    char buf[64];
    while (read(loop->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
    }
    ret--;
  }
#endif

  if (ret > 0) {
    for (i = 0; i < loop->fd_count; ++i) {
      if (loop->fds[i].active) {
        int revents = 0;
        if (FD_ISSET(loop->fds[i].fd, &read_fds))
          revents |= HTTP_LOOP_READ;
        if (FD_ISSET(loop->fds[i].fd, &write_fds))
          revents |= HTTP_LOOP_WRITE;
        if (FD_ISSET(loop->fds[i].fd, &error_fds))
          revents |= HTTP_LOOP_ERROR;

        if (revents) {
          cdd_int64_t start_cb = real_math_get_current_time_ms();
          loop->fds[i].cb(loop, loop->fds[i].fd, revents,
                          loop->fds[i].user_data);
          if (real_math_get_current_time_ms() - start_cb > 50) {
            fprintf(stderr, "[WARN] ModalityEventLoop: Blocking CPU task "
                            "detected (callback took >50ms). This breaks "
                            "asynchronous concurrency!\n");
          }
        }
      }
    }
  }

  LOG_DEBUG("http_loop_tick: Success");
  return 0;
}

int http_loop_run(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_run: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_run: Error EINVAL");
    return EINVAL;
  }

  if (loop->has_hooks) {
    /* If we have hooks, running the loop is not our responsibility */
    LOG_DEBUG("http_loop_run: Error ENOTSUP (external loop)");
    return ENOTSUP;
  }

  loop->running = 1;
  loop->stop_requested = 0;

  while (loop->running && !loop->stop_requested) {
    cdd_int64_t now;
    cdd_int64_t next_timeout = -1;
    size_t i;
    int active_fds = 0;
    int max_fd = -1;
    fd_set read_fds, write_fds, error_fds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int ret;

    /* Process expired timers first */
    process_timers(loop);

    if (loop->stop_requested)
      break;

    /* Calculate next timeout */
    if (loop->timer_count > 0) {
      now = real_math_get_current_time_ms();
      while (loop->timer_count > 0 && !loop->timers[0].active) {
        loop->timers[0] = loop->timers[loop->timer_count - 1];
        loop->timer_count--;
        if (loop->timer_count > 0)
          timer_heap_down(loop, 0);
      }
      if (loop->timer_count > 0) {
        next_timeout = loop->timers[0].expiration - now;
        if (next_timeout < 0)
          next_timeout = 0;
      }
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);

    /* Setup wakeup pipe */
#if !defined(_WIN32)
    FD_SET(loop->wakeup_pipe[0], &read_fds);
    max_fd = loop->wakeup_pipe[0];
#endif

    /* Setup sockets */
    for (i = 0; i < loop->fd_count; ++i) {
      if (loop->fds[i].active) {
        active_fds++;
        if (loop->fds[i].events & HTTP_LOOP_READ)
          FD_SET((unsigned)loop->fds[i].fd, &read_fds);
        if (loop->fds[i].events & HTTP_LOOP_WRITE)
          FD_SET((unsigned)loop->fds[i].fd, &write_fds);
        if (loop->fds[i].events & HTTP_LOOP_ERROR)
          FD_SET((unsigned)loop->fds[i].fd, &error_fds);
        if (loop->fds[i].fd > max_fd)
          max_fd = loop->fds[i].fd;
      }
    }

    if (active_fds == 0 && loop->timer_count == 0) {
      break; /* Nothing to do */
    }

    if (next_timeout >= 0) {
      tv.tv_sec = (long)(next_timeout / 1000);
      tv.tv_usec = (long)((next_timeout % 1000) * 1000);
      ptv = &tv;
    }

#if defined(_WIN32)
    if (active_fds == 0) {
      DWORD wait_ms = next_timeout >= 0 ? (DWORD)next_timeout : INFINITE;
      WaitForSingleObject(loop->wakeup_event, wait_ms);
      ResetEvent(loop->wakeup_event);
      continue;
    }
#endif

    ret = select(max_fd + 1, &read_fds, &write_fds, &error_fds, ptv);

#if defined(_WIN32)
    if (next_timeout < 0 || next_timeout > 50) {
      tv.tv_sec = 0;
      tv.tv_usec = 50000;
      ptv = &tv;
      ret = select(max_fd + 1, &read_fds, &write_fds, &error_fds, ptv);
      if (WaitForSingleObject(loop->wakeup_event, 0) == WAIT_OBJECT_0) {
        ResetEvent(loop->wakeup_event);
      }
    }
#else
    if (ret > 0 && FD_ISSET(loop->wakeup_pipe[0], &read_fds)) {
      char buf[64];
      while (read(loop->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
      }
      ret--;
    }
#endif

    if (ret > 0) {
      for (i = 0; i < loop->fd_count; ++i) {
        if (loop->fds[i].active) {
          int revents = 0;
          if (FD_ISSET(loop->fds[i].fd, &read_fds))
            revents |= HTTP_LOOP_READ;
          if (FD_ISSET(loop->fds[i].fd, &write_fds))
            revents |= HTTP_LOOP_WRITE;
          if (FD_ISSET(loop->fds[i].fd, &error_fds))
            revents |= HTTP_LOOP_ERROR;

          if (revents) {
            cdd_int64_t start_cb = real_math_get_current_time_ms();
            loop->fds[i].cb(loop, loop->fds[i].fd, revents,
                            loop->fds[i].user_data);
            if (real_math_get_current_time_ms() - start_cb > 50) {
              fprintf(stderr, "[WARN] ModalityEventLoop: Blocking CPU task "
                              "detected (callback took >50ms). This breaks "
                              "asynchronous concurrency!\n");
            }
          }
        }
      }
    }
  }

  loop->running = 0;
  LOG_DEBUG("http_loop_run: Exiting cleanly");
  return 0;
}

int http_loop_stop(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_stop: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_stop: Error EINVAL");
    return EINVAL;
  }
  loop->stop_requested = 1;
  http_loop_wakeup(loop);
  LOG_DEBUG("http_loop_stop: Success");
  return 0;
}

#if 1
void cdd_event_loop_test_unstop(struct ModalityEventLoop *loop);
void cdd_event_loop_test_unstop(struct ModalityEventLoop *loop) {
  if (loop)
    loop->stop_requested = 0;
}
#endif
