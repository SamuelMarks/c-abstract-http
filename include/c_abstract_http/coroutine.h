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

#ifndef C_CDD_HTTP_COROUTINE_H
#define C_CDD_HTTP_COROUTINE_H

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#include <stddef.h>
/* clang-format on */

/**
 * @brief Opaque Coroutine state.
 */
struct CddCoroutine;

/**
 * @brief Coroutine entry point function signature.
 */
typedef void (*cdd_coroutine_cb)(void *arg);

/**
 * @brief External hooks for overriding coroutine management.
 */
struct CddCoroutineHooks {
  int (*init)(struct CddCoroutine **co, size_t stack_size, cdd_coroutine_cb cb,
              void *arg);
  void (*free)(struct CddCoroutine *co);
  int (*resume)(struct CddCoroutine *co);
  int (*yield)(void);
  int (*is_done)(const struct CddCoroutine *co);
};

/**
 * @brief Register external coroutine hooks.
 * @param[in] hooks The hooks structure.
 */
extern void cdd_coroutine_set_hooks(const struct CddCoroutineHooks *hooks);

/**
 * @brief Initialize a new coroutine.
 * @param[out] co Pointer to receive the allocated coroutine handle.
 * @param[in] stack_size Stack size in bytes (0 for default).
 * @param[in] cb The entry point function.
 * @param[in] arg Argument to pass to the entry point.
 * @return 0 on success, ENOMEM or EINVAL on failure.
 */
extern int cdd_coroutine_init(struct CddCoroutine **co, size_t stack_size,
                              cdd_coroutine_cb cb, void *arg);

/**
 * @brief Free resources associated with a coroutine.
 * Must be called after the coroutine finishes.
 * @param[in] co The coroutine handle.
 */
extern void cdd_coroutine_free(struct CddCoroutine *co);

/**
 * @brief Transfer execution to the coroutine.
 * The caller will block until the coroutine calls `cdd_coroutine_yield` or
 * finishes.
 * @param[in] co The coroutine handle.
 * @return 0 on success.
 */
extern int cdd_coroutine_resume(struct CddCoroutine *co);

/**
 * @brief Yield execution back to the resumer.
 * MUST be called from within the currently active coroutine.
 * @return 0 on success.
 */
extern int cdd_coroutine_yield(void);

/**
 * @brief Check if the coroutine has finished executing.
 * @param[in] co The coroutine handle.
 * @return 1 if finished, 0 if still active.
 */
extern int cdd_coroutine_is_done(const struct CddCoroutine *co);

#ifdef __cplusplus
}
#endif

#endif /* C_CDD_HTTP_COROUTINE_H */
