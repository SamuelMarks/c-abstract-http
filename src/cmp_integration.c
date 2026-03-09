/**
 * @file cmp_integration.c
 * @brief Implementation of the c-multiplatform integration layer.
 *
 * Provides adapters to convert execution modalities and thread limits.
 */

/* clang-format off */
#include "../include/c_abstract_http/cmp_integration.h"
#include <errno.h>
/* clang-format on */

int cmp_http_modality_adapter(int cmp_mod, enum ExecutionModality *out_mod) {
  if (!out_mod) {
    return EINVAL;
  }

  switch ((enum CmpModality)cmp_mod) {
  case CMP_MODALITY_SYNC_SINGLE:
    *out_mod = MODALITY_SYNC;
    break;
  case CMP_MODALITY_SYNC_MULTI:
    *out_mod = MODALITY_THREAD_POOL;
    break;
  case CMP_MODALITY_ASYNC_SINGLE:
    *out_mod = MODALITY_ASYNC;
    break;
  case CMP_MODALITY_ASYNC_MULTI:
    *out_mod = MODALITY_ASYNC; /* Note: Async loop can dispatch to threads */
    break;
  case CMP_MODALITY_GREENTHREADS:
    *out_mod = MODALITY_GREENTHREAD;
    break;
  case CMP_MODALITY_MULTIPROCESS_ACTOR:
    *out_mod = MODALITY_MULTIPROCESS; /* OR MODALITY_MESSAGE_PASSING */
    break;
  default:
    return EINVAL;
  }

  return 0;
}

int cmp_http_inject_config(const struct CmpAppConfig *cmp_config,
                           struct HttpConfig *http_config) {
  enum ExecutionModality http_mod;
  int rc;

  if (!cmp_config || !http_config) {
    return EINVAL;
  }

  /* Adapter converts the framework modality integer into HTTP enum */
  rc = cmp_http_modality_adapter(cmp_config->modality, &http_mod);
  if (rc != 0) {
    return rc;
  }

  http_config->modality = http_mod;

  /* Synchronize thread-pool sizing constraints */
  if (http_mod == MODALITY_THREAD_POOL) {
    http_config->min_threads = cmp_config->min_threads;
    http_config->max_threads = cmp_config->max_threads;
  }

  return 0;
}

int cmp_http_progress_adapter(size_t current_bytes, size_t total_bytes,
                              void *user_data) {
  struct CmpProgressBinding *binding = (struct CmpProgressBinding *)user_data;
  float pct;

  if (!binding) {
    return 0; /* continue */
  }

  if (binding->cancel_requested) {
    return 1; /* abort */
  }

  if (binding->update_progress && total_bytes > 0) {
    pct = (float)current_bytes / (float)total_bytes;
    return binding->update_progress(binding->ui_component, pct);
  }

  return 0;
}
