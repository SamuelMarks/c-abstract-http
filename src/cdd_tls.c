/**
 * @file cdd_tls.c
 * @brief Cross-platform TLS
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

#include <c_abstract_http/cdd_tls.h>
/* clang-format on */

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
struct CddTlsKey {
  void *dummy;
};
static void *dummy_tls_val = NULL;
int cdd_tls_key_create(struct CddTlsKey **key, void (*destructor)(void *)) {
  (void)key;
  (void)destructor;
  return EINVAL;
}
int cdd_tls_set(struct CddTlsKey *key, void *value) {
  (void)key;
  dummy_tls_val = value;
  return 0;
}
int cdd_tls_get(struct CddTlsKey *key, void **out_value) {
  (void)key;
  if (out_value)
    *out_value = dummy_tls_val;
  return 0;
}
void cdd_tls_key_delete(struct CddTlsKey *key) {
  (void)key;
  dummy_tls_val = NULL;
}

#elif defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

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

int cdd_tls_get(struct CddTlsKey *key, void **out_value) {
  if (!key || !out_value)
    return -1;
  *out_value = TlsGetValue(key->dwTlsIndex);
  if (!*out_value && GetLastError() != ERROR_SUCCESS)
    return -1;
  return 0;
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

int cdd_tls_get(struct CddTlsKey *key, void **out_value) {
  if (!key || !out_value)
    return EINVAL;
  *out_value = pthread_getspecific(key->key);
  return 0;
}

void cdd_tls_key_delete(struct CddTlsKey *key) {
  if (key) {
    pthread_key_delete(key->key);
    free(key);
  }
}

#endif
