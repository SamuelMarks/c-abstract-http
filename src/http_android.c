/**
 * @file http_android.c
 * @brief Android JNI implementation of the Abstract Network Interface.
 *
 * Implements the HTTP transport using Android's java.net.HttpURLConnection.
 * Due to JNI requirements, this depends heavily on the Android JVM.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <c_abstract_http/http_android.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#if defined(__ANDROID__)
#include <jni.h>
/* clang-format on */

/* JNI implementation */

struct HttpTransportContext {
  JavaVM *jvm;
  jclass url_class;
  jclass http_conn_class;
  struct HttpConfig config;
  int verify_peer;
  int verify_host;
};

/**
 * @brief Executes the http_android_global_init operation.
 */
int http_android_global_init(void) {
  /* Typically JVM is retrieved via JNI_OnLoad, assuming it's done elsewhere */
  return 0;
}

/**
 * @brief Executes the http_android_global_cleanup operation.
 */
void http_android_global_cleanup(void) { /* No-op */ }

/**
 * @brief Executes the http_android_context_init operation.
 */
int http_android_context_init(struct HttpTransportContext **ctx) {
  int rc;
  LOG_DEBUG("http_android_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_android_context_init: Error EINVAL");
    return EINVAL;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_android_context_init: Error ENOMEM");
    return ENOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != 0) {
    LOG_DEBUG(
        "http_android_context_init: Error http_config_init failed with %d", rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->jvm = NULL;
  (*ctx)->url_class = NULL;
  (*ctx)->http_conn_class = NULL;

  LOG_DEBUG("http_android_context_init: Success");
  return 0;
}

/**
 * @brief Executes the http_android_context_free operation.
 */
void http_android_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_android_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_android_context_free: Exiting");
}

/**
 * @brief Executes the http_android_config_apply operation.
 */
int http_android_config_apply(struct HttpTransportContext *ctx,
                              const struct HttpConfig *config) {
  LOG_DEBUG("http_android_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_android_config_apply: Error EINVAL");
    return EINVAL;
  }
  /* Configuration (like timeouts) can be applied per-request in
   * HttpURLConnection */
  ctx->verify_peer = config->verify_peer;
  ctx->verify_host = config->verify_host;
  ctx->config = *config;
  LOG_DEBUG("http_android_config_apply: Success");
  return 0;
}

/**
 * @brief Executes the http_android_send operation.
 */
int http_android_send(struct HttpTransportContext *ctx,
                      const struct HttpRequest *req,
                      struct HttpResponse **res) {
  LOG_DEBUG("http_android_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_android_send: Error EINVAL");
    return EINVAL;
  }
  /* Not fully implemented without an active JVM environment */
  LOG_DEBUG("http_android_send: Error ENOSYS");
  return ENOSYS;
}

#endif
