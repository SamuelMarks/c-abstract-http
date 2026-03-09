/**
 * @file cmp_integration.h
 * @brief Integration layer for c-multiplatform modalities.
 *
 * Maps c-multiplatform paradigms (like CMP_MODALITY_ASYNC_SINGLE) to
 * c-abstract-http paradigms (like MODALITY_ASYNC).
 */

#ifndef C_ABSTRACT_HTTP_CMP_INTEGRATION_H
#define C_ABSTRACT_HTTP_CMP_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stddef.h>
#include "http_types.h"
/* clang-format on */

/**
 * @brief Expected modality values from c-multiplatform.
 *
 * This mimics cmp_modality_t to avoid circular header dependencies.
 */
enum CmpModality {
  CMP_MODALITY_SYNC_SINGLE = 0,
  CMP_MODALITY_SYNC_MULTI = 1,
  CMP_MODALITY_ASYNC_SINGLE = 2,
  CMP_MODALITY_ASYNC_MULTI = 3,
  CMP_MODALITY_GREENTHREADS = 4,
  CMP_MODALITY_MULTIPROCESS_ACTOR = 5
};

/**
 * @brief Mock representation of cmp_app_config_t for integration.
 */
struct CmpAppConfig {
  int modality;       /**< cast from cmp_modality_t */
  size_t min_threads; /**< minimum threads for pools */
  size_t max_threads; /**< maximum threads for pools */
};

/**
 * @brief Progress binding context for UI updates.
 */
struct CmpProgressBinding {
  void *ui_component;
  int (*update_progress)(void *ui_component, float percentage);
  int cancel_requested; /* 1 if the user closed the UI view and wants to abort
                         */
};

/**
 * @brief Converts a c-multiplatform modality to a c-abstract-http modality.
 *
 * @param cmp_mod The c-multiplatform modality (e.g.,
 * CMP_MODALITY_ASYNC_SINGLE).
 * @param out_mod Pointer to store the resulting ExecutionModality.
 * @return 0 on success, EINVAL on invalid input.
 */
int cmp_http_modality_adapter(int cmp_mod, enum ExecutionModality *out_mod);

/**
 * @brief Injects c-multiplatform configuration into the HTTP configuration.
 *
 * Applies the framework's modality and thread pool limits to the HTTP layer.
 *
 * @param cmp_config Pointer to the framework configuration.
 * @param http_config Pointer to the HTTP configuration to update.
 * @return 0 on success, EINVAL on invalid parameters.
 */
int cmp_http_inject_config(const struct CmpAppConfig *cmp_config,
                           struct HttpConfig *http_config);

/**
 * @brief Default progress callback that hooks into the CmpProgressBinding.
 * @param current_bytes Bytes transferred so far.
 * @param total_bytes Total bytes expected.
 * @param user_data Should be a pointer to CmpProgressBinding.
 * @return 0 to continue, non-zero to abort (e.g. if cancel_requested is true).
 */
int cmp_http_progress_adapter(size_t current_bytes, size_t total_bytes,
                              void *user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_CMP_INTEGRATION_H */
