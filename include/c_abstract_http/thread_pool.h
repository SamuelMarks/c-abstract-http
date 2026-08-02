/**
 * @file thread_pool.h
 * @brief Cross-platform Thread Pool API.
 *
 * Implements a lightweight thread pool, mutexes, and condition variables
 * using Windows APIs on Windows and POSIX threads (pthreads) on POSIX.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_THREAD_POOL_H
#define C_ABSTRACT_HTTP_HTTP_THREAD_POOL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stddef.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Opaque mutex type.
 */
struct AbstractHttpMutex;

/**
 * @brief Opaque condition variable type.
 */
struct AbstractHttpCond;

/**
 * @brief Opaque thread pool type.
 */
struct AbstractHttpThreadPool;

/**
 * @brief Initialize a mutex.
 * @param[out] mutex Pointer to receive the allocated mutex.
 * @return 0 on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_mutex_init(struct AbstractHttpMutex **mutex);

/**
 * @brief Lock a mutex.
 * @param[in] mutex The mutex.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_mutex_lock(struct AbstractHttpMutex *mutex);

/**
 * @brief Unlock a mutex.
 * @param[in] mutex The mutex.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_mutex_unlock(struct AbstractHttpMutex *mutex);

/**
 * @brief Free a mutex.
 * @param[in] mutex The mutex.
 */
C_ABSTRACT_HTTP_API void
abstract_http_mutex_free(struct AbstractHttpMutex *mutex);

/**
 * @brief Initialize a condition variable.
 * @param[out] cond Pointer to receive the allocated condition variable.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_cond_init(struct AbstractHttpCond **cond);

/**
 * @brief Wait on a condition variable.
 * @param[in] cond The condition variable.
 * @param[in] mutex The associated mutex.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t abstract_http_cond_wait(
    struct AbstractHttpCond *cond, struct AbstractHttpMutex *mutex);

/**
 * @brief Signal a condition variable (wake one).
 * @param[in] cond The condition variable.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_cond_signal(struct AbstractHttpCond *cond);

/**
 * @brief Broadcast a condition variable (wake all).
 * @param[in] cond The condition variable.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_cond_broadcast(struct AbstractHttpCond *cond);

/**
 * @brief Free a condition variable.
 * @param[in] cond The condition variable.
 */
C_ABSTRACT_HTTP_API void abstract_http_cond_free(struct AbstractHttpCond *cond);

/**
 * @brief Thread pool task callback signature.
 * @param[in] arg User-provided argument.
 */
typedef void (*abstract_http_thread_task_cb)(void *arg);

/**
 * @brief Hooks for integrating with an external thread pool.
 */
struct AbstractHttpThreadPoolHooks {
  void *external_context; /**< Context passed to hooks */
  int (*push)(void *ctx, abstract_http_thread_task_cb cb,
              void *arg); /**< Hook for pushing a task */
};

/**
 * @brief Initialize a thread pool.
 * @param[out] pool Pointer to receive the allocated thread pool.
 * @param[in] num_threads Number of worker threads to spawn.
 * @return 0 on success, ENOMEM or other error on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_thread_pool_init(struct AbstractHttpThreadPool **pool,
                               size_t num_threads);

/**
 * @brief Initialize a thread pool that delegates tasks to an external worker
 * pool.
 * @param[out] pool Pointer to receive the allocated thread pool proxy.
 * @param[in] hooks External thread pool hooks.
 * @return 0 on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_thread_pool_init_external(
    struct AbstractHttpThreadPool **pool,
    const struct AbstractHttpThreadPoolHooks *hooks);

/**
 * @brief Push a task to the thread pool queue.
 * @param[in] pool The thread pool.
 * @param[in] cb The function to execute.
 * @param[in] arg The argument to pass to the function.
 * @return 0 on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_thread_pool_push(struct AbstractHttpThreadPool *pool,
                               abstract_http_thread_task_cb cb, void *arg);

/**
 * @brief Destroy the thread pool, waiting for tasks to complete.
 * @param[in] pool The thread pool.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
abstract_http_thread_pool_free(struct AbstractHttpThreadPool *pool);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_THREAD_POOL_H */
