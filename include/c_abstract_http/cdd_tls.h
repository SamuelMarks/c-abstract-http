/**
 * @file cdd_tls.h
 * @brief Thread Local Storage (TLS) API.
 *
 * Provides a minimal, cross-platform abstraction for thread-local storage.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_TLS_H
#define C_CDD_HTTP_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#include <stddef.h>
/* clang-format on */

/**
 * @brief Opaque structure representing a Thread Local Storage key.
 */
struct CddTlsKey;

/**
 * @brief Creates a new TLS key.
 *
 * @param[out] key Pointer to the key pointer to initialize.
 * @param[in] destructor Optional callback to clean up values when a thread
 * exits.
 * @return 0 on success, or an error code on failure.
 */
extern int cdd_tls_key_create(struct CddTlsKey **key,
                              void (*destructor)(void *));

/**
 * @brief Sets a thread-local value for the given key.
 *
 * @param[in] key The TLS key.
 * @param[in] value The value to store.
 * @return 0 on success, or an error code on failure.
 */
extern int cdd_tls_set(struct CddTlsKey *key, void *value);

/**
 * @brief Gets the thread-local value for the given key.
 *
 * @param[in] key The TLS key.
 * @param[out] out_value Pointer to a void pointer to store the retrieved value.
 * @return 0 on success, or an error code on failure.
 */
extern int cdd_tls_get(struct CddTlsKey *key, void **out_value);

/**
 * @brief Deletes a TLS key.
 *
 * @param[in] key The TLS key to delete.
 */
extern void cdd_tls_key_delete(struct CddTlsKey *key);

#ifdef __cplusplus
}
#endif

#endif
