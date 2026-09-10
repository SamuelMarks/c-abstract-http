/* clang-format off */
#include <c_abstract_http/http_android.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ANDROID__)
#include <jni.h>
#endif

#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_android_getenv_detached(void);
extern int *abstract_http_mock_get_g_mock_android_getenv_fail(void);
extern int *abstract_http_mock_get_g_mock_android_attach_fail(void);
extern int *abstract_http_mock_get_g_mock_android_new_string_fail(void);
extern int *abstract_http_mock_get_g_mock_android_find_class_fail(void);
extern int *abstract_http_mock_get_g_mock_android_get_method_fail(void);
extern int *abstract_http_mock_get_g_mock_android_new_object_fail(void);
extern int *abstract_http_mock_get_g_mock_android_call_object_fail(void);
extern int *abstract_http_mock_get_g_mock_android_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_android_input_stream_fail(void);
extern int *abstract_http_mock_get_g_mock_android_error_stream_fail(void);
extern int *abstract_http_mock_get_g_mock_android_read_exception(void);
extern int *abstract_http_mock_get_g_mock_android_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_final_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_cleanup_exception(void);
extern int *abstract_http_mock_get_g_mock_android_read_chunks(void);
extern int *abstract_http_mock_get_g_mock_android_status_code(void);
extern int *abstract_http_mock_get_g_mock_android_config_init_fail(void);

#define g_mock_android_getenv_detached                                         \
  (*abstract_http_mock_get_g_mock_android_getenv_detached())
#define g_mock_android_getenv_fail                                             \
  (*abstract_http_mock_get_g_mock_android_getenv_fail())
#define g_mock_android_attach_fail                                             \
  (*abstract_http_mock_get_g_mock_android_attach_fail())
#define g_mock_android_new_string_fail                                         \
  (*abstract_http_mock_get_g_mock_android_new_string_fail())
#define g_mock_android_find_class_fail                                         \
  (*abstract_http_mock_get_g_mock_android_find_class_fail())
#define g_mock_android_get_method_fail                                         \
  (*abstract_http_mock_get_g_mock_android_get_method_fail())
#define g_mock_android_new_object_fail                                         \
  (*abstract_http_mock_get_g_mock_android_new_object_fail())
#define g_mock_android_call_object_fail                                        \
  (*abstract_http_mock_get_g_mock_android_call_object_fail())
#define g_mock_android_res_alloc_fail                                          \
  (*abstract_http_mock_get_g_mock_android_res_alloc_fail())
#define g_mock_android_res_init_fail                                           \
  (*abstract_http_mock_get_g_mock_android_res_init_fail())
#define g_mock_android_input_stream_fail                                       \
  (*abstract_http_mock_get_g_mock_android_input_stream_fail())
#define g_mock_android_error_stream_fail                                       \
  (*abstract_http_mock_get_g_mock_android_error_stream_fail())
#define g_mock_android_read_exception                                          \
  (*abstract_http_mock_get_g_mock_android_read_exception())
#define g_mock_android_body_alloc_fail                                         \
  (*abstract_http_mock_get_g_mock_android_body_alloc_fail())
#define g_mock_android_body_realloc_fail                                       \
  (*abstract_http_mock_get_g_mock_android_body_realloc_fail())
#define g_mock_android_final_body_realloc_fail                                 \
  (*abstract_http_mock_get_g_mock_android_final_body_realloc_fail())
#define g_mock_android_cleanup_exception                                       \
  (*abstract_http_mock_get_g_mock_android_cleanup_exception())
#define g_mock_android_read_chunks                                             \
  (*abstract_http_mock_get_g_mock_android_read_chunks())
#define g_mock_android_status_code                                             \
  (*abstract_http_mock_get_g_mock_android_status_code())
#define g_mock_android_config_init_fail                                        \
  (*abstract_http_mock_get_g_mock_android_config_init_fail())
#endif

#if !defined(__ANDROID__)
typedef unsigned char jboolean;
typedef signed char jbyte;
typedef short jshort;
typedef int jint;
typedef long jlong;
typedef float jfloat;
typedef double jdouble;
typedef jint jsize;

typedef void *jobject;
typedef jobject jclass;
typedef jobject jstring;
typedef jobject jarray;
typedef jobject jbyteArray;
typedef jobject jthrowable;
typedef void *jmethodID;
typedef void *jfieldID;

#define JNI_OK 0
#define JNI_ERR (-1)
#define JNI_EDETACHED (-2)
#define JNI_EVERSION (-3)
#define JNI_ENOMEM (-4)
#define JNI_VERSION_1_6 0x00010006
#define JNI_ABORT 2

struct JNINativeInterface_;
typedef const struct JNINativeInterface_ *JNIEnv;

struct JNIInvokeInterface_;
typedef const struct JNIInvokeInterface_ *JavaVM;

struct JNINativeInterface_ {
  void *reserved0;
  void *reserved1;
  void *reserved2;
  void *reserved3;
  jclass (*FindClass)(JNIEnv *env, const char *name);
  jmethodID (*GetMethodID)(JNIEnv *env, jclass clazz, const char *name,
                           const char *sig);
  jobject (*NewObject)(JNIEnv *env, jclass clazz, jmethodID methodID, ...);
  jclass (*GetObjectClass)(JNIEnv *env, jobject obj);
  jobject (*CallObjectMethod)(JNIEnv *env, jobject obj, jmethodID methodID,
                              ...);
  void (*CallVoidMethod)(JNIEnv *env, jobject obj, jmethodID methodID, ...);
  jint (*CallIntMethod)(JNIEnv *env, jobject obj, jmethodID methodID, ...);
  jboolean (*ExceptionCheck)(JNIEnv *env);
  void (*ExceptionClear)(JNIEnv *env);
  jstring (*NewStringUTF)(JNIEnv *env, const char *bytes);
  void (*DeleteLocalRef)(JNIEnv *env, jobject localRef);
  jbyteArray (*NewByteArray)(JNIEnv *env, jsize length);
  jbyte *(*GetByteArrayElements)(JNIEnv *env, jbyteArray array,
                                 jboolean *isCopy);
  void (*ReleaseByteArrayElements)(JNIEnv *env, jbyteArray array, jbyte *elems,
                                   jint mode);
};

struct JNIInvokeInterface_ {
  void *reserved0;
  void *reserved1;
  void *reserved2;
  jint (*DestroyJavaVM)(JavaVM *vm);
  jint (*AttachCurrentThread)(JavaVM *vm, void **penv, void *args);
  jint (*DetachCurrentThread)(JavaVM *vm);
  jint (*GetEnv)(JavaVM *vm, void **penv, jint version);
  jint (*AttachCurrentThreadAsDaemon)(JavaVM *vm, void **penv, void *args);
};

static jbyte g_mock_android_byte_buf[8192];
static int g_mock_android_has_exception = 0;

static jclass mock_FindClass(JNIEnv *env, const char *name) {
  (void)env;
  (void)name;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_find_class_fail) {
    return NULL;
  }
#endif
  return (jclass)(size_t)1;
}

static jmethodID mock_GetMethodID(JNIEnv *env, jclass clazz, const char *name,
                                  const char *sig) {
  (void)env;
  (void)clazz;
  (void)sig;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_get_method_fail == 1) {
    return NULL;
  }
#endif
  if (strcmp(name, "getResponseCode") == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_get_method_fail == 2) {
      return NULL;
    }
#endif
    return (jmethodID)(size_t)10;
  }
  if (strcmp(name, "read") == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_get_method_fail == 5) {
      return NULL;
    }
#endif
    return (jmethodID)(size_t)20;
  }
  if (strcmp(name, "getInputStream") == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_get_method_fail == 6) {
      return NULL;
    }
#endif
    return (jmethodID)(size_t)30;
  }
  if (strcmp(name, "getErrorStream") == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_get_method_fail == 7) {
      return NULL;
    }
#endif
    return (jmethodID)(size_t)31;
  }
  if (strcmp(name, "openConnection") == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_get_method_fail == 3) {
      return NULL;
    }
#endif
    return (jmethodID)(size_t)40;
  }
  if (strcmp(name, "setRequestMethod") == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_get_method_fail == 4) {
      return NULL;
    }
#endif
    return (jmethodID)(size_t)50;
  }
  return (jmethodID)(size_t)1;
}

static jobject mock_NewObject(JNIEnv *env, jclass clazz, jmethodID methodID,
                              ...) {
  (void)env;
  (void)clazz;
  (void)methodID;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_new_object_fail) {
    return NULL;
  }
#endif
  return (jobject)(size_t)2;
}

static jclass mock_GetObjectClass(JNIEnv *env, jobject obj) {
  (void)env;
  (void)obj;
  return (jclass)(size_t)3;
}

static jobject mock_CallObjectMethod(JNIEnv *env, jobject obj,
                                     jmethodID methodID, ...) {
  (void)env;
  (void)obj;
  (void)methodID;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_call_object_fail) {
    return NULL;
  }
  if (methodID == (jmethodID)(size_t)30 && g_mock_android_input_stream_fail) {
    g_mock_android_input_stream_fail = 0;
    g_mock_android_has_exception = 1;
    return NULL;
  }
  if (methodID == (jmethodID)(size_t)31 && g_mock_android_error_stream_fail) {
    g_mock_android_error_stream_fail = 0;
    g_mock_android_has_exception = 1;
    return NULL;
  }
#endif
  return (jobject)(size_t)4;
}

static void mock_CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID,
                                ...) {
  (void)env;
  (void)obj;
  (void)methodID;
}

static jint mock_CallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID,
                               ...) {
  (void)env;
  (void)obj;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (methodID == (jmethodID)(size_t)10) {
    if (g_mock_android_status_code) {
      return (jint)g_mock_android_status_code;
    }
    return 200;
  }
  if (g_mock_android_read_exception) {
    g_mock_android_read_exception = 0;
    g_mock_android_has_exception = 1;
    return 0;
  }
  if (g_mock_android_read_chunks == 99) {
    g_mock_android_read_chunks = 0;
    return 0;
  }
  if (g_mock_android_read_chunks > 0) {
    g_mock_android_read_chunks--;
    return 5000;
  }
  return -1;
#else
  if (methodID == (jmethodID)(size_t)10) {
    return 200;
  }
  return -1;
#endif
}

static jboolean mock_ExceptionCheck(JNIEnv *env) {
  (void)env;
  return (jboolean)(g_mock_android_has_exception ? 1 : 0);
}

static void mock_ExceptionClear(JNIEnv *env) {
  (void)env;
  g_mock_android_has_exception = 0;
}

static jstring mock_NewStringUTF(JNIEnv *env, const char *bytes) {
  (void)env;
  (void)bytes;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_new_string_fail == 1) {
    return NULL;
  }
  if (g_mock_android_new_string_fail == 2 && strcmp(bytes, "GET") == 0) {
    return NULL;
  }
#endif
  return (jstring)(size_t)5;
}

static void mock_DeleteLocalRef(JNIEnv *env, jobject localRef) {
  (void)env;
  (void)localRef;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_cleanup_exception) {
    g_mock_android_has_exception = 1;
  }
#endif
}

static jbyteArray mock_NewByteArray(JNIEnv *env, jsize length) {
  (void)env;
  (void)length;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_res_alloc_fail == 2) {
    return NULL;
  }
#endif
  return (jbyteArray)(size_t)6;
}

static jbyte *mock_GetByteArrayElements(JNIEnv *env, jbyteArray array,
                                        jboolean *isCopy) {
  (void)env;
  (void)array;
  (void)isCopy;
  return g_mock_android_byte_buf;
}

static void mock_ReleaseByteArrayElements(JNIEnv *env, jbyteArray array,
                                          jbyte *elems, jint mode) {
  (void)env;
  (void)array;
  (void)elems;
  (void)mode;
}

static const struct JNINativeInterface_ g_mock_native_interface = {
    NULL,
    NULL,
    NULL,
    NULL,
    mock_FindClass,
    mock_GetMethodID,
    mock_NewObject,
    mock_GetObjectClass,
    mock_CallObjectMethod,
    mock_CallVoidMethod,
    mock_CallIntMethod,
    mock_ExceptionCheck,
    mock_ExceptionClear,
    mock_NewStringUTF,
    mock_DeleteLocalRef,
    mock_NewByteArray,
    mock_GetByteArrayElements,
    mock_ReleaseByteArrayElements};

static const JNIEnv g_mock_env = &g_mock_native_interface;

static jint mock_AttachCurrentThread(JavaVM *vm, void **penv, void *args) {
  (void)vm;
  (void)args;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_attach_fail) {
    return JNI_ERR;
  }
#endif
  *penv = (void *)&g_mock_env;
  return JNI_OK;
}

static jint mock_DetachCurrentThread(JavaVM *vm) {
  (void)vm;
  return JNI_OK;
}

static jint mock_GetEnv(JavaVM *vm, void **penv, jint version) {
  (void)vm;
  (void)version;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_getenv_fail) {
    return JNI_ERR;
  }
  if (g_mock_android_getenv_detached) {
    g_mock_android_getenv_detached = 0;
    return JNI_EDETACHED;
  }
#endif
  *penv = (void *)&g_mock_env;
  return JNI_OK;
}

static const struct JNIInvokeInterface_ g_mock_invoke_interface = {
    NULL,
    NULL,
    NULL,
    NULL,
    mock_AttachCurrentThread,
    mock_DetachCurrentThread,
    mock_GetEnv,
    NULL};

static const JavaVM g_mock_jvm = &g_mock_invoke_interface;
#endif

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief JavaVM pointer */
  JavaVM *jvm;
  /** @brief Cached URL class */
  jclass url_class;
  /** @brief Cached HttpURLConnection class */
  jclass http_conn_class;
  /** @brief Configuration settings */
  struct HttpConfig config;
  /** @brief Peer verification flag */
  int verify_peer;
  /** @brief Host verification flag */
  int verify_host;
};

static JavaVM *g_android_jvm = NULL;

/**
 * @brief Initialize the global Android JNI environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_android_global_init(void) {
#if !defined(__ANDROID__)
  g_android_jvm = (JavaVM *)&g_mock_jvm;
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Cleanup global Android environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_android_global_cleanup(void) {
  g_android_jvm = NULL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Set the global JavaVM handle.
 *
 * @param[in] jvm Pointer to the JavaVM instance.
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_android_set_jvm(void *jvm) {
  g_android_jvm = (JavaVM *)jvm;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Set the JavaVM handle for a specific transport context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] jvm Pointer to the JavaVM instance.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_android_context_set_jvm(struct HttpTransportContext *ctx, void *jvm) {
  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->jvm = (JavaVM *)jvm;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Create a new Android-backed transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_android_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&(*ctx)->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_android_context_init: Error http_config_init failed with %d",
        (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->jvm = g_android_jvm;
  (*ctx)->url_class = NULL;
  (*ctx)->http_conn_class = NULL;
  (*ctx)->verify_peer = 1;
  (*ctx)->verify_host = 1;

  LOG_DEBUG("http_android_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free the transport context.
 *
 * @param[in] ctx The context to free. Satisfies NULL-safety.
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
 * @brief Apply configuration settings to the Android context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config The configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_android_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
  LOG_DEBUG("http_android_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_android_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->verify_peer = config->verify_peer;
  ctx->verify_host = config->verify_host;
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  LOG_DEBUG("http_android_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send HTTP request via Android HttpURLConnection.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_android_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
  JNIEnv *env = NULL;
  jclass url_cls = NULL;
  jclass conn_cls = NULL;
  jclass is_cls = NULL;
  jmethodID url_init = NULL;
  jmethodID url_open_conn = NULL;
  jmethodID conn_set_req_method = NULL;
  jmethodID conn_get_res_code = NULL;
  jmethodID conn_get_input_stream = NULL;
  jmethodID conn_get_error_stream = NULL;
  jmethodID is_read = NULL;
  jobject url_obj = NULL;
  jobject conn_obj = NULL;
  jobject input_stream = NULL;
  jstring url_str = NULL;
  jstring method_str = NULL;
  jbyteArray byte_array = NULL;
  jint res_code = 0;
  int attached = 0;
  const char *method_c_str = "GET";
  size_t body_cap = 8192;
  size_t body_len = 0;
  char *body = NULL;
  char *final_body = NULL;
  char *new_body = NULL;
  jbyte *bytes = NULL;
  jint read_len = 0;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_android_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_android_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = NULL;

  if (!ctx->jvm) {
    LOG_DEBUG("http_android_send: Error ENOTSUP (JVM not set)");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  rc = (enum c_abstract_http_error)(*ctx->jvm)->GetEnv(ctx->jvm, (void **)&env,
                                                       JNI_VERSION_1_6);
  if (rc == (enum c_abstract_http_error)JNI_EDETACHED) {
    if ((*ctx->jvm)->AttachCurrentThread(ctx->jvm, (void **)&env, NULL) != 0) {
      LOG_DEBUG("http_android_send: Error AttachCurrentThread failed");
      return C_ABSTRACT_HTTP_ERR_NOTSUP;
    }
    attached = 1;
    rc = C_ABSTRACT_HTTP_SUCCESS;
  } else if (rc != (enum c_abstract_http_error)JNI_OK) {
    LOG_DEBUG("http_android_send: Error GetEnv failed");
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  rc = C_ABSTRACT_HTTP_SUCCESS;

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

  if (!url_init) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }
  if (!url_open_conn) {
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

  if (!conn_set_req_method) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }
  if (!conn_get_res_code) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }

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
  if (!method_str) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
  (*env)->CallVoidMethod(env, conn_obj, conn_set_req_method, method_str);
  (*env)->DeleteLocalRef(env, method_str);
  method_str = NULL;

  /* Connect and get response code */
  res_code = (*env)->CallIntMethod(env, conn_obj, conn_get_res_code);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_res_alloc_fail == 1) {
    *res = NULL;
  } else
#endif
  {
    *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!*res) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_android_res_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(*res);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    goto cleanup;
  }

  (*res)->status_code = (int)res_code;

  /* Read input stream */
  if (!conn_get_input_stream) {
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }
  input_stream = (*env)->CallObjectMethod(env, conn_obj, conn_get_input_stream);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
    if (!conn_get_error_stream) {
      rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
      goto cleanup;
    }
    input_stream =
        (*env)->CallObjectMethod(env, conn_obj, conn_get_error_stream);
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
      input_stream = NULL;
    }
  }

  if (input_stream) {
    is_cls = (*env)->GetObjectClass(env, input_stream);
    is_read = (*env)->GetMethodID(env, is_cls, "read", "([B)I");
    if (!is_read) {
      rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
      goto cleanup;
    }
    byte_array = (*env)->NewByteArray(env, 8192);
    if (!byte_array) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      goto cleanup;
    }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_android_body_alloc_fail) {
      body = NULL;
    } else
#endif
    {
      body = (char *)malloc(body_cap);
    }

    if (!body) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else {
      for (;;) {
        read_len =
            (*env)->CallIntMethod(env, input_stream, is_read, byte_array);
        if ((*env)->ExceptionCheck(env)) {
          (*env)->ExceptionClear(env);
          break;
        }
        if (read_len == -1) {
          break; /* EOF */
        }

        if (read_len > 0) {
          if (body_len + (size_t)read_len > body_cap) {
            body_cap = (body_len + (size_t)read_len) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
            if (g_mock_android_body_realloc_fail) {
              new_body = NULL;
            } else
#endif
            {
              new_body = (char *)realloc(body, body_cap);
            }
            if (!new_body) {
              rc = C_ABSTRACT_HTTP_ERR_NOMEM;
              break;
            }
            body = new_body;
          }

          bytes = (*env)->GetByteArrayElements(env, byte_array, NULL);
          memcpy(body + body_len, bytes, (size_t)read_len);
          (*env)->ReleaseByteArrayElements(env, byte_array, bytes, JNI_ABORT);
          body_len += (size_t)read_len;
        }
      }
    }

    if (rc == C_ABSTRACT_HTTP_SUCCESS && body_len > 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_android_final_body_realloc_fail) {
        final_body = NULL;
      } else
#endif
      {
        final_body = (char *)realloc(body, body_len + 1);
      }
      if (final_body) {
        final_body[body_len] = '\0';
        (*res)->body = final_body;
        (*res)->body_len = body_len;
      } else {
        (*res)->body = body;
        (*res)->body_len = body_len;
        body[body_len - 1] = '\0';
      }
    } else if (body) {
      free(body);
    }

    (*env)->DeleteLocalRef(env, byte_array);
    byte_array = NULL;
    (*env)->DeleteLocalRef(env, input_stream);
    input_stream = NULL;
  }

cleanup:
  if (url_str) {
    (*env)->DeleteLocalRef(env, url_str);
  }
  if (url_obj) {
    (*env)->DeleteLocalRef(env, url_obj);
  }
  if (conn_obj) {
    (*env)->DeleteLocalRef(env, conn_obj);
  }
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
    rc = C_ABSTRACT_HTTP_ERR_IO;
  }
  if (attached) {
    (*ctx->jvm)->DetachCurrentThread(ctx->jvm);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (*res) {
      http_response_free(*res);
      free(*res);
      *res = NULL;
    }
  }

  LOG_DEBUG("http_android_send: Exiting");
  return rc;
}
