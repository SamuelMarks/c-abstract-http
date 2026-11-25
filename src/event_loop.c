
/**
 * @file event_loop.c
 * @brief Scalable native I/O event loop (epoll, kqueue, IOCP/WSAPoll, select),
 * timer min-heap, and external loop hook adapter.
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#ifndef _WIN32_WINNT
/** @brief Target Windows Vista+ API level */
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#if (!defined(_MSC_VER) || _MSC_VER >= 1500) && (_WIN32_WINNT >= 0x0600)
/** @brief Enable Windows IOCP + WSAPoll scalable backend */
#define CAH_HAVE_IOCP_WSAPOLL 1
#endif
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <dos.h>
#include <time.h>
#else
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <fcntl.h>
#if defined(__linux__)
/** @brief Enable Linux epoll O(1) backend */
#define CAH_HAVE_EPOLL 1
#include <sys/epoll.h>
#elif (defined(__APPLE__) && defined(__MACH__)) || defined(__FreeBSD__) ||     \
    defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
/** @brief Enable Darwin / BSD kqueue O(1) backend */
#define CAH_HAVE_KQUEUE 1
#include <sys/event.h>
#endif
#endif

#include <c_abstract_http/event_loop.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#ifdef _WIN32
#ifdef _MSC_VER
/** @brief Warning-free do-while(0) terminator under MSVC /W4 */
#define CAH_FD_SET_WHILE_ZERO while (0, 0)
#else
/** @brief Standard do-while(0) terminator */
#define CAH_FD_SET_WHILE_ZERO while (0)
#endif
#undef FD_SET
/**
 * @brief Warning-safe Winsock FD_SET implementation avoiding C4127.
 * @param fd Socket descriptor to add.
 * @param set Target fd_set pointer.
 */
#define FD_SET(fd, set)                                                        \
  do {                                                                         \
    u_int __i;                                                                 \
    for (__i = 0; __i < ((fd_set FAR *)(set))->fd_count; __i++) {              \
      if (((fd_set FAR *)(set))->fd_array[__i] == (SOCKET)(fd)) {              \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
    if (__i == ((fd_set FAR *)(set))->fd_count) {                              \
      if (((fd_set FAR *)(set))->fd_count < FD_SETSIZE) {                      \
        ((fd_set FAR *)(set))->fd_array[__i] = (SOCKET)(fd);                   \
        ((fd_set FAR *)(set))->fd_count++;                                     \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  CAH_FD_SET_WHILE_ZERO
/**
 * @brief Portable wrapper around FD_SET.
 * @param d Socket or file descriptor.
 * @param s Pointer to fd_set.
 */
#define ABSTRACT_HTTP_FD_SET(d, s) FD_SET(d, s)
/**
 * @brief Portable wrapper around FD_ISSET.
 * @param d Socket or file descriptor.
 * @param s Pointer to fd_set.
 * @return Non-zero if descriptor is set.
 */
#define ABSTRACT_HTTP_FD_ISSET(d, s) FD_ISSET(d, s)
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No POSIX fd_set on DOS */
#else
/**
 * @brief Portable wrapper around POSIX FD_SET with unsigned cast.
 * @param d File descriptor.
 * @param s Pointer to fd_set.
 */
#define ABSTRACT_HTTP_FD_SET(d, s) FD_SET((size_t)(d), (s))
/**
 * @brief Portable wrapper around POSIX FD_ISSET with unsigned cast.
 * @param d File descriptor.
 * @param s Pointer to fd_set.
 * @return Non-zero if descriptor is set.
 */
#define ABSTRACT_HTTP_FD_ISSET(d, s) FD_ISSET((size_t)(d), (s))
#endif

/** @brief Internal struct TimerNode */
struct TimerNode {
  /** @brief expiration (variable) of struct TimerNode */
  abstract_http_int64_t expiration;
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
  /** @brief Ready event bitmask populated during poll cycle */
  int pending_revents;
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
  /** @brief Active I/O multiplexing backend */
  enum HttpLoopBackend backend;
  /** @brief Non-zero when a non-pollable fd forced fallback to `select()` */
  int fallback_to_select;
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
  /* Wakeup & Native Multiplexer Handles */
#if defined(_WIN32)
  /** @brief Win32 manual/auto-reset event handle for loop wakeup */
  HANDLE wakeup_event;
#if defined(CAH_HAVE_IOCP_WSAPOLL)
  /** @brief Win32 I/O Completion Port handle for scalable wakeup */
  HANDLE iocp_handle;
#endif
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  /** @brief DOS wakeup pending flag */
  int wakeup_pending;
#else
  /** @brief wakeup_pipe[2] (variable) of struct ModalityEventLoop */
  int wakeup_pipe[2];
#if defined(CAH_HAVE_EPOLL)
  /** @brief Linux epoll instance descriptor */
  int epoll_fd;
#endif
#if defined(CAH_HAVE_KQUEUE)
  /** @brief Darwin / BSD kqueue instance descriptor */
  int kq_fd;
#endif
#endif
};

#ifndef CAH_MATH_TIME_DECLARED
/** @brief Guard macro for math_get_current_time_ms declaration */
#define CAH_MATH_TIME_DECLARED 1
/** @brief Return current monotonic/wall time in milliseconds */
abstract_http_int64_t math_get_current_time_ms(void);
C_ABSTRACT_HTTP_API abstract_http_int64_t real_math_get_current_time_ms(void);
#ifndef math_get_current_time_ms
/** @brief Default mapping of math_get_current_time_ms to real implementation */
#define math_get_current_time_ms real_math_get_current_time_ms
#endif
#endif
C_ABSTRACT_HTTP_API abstract_http_int64_t real_math_get_current_time_ms(void) {

#if defined(_WIN32)
#if (defined(_MSC_VER) && _MSC_VER < 1600) ||                                  \
    (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600)
  return (abstract_http_int64_t)GetTickCount();
#else
  return (abstract_http_int64_t)GetTickCount64();
#endif
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  return ((abstract_http_int64_t)clock() * 1000) /
         (abstract_http_int64_t)CLOCKS_PER_SEC;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (abstract_http_int64_t)tv.tv_sec * 1000 +
         (abstract_http_int64_t)tv.tv_usec / 1000;
#endif
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection countdown for timer heap swap failure tests */
extern int g_mock_timer_heap_swap_fail;
#endif

/**
 * @brief Swap two timer min-heap nodes.
 * @param a First timer node.
 * @param b Second timer node.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error enum.
 */
NO_DISCARD static c_abstract_http_error_t timer_heap_swap(struct TimerNode *a,
                                                          struct TimerNode *b) {
  struct TimerNode temp = *a;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_timer_heap_swap_fail) {
    g_mock_timer_heap_swap_fail--;
    if (g_mock_timer_heap_swap_fail == 0)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  *a = *b;
  *b = temp;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Sift a timer node upward in the expiration min-heap.
 * @param loop Event loop owning the timer heap.
 * @param idx Index of inserted node.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD static c_abstract_http_error_t
timer_heap_up(struct ModalityEventLoop *loop, size_t idx) {
  c_abstract_http_error_t rc;
  while (idx > 0) {
    size_t parent = (idx - 1) / 2;
    if (loop->timers[idx].expiration >= loop->timers[parent].expiration) {
      break;
    }
    rc = timer_heap_swap(&loop->timers[idx], &loop->timers[parent]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    idx = parent;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Sift a timer node downward in the expiration min-heap.
 * @param loop Event loop owning the timer heap.
 * @param idx Index of node to sift down.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD static c_abstract_http_error_t
timer_heap_down(struct ModalityEventLoop *loop, size_t idx) {
  c_abstract_http_error_t rc;
  for (;;) {
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

    rc = timer_heap_swap(&loop->timers[idx], &loop->timers[smallest]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    idx = smallest;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection flag for select failure tests */
extern int g_mock_select_fail;
/** @brief Fault-injection flag for select error-fd readiness tests */
extern int g_mock_select_error_fds;
#if defined(CAH_HAVE_EPOLL) || defined(CAH_HAVE_KQUEUE) ||                     \
    defined(CAH_HAVE_IOCP_WSAPOLL)
/** @brief Fault-injection flag for native multiplexer init failure */
static int g_mock_native_mux_fail = 0;
#endif
/** @brief Fault-injection operation code for event-loop op failures */
static int g_mock_loop_op_fail = 0;
/** @brief Countdown before `g_mock_loop_op_fail` triggers */
static int g_mock_loop_op_countdown = 0;

/**
 * @brief Configure test-only native multiplexer (`epoll`/`kqueue`/`IOCP`)
 * creation failure.
 * @param v Non-zero to force native multiplexer init failure.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_test_set_native_mux_fail(int v);

NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_test_set_native_mux_fail(int v) {
#if defined(CAH_HAVE_EPOLL) || defined(CAH_HAVE_KQUEUE) ||                     \
    defined(CAH_HAVE_IOCP_WSAPOLL)
  g_mock_native_mux_fail = v;
#else
  (void)v;
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Configure one-shot event-loop operation fault injection for tests.
 * @param op Operation code (0=off, 1=free, 2=unstop, 3=wakeup, 4=add_timer,
 * 5=run, 6=cancel_timer, 7=remove_fd).
 * @param countdown Number of successful calls before triggering (`0` = next).
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_test_set_loop_op_fail(int op, int countdown);

NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_test_set_loop_op_fail(int op, int countdown) {
  g_mock_loop_op_fail = op;
  g_mock_loop_op_countdown = countdown;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Check whether a one-shot event-loop fault-injection op should fire.
 * @param op Operation code to match.
 * @return 1 if fault should fire on this call, 0 otherwise.
 */
static int math_loop_should_fail_op(int op) {
  if (g_mock_loop_op_fail != op) {
    return 0;
  }
  if (g_mock_loop_op_countdown > 0) {
    g_mock_loop_op_countdown--;
    return 0;
  }
  g_mock_loop_op_fail = 0;
  return 1;
}
#endif

/**
 * @brief Pure helper resolving `HTTP_LOOP_BACKEND_AUTO` to host-optimal
 * backend.
 * @param requested Caller-requested backend enum.
 * @return Concrete `enum HttpLoopBackend`, or `(enum HttpLoopBackend)-1` if
 * invalid, or `(enum HttpLoopBackend)-2` if unsupported on this OS.
 */
static int math_resolve_loop_backend(enum HttpLoopBackend requested) {
  if (requested == HTTP_LOOP_BACKEND_AUTO) {
#if defined(CAH_HAVE_EPOLL)
    return (int)HTTP_LOOP_BACKEND_EPOLL;
#elif defined(CAH_HAVE_KQUEUE)
    return (int)HTTP_LOOP_BACKEND_KQUEUE;
#elif defined(CAH_HAVE_IOCP_WSAPOLL)
    return (int)HTTP_LOOP_BACKEND_IOCP;
#else
    return (int)HTTP_LOOP_BACKEND_SELECT;
#endif
  }
  if (requested == HTTP_LOOP_BACKEND_SELECT) {
    return (int)HTTP_LOOP_BACKEND_SELECT;
  }
  if (requested == HTTP_LOOP_BACKEND_EPOLL) {
#if defined(CAH_HAVE_EPOLL)
    return (int)HTTP_LOOP_BACKEND_EPOLL;
#else
    return -2;
#endif
  }
  if (requested == HTTP_LOOP_BACKEND_KQUEUE) {
#if defined(CAH_HAVE_KQUEUE)
    return (int)HTTP_LOOP_BACKEND_KQUEUE;
#else
    return -2;
#endif
  }
  if (requested == HTTP_LOOP_BACKEND_IOCP) {
#if defined(CAH_HAVE_IOCP_WSAPOLL)
    return (int)HTTP_LOOP_BACKEND_IOCP;
#else
    return -2;
#endif
  }
  return -1;
}

#if defined(CAH_HAVE_EPOLL)
/**
 * @brief Synchronize a descriptor's watched mask with the Linux `epoll`
 * instance.
 * @param loop Event loop context.
 * @param op `EPOLL_CTL_ADD`, `EPOLL_CTL_MOD`, or `EPOLL_CTL_DEL`.
 * @param fd Descriptor to update.
 * @param events `HttpLoopEvent` bitmask.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
epoll_sync_fd(struct ModalityEventLoop *loop, int op, int fd, int events) {
  struct epoll_event ev;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(9)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  if (loop->backend != HTTP_LOOP_BACKEND_EPOLL || loop->epoll_fd < 0 ||
      loop->fallback_to_select) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  memset(&ev, 0, sizeof(ev));
  ev.data.fd = fd;
  if (events & HTTP_LOOP_READ)
    ev.events |= EPOLLIN;
  if (events & HTTP_LOOP_WRITE)
    ev.events |= EPOLLOUT;
  if (events & HTTP_LOOP_ERROR)
    ev.events |= (EPOLLERR | EPOLLHUP);
  if (epoll_ctl(loop->epoll_fd, op, fd, op == EPOLL_CTL_DEL ? NULL : &ev) < 0) {
    if (op != EPOLL_CTL_DEL) {
      loop->fallback_to_select = 1;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

#if defined(CAH_HAVE_KQUEUE)
/**
 * @brief Synchronize a descriptor's watched mask with the Darwin/BSD `kqueue`
 * instance.
 * @param loop Event loop context.
 * @param fd Descriptor to update.
 * @param old_events Previous `HttpLoopEvent` bitmask (`0` on add).
 * @param new_events New `HttpLoopEvent` bitmask (`0` on remove).
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
kqueue_sync_fd(struct ModalityEventLoop *loop, int fd, int old_events,
               int new_events) {
  struct kevent changes[2];
  struct timespec zero_ts;
  int n = 0;
  int old_r = (old_events & (HTTP_LOOP_READ | HTTP_LOOP_ERROR)) ? 1 : 0;
  int new_r = (new_events & (HTTP_LOOP_READ | HTTP_LOOP_ERROR)) ? 1 : 0;
  int old_w = (old_events & HTTP_LOOP_WRITE) ? 1 : 0;
  int new_w = (new_events & HTTP_LOOP_WRITE) ? 1 : 0;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(9)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  if (loop->backend != HTTP_LOOP_BACKEND_KQUEUE || loop->kq_fd < 0 ||
      loop->fallback_to_select) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (new_r && !old_r) {
    EV_SET(&changes[n], (uintptr_t)fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
           NULL);
    n++;
  } else if (!new_r && old_r) {
    EV_SET(&changes[n], (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    n++;
  }
  if (new_w && !old_w) {
    EV_SET(&changes[n], (uintptr_t)fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
           NULL);
    n++;
  } else if (!new_w && old_w) {
    EV_SET(&changes[n], (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    n++;
  }
  if (n > 0) {
    zero_ts.tv_sec = 0;
    zero_ts.tv_nsec = 0;
    if (kevent(loop->kq_fd, changes, n, NULL, 0, &zero_ts) < 0 &&
        new_events != 0) {
      loop->fallback_to_select = 1;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

NO_DISCARD c_abstract_http_error_t http_loop_init_external(
    struct ModalityEventLoop **loop, const struct HttpLoopHooks *hooks) {
  struct ModalityEventLoop *l;
  if (!loop || !hooks) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  l = (struct ModalityEventLoop *)malloc(sizeof(struct ModalityEventLoop));
  if (!l) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(l, 0, sizeof(struct ModalityEventLoop));

  l->has_hooks = 1;
  l->hooks = *hooks;
  l->backend = HTTP_LOOP_BACKEND_SELECT;

  *loop = l;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Query active I/O backend of an event loop */
NO_DISCARD c_abstract_http_error_t http_loop_get_backend(
    const struct ModalityEventLoop *loop, enum HttpLoopBackend *out_backend) {
  if (!loop || !out_backend) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_backend =
      loop->fallback_to_select ? HTTP_LOOP_BACKEND_SELECT : loop->backend;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize a new event loop instance with an explicit backend */
NO_DISCARD c_abstract_http_error_t http_loop_init_with_backend(
    struct ModalityEventLoop **loop, enum HttpLoopBackend backend) {
  struct ModalityEventLoop *l;
  int resolved;
  LOG_DEBUG("http_loop_init_with_backend: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_init_with_backend: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  resolved = math_resolve_loop_backend(backend);
  if (resolved == -1) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (resolved == -2) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  l = (struct ModalityEventLoop *)malloc(sizeof(struct ModalityEventLoop));
  if (!l) {
    LOG_DEBUG("http_loop_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(l, 0, sizeof(struct ModalityEventLoop));
  l->backend = (enum HttpLoopBackend)resolved;

#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
#if defined(CAH_HAVE_EPOLL)
  l->epoll_fd = -1;
#endif
#if defined(CAH_HAVE_KQUEUE)
  l->kq_fd = -1;
#endif
#endif

  l->timer_capacity = 16;
  l->timers =
      (struct TimerNode *)malloc(l->timer_capacity * sizeof(struct TimerNode));
  if (!l->timers) {
    LOG_DEBUG("http_loop_init: Error ENOMEM (timers array)");
    free(l);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  l->fd_capacity = 16;
  l->fds = (struct FdNode *)malloc(l->fd_capacity * sizeof(struct FdNode));
  if (!l->fds) {
    LOG_DEBUG("http_loop_init: Error ENOMEM (fds array)");
    free(l->timers);
    free(l);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(_WIN32)
  l->wakeup_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!l->wakeup_event) {
    LOG_DEBUG("http_loop_init: Error EIO (CreateEvent failed)");
    free(l->fds);
    free(l->timers);
    free(l);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#if defined(CAH_HAVE_IOCP_WSAPOLL)
  if (l->backend == HTTP_LOOP_BACKEND_IOCP) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_native_mux_fail) {
      l->iocp_handle = NULL;
    } else
#endif
    {
      l->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    }
    if (!l->iocp_handle) {
      CloseHandle(l->wakeup_event);
      free(l->fds);
      free(l->timers);
      free(l);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
#endif
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  l->wakeup_pending = 0;
#else
  if (pipe(l->wakeup_pipe) < 0) {
    LOG_DEBUG("http_loop_init: Error EIO (pipe failed)");
    free(l->fds);
    free(l->timers);
    free(l);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  fcntl(l->wakeup_pipe[0], F_SETFL, O_NONBLOCK);
  fcntl(l->wakeup_pipe[1], F_SETFL, O_NONBLOCK);
#if defined(CAH_HAVE_EPOLL)
  if (l->backend == HTTP_LOOP_BACKEND_EPOLL) {
    struct epoll_event ev;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_native_mux_fail) {
      l->epoll_fd = -1;
    } else
#endif
    {
#ifdef EPOLL_CLOEXEC
      l->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
#else
      l->epoll_fd = epoll_create(64);
#endif
    }
    if (l->epoll_fd < 0) {
      close(l->wakeup_pipe[0]);
      close(l->wakeup_pipe[1]);
      free(l->fds);
      free(l->timers);
      free(l);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = l->wakeup_pipe[0];
    epoll_ctl(l->epoll_fd, EPOLL_CTL_ADD, l->wakeup_pipe[0], &ev);
  }
#endif
#if defined(CAH_HAVE_KQUEUE)
  if (l->backend == HTTP_LOOP_BACKEND_KQUEUE) {
    struct kevent kev;
    struct timespec zts;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_native_mux_fail) {
      l->kq_fd = -1;
    } else
#endif
    {
      l->kq_fd = kqueue();
    }
    if (l->kq_fd < 0) {
      close(l->wakeup_pipe[0]);
      close(l->wakeup_pipe[1]);
      free(l->fds);
      free(l->timers);
      free(l);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    EV_SET(&kev, (uintptr_t)l->wakeup_pipe[0], EVFILT_READ, EV_ADD | EV_ENABLE,
           0, 0, NULL);
    zts.tv_sec = 0;
    zts.tv_nsec = 0;
    kevent(l->kq_fd, &kev, 1, NULL, 0, &zts);
  }
#endif
#endif

  *loop = l;
  LOG_DEBUG("http_loop_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize a new event loop instance */
NO_DISCARD c_abstract_http_error_t
http_loop_init(struct ModalityEventLoop **loop) {
  return http_loop_init_with_backend(loop, HTTP_LOOP_BACKEND_AUTO);
}

/** @brief Free an event loop instance */
NO_DISCARD c_abstract_http_error_t
http_loop_free(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_free: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_free: Exiting early (loop is NULL)");
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  if (!loop->has_hooks) {
#if defined(_WIN32)
#if defined(CAH_HAVE_IOCP_WSAPOLL)
    if (loop->iocp_handle) {
      CloseHandle(loop->iocp_handle);
    }
#endif
    if (loop->wakeup_event) {
      CloseHandle(loop->wakeup_event);
    }
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
    loop->wakeup_pending = 0;
#else
#if defined(CAH_HAVE_EPOLL)
    if (loop->epoll_fd >= 0) {
      close(loop->epoll_fd);
    }
#endif
#if defined(CAH_HAVE_KQUEUE)
    if (loop->kq_fd >= 0) {
      close(loop->kq_fd);
    }
#endif
    close(loop->wakeup_pipe[0]);
    close(loop->wakeup_pipe[1]);
#endif

    free(loop->timers);
    free(loop->fds);
  }
  free(loop);
  LOG_DEBUG("http_loop_free: Exiting");
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(1)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Wake up an event loop waiting for events */
NO_DISCARD c_abstract_http_error_t
http_loop_wakeup(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_wakeup: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_wakeup: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(3)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  if (loop->has_hooks) {
    if (loop->hooks.wakeup) {
      LOG_DEBUG("http_loop_wakeup: Hooking");
      return loop->hooks.wakeup(loop->hooks.external_context);
    }
    LOG_DEBUG("http_loop_wakeup: Hooking (No-op)");
    return C_ABSTRACT_HTTP_SUCCESS; /* No-op if not provided */
  }

#if defined(_WIN32)
  SetEvent(loop->wakeup_event);
#if defined(CAH_HAVE_IOCP_WSAPOLL)
  if (loop->backend == HTTP_LOOP_BACKEND_IOCP && loop->iocp_handle) {
    PostQueuedCompletionStatus(loop->iocp_handle, 0, 1, NULL);
  }
#endif
  LOG_DEBUG("http_loop_wakeup: Success (Windows)");
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  loop->wakeup_pending = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  {
    char c = 'w';
    if (write(loop->wakeup_pipe[1], &c, 1) < 0) {
      /* Ignore EAGAIN, pipe is full and wakeup is already pending */
    }
  }
  LOG_DEBUG("http_loop_wakeup: Success (POSIX)");
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/** @brief Add a file descriptor to the event loop */
NO_DISCARD c_abstract_http_error_t
http_loop_add_fd(struct ModalityEventLoop *loop, int fd, int events,
                 http_loop_cb cb, void *user_data) {
  size_t i;
  LOG_DEBUG("http_loop_add_fd: Entering");
  if (!loop || fd < 0) {
    LOG_DEBUG("http_loop_add_fd: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.add_fd) {
      LOG_DEBUG("http_loop_add_fd: Hooking");
      return loop->hooks.add_fd(loop->hooks.external_context, fd, events, cb,
                                user_data);
    }
    LOG_DEBUG("http_loop_add_fd: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].fd == fd) {
      LOG_DEBUG("http_loop_add_fd: Error EEXIST");
      return C_ABSTRACT_HTTP_ERR_EXIST;
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
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      loop->fds = new_arr;
      loop->fd_capacity = new_cap;
    }
    loop->fd_count++;
  }

  loop->fds[i].fd = fd;
  loop->fds[i].events = events;
  loop->fds[i].pending_revents = 0;
  loop->fds[i].cb = cb;
  loop->fds[i].user_data = user_data;
  loop->fds[i].active = 1;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(8)) {
    loop->fds[i].active = 0;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

#if defined(CAH_HAVE_EPOLL)
  return epoll_sync_fd(loop, EPOLL_CTL_ADD, fd, events);
#elif defined(CAH_HAVE_KQUEUE)
  return kqueue_sync_fd(loop, fd, 0, events);
#else
  LOG_DEBUG("http_loop_add_fd: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/** @brief Modify watched events for a file descriptor in the event loop */
NO_DISCARD c_abstract_http_error_t
http_loop_mod_fd(struct ModalityEventLoop *loop, int fd, int events) {
  size_t i;
  LOG_DEBUG("http_loop_mod_fd: Entering");
  if (!loop || fd < 0) {
    LOG_DEBUG("http_loop_mod_fd: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.mod_fd) {
      LOG_DEBUG("http_loop_mod_fd: Hooking");
      return loop->hooks.mod_fd(loop->hooks.external_context, fd, events);
    }
    LOG_DEBUG("http_loop_mod_fd: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].fd == fd) {
#if defined(CAH_HAVE_KQUEUE)
      int old_ev = loop->fds[i].events;
#endif
      loop->fds[i].events = events;
#if defined(CAH_HAVE_EPOLL)
      return epoll_sync_fd(loop, EPOLL_CTL_MOD, fd, events);
#elif defined(CAH_HAVE_KQUEUE)
      return kqueue_sync_fd(loop, fd, old_ev, events);
#else
      LOG_DEBUG("http_loop_mod_fd: Success");
      return C_ABSTRACT_HTTP_SUCCESS;
#endif
    }
  }
  LOG_DEBUG("http_loop_mod_fd: Error ENOENT");
  return C_ABSTRACT_HTTP_ERR_INVAL;
}

/** @brief Remove a file descriptor from the event loop */
NO_DISCARD c_abstract_http_error_t
http_loop_remove_fd(struct ModalityEventLoop *loop, int fd) {
  size_t i;
  LOG_DEBUG("http_loop_remove_fd: Entering");
  if (!loop || fd < 0) {
    LOG_DEBUG("http_loop_remove_fd: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.remove_fd) {
      LOG_DEBUG("http_loop_remove_fd: Hooking");
      return loop->hooks.remove_fd(loop->hooks.external_context, fd);
    }
    LOG_DEBUG("http_loop_remove_fd: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].fd == fd) {
#if defined(CAH_HAVE_KQUEUE)
      int old_ev = loop->fds[i].events;
#endif
      loop->fds[i].active = 0;
      loop->fds[i].pending_revents = 0;
#if defined(CAH_HAVE_EPOLL)
      {
        c_abstract_http_error_t src_rc =
            epoll_sync_fd(loop, EPOLL_CTL_DEL, fd, 0);
        if (src_rc != C_ABSTRACT_HTTP_SUCCESS) {
          return src_rc;
        }
      }
#elif defined(CAH_HAVE_KQUEUE)
      {
        c_abstract_http_error_t src_rc = kqueue_sync_fd(loop, fd, old_ev, 0);
        if (src_rc != C_ABSTRACT_HTTP_SUCCESS) {
          return src_rc;
        }
      }
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (math_loop_should_fail_op(7)) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
#endif
      LOG_DEBUG("http_loop_remove_fd: Success");
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  LOG_DEBUG("http_loop_remove_fd: Error ENOENT");
  return C_ABSTRACT_HTTP_ERR_INVAL;
}

NO_DISCARD c_abstract_http_error_t
http_loop_add_timer(struct ModalityEventLoop *loop, long timeout_ms,
                    http_timer_cb cb, void *user_data, int *out_timer_id) {
  c_abstract_http_error_t rc;
  int id;
  LOG_DEBUG("http_loop_add_timer: Entering");
  if (!loop || !cb) {
    LOG_DEBUG("http_loop_add_timer: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(4)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  if (loop->has_hooks) {
    if (loop->hooks.add_timer) {
      LOG_DEBUG("http_loop_add_timer: Hooking");
      return loop->hooks.add_timer(loop->hooks.external_context, timeout_ms, cb,
                                   user_data, out_timer_id);
    }
    LOG_DEBUG("http_loop_add_timer: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  if (loop->timer_count >= loop->timer_capacity) {
    size_t new_cap = loop->timer_capacity * 2;
    struct TimerNode *new_arr = (struct TimerNode *)realloc(
        loop->timers, new_cap * sizeof(struct TimerNode));
    if (!new_arr) {
      LOG_DEBUG("http_loop_add_timer: Error ENOMEM");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    loop->timers = new_arr;
    loop->timer_capacity = new_cap;
  }

  id = ++loop->next_timer_id;
  if (out_timer_id)
    *out_timer_id = id;

  loop->timers[loop->timer_count].expiration =
      math_get_current_time_ms() + timeout_ms;
  loop->timers[loop->timer_count].id = id;
  loop->timers[loop->timer_count].cb = cb;
  loop->timers[loop->timer_count].user_data = user_data;
  loop->timers[loop->timer_count].active = 1;

  rc = timer_heap_up(loop, loop->timer_count);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  loop->timer_count++;

  LOG_DEBUG("http_loop_add_timer: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_loop_cancel_timer(struct ModalityEventLoop *loop, int timer_id) {
  size_t i;
  LOG_DEBUG("http_loop_cancel_timer: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_cancel_timer: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop->has_hooks) {
    if (loop->hooks.cancel_timer) {
      LOG_DEBUG("http_loop_cancel_timer: Hooking");
      return loop->hooks.cancel_timer(loop->hooks.external_context, timer_id);
    }
    LOG_DEBUG("http_loop_cancel_timer: Error ENOTSUP (hook missing)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  for (i = 0; i < loop->timer_count; ++i) {
    if (loop->timers[i].id == timer_id && loop->timers[i].active) {
      loop->timers[i].active = 0;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (math_loop_should_fail_op(6)) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
#endif
      /* We lazily remove it when it reaches the top of the heap */
      LOG_DEBUG("http_loop_cancel_timer: Success");
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  LOG_DEBUG("http_loop_cancel_timer: Error ENOENT");
  return C_ABSTRACT_HTTP_ERR_INVAL;
}

/**
 * @brief Pop and fire all expired timers in the event loop min-heap.
 * @param loop Active event loop instance.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum percolated from callbacks.
 */
NO_DISCARD static c_abstract_http_error_t
process_timers(struct ModalityEventLoop *loop) {
  c_abstract_http_error_t rc;
  abstract_http_int64_t now;
  struct TimerNode fired;

  now = math_get_current_time_ms();

  while (loop->timer_count > 0) {
    if (loop->timers[0].expiration > now) {
      break; /* Min element is in the future */
    }

    fired = loop->timers[0];
    loop->timers[0].active = 0;
    fired.active = fired.active ? 1 : 0;

    /* Remove min element */
    if (loop->timer_count > 1) {
      loop->timers[0] = loop->timers[loop->timer_count - 1];
      rc = timer_heap_down(loop, 0);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        loop->timers[0] = fired;
        return rc;
      }
    }
    loop->timer_count--;

    if (fired.active) {
      c_abstract_http_error_t cb_rc = fired.cb(loop, fired.id, fired.user_data);
      if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return cb_rc;
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
/**
 * @brief Invoke callbacks for all active descriptors with non-zero
 * `pending_revents`.
 * @param loop Event loop context.
 * @return C_ABSTRACT_HTTP_SUCCESS or first callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
dispatch_pending_fd_callbacks(struct ModalityEventLoop *loop) {
  size_t i;
  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active && loop->fds[i].pending_revents != 0) {
      int revents = loop->fds[i].pending_revents;
      abstract_http_int64_t start_cb = math_get_current_time_ms();
      c_abstract_http_error_t cb_rc;
      loop->fds[i].pending_revents = 0;
      cb_rc = loop->fds[i].cb(loop, loop->fds[i].fd, revents,
                              loop->fds[i].user_data);
      if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return cb_rc;
      }
      if (math_get_current_time_ms() - start_cb > 50) {
        fprintf(stderr, "[WARN] ModalityEventLoop: Blocking CPU task "
                        "detected (callback took >50ms). This breaks "
                        "asynchronous concurrency!\n");
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Count active descriptors and clear `pending_revents`.
 * @param loop Event loop context.
 * @return Number of active descriptors.
 */
static int math_count_active_fds(struct ModalityEventLoop *loop) {
  size_t i;
  int count = 0;
  for (i = 0; i < loop->fd_count; ++i) {
    loop->fds[i].pending_revents = 0;
    if (loop->fds[i].active) {
      count++;
    }
  }
  return count;
}

/**
 * @brief Poll descriptors using portable `select()`.
 * @param loop Event loop context.
 * @param active_fds Number of active registered descriptors.
 * @param timeout_ms Timeout in milliseconds (`0` non-blocking, `<0` infinite).
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
poll_select_once(struct ModalityEventLoop *loop, int active_fds,
                 abstract_http_int64_t timeout_ms) {
  size_t i;
  int max_fd = -1;
  fd_set read_fds;
  fd_set write_fds;
  fd_set error_fds;
  struct timeval tv;
  struct timeval *ptv = NULL;
  int ret;

#if defined(_WIN32)
  if (active_fds == 0) {
    DWORD wait_ms = timeout_ms >= 0 ? (DWORD)timeout_ms : INFINITE;
    if (WaitForSingleObject(loop->wakeup_event, wait_ms) == WAIT_OBJECT_0) {
      ResetEvent(loop->wakeup_event);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif

  FD_ZERO(&read_fds);
  FD_ZERO(&write_fds);
  FD_ZERO(&error_fds);

#if !defined(_WIN32)
  ABSTRACT_HTTP_FD_SET(loop->wakeup_pipe[0], &read_fds);
  max_fd = loop->wakeup_pipe[0];
#endif

  if (active_fds > 0) {
    for (i = 0; i < loop->fd_count; ++i) {
      if (loop->fds[i].active) {
#if !defined(_WIN32)
        if (loop->fds[i].fd >= (int)FD_SETSIZE) {
          continue;
        }
#endif
        if (loop->fds[i].events & HTTP_LOOP_READ)
          ABSTRACT_HTTP_FD_SET(loop->fds[i].fd, &read_fds);
        if (loop->fds[i].events & HTTP_LOOP_WRITE)
          ABSTRACT_HTTP_FD_SET(loop->fds[i].fd, &write_fds);
        if (loop->fds[i].events & HTTP_LOOP_ERROR)
          ABSTRACT_HTTP_FD_SET(loop->fds[i].fd, &error_fds);
        if (loop->fds[i].fd > max_fd)
          max_fd = loop->fds[i].fd;
      }
    }
  }

  if (timeout_ms >= 0) {
#if defined(_WIN32)
    abstract_http_int64_t eff_ms =
        (timeout_ms > 50) ? (abstract_http_int64_t)50 : timeout_ms;
    tv.tv_sec = (long)(eff_ms / 1000);
    tv.tv_usec = (int)((eff_ms % 1000) * 1000);
#else
    tv.tv_sec = (long)(timeout_ms / 1000);
    tv.tv_usec = (int)((timeout_ms % 1000) * 1000);
#endif
    ptv = &tv;
  }
#if defined(_WIN32)
  else {
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    ptv = &tv;
  }
#endif

  ret = select(max_fd + 1, &read_fds, &write_fds, &error_fds, ptv);

#if defined(_WIN32)
  if (WaitForSingleObject(loop->wakeup_event, 0) == WAIT_OBJECT_0) {
    ResetEvent(loop->wakeup_event);
  }
#else
  if (ret > 0 && ABSTRACT_HTTP_FD_ISSET(loop->wakeup_pipe[0], &read_fds)) {
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
#if !defined(_WIN32)
        if (loop->fds[i].fd >= (int)FD_SETSIZE) {
          continue;
        }
#endif
        if (ABSTRACT_HTTP_FD_ISSET(loop->fds[i].fd, &read_fds))
          revents |= HTTP_LOOP_READ;
        if (ABSTRACT_HTTP_FD_ISSET(loop->fds[i].fd, &write_fds))
          revents |= HTTP_LOOP_WRITE;
        if (ABSTRACT_HTTP_FD_ISSET(loop->fds[i].fd, &error_fds))
          revents |= HTTP_LOOP_ERROR;
        loop->fds[i].pending_revents = revents;
      }
    }
    return dispatch_pending_fd_callbacks(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(CAH_HAVE_EPOLL)
/**
 * @brief Poll descriptors in O(1) using Linux `epoll_wait`.
 * @param loop Event loop context.
 * @param timeout_ms Timeout in milliseconds (`0` non-blocking, `<0` infinite).
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
poll_epoll_once(struct ModalityEventLoop *loop,
                abstract_http_int64_t timeout_ms) {
  struct epoll_event ev_batch[64];
  int wait_arg = timeout_ms < 0 ? -1 : (int)timeout_ms;
  int nev;
  int k;

  nev = epoll_wait(loop->epoll_fd, ev_batch, 64, wait_arg);
  if (nev <= 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  for (k = 0; k < nev; ++k) {
    int fd = ev_batch[k].data.fd;
    unsigned int mask = ev_batch[k].events;
    size_t i;
    if (fd == loop->wakeup_pipe[0]) {
      char buf[64];
      while (read(loop->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
      }
      continue;
    }
    for (i = 0; i < loop->fd_count; ++i) {
      if (loop->fds[i].active && loop->fds[i].fd == fd) {
        int revents = 0;
        if ((mask & (EPOLLIN | EPOLLHUP)) &&
            (loop->fds[i].events & HTTP_LOOP_READ)) {
          revents |= HTTP_LOOP_READ;
        }
        if ((mask & EPOLLOUT) && (loop->fds[i].events & HTTP_LOOP_WRITE)) {
          revents |= HTTP_LOOP_WRITE;
        }
        if ((mask & (EPOLLERR | EPOLLHUP)) &&
            (loop->fds[i].events & HTTP_LOOP_ERROR)) {
          revents |= HTTP_LOOP_ERROR;
        }
        loop->fds[i].pending_revents |= revents;
        break;
      }
    }
  }
  return dispatch_pending_fd_callbacks(loop);
}
#endif

#if defined(CAH_HAVE_KQUEUE)
/**
 * @brief Poll descriptors in O(1) using Darwin/BSD `kevent`.
 * @param loop Event loop context.
 * @param timeout_ms Timeout in milliseconds (`0` non-blocking, `<0` infinite).
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
poll_kqueue_once(struct ModalityEventLoop *loop,
                 abstract_http_int64_t timeout_ms) {
  struct kevent ev_batch[64];
  struct timespec ts;
  struct timespec *pts = NULL;
  int nev;
  int k;

  if (timeout_ms >= 0) {
    ts.tv_sec = (time_t)(timeout_ms / 1000);
    ts.tv_nsec = (long)((timeout_ms % 1000) * 1000000L);
    pts = &ts;
  }
  nev = kevent(loop->kq_fd, NULL, 0, ev_batch, 64, pts);
  if (nev <= 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  for (k = 0; k < nev; ++k) {
    int fd = (int)ev_batch[k].ident;
    size_t i;
    if (fd == loop->wakeup_pipe[0]) {
      char buf[64];
      while (read(loop->wakeup_pipe[0], buf, sizeof(buf)) > 0) {
      }
      continue;
    }
    for (i = 0; i < loop->fd_count; ++i) {
      if (loop->fds[i].active && loop->fds[i].fd == fd) {
        int revents = 0;
        if (ev_batch[k].filter == EVFILT_READ &&
            (loop->fds[i].events & HTTP_LOOP_READ)) {
          revents |= HTTP_LOOP_READ;
        }
        if (ev_batch[k].filter == EVFILT_WRITE &&
            (loop->fds[i].events & HTTP_LOOP_WRITE)) {
          revents |= HTTP_LOOP_WRITE;
        }
        if ((ev_batch[k].flags & (EV_EOF | EV_ERROR)) &&
            (loop->fds[i].events & HTTP_LOOP_ERROR)) {
          revents |= HTTP_LOOP_ERROR;
        }
        loop->fds[i].pending_revents |= revents;
        break;
      }
    }
  }
  return dispatch_pending_fd_callbacks(loop);
}
#endif

#if defined(CAH_HAVE_IOCP_WSAPOLL)
/**
 * @brief Poll sockets via scalable `WSAPoll` (>64 descriptors) and IOCP wakeup.
 * @param loop Event loop context.
 * @param active_fds Number of active registered sockets.
 * @param timeout_ms Timeout in milliseconds (`0` non-blocking, `<0` infinite).
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
poll_iocp_once(struct ModalityEventLoop *loop, int active_fds,
               abstract_http_int64_t timeout_ms) {
  DWORD bytes = 0;
  ULONG_PTR comp_key = 0;
  LPOVERLAPPED ov = NULL;
  WSAPOLLFD stack_pfds[64];
  WSAPOLLFD *pfds = stack_pfds;
  size_t idx_map_stack[64];
  size_t *idx_map = idx_map_stack;
  size_t n_poll = 0;
  size_t i;
  int slice_ms;
  int ret;

  if (active_fds == 0) {
    DWORD wait_ms = timeout_ms >= 0 ? (DWORD)timeout_ms : INFINITE;
    GetQueuedCompletionStatus(loop->iocp_handle, &bytes, &comp_key, &ov,
                              wait_ms);
    while (GetQueuedCompletionStatus(loop->iocp_handle, &bytes, &comp_key, &ov,
                                     0)) {
    }
    ResetEvent(loop->wakeup_event);
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  if ((size_t)active_fds > 64U) {
    pfds = (WSAPOLLFD *)malloc((size_t)active_fds * sizeof(WSAPOLLFD));
    if (!pfds) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    idx_map = (size_t *)malloc((size_t)active_fds * sizeof(size_t));
    if (!idx_map) {
      free(pfds);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }

  for (i = 0; i < loop->fd_count; ++i) {
    if (loop->fds[i].active) {
      short ev = 0;
      if (loop->fds[i].events & HTTP_LOOP_READ)
        ev |= POLLRDNORM;
      if (loop->fds[i].events & HTTP_LOOP_WRITE)
        ev |= POLLWRNORM;
      pfds[n_poll].fd = (SOCKET)loop->fds[i].fd;
      pfds[n_poll].events = ev;
      pfds[n_poll].revents = 0;
      idx_map[n_poll] = i;
      n_poll++;
    }
  }

  if (timeout_ms < 0 || timeout_ms > 50) {
    slice_ms = 50;
  } else {
    slice_ms = (int)timeout_ms;
  }

  ret = WSAPoll(pfds, (ULONG)n_poll, slice_ms);
  if (ret == 0 && n_poll > 0 && n_poll <= FD_SETSIZE) {
    fd_set err_set;
    struct timeval zero_tv;
    size_t p;
    FD_ZERO(&err_set);
    for (p = 0; p < n_poll; ++p) {
      FD_SET(pfds[p].fd, &err_set);
    }
    zero_tv.tv_sec = 0;
    zero_tv.tv_usec = 0;
    if (select(0, NULL, NULL, &err_set, &zero_tv) > 0) {
      for (p = 0; p < n_poll; ++p) {
        if (FD_ISSET(pfds[p].fd, &err_set)) {
          pfds[p].revents |= POLLERR;
          ret++;
        }
      }
    }
  }
  while (
      GetQueuedCompletionStatus(loop->iocp_handle, &bytes, &comp_key, &ov, 0)) {
  }
  if (WaitForSingleObject(loop->wakeup_event, 0) == WAIT_OBJECT_0) {
    ResetEvent(loop->wakeup_event);
  }

  if (ret > 0) {
    size_t p;
    for (p = 0; p < n_poll; ++p) {
      short rev = pfds[p].revents;
      if (rev != 0) {
        size_t slot = idx_map[p];
        int mapped = 0;
        if ((rev & (POLLRDNORM | POLLIN | POLLHUP)) &&
            (loop->fds[slot].events & HTTP_LOOP_READ)) {
          mapped |= HTTP_LOOP_READ;
        }
        if ((rev & (POLLWRNORM | POLLOUT)) &&
            (loop->fds[slot].events & HTTP_LOOP_WRITE)) {
          mapped |= HTTP_LOOP_WRITE;
        }
        if (rev & (POLLERR | POLLNVAL)) {
          mapped |= HTTP_LOOP_ERROR;
        }
        loop->fds[slot].pending_revents = mapped;
      }
    }
  }

  if (pfds != stack_pfds) {
    free(pfds);
    free(idx_map);
  }

  if (ret > 0) {
    return dispatch_pending_fd_callbacks(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Dispatch one I/O readiness poll cycle using the loop's active backend.
 * @param loop Event loop context.
 * @param active_fds Number of active registered descriptors.
 * @param timeout_ms Timeout in milliseconds (`0` non-blocking, `<0` infinite).
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
poll_fds_once(struct ModalityEventLoop *loop, int active_fds,
              abstract_http_int64_t timeout_ms) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_select_fail || g_mock_select_error_fds) {
    return poll_select_once(loop, active_fds, timeout_ms);
  }
#endif
  if (!loop->fallback_to_select) {
#if defined(CAH_HAVE_EPOLL)
    if (loop->backend == HTTP_LOOP_BACKEND_EPOLL && loop->epoll_fd >= 0) {
      return poll_epoll_once(loop, timeout_ms);
    }
#endif
#if defined(CAH_HAVE_KQUEUE)
    if (loop->backend == HTTP_LOOP_BACKEND_KQUEUE && loop->kq_fd >= 0) {
      return poll_kqueue_once(loop, timeout_ms);
    }
#endif
#if defined(CAH_HAVE_IOCP_WSAPOLL)
    if (loop->backend == HTTP_LOOP_BACKEND_IOCP && loop->iocp_handle != NULL) {
      return poll_iocp_once(loop, active_fds, timeout_ms);
    }
#endif
  }
  return poll_select_once(loop, active_fds, timeout_ms);
}
#endif

/** @brief Run a single non-blocking tick of the event loop */
NO_DISCARD c_abstract_http_error_t
http_loop_tick(struct ModalityEventLoop *loop) {
#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  c_abstract_http_error_t rc;
  if (!loop) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (loop->has_hooks) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  rc = process_timers(loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  loop->wakeup_pending = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  int active_fds;
  c_abstract_http_error_t rc;

  LOG_DEBUG("http_loop_tick: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_tick: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop->has_hooks) {
    LOG_DEBUG("http_loop_tick: Exiting (external loop)");
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  rc = process_timers(loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (loop->stop_requested) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  while (loop->timer_count > 0 && !loop->timers[0].active) {
    loop->timers[0] = loop->timers[loop->timer_count - 1];
    loop->timer_count--;
    if (loop->timer_count > 0) {
      rc = timer_heap_down(loop, 0);
      if (rc != C_ABSTRACT_HTTP_SUCCESS)
        return rc;
    }
  }

  active_fds = math_count_active_fds(loop);
  if (active_fds == 0 && loop->timer_count == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  rc = poll_fds_once(loop, active_fds, 0);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  LOG_DEBUG("http_loop_tick: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/** @brief Run the event loop until stopped or idle */
NO_DISCARD c_abstract_http_error_t
http_loop_run(struct ModalityEventLoop *loop) {
  LOG_DEBUG("http_loop_run: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_run: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (loop->has_hooks) {
    LOG_DEBUG("http_loop_run: Error ENOTSUP (external loop)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(5)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  loop->running = 1;
  loop->stop_requested = 0;

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  while (!loop->stop_requested) {
    c_abstract_http_error_t rc = process_timers(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      loop->running = 0;
      return rc;
    }
    if (loop->stop_requested) {
      break;
    }
    while (loop->timer_count > 0 && !loop->timers[0].active) {
      loop->timers[0] = loop->timers[loop->timer_count - 1];
      loop->timer_count--;
      if (loop->timer_count > 0) {
        rc = timer_heap_down(loop, 0);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          loop->running = 0;
          return rc;
        }
      }
    }
    if (loop->timer_count == 0) {
      break;
    }
    loop->wakeup_pending = 0;
  }
  loop->running = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  while (!loop->stop_requested) {
    abstract_http_int64_t now;
    abstract_http_int64_t next_timeout = -1;
    int active_fds;
    c_abstract_http_error_t rc;

    rc = process_timers(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      loop->running = 0;
      return rc;
    }

    if (loop->stop_requested)
      break;

    if (loop->timer_count > 0) {
      now = math_get_current_time_ms();
      while (loop->timer_count > 0 && !loop->timers[0].active) {
        loop->timers[0] = loop->timers[loop->timer_count - 1];
        loop->timer_count--;
        if (loop->timer_count > 0) {
          rc = timer_heap_down(loop, 0);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            loop->running = 0;
            return rc;
          }
        }
      }
      if (loop->timer_count > 0) {
        abstract_http_int64_t timer_timeout = loop->timers[0].expiration - now;
        if (timer_timeout < 0)
          timer_timeout = 0;
        next_timeout = timer_timeout;
      }
    }

    active_fds = math_count_active_fds(loop);
    if (active_fds == 0 && loop->timer_count == 0) {
      break;
    }

    rc = poll_fds_once(loop, active_fds, next_timeout);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      loop->running = 0;
      return rc;
    }
  }

  loop->running = 0;
  LOG_DEBUG("http_loop_run: Exiting cleanly");
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/** @brief Request the event loop to stop */
NO_DISCARD c_abstract_http_error_t
http_loop_stop(struct ModalityEventLoop *loop) {
  c_abstract_http_error_t rc;
  LOG_DEBUG("http_loop_stop: Entering");
  if (!loop) {
    LOG_DEBUG("http_loop_stop: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  loop->stop_requested = 1;
  rc = http_loop_wakeup(loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  LOG_DEBUG("http_loop_stop: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_event_loop_test_unstop(struct ModalityEventLoop *loop) {
  if (loop)
    loop->stop_requested = 0;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (math_loop_should_fail_op(2)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}
