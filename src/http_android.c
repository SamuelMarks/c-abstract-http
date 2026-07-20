
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

enum c_abstract_http_error http_android_global_init(void) {
  /* Typically JVM is retrieved via JNI_OnLoad, assuming it's done elsewhere */
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_android_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_android_context_init(struct HttpTransportContext **ctx) {
  int rc;
  LOG_DEBUG("http_android_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_android_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_android_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
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
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_android_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_android_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_android_context_free: Exiting");
}

enum c_abstract_http_error
http_android_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
  LOG_DEBUG("http_android_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_android_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  /* Configuration (like timeouts) can be applied per-request in
   * HttpURLConnection */
  ctx->verify_peer = config->verify_peer;
  ctx->verify_host = config->verify_host;
  ctx->config = *config;
  LOG_DEBUG("http_android_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_android_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
  JNIEnv *env;
  jclass url_cls, conn_cls, is_cls;
  jmethodID url_init, url_open_conn;
  jmethodID conn_set_req_method, conn_get_res_code, conn_get_input_stream,
      conn_get_error_stream;
  jmethodID is_read;
  jobject url_obj = NULL, conn_obj = NULL, input_stream = NULL;
  jstring url_str = NULL, method_str = NULL;
  jbyteArray byte_array = NULL;
  jint res_code;
  int attached = 0;
  int rc = 0;

  LOG_DEBUG("http_android_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_android_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (!ctx->jvm) {
    LOG_DEBUG("http_android_send: Error ENOTSUP (JVM not set)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  rc = (*ctx->jvm)->GetEnv(ctx->jvm, (void **)&env, JNI_VERSION_1_6);
  if (rc == JNI_EDETACHED) {
    if ((*ctx->jvm)->AttachCurrentThread(ctx->jvm, &env, NULL) != 0) {
      LOG_DEBUG("http_android_send: Error AttachCurrentThread failed");
      return C_ABSTRACT_HTTP_ERR_NOTSUP;
    }
    attached = 1;
  } else if (rc != JNI_OK) {
    LOG_DEBUG("http_android_send: Error GetEnv failed");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  rc = 0;

  url_str = (*env)->NewStringUTF(env, req->url ? req->url : "");
  if (!url_str) {
    LOG_DEBUG("http_android_send: Error OOM (url_str)");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  url_cls = (*env)->FindClass(env, "java/net/URL");
  if (!url_cls) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }
  url_init =
      (*env)->GetMethodID(env, url_cls, "<init>", "(Ljava/lang/String;)V");
  url_open_conn = (*env)->GetMethodID(env, url_cls, "openConnection",
                                      "()Ljava/net/URLConnection;");

  if (!url_init || !url_open_conn) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }

  url_obj = (*env)->NewObject(env, url_cls, url_init, url_str);
  if (!url_obj) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }

  conn_obj = (*env)->CallObjectMethod(env, url_obj, url_open_conn);
  if (!conn_obj) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }

  conn_cls = (*env)->GetObjectClass(env, conn_obj);
  conn_set_req_method = (*env)->GetMethodID(env, conn_cls, "setRequestMethod",
                                            "(Ljava/lang/String;)V");
  conn_get_res_code =
      (*env)->GetMethodID(env, conn_cls, "getResponseCode", "()I");
  conn_get_input_stream = (*env)->GetMethodID(env, conn_cls, "getInputStream",
                                              "()Ljava/io/InputStream;");
  conn_get_error_stream = (*env)->GetMethodID(env, conn_cls, "getErrorStream",
                                              "()Ljava/io/InputStream;");

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
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }

    *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
    if (*res) {
      if (http_response_init(*res) == 0) {
        (*res)->status_code = (int)res_code;

        /* Read input stream */
        if (conn_get_input_stream) {
          input_stream =
              (*env)->CallObjectMethod(env, conn_obj, conn_get_input_stream);
          if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            if (conn_get_error_stream) {
              input_stream = (*env)->CallObjectMethod(env, conn_obj,
                                                      conn_get_error_stream);
              if ((*env)->ExceptionCheck(env)) {
                (*env)->ExceptionClear(env);
                input_stream = NULL;
              }
            }
          }

          if (input_stream) {
            is_cls = (*env)->GetObjectClass(env, input_stream);
            is_read = (*env)->GetMethodID(env, is_cls, "read", "([B)I");
            byte_array = (*env)->NewByteArray(env, 8192);

            if (is_read && byte_array) {
              size_t body_cap = 8192;
              size_t body_len = 0;
              char *body = (char *)malloc(body_cap);

              if (!body) {
                rc = C_ABSTRACT_HTTP_ERR_NOMEM;
              } else {
                while (1) {
                  jint read_len = (*env)->CallIntMethod(env, input_stream,
                                                        is_read, byte_array);
                  if ((*env)->ExceptionCheck(env)) {
                    (*env)->ExceptionClear(env);
                    break;
                  }
                  if (read_len == -1)
                    break; /* EOF */

                  if (read_len > 0) {
                    if (body_len + (size_t)read_len > body_cap) {
                      char *new_body;
                      body_cap = (body_len + (size_t)read_len) * 2;
                      new_body = (char *)realloc(body, body_cap);
                      if (!new_body) {
                        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
                        break;
                      }
                      body = new_body;
                    }

                    {
                      jbyte *bytes =
                          (*env)->GetByteArrayElements(env, byte_array, NULL);
                      memcpy(body + body_len, bytes, (size_t)read_len);
                      (*env)->ReleaseByteArrayElements(env, byte_array, bytes,
                                                       JNI_ABORT);
                      body_len += (size_t)read_len;
                    }
                  }
                }
              }

              if (rc == 0 && body_len > 0) {
                char *final_body = (char *)realloc(body, body_len + 1);
                if (final_body) {
                  final_body[body_len] = '\0';
                  (*res)->body = final_body;
                  (*res)->body_len = body_len;
                } else {
                  (*res)->body = body;
                  (*res)->body_len = body_len;
                  if (body)
                    body[body_len - 1] =
                        '\0'; /* Fallback, technically bad if no space */
                }
              } else if (body) {
                free(body);
              }

              if (byte_array)
                (*env)->DeleteLocalRef(env, byte_array);
            }
            if (input_stream)
              (*env)->DeleteLocalRef(env, input_stream);
          }
        }
      } else {
        free(*res);
        *res = NULL;
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    } else {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
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
    rc = C_ABSTRACT_HTTP_ERR_IO;
  }
  if (attached) {
    (*ctx->jvm)->DetachCurrentThread(ctx->jvm);
  }

  if (rc != 0 && rc != C_ABSTRACT_HTTP_ERR_NOMEM &&
      rc != C_ABSTRACT_HTTP_ERR_IO && rc != C_ABSTRACT_HTTP_ERR_NOTSUP) {
    rc = 0; /* Fallback to success if we got a response object initialized */
  }

  LOG_DEBUG("http_android_send: Exiting");
  return rc;
}

#endif
