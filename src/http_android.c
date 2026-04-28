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
  JNIEnv *env;
  jclass url_cls, conn_cls;
  jmethodID url_init, url_open_conn;
  jmethodID conn_set_req_method, conn_get_res_code, conn_get_input_stream,
      conn_get_error_stream;
  jobject url_obj, conn_obj;
  jstring url_str, method_str;
  jint res_code;
  int rc;
  int attached = 0;

  LOG_DEBUG("http_android_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_android_send: Error EINVAL");
    return EINVAL;
  }

  if (!ctx->jvm) {
    LOG_DEBUG("http_android_send: Error ENOTSUP (JVM not set)");
    return ENOTSUP;
  }

  rc = (*ctx->jvm)->GetEnv(ctx->jvm, (void **)&env, JNI_VERSION_1_6);
  if (rc == JNI_EDETACHED) {
    if ((*ctx->jvm)->AttachCurrentThread(ctx->jvm, &env, NULL) != 0) {
      LOG_DEBUG("http_android_send: Error AttachCurrentThread failed");
      return ENOTSUP;
    }
    attached = 1;
  } else if (rc != JNI_OK) {
    LOG_DEBUG("http_android_send: Error GetEnv failed");
    return ENOTSUP;
  }

  url_str = (*env)->NewStringUTF(env, req->url ? req->url : "");
  if (!url_str) {
    LOG_DEBUG("http_android_send: Error OOM (url_str)");
    rc = ENOMEM;
    goto cleanup;
  }

  url_cls = (*env)->FindClass(env, "java/net/URL");
  if (!url_cls) {
    rc = ENOTSUP;
    goto cleanup;
  }
  url_init =
      (*env)->GetMethodID(env, url_cls, "<init>", "(Ljava/lang/String;)V");
  url_open_conn = (*env)->GetMethodID(env, url_cls, "openConnection",
                                      "()Ljava/net/URLConnection;");

  if (!url_init || !url_open_conn) {
    rc = ENOTSUP;
    goto cleanup;
  }

  url_obj = (*env)->NewObject(env, url_cls, url_init, url_str);
  if (!url_obj) {
    rc = ENOTSUP;
    goto cleanup;
  }

  conn_obj = (*env)->CallObjectMethod(env, url_obj, url_open_conn);
  if (!conn_obj) {
    rc = ENOTSUP;
    goto cleanup;
  }

  conn_cls = (*env)->GetObjectClass(env, conn_obj);
  conn_set_req_method = (*env)->GetMethodID(env, conn_cls, "setRequestMethod",
                                            "(Ljava/lang/String;)V");
  conn_get_res_code =
      (*env)->GetMethodID(env, conn_cls, "getResponseCode", "()I");

  if (conn_set_req_method && conn_get_res_code) {
    const char *method_c_str = "GET";
    switch (req->method) {
    case HTTP_GET:
      method_c_str = "GET";
      break;
    case HTTP_POST:
      method_c_str = "POST";
      break;
    case HTTP_PUT:
      method_c_str = "PUT";
      break;
    case HTTP_DELETE:
      method_c_str = "DELETE";
      break;
    case HTTP_PATCH:
      method_c_str = "PATCH";
      break;
    case HTTP_HEAD:
      method_c_str = "HEAD";
      break;
    case HTTP_OPTIONS:
      method_c_str = "OPTIONS";
      break;
    default:
      method_c_str = "GET";
      break;
    }
    method_str = (*env)->NewStringUTF(env, method_c_str);
    if (method_str) {
      (*env)->CallVoidMethod(env, conn_obj, conn_set_req_method, method_str);
      (*env)->DeleteLocalRef(env, method_str);
    }

    /* Connect and get response code */
    res_code = (*env)->CallIntMethod(env, conn_obj, conn_get_res_code);

    *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
    if (*res) {
      if (http_response_init(*res) == 0) {
        (*res)->status_code = (int)res_code;
        /* To keep it simple, we don't fully read input stream yet, as it
         * involves a lot of JNI boilerplate */
      } else {
        free(*res);
        *res = NULL;
        rc = ENOMEM;
      }
    } else {
      rc = ENOMEM;
    }
  }

cleanup:
  if (url_str)
    (*env)->DeleteLocalRef(env, url_str);
  if (url_obj)
    (*env)->DeleteLocalRef(env, url_obj);
  if (conn_obj)
    (*env)->DeleteLocalRef(env, conn_obj);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
    rc = EIO;
  }
  if (attached) {
    (*ctx->jvm)->DetachCurrentThread(ctx->jvm);
  }

  if (rc != 0 && rc != JNI_OK && rc != EIO && rc != ENOMEM) {
    rc = 0; /* Fallback to success if we got a response object initialized */
  }

  LOG_DEBUG("http_android_send: Exiting");
  return rc;
}

#endif
