
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

#include <c_abstract_http/cdd_tls.h>
#include "c_abstract_http/log.h"
/* clang-format on */

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct CddTlsKey */
struct CddTlsKey {
  DWORD dwTlsIndex;
};

enum c_abstract_http_error cdd_tls_key_create(struct CddTlsKey **key,
                                              void (*destructor)(void *)) {
  LOG_DEBUG("cdd_tls_key_create: Entering");
  if (!key) {
    LOG_DEBUG("cdd_tls_key_create: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  /* Windows FlsAlloc supports destructors, TlsAlloc does not. We'll stick to
   * TlsAlloc for maximum compatibility but ignore destructors for now. */
  (void)destructor;
  *key = (struct CddTlsKey *)malloc(sizeof(struct CddTlsKey));
  if (!*key) {
    LOG_DEBUG("cdd_tls_key_create: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  (*key)->dwTlsIndex = TlsAlloc();
  if ((*key)->dwTlsIndex == TLS_OUT_OF_INDEXES) {
    LOG_DEBUG("cdd_tls_key_create: Error EIO (TLS_OUT_OF_INDEXES)");
    free(*key);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_tls_key_create: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_tls_set(struct CddTlsKey *key, void *value) {
  LOG_DEBUG("cdd_tls_set: Entering");
  if (!key) {
    LOG_DEBUG("cdd_tls_set: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!TlsSetValue(key->dwTlsIndex, value)) {
    LOG_DEBUG("cdd_tls_set: Error EIO (TlsSetValue failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_tls_set: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_tls_get(struct CddTlsKey *key,
                                       void **out_value) {
  LOG_DEBUG("cdd_tls_get: Entering");
  if (!key || !out_value) {
    LOG_DEBUG("cdd_tls_get: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = TlsGetValue(key->dwTlsIndex);
  if (!*out_value && GetLastError() != ERROR_SUCCESS) {
    LOG_DEBUG("cdd_tls_get: Error EIO (TlsGetValue failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_tls_get: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_tls_key_delete(struct CddTlsKey *key) {
  LOG_DEBUG("cdd_tls_key_delete: Entering");
  if (key) {
    TlsFree(key->dwTlsIndex);
    free(key);
  }
  LOG_DEBUG("cdd_tls_key_delete: Exiting");
}

#else

/** @brief Internal struct CddTlsKey */
struct CddTlsKey {
  /** @brief key (variable) of struct CddTlsKey */
  pthread_key_t key;
};

enum c_abstract_http_error cdd_tls_key_create(struct CddTlsKey **key,
                                              void (*destructor)(void *)) {
  int rc;
  LOG_DEBUG("cdd_tls_key_create: Entering");
  if (!key) {
    LOG_DEBUG("cdd_tls_key_create: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *key = (struct CddTlsKey *)malloc(sizeof(struct CddTlsKey));
  if (!*key) {
    LOG_DEBUG("cdd_tls_key_create: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = pthread_key_create(&(*key)->key, destructor);
  if (rc != 0) {
    LOG_DEBUG("cdd_tls_key_create: Error pthread_key_create failed with %d",
              rc);
    free(*key);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_tls_key_create: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_tls_set(struct CddTlsKey *key, void *value) {
  int rc;
  LOG_DEBUG("cdd_tls_set: Entering");
  if (!key) {
    LOG_DEBUG("cdd_tls_set: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc = pthread_setspecific(key->key, value);
  if (rc != 0) {
    LOG_DEBUG("cdd_tls_set: Error pthread_setspecific failed with %d", rc);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_tls_set: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_tls_get(struct CddTlsKey *key,
                                       void **out_value) {
  LOG_DEBUG("cdd_tls_get: Entering");
  if (!key || !out_value) {
    LOG_DEBUG("cdd_tls_get: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = pthread_getspecific(key->key);
  LOG_DEBUG("cdd_tls_get: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_tls_key_delete(struct CddTlsKey *key) {
  LOG_DEBUG("cdd_tls_key_delete: Entering");
  if (key) {
    pthread_key_delete(key->key);
    free(key);
  }
  LOG_DEBUG("cdd_tls_key_delete: Exiting");
}

#endif
