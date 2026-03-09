/**
 * @file cdd_tls.c
 * @brief Cross-platform TLS
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <c_abstract_http/cdd_tls.h>
/* clang-format on */

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

struct CddTlsKey {
  DWORD dwTlsIndex;
};

int cdd_tls_key_create(struct CddTlsKey **key, void (*destructor)(void *)) {
  if (!key)
    return EINVAL;
  /* Windows FlsAlloc supports destructors, TlsAlloc does not. We'll stick to
   * TlsAlloc for maximum compatibility but ignore destructors for now. */
  (void)destructor;
  *key = (struct CddTlsKey *)malloc(sizeof(struct CddTlsKey));
  if (!*key)
    return ENOMEM;
  (*key)->dwTlsIndex = TlsAlloc();
  if ((*key)->dwTlsIndex == TLS_OUT_OF_INDEXES) {
    free(*key);
    return EIO;
  }
  return 0;
}

int cdd_tls_set(struct CddTlsKey *key, void *value) {
  if (!key)
    return EINVAL;
  return TlsSetValue(key->dwTlsIndex, value) ? 0 : EIO;
}

void *cdd_tls_get(struct CddTlsKey *key) {
  if (!key)
    return NULL;
  return TlsGetValue(key->dwTlsIndex);
}

void cdd_tls_key_delete(struct CddTlsKey *key) {
  if (key) {
    TlsFree(key->dwTlsIndex);
    free(key);
  }
}

#else

struct CddTlsKey {
  pthread_key_t key;
};

int cdd_tls_key_create(struct CddTlsKey **key, void (*destructor)(void *)) {
  if (!key)
    return EINVAL;
  *key = (struct CddTlsKey *)malloc(sizeof(struct CddTlsKey));
  if (!*key)
    return ENOMEM;
  if (pthread_key_create(&(*key)->key, destructor) != 0) {
    free(*key);
    return EIO;
  }
  return 0;
}

int cdd_tls_set(struct CddTlsKey *key, void *value) {
  if (!key)
    return EINVAL;
  return (pthread_setspecific(key->key, value) == 0) ? 0 : EIO;
}

void *cdd_tls_get(struct CddTlsKey *key) {
  if (!key)
    return NULL;
  return pthread_getspecific(key->key);
}

void cdd_tls_key_delete(struct CddTlsKey *key) {
  if (key) {
    pthread_key_delete(key->key);
    free(key);
  }
}

#endif
