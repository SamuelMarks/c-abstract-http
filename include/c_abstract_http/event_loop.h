/**
 * @file event_loop.h
 * @brief Platform-agnostic Asynchronous Event Loop API.
 *
 * Provides a `uv_run`-like loop, socket polling (select/epoll/IOCP fallback),
 * and a min-heap backed timer system for timeouts.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_EVENT_LOOP_H
#define C_CDD_HTTP_EVENT_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Opaque structure representing the event loop.
 */
struct ModalityEventLoop;

/**
 * @brief Event types for the polling mechanism.
 */
enum HttpLoopEvent {
  HTTP_LOOP_READ = 1,
  HTTP_LOOP_WRITE = 2,
  HTTP_LOOP_ERROR = 4
};

/**
 * @brief Callback for socket events.
 */
typedef void (*http_loop_cb)(struct ModalityEventLoop *loop, int fd, int events,
                             void *user_data);

/**
 * @brief Callback for timeout events.
 */
typedef void (*http_timer_cb)(struct ModalityEventLoop *loop, int timer_id,
                              void *user_data);

/**
 * @brief Hooks for integrating with an external framework's event loop (e.g.,
 * c-multiplatform).
 */
struct HttpLoopHooks {
  void *external_context;

  /* Polling Hooks */
  int (*add_fd)(void *ctx, int fd, int events, http_loop_cb cb,
                void *user_data);
  int (*mod_fd)(void *ctx, int fd, int events);
  int (*remove_fd)(void *ctx, int fd);

  /* Timer Hooks */
  int (*add_timer)(void *ctx, long timeout_ms, http_timer_cb cb,
                   void *user_data, int *out_timer_id);
  int (*cancel_timer)(void *ctx, int timer_id);

  /* Wakeup Hook */
  int (*wakeup)(void *ctx);
};

/**
 * @brief Initialize an event loop context.
 * @param[out] loop Pointer to receive the allocated loop.
 * @return 0 on success, ENOMEM on allocation failure.
 */
extern int http_loop_init(struct ModalityEventLoop **loop);

/**
 * @brief Initialize an event loop context that proxies to an external reactor.
 * @param[out] loop Pointer to receive the allocated loop.
 * @param[in] hooks The external hooks to use.
 * @return 0 on success, ENOMEM on allocation failure.
 */
extern int http_loop_init_external(struct ModalityEventLoop **loop,
                                   const struct HttpLoopHooks *hooks);

/**
 * @brief Free an event loop context.
 * @param[in] loop The event loop context.
 */
extern void http_loop_free(struct ModalityEventLoop *loop);

/**
 * @brief Run the event loop. Mimics Node's uv_run.
 * Blocks until no more active handles/timers exist or stop is called.
 * @param[in] loop The event loop context.
 * @return 0 on success.
 */
extern int http_loop_run(struct ModalityEventLoop *loop);

/**
 * @brief Run a single iteration of the event loop.
 * Non-blocking by default. Useful for hooking into a framework's idle phase
 * (e.g. cmp_run_loop).
 * @param[in] loop The event loop context.
 * @return 0 on success, non-zero on failure.
 */
extern int http_loop_tick(struct ModalityEventLoop *loop);

/**
 * @brief Stop the event loop from running further.
 * @param[in] loop The event loop context.
 * @return 0 on success.
 */
extern int http_loop_stop(struct ModalityEventLoop *loop);

/**
 * @brief Register a file descriptor/socket for polling.
 * @param[in] loop Event loop context.
 * @param[in] fd File descriptor to poll.
 * @param[in] events Bitmask of HttpLoopEvent.
 * @param[in] cb Callback to trigger.
 * @param[in] user_data User context for callback.
 * @return 0 on success, ENOMEM or EINVAL on error.
 */
extern int http_loop_add_fd(struct ModalityEventLoop *loop, int fd, int events,
                            http_loop_cb cb, void *user_data);

/**
 * @brief Modify the polled events for a file descriptor.
 * @param[in] loop Event loop context.
 * @param[in] fd File descriptor.
 * @param[in] events New bitmask.
 * @return 0 on success, ENOENT if not found.
 */
extern int http_loop_mod_fd(struct ModalityEventLoop *loop, int fd, int events);

/**
 * @brief Remove a file descriptor from polling.
 * @param[in] loop Event loop context.
 * @param[in] fd File descriptor.
 * @return 0 on success, ENOENT if not found.
 */
extern int http_loop_remove_fd(struct ModalityEventLoop *loop, int fd);

/**
 * @brief Register a timeout using the internal min-heap.
 * @param[in] loop Event loop context.
 * @param[in] timeout_ms Milliseconds until timeout.
 * @param[in] cb Callback to trigger.
 * @param[in] user_data User context for callback.
 * @param[out] out_timer_id Pointer to receive the timer ID.
 * @return 0 on success, ENOMEM on failure.
 */
extern int http_loop_add_timer(struct ModalityEventLoop *loop, long timeout_ms,
                               http_timer_cb cb, void *user_data,
                               int *out_timer_id);

/**
 * @brief Cancel a timer.
 * @param[in] loop Event loop context.
 * @param[in] timer_id Timer ID to cancel.
 * @return 0 on success, ENOENT if not found.
 */
extern int http_loop_cancel_timer(struct ModalityEventLoop *loop, int timer_id);

/**
 * @brief Signal the event loop to wake up from an external thread.
 * Used for connecting WinHTTP's asynchronous callbacks.
 * @param[in] loop Event loop context.
 * @return 0 on success.
 */
extern int http_loop_wakeup(struct ModalityEventLoop *loop);

#ifdef __cplusplus
}
#endif

#endif /* C_CDD_HTTP_EVENT_LOOP_H */
