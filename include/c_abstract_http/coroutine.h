/**
 * @file coroutine.h
 * @brief Cross-platform minimalistic Greenthread / Coroutine API.
 *
 * Implements cooperative multitasking using `ucontext` on POSIX and
 * Fibers on Windows. Allows pausing tasks during heavy I/O and yielding
 * execution back to the caller/scheduler.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_COROUTINE_H
#define C_ABSTRACT_HTTP_HTTP_COROUTINE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stddef.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Opaque Coroutine state.
 */
struct AbstractHttpCoroutine;

/**
 * @brief Coroutine entry point function signature.
 */
typedef void (*abstract_http_coroutine_cb)(void *arg);

/**
 * @brief External hooks for overriding coroutine management.
 */
struct AbstractHttpCoroutineHooks {
  int (*init)(struct AbstractHttpCoroutine **co, size_t stack_size,
              abstract_http_coroutine_cb cb,
              void *arg); /**< Hook for coroutine initialization */
  void (*free)(
      struct AbstractHttpCoroutine *co); /**< Hook for coroutine destruction */
  int (*resume)(
      struct AbstractHttpCoroutine *co); /**< Hook for resuming execution */
  int (*yield)(void);                    /**< Hook for yielding execution */
  int (*is_done)(const struct AbstractHttpCoroutine *co,
                 int *out_is_done); /**< Hook for checking status */
};

/**
 * @brief Register external coroutine hooks.
 * @param[in] hooks The hooks structure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_coroutine_set_hooks(
    const struct AbstractHttpCoroutineHooks *hooks);

/**
 * @brief Initialize a new coroutine.
 * @param[out] co Pointer to receive the allocated coroutine handle.
 * @param[in] stack_size Stack size in bytes (0 for default).
 * @param[in] cb The entry point function.
 * @param[in] arg Argument to pass to the entry point.
 * @return 0 on success, ENOMEM or EINVAL on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_coroutine_init(struct AbstractHttpCoroutine **co,
                             size_t stack_size, abstract_http_coroutine_cb cb,
                             void *arg);

/**
 * @brief Free resources associated with a coroutine.
 * Must be called after the coroutine finishes.
 * @param[in] co The coroutine handle.
 */
C_ABSTRACT_HTTP_API void
abstract_http_coroutine_free(struct AbstractHttpCoroutine *co);

/**
 * @brief Transfer execution to the coroutine.
 * The caller will block until the coroutine calls
 * `abstract_http_coroutine_yield` or finishes.
 * @param[in] co The coroutine handle.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_coroutine_resume(struct AbstractHttpCoroutine *co);

/**
 * @brief Yield execution back to the resumer.
 * MUST be called from within the currently active coroutine.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_coroutine_yield(void);

/**
 * @brief Check if the coroutine has finished executing.
 * @param[in] co The coroutine handle.
 * @param[out] out_is_done Pointer to store 1 if finished, 0 if still active.
 * @return Error enum.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_coroutine_is_done(const struct AbstractHttpCoroutine *co,
                                int *out_is_done);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_COROUTINE_H */
