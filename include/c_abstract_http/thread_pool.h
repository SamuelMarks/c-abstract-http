/**
 * @file thread_pool.h
 * @brief Cross-platform Thread Pool API.
 *
 * Implements a lightweight thread pool, mutexes, and condition variables
 * using Windows APIs on Windows and POSIX threads (pthreads) on POSIX.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_THREAD_POOL_H
#define C_CDD_HTTP_THREAD_POOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#include <stddef.h>
/* clang-format on */

/**
 * @brief Opaque mutex type.
 */
struct CddMutex;

/**
 * @brief Opaque condition variable type.
 */
struct CddCond;

/**
 * @brief Opaque thread pool type.
 */
struct CddThreadPool;

/**
 * @brief Initialize a mutex.
 * @param[out] mutex Pointer to receive the allocated mutex.
 * @return 0 on success, error code on failure.
 */
extern int cdd_mutex_init(struct CddMutex **mutex);

/**
 * @brief Lock a mutex.
 * @param[in] mutex The mutex.
 * @return 0 on success.
 */
extern int cdd_mutex_lock(struct CddMutex *mutex);

/**
 * @brief Unlock a mutex.
 * @param[in] mutex The mutex.
 * @return 0 on success.
 */
extern int cdd_mutex_unlock(struct CddMutex *mutex);

/**
 * @brief Free a mutex.
 * @param[in] mutex The mutex.
 */
extern void cdd_mutex_free(struct CddMutex *mutex);

/**
 * @brief Initialize a condition variable.
 * @param[out] cond Pointer to receive the allocated condition variable.
 * @return 0 on success.
 */
extern int cdd_cond_init(struct CddCond **cond);

/**
 * @brief Wait on a condition variable.
 * @param[in] cond The condition variable.
 * @param[in] mutex The associated mutex.
 * @return 0 on success.
 */
extern int cdd_cond_wait(struct CddCond *cond, struct CddMutex *mutex);

/**
 * @brief Signal a condition variable (wake one).
 * @param[in] cond The condition variable.
 * @return 0 on success.
 */
extern int cdd_cond_signal(struct CddCond *cond);

/**
 * @brief Broadcast a condition variable (wake all).
 * @param[in] cond The condition variable.
 * @return 0 on success.
 */
extern int cdd_cond_broadcast(struct CddCond *cond);

/**
 * @brief Free a condition variable.
 * @param[in] cond The condition variable.
 */
extern void cdd_cond_free(struct CddCond *cond);

/**
 * @brief Thread pool task callback signature.
 * @param[in] arg User-provided argument.
 */
typedef void (*cdd_thread_task_cb)(void *arg);

/**
 * @brief Initialize a thread pool.
 * @param[out] pool Pointer to receive the allocated thread pool.
 * @param[in] num_threads Number of worker threads to spawn.
 * @return 0 on success, ENOMEM or other error on failure.
 */
extern int cdd_thread_pool_init(struct CddThreadPool **pool,
                                size_t num_threads);

/**
 * @brief Push a task to the thread pool queue.
 * @param[in] pool The thread pool.
 * @param[in] cb The function to execute.
 * @param[in] arg The argument to pass to the function.
 * @return 0 on success, error code on failure.
 */
extern int cdd_thread_pool_push(struct CddThreadPool *pool,
                                cdd_thread_task_cb cb, void *arg);

/**
 * @brief Destroy the thread pool, waiting for tasks to complete.
 * @param[in] pool The thread pool.
 */
extern void cdd_thread_pool_free(struct CddThreadPool *pool);

#ifdef __cplusplus
}
#endif

#endif /* C_CDD_HTTP_THREAD_POOL_H */