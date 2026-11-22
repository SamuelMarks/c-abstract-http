
/**
 * @file c_abstract_http_tls.c
 * @brief Cross-platform Thread-Local Storage (TLS) implementation.
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No TLS on DOS */
#else
#include <pthread.h>
#endif

#include <c_abstract_http/c_abstract_http_tls.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct AbstractHttpTlsKey */
struct AbstractHttpTlsKey {
  DWORD dwTlsIndex; /**< Win32 TlsAlloc slot index */
};

enum c_abstract_http_error
abstract_http_tls_key_create(struct AbstractHttpTlsKey **key,
                             void (*destructor)(void *)) {
  LOG_DEBUG("abstract_http_tls_key_create: Entering");
  if (!key) {
    LOG_DEBUG("abstract_http_tls_key_create: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  /* Windows FlsAlloc supports destructors, TlsAlloc does not. We'll stick to
   * TlsAlloc for maximum compatibility but ignore destructors for now. */
  (void)destructor;
  *key = (struct AbstractHttpTlsKey *)malloc(sizeof(struct AbstractHttpTlsKey));
  if (!*key) {
    LOG_DEBUG("abstract_http_tls_key_create: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  (*key)->dwTlsIndex = TlsAlloc();
  if ((*key)->dwTlsIndex == TLS_OUT_OF_INDEXES) {
    LOG_DEBUG("abstract_http_tls_key_create: Error EIO (TLS_OUT_OF_INDEXES)");
    free(*key);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_key_create: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set TLS value for the current thread */
enum c_abstract_http_error abstract_http_tls_set(struct AbstractHttpTlsKey *key,
                                                 void *value) {
  LOG_DEBUG("abstract_http_tls_set: Entering");
  if (!key) {
    LOG_DEBUG("abstract_http_tls_set: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!TlsSetValue(key->dwTlsIndex, value)) {
    LOG_DEBUG("abstract_http_tls_set: Error EIO (TlsSetValue failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_set: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Get TLS value for the current thread */
enum c_abstract_http_error abstract_http_tls_get(struct AbstractHttpTlsKey *key,
                                                 void **out_value) {
  LOG_DEBUG("abstract_http_tls_get: Entering");
  if (!key || !out_value) {
    LOG_DEBUG("abstract_http_tls_get: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = TlsGetValue(key->dwTlsIndex);
  if (!*out_value && GetLastError() != ERROR_SUCCESS) {
    LOG_DEBUG("abstract_http_tls_get: Error EIO (TlsGetValue failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_get: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_tls_key_delete(struct AbstractHttpTlsKey *key) {
  LOG_DEBUG("abstract_http_tls_key_delete: Entering");
  if (key) {
    TlsFree(key->dwTlsIndex);
    free(key);
  }
  LOG_DEBUG("abstract_http_tls_key_delete: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)

/** @brief Internal struct AbstractHttpTlsKey for single-threaded DOS */
struct AbstractHttpTlsKey {
  /** @brief Stored slot pointer */
  void *value;
  /** @brief Optional destructor function pointer */
  void (*destructor)(void *);
};

enum c_abstract_http_error
abstract_http_tls_key_create(struct AbstractHttpTlsKey **key,
                             void (*destructor)(void *)) {
  if (!key) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *key = (struct AbstractHttpTlsKey *)malloc(sizeof(struct AbstractHttpTlsKey));
  if (!*key) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  (*key)->value = NULL;
  (*key)->destructor = destructor;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set TLS value for the current thread on DOS */
enum c_abstract_http_error abstract_http_tls_set(struct AbstractHttpTlsKey *key,
                                                 void *value) {
  if (!key) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  key->value = value;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Get TLS value for the current thread on DOS */
enum c_abstract_http_error abstract_http_tls_get(struct AbstractHttpTlsKey *key,
                                                 void **out_value) {
  if (!key || !out_value) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = key->value;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_tls_key_delete(struct AbstractHttpTlsKey *key) {
  if (key) {
    free(key);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#else

/** @brief Internal struct AbstractHttpTlsKey */
struct AbstractHttpTlsKey {
  /** @brief key (variable) of struct AbstractHttpTlsKey */
  pthread_key_t key;
};

enum c_abstract_http_error
abstract_http_tls_key_create(struct AbstractHttpTlsKey **key,
                             void (*destructor)(void *)) {
  int err;
  LOG_DEBUG("abstract_http_tls_key_create: Entering");
  if (!key) {
    LOG_DEBUG("abstract_http_tls_key_create: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *key = (struct AbstractHttpTlsKey *)malloc(sizeof(struct AbstractHttpTlsKey));
  if (!*key) {
    LOG_DEBUG("abstract_http_tls_key_create: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  err = pthread_key_create(&(*key)->key, destructor);
  if (err != 0) {
    LOG_DEBUG(
        "abstract_http_tls_key_create: Error pthread_key_create failed with %d",
        err);
    free(*key);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_key_create: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set TLS value for the current thread */
enum c_abstract_http_error abstract_http_tls_set(struct AbstractHttpTlsKey *key,
                                                 void *value) {
  int err;
  LOG_DEBUG("abstract_http_tls_set: Entering");
  if (!key) {
    LOG_DEBUG("abstract_http_tls_set: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  err = pthread_setspecific(key->key, value);
  if (err != 0) {
    LOG_DEBUG("abstract_http_tls_set: Error pthread_setspecific failed with %d",
              err);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_set: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Get TLS value for the current thread */
enum c_abstract_http_error abstract_http_tls_get(struct AbstractHttpTlsKey *key,
                                                 void **out_value) {
  LOG_DEBUG("abstract_http_tls_get: Entering");
  if (!key || !out_value) {
    LOG_DEBUG("abstract_http_tls_get: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = pthread_getspecific(key->key);
  LOG_DEBUG("abstract_http_tls_get: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_tls_key_delete(struct AbstractHttpTlsKey *key) {
  LOG_DEBUG("abstract_http_tls_key_delete: Entering");
  if (key) {
    pthread_key_delete(key->key);
    free(key);
  }
  LOG_DEBUG("abstract_http_tls_key_delete: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif
