/**
 * @file c_abstract_http_tls.c
 * @brief Cross-platform Thread-Local Storage (TLS) implementation.
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
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

#ifndef FLS_OUT_OF_INDEXES
/** @brief Sentinel index returned by FlsAlloc on failure */
#define FLS_OUT_OF_INDEXES ((DWORD)0xFFFFFFFFUL)
#endif

/** @brief Win32 FlsAlloc callback signature */
typedef void(WINAPI *cah_win32_fls_cb_t)(void *);
/** @brief Dynamic function pointer type for kernel32!FlsAlloc */
typedef DWORD(WINAPI *cah_fls_alloc_fn)(cah_win32_fls_cb_t);
/** @brief Dynamic function pointer type for kernel32!FlsGetValue */
typedef void *(WINAPI *cah_fls_get_fn)(DWORD);
/** @brief Dynamic function pointer type for kernel32!FlsSetValue */
typedef BOOL(WINAPI *cah_fls_set_fn)(DWORD, void *);
/** @brief Dynamic function pointer type for kernel32!FlsFree */
typedef BOOL(WINAPI *cah_fls_free_fn)(DWORD);

/** @brief Internal struct AbstractHttpTlsKey */
struct AbstractHttpTlsKey {
  /** @brief Win32 FLS or TLS slot index */
  DWORD dwTlsIndex;
  /** @brief Optional C thread-exit destructor callback */
  void (*destructor)(void *);
  /** @brief Non-zero when backed by Fiber-Local Storage (FlsAlloc) */
  int use_fls;
  /** @brief Non-zero while abstract_http_tls_key_delete is releasing FLS */
  int is_deleting;
  /** @brief Dynamically resolved FlsGetValue function */
  cah_fls_get_fn fls_get;
  /** @brief Dynamically resolved FlsSetValue function */
  cah_fls_set_fn fls_set;
  /** @brief Dynamically resolved FlsFree function */
  cah_fls_free_fn fls_free;
};

/** @brief Per-thread wrapper bridging Win32 __stdcall FLS callback to C */
struct CahWin32TlsSlot {
  /** @brief User thread-local value pointer */
  void *value;
  /** @brief Owning TLS key metadata */
  const struct AbstractHttpTlsKey *owner;
};

/**
 * @brief Win32 __stdcall Fiber-Local Storage callback invoked on thread exit.
 * @param lpFlsData Pointer to per-thread CahWin32TlsSlot wrapper.
 */
static void WINAPI cah_win32_fls_trampoline(void *lpFlsData) {
  struct CahWin32TlsSlot *slot = (struct CahWin32TlsSlot *)lpFlsData;
  if (!slot) {
    return;
  }
  if (slot->owner && !slot->owner->is_deleting && slot->owner->destructor &&
      slot->value) {
    void *val = slot->value;
    slot->value = NULL;
    slot->owner->destructor(val);
  }
  free(slot);
}

NO_DISCARD c_abstract_http_error_t abstract_http_tls_key_create(
    struct AbstractHttpTlsKey **key, void (*destructor)(void *)) {
  struct AbstractHttpTlsKey *k;
  HMODULE hKernel32;
  LOG_DEBUG("abstract_http_tls_key_create: Entering");
  if (!key) {
    LOG_DEBUG("abstract_http_tls_key_create: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  k = (struct AbstractHttpTlsKey *)malloc(sizeof(struct AbstractHttpTlsKey));
  if (!k) {
    LOG_DEBUG("abstract_http_tls_key_create: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(k, 0, sizeof(*k));
  k->destructor = destructor;

  hKernel32 = GetModuleHandleA("kernel32.dll");
  if (destructor != NULL && hKernel32 != NULL) {
    FARPROC pAlloc = GetProcAddress(hKernel32, "FlsAlloc");
    FARPROC pGet = GetProcAddress(hKernel32, "FlsGetValue");
    FARPROC pSet = GetProcAddress(hKernel32, "FlsSetValue");
    FARPROC pFree = GetProcAddress(hKernel32, "FlsFree");
    if (pAlloc && pGet && pSet && pFree) {
      cah_fls_alloc_fn fn_alloc = NULL;
      memcpy(&fn_alloc, &pAlloc, sizeof(fn_alloc));
      memcpy(&k->fls_get, &pGet, sizeof(k->fls_get));
      memcpy(&k->fls_set, &pSet, sizeof(k->fls_set));
      memcpy(&k->fls_free, &pFree, sizeof(k->fls_free));
      k->dwTlsIndex = fn_alloc(cah_win32_fls_trampoline);
      if (k->dwTlsIndex == FLS_OUT_OF_INDEXES) {
        free(k);
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      k->use_fls = 1;
      *key = k;
      LOG_DEBUG("abstract_http_tls_key_create: Success (FLS)");
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }

  k->dwTlsIndex = TlsAlloc();
  if (k->dwTlsIndex == TLS_OUT_OF_INDEXES) {
    LOG_DEBUG("abstract_http_tls_key_create: Error EIO (TLS_OUT_OF_INDEXES)");
    free(k);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  *key = k;
  LOG_DEBUG("abstract_http_tls_key_create: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set TLS value for the current thread */
NO_DISCARD c_abstract_http_error_t
abstract_http_tls_set(struct AbstractHttpTlsKey *key, void *value) {
  LOG_DEBUG("abstract_http_tls_set: Entering");
  if (!key) {
    LOG_DEBUG("abstract_http_tls_set: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (key->use_fls) {
    struct CahWin32TlsSlot *existing =
        (struct CahWin32TlsSlot *)key->fls_get(key->dwTlsIndex);
    if (!value) {
      if (!key->fls_set(key->dwTlsIndex, NULL)) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      free(existing);
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (existing) {
      existing->value = value;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    existing = (struct CahWin32TlsSlot *)malloc(sizeof(struct CahWin32TlsSlot));
    if (!existing) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    existing->value = value;
    existing->owner = key;
    if (!key->fls_set(key->dwTlsIndex, existing)) {
      free(existing);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!TlsSetValue(key->dwTlsIndex, value)) {
    LOG_DEBUG("abstract_http_tls_set: Error EIO (TlsSetValue failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_set: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Get TLS value for the current thread */
NO_DISCARD c_abstract_http_error_t
abstract_http_tls_get(struct AbstractHttpTlsKey *key, void **out_value) {
  LOG_DEBUG("abstract_http_tls_get: Entering");
  if (!key || !out_value) {
    LOG_DEBUG("abstract_http_tls_get: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (key->use_fls) {
    struct CahWin32TlsSlot *slot =
        (struct CahWin32TlsSlot *)key->fls_get(key->dwTlsIndex);
    if (!slot && GetLastError() != ERROR_SUCCESS) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    *out_value = slot ? slot->value : NULL;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_value = TlsGetValue(key->dwTlsIndex);
  if (!*out_value && GetLastError() != ERROR_SUCCESS) {
    LOG_DEBUG("abstract_http_tls_get: Error EIO (TlsGetValue failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("abstract_http_tls_get: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_tls_key_delete(struct AbstractHttpTlsKey *key) {
  LOG_DEBUG("abstract_http_tls_key_delete: Entering");
  if (key) {
    if (key->use_fls) {
      key->is_deleting = 1;
      key->fls_free(key->dwTlsIndex);
    } else {
      TlsFree(key->dwTlsIndex);
    }
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

NO_DISCARD c_abstract_http_error_t abstract_http_tls_key_create(
    struct AbstractHttpTlsKey **key, void (*destructor)(void *)) {
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
NO_DISCARD c_abstract_http_error_t
abstract_http_tls_set(struct AbstractHttpTlsKey *key, void *value) {
  if (!key) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  key->value = value;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Get TLS value for the current thread on DOS */
NO_DISCARD c_abstract_http_error_t
abstract_http_tls_get(struct AbstractHttpTlsKey *key, void **out_value) {
  if (!key || !out_value) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = key->value;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
abstract_http_tls_key_delete(struct AbstractHttpTlsKey *key) {
  if (key) {
    if (key->destructor && key->value) {
      void *val = key->value;
      key->value = NULL;
      key->destructor(val);
    }
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

NO_DISCARD c_abstract_http_error_t abstract_http_tls_key_create(
    struct AbstractHttpTlsKey **key, void (*destructor)(void *)) {
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
NO_DISCARD c_abstract_http_error_t
abstract_http_tls_set(struct AbstractHttpTlsKey *key, void *value) {
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
NO_DISCARD c_abstract_http_error_t
abstract_http_tls_get(struct AbstractHttpTlsKey *key, void **out_value) {
  LOG_DEBUG("abstract_http_tls_get: Entering");
  if (!key || !out_value) {
    LOG_DEBUG("abstract_http_tls_get: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_value = pthread_getspecific(key->key);
  LOG_DEBUG("abstract_http_tls_get: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
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
