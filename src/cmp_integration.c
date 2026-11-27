/**
 * @file cmp_integration.c
 * @brief C-Multiplatform (CMP) execution modality and progress adapters.
 */

/* clang-format off */
#include <c_abstract_http/cmp_integration.h>
#include "c_abstract_http/log.h"
#include <errno.h>
#include <string.h>
/* clang-format on */

NO_DISCARD c_abstract_http_error_t
cmp_http_modality_adapter(int cmp_mod, enum ExecutionModality *out_mod) {
  LOG_DEBUG("cmp_http_modality_adapter: Entering");
  if (!out_mod) {
    LOG_DEBUG("cmp_http_modality_adapter: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  switch (cmp_mod) {
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
    LOG_DEBUG("cmp_http_modality_adapter: Error EINVAL (unknown modality %d)",
              cmp_mod);
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  LOG_DEBUG("cmp_http_modality_adapter: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t cmp_http_inject_config(
    const struct CmpAppConfig *cmp_config, struct HttpConfig *http_config) {
  enum ExecutionModality http_mod;
  c_abstract_http_error_t rc;

  LOG_DEBUG("cmp_http_inject_config: Entering");
  if (!cmp_config || !http_config) {
    LOG_DEBUG("cmp_http_inject_config: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  /* Adapter converts the framework modality integer into HTTP enum */
  rc = cmp_http_modality_adapter(cmp_config->modality, &http_mod);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("cmp_http_inject_config: Error cmp_http_modality_adapter failed "
              "with %d",
              rc);
    return rc;
  }

  http_config->modality = http_mod;

  /* Synchronize thread-pool sizing constraints */
  if (http_mod == MODALITY_THREAD_POOL) {
    http_config->min_threads = cmp_config->min_threads;
    http_config->max_threads = cmp_config->max_threads;
  }

  LOG_DEBUG("cmp_http_inject_config: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Adapt HTTP progress updates to CMP UI progress binding */
NO_DISCARD c_abstract_http_error_t cmp_http_progress_adapter(
    size_t current_bytes, size_t total_bytes, void *user_data) {
  struct CmpProgressBinding *binding = (struct CmpProgressBinding *)user_data;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  LOG_DEBUG("cmp_http_progress_adapter: Entering");
  if (!binding) {
    LOG_DEBUG("cmp_http_progress_adapter: Success (no binding)");
    return C_ABSTRACT_HTTP_SUCCESS; /* continue */
  }

  if (binding->cancel_requested) {
    LOG_DEBUG("cmp_http_progress_adapter: Cancel requested");
    return C_ABSTRACT_HTTP_ERR_IO; /* abort */
  }

  if (total_bytes > 0) {
    float pct = (float)current_bytes / (float)total_bytes;
    if (binding->update_progress) {
      rc = binding->update_progress(binding->ui_component, pct);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    }
    if (binding->framework_cb) {
      CmpFrameworkUiHookFn fw_fn = NULL;
      memcpy(&fw_fn, &binding->framework_cb, sizeof(fw_fn));
      rc = fw_fn(binding->context, binding->ui_component, pct);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t cmp_http_bind_request(
    struct HttpRequest *req, struct CmpProgressBinding *binding) {
  if (!req) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!binding) {
    req->on_progress = NULL;
    req->on_progress_user_data = NULL;
    req->cancel_token = NULL;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  req->on_progress = cmp_http_progress_adapter;
  req->on_progress_user_data = binding;
  req->cancel_token = &binding->cancel_requested;
  return C_ABSTRACT_HTTP_SUCCESS;
}
