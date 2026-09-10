/* clang-format off */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libsoup3.h>
#include "c_abstract_http/log.h"
#include "str.h"

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBSOUP3)
#include <libsoup/soup.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_libsoup3_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_session_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_msg_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_send_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_uri_parse_fail(void);

#define g_mock_libsoup3_global_init_fail                                       \
  (*abstract_http_mock_get_g_mock_libsoup3_global_init_fail())
#define g_mock_libsoup3_context_init_fail                                      \
  (*abstract_http_mock_get_g_mock_libsoup3_context_init_fail())
#define g_mock_libsoup3_config_init_fail                                       \
  (*abstract_http_mock_get_g_mock_libsoup3_config_init_fail())
#define g_mock_libsoup3_session_new_fail                                       \
  (*abstract_http_mock_get_g_mock_libsoup3_session_new_fail())
#define g_mock_libsoup3_msg_new_fail                                           \
  (*abstract_http_mock_get_g_mock_libsoup3_msg_new_fail())
#define g_mock_libsoup3_send_fail                                              \
  (*abstract_http_mock_get_g_mock_libsoup3_send_fail())
#define g_mock_libsoup3_res_alloc_fail                                         \
  (*abstract_http_mock_get_g_mock_libsoup3_res_alloc_fail())
#define g_mock_libsoup3_res_init_fail                                          \
  (*abstract_http_mock_get_g_mock_libsoup3_res_init_fail())
#define g_mock_libsoup3_body_alloc_fail                                        \
  (*abstract_http_mock_get_g_mock_libsoup3_body_alloc_fail())
#define g_mock_libsoup3_uri_parse_fail                                         \
  (*abstract_http_mock_get_g_mock_libsoup3_uri_parse_fail())
#endif

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBSOUP3)

typedef unsigned int guint;
typedef unsigned char guint8;
typedef unsigned long gsize;
typedef const void *gconstpointer;
typedef int gboolean;

#define TRUE 1
#define FALSE 0

#define G_IO_ERROR 1
#define G_IO_ERROR_CONNECTION_REFUSED 1
#define G_IO_ERROR_TIMED_OUT 2
#define G_URI_FLAGS_NONE 0

/** @brief Mock GError structure */
struct GError {
  /** @brief Error domain */
  int domain;
  /** @brief Error code */
  int code;
  /** @brief Error message */
  char *message;
};
typedef struct GError GError;

/** @brief Mock GUri structure */
struct GUri {
  /** @brief URI string */
  char *uri_string;
};
typedef struct GUri GUri;

/** @brief Mock GBytes structure */
struct GBytes {
  /** @brief Buffer data pointer */
  void *data;
  /** @brief Buffer size */
  size_t size;
};
typedef struct GBytes GBytes;

/** @brief Mock GByteArray structure */
struct GByteArray {
  /** @brief Array data */
  guint8 *data;
  /** @brief Current length */
  size_t len;
  /** @brief Capacity */
  size_t cap;
};
typedef struct GByteArray GByteArray;

/** @brief Mock SoupMessageHeaders structure */
struct SoupMessageHeaders {
  /** @brief Header keys */
  char *keys[32];
  /** @brief Header values */
  char *values[32];
  /** @brief Count of headers */
  size_t count;
};
typedef struct SoupMessageHeaders SoupMessageHeaders;

/** @brief Mock SoupMessage structure */
struct SoupMessage {
  /** @brief HTTP method */
  char *method;
  /** @brief URL */
  char *url;
  /** @brief Request headers */
  SoupMessageHeaders headers;
  /** @brief Request body bytes */
  GBytes *request_body;
  /** @brief HTTP response status code */
  unsigned int status_code;
};
typedef struct SoupMessage SoupMessage;

/** @brief Mock SoupSession structure */
struct SoupSession {
  /** @brief Session timeout */
  guint timeout;
  /** @brief User agent string */
  char *user_agent;
  /** @brief Proxy URI */
  GUri *proxy_uri;
};
typedef struct SoupSession SoupSession;

/**
 * @brief Create a new mock SoupSession.
 *
 * @return Pointer to session or NULL on failure.
 */
static SoupSession *soup_session_new(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_session_new_fail) {
    return NULL;
  }
#endif
  return (SoupSession *)calloc(1, sizeof(SoupSession));
}

/**
 * @brief Create a new mock SoupMessage.
 *
 * @param[in] method HTTP method string.
 * @param[in] uri URL string.
 * @return Pointer to message or NULL on failure.
 */
static SoupMessage *soup_message_new(const char *method, const char *uri) {
  SoupMessage *msg;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_msg_new_fail == 1) {
    return NULL;
  }
#endif
  if (!method || !uri) {
    return NULL;
  }
  msg = (SoupMessage *)calloc(1, sizeof(SoupMessage));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_msg_new_fail == 2) {
    free(msg);
    msg = NULL;
  }
#endif
  if (!msg) {
    return NULL;
  }
  c_abstract_http_strdup(method, &msg->method);
  c_abstract_http_strdup(uri, &msg->url);
  msg->status_code = 200;
  return msg;
}

/**
 * @brief Get request headers from mock SoupMessage.
 *
 * @param[in] msg Message.
 * @return Pointer to headers.
 */
static SoupMessageHeaders *soup_message_get_request_headers(SoupMessage *msg) {
  return msg ? &msg->headers : NULL;
}

/**
 * @brief Append header to mock headers.
 *
 * @param[in,out] hdrs Headers structure.
 * @param[in] name Header name.
 * @param[in] value Header value.
 */
static void soup_message_headers_append(SoupMessageHeaders *hdrs,
                                        const char *name, const char *value) {
  if (hdrs && name && value && hdrs->count < 32) {
    c_abstract_http_strdup(name, &hdrs->keys[hdrs->count]);
    c_abstract_http_strdup(value, &hdrs->values[hdrs->count]);
    hdrs->count++;
  }
}

/**
 * @brief Create mock GBytes.
 *
 * @param[in] data Data pointer.
 * @param[in] size Size of data.
 * @return Pointer to GBytes or NULL on failure.
 */
static GBytes *g_bytes_new(const void *data, size_t size) {
  GBytes *b = (GBytes *)calloc(1, sizeof(GBytes));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_msg_new_fail == 3) {
    free(b);
    b = NULL;
  }
#endif
  if (!b) {
    return NULL;
  }
  if (data && size > 0) {
    b->data = malloc(size);
    if (b->data) {
      memcpy(b->data, data, size);
      b->size = size;
    }
  }
  return b;
}

/**
 * @brief Get data and size from mock GBytes.
 *
 * @param[in] bytes GBytes object.
 * @param[out] size Pointer to receive size.
 * @return Pointer to data.
 */
static gconstpointer g_bytes_get_data(GBytes *bytes, gsize *size) {
  if (!bytes) {
    if (size) {
      *size = 0;
    }
    return NULL;
  }
  if (size) {
    *size = (gsize)bytes->size;
  }
  return bytes->data;
}

/**
 * @brief Free mock GBytes.
 *
 * @param[in] bytes GBytes to free.
 */
static void g_bytes_unref(GBytes *bytes) {
  if (bytes) {
    if (bytes->data) {
      free(bytes->data);
    }
    free(bytes);
  }
}

/**
 * @brief Set request body on mock SoupMessage.
 *
 * @param[in,out] msg Message.
 * @param[in] content_type Content type.
 * @param[in] bytes Body bytes.
 */
static void soup_message_set_request_body_from_bytes(SoupMessage *msg,
                                                     const char *content_type,
                                                     GBytes *bytes) {
  (void)content_type;
  if (msg) {
    msg->request_body = bytes;
  }
}

/**
 * @brief Create mock GByteArray.
 *
 * @return Pointer to GByteArray.
 */
static GByteArray *g_byte_array_new(void) {
  return (GByteArray *)calloc(1, sizeof(GByteArray));
}

/**
 * @brief Append data to mock GByteArray.
 *
 * @param[in,out] arr Array.
 * @param[in] data Data.
 * @param[in] len Length.
 * @return Array pointer.
 */
static GByteArray *g_byte_array_append(GByteArray *arr, const guint8 *data,
                                       guint len) {
  if (arr && data && len > 0) {
    if (arr->len + len > arr->cap) {
      size_t new_cap = arr->cap == 0 ? 128 : arr->cap * 2;
      guint8 *new_data;
      while (new_cap < arr->len + len) {
        new_cap *= 2;
      }
      new_data = (guint8 *)realloc(arr->data, new_cap);
      if (new_data) {
        arr->data = new_data;
        arr->cap = new_cap;
      }
    }
    if (arr->data && arr->len + len <= arr->cap) {
      memcpy(arr->data + arr->len, data, len);
      arr->len += len;
    }
  }
  return arr;
}

/**
 * @brief Free mock GByteArray.
 *
 * @param[in] arr Array.
 * @param[in] free_segment Free segment flag.
 * @return NULL.
 */
static guint8 *g_byte_array_free(GByteArray *arr, gboolean free_segment) {
  if (arr) {
    if (free_segment && arr->data) {
      free(arr->data);
    }
    free(arr);
  }
  return NULL;
}

/**
 * @brief Free mock GByteArray to GBytes.
 *
 * @param[in] arr Array.
 * @return Pointer to GBytes.
 */
static GBytes *g_byte_array_free_to_bytes(GByteArray *arr) {
  GBytes *b;
  if (!arr) {
    return NULL;
  }
  b = (GBytes *)calloc(1, sizeof(GBytes));
  if (b) {
    b->data = arr->data;
    b->size = arr->len;
  }
  free(arr);
  return b;
}

/**
 * @brief Set mock object properties.
 *
 * @param[in,out] obj Object pointer.
 * @param[in] first_property_name Property name.
 */
static void g_object_set(void *obj, const char *first_property_name, ...) {
  va_list args;
  const char *prop;
  SoupSession *session = (SoupSession *)obj;

  if (!obj || !first_property_name) {
    return;
  }

  va_start(args, first_property_name);
  prop = first_property_name;
  while (prop) {
    if (strcmp(prop, "timeout") == 0) {
      session->timeout = va_arg(args, guint);
    } else if (strcmp(prop, "user-agent") == 0) {
      const char *ua = va_arg(args, const char *);
      if (session->user_agent) {
        free(session->user_agent);
        session->user_agent = NULL;
      }
      if (ua) {
        c_abstract_http_strdup(ua, &session->user_agent);
      }
    } else if (strcmp(prop, "proxy-uri") == 0) {
      session->proxy_uri = va_arg(args, GUri *);
    } else {
      (void)va_arg(args, void *);
    }
    prop = va_arg(args, const char *);
  }
  va_end(args);
}

/**
 * @brief Free mock object.
 *
 * @param[in] obj Object to free.
 */
static void g_object_unref(void *obj) {
  if (obj) {
    SoupSession *s = (SoupSession *)obj;
    if (s->user_agent) {
      free(s->user_agent);
    }
    free(s);
  }
}

/**
 * @brief Free mock GError.
 *
 * @param[in] error Error to free.
 */
static void g_error_free(GError *error) {
  if (error) {
    if (error->message) {
      free(error->message);
    }
    free(error);
  }
}

/**
 * @brief Check if mock error matches domain and code.
 *
 * @param[in] error Error.
 * @param[in] domain Domain.
 * @param[in] code Code.
 * @return TRUE if matching, FALSE otherwise.
 */
static gboolean g_error_matches(const GError *error, int domain, int code) {
  return error && error->domain == domain && error->code == code;
}

/**
 * @brief Parse mock URI.
 *
 * @param[in] uri URI string.
 * @param[in] flags Flags.
 * @param[out] error Optional error pointer.
 * @return Pointer to GUri or NULL on failure.
 */
static GUri *g_uri_parse(const char *uri, int flags, GError **error) {
  GUri *u;
  (void)flags;
  (void)error;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_uri_parse_fail) {
    return NULL;
  }
#endif
  if (!uri) {
    return NULL;
  }
  u = (GUri *)calloc(1, sizeof(GUri));
  if (u) {
    c_abstract_http_strdup(uri, &u->uri_string);
  }
  return u;
}

/**
 * @brief Free mock GUri.
 *
 * @param[in] uri URI to free.
 */
static void g_uri_unref(GUri *uri) {
  if (uri) {
    if (uri->uri_string) {
      free(uri->uri_string);
    }
    free(uri);
  }
}

/**
 * @brief Get response status code from mock message.
 *
 * @param[in] msg Message.
 * @return HTTP status code.
 */
static guint soup_message_get_status(const SoupMessage *msg) {
  return msg ? msg->status_code : 0;
}

/**
 * @brief Send request and read response synchronously on mock session.
 *
 * @param[in] session Session.
 * @param[in] msg Message.
 * @param[in] cancellable Cancellable.
 * @param[out] error Error pointer.
 * @return GBytes response or NULL on failure.
 */
static GBytes *soup_session_send_and_read(SoupSession *session,
                                          SoupMessage *msg, void *cancellable,
                                          GError **error) {
  (void)session;
  (void)cancellable;
  if (error) {
    *error = NULL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_send_fail == 1) {
    if (error) {
      *error = (GError *)calloc(1, sizeof(GError));
      if (*error) {
        (*error)->domain = G_IO_ERROR;
        (*error)->code = G_IO_ERROR_CONNECTION_REFUSED;
        c_abstract_http_strdup("Connection refused", &(*error)->message);
      }
    }
    return NULL;
  }
  if (g_mock_libsoup3_send_fail == 2) {
    if (error) {
      *error = (GError *)calloc(1, sizeof(GError));
      if (*error) {
        (*error)->domain = G_IO_ERROR;
        (*error)->code = G_IO_ERROR_TIMED_OUT;
        c_abstract_http_strdup("Timed out", &(*error)->message);
      }
    }
    return NULL;
  }
  if (g_mock_libsoup3_send_fail == 3) {
    if (error) {
      *error = (GError *)calloc(1, sizeof(GError));
      if (*error) {
        (*error)->domain = G_IO_ERROR;
        (*error)->code = 999;
        c_abstract_http_strdup("Generic IO error", &(*error)->message);
      }
    }
    return NULL;
  }
#endif
  if (msg && msg->url && strstr(msg->url, "59999")) {
    if (error) {
      *error = (GError *)calloc(1, sizeof(GError));
      if (*error) {
        (*error)->domain = G_IO_ERROR;
        (*error)->code = G_IO_ERROR_CONNECTION_REFUSED;
        c_abstract_http_strdup("Connection refused", &(*error)->message);
      }
    }
    return NULL;
  }
  return g_bytes_new("OK", 2);
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error c_abstract_http_test_libsoup3_helpers(void);

/**
 * @brief Expose internal helpers for test coverage.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_libsoup3_helpers(void) {
  gsize sz = 0;
  GBytes *b;
  SoupSession *sess;

  g_mock_libsoup3_msg_new_fail = 2;
  soup_message_new("GET", "http://example.com");
  g_mock_libsoup3_msg_new_fail = 3;
  g_bytes_new("A", 1);
  g_mock_libsoup3_msg_new_fail = 0;

  b = g_bytes_new("A", 1);
  sess = soup_session_new();
  soup_message_new(NULL, NULL);
  g_bytes_get_data(NULL, NULL);
  g_bytes_get_data(NULL, &sz);
  g_bytes_get_data(b, NULL);
  g_bytes_unref(b);
  g_byte_array_free_to_bytes(NULL);
  {
    GByteArray *tmp = g_byte_array_new();
    tmp->data = (guint8 *)malloc(1);
    g_byte_array_free(tmp, TRUE);
    tmp = g_byte_array_new();
    g_byte_array_free(tmp, FALSE);
    g_byte_array_free(NULL, FALSE);
  }
  g_object_set(NULL, NULL);
  g_object_set(sess, "user-agent", "agent", NULL);
  g_object_set(sess, "unknown", NULL, NULL);
  g_object_unref(sess);
  g_uri_parse(NULL, 0, NULL);
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBSOUP3) */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief SoupSession pointer */
  SoupSession *session;
  /** @brief Cookie jar */
  struct HttpCookieJar *cookie_jar;
  /** @brief Configuration */
  struct HttpConfig config;
};

static int g_libsoup3_init_count = 0;

/**
 * @brief Initialize global libsoup3 state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libsoup3_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  g_libsoup3_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global libsoup3 state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libsoup3_global_cleanup(void) {
  if (g_libsoup3_init_count > 0) {
    g_libsoup3_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize libsoup3 transport context.
 *
 * @param[out] ctx Double pointer to receive context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_libsoup3_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_libsoup3_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libsoup3_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_context_init_fail) {
    *ctx = NULL;
  } else
#endif
  {
    *ctx = (struct HttpTransportContext *)malloc(
        sizeof(struct HttpTransportContext));
  }
  if (!*ctx) {
    LOG_DEBUG("http_libsoup3_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&(*ctx)->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_libsoup3_context_init: Error http_config_init failed with %d",
        (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->session = soup_session_new();
  if (!(*ctx)->session) {
    LOG_DEBUG("http_libsoup3_context_init: Error soup_session_new failed");
    http_config_free(&(*ctx)->config);
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  (*ctx)->cookie_jar = NULL;

  LOG_DEBUG("http_libsoup3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free libsoup3 transport context.
 *
 * @param[in] ctx Context to free.
 */
void http_libsoup3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libsoup3_context_free: Entering");
  if (ctx) {
    if (ctx->session) {
      g_object_unref(ctx->session);
    }
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_libsoup3_context_free: Exiting");
}

/**
 * @brief Apply configuration to libsoup3 context.
 *
 * @param[in,out] ctx Transport context.
 * @param[in] config Configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_libsoup3_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_libsoup3_config_apply: Entering");
  if (!ctx || !ctx->session || !config) {
    LOG_DEBUG("http_libsoup3_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (config->timeout_ms > 0) {
    g_object_set(ctx->session, "timeout", (guint)(config->timeout_ms / 1000),
                 NULL);
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;

  if (config->user_agent) {
    if (ctx->config.user_agent) {
      free(ctx->config.user_agent);
      ctx->config.user_agent = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    g_object_set(ctx->session, "user-agent", config->user_agent, NULL);
  } else if (ctx->config.user_agent) {
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
    g_object_set(ctx->session, "user-agent", NULL, NULL);
  }

  if (config->proxy_url) {
    GUri *proxy_uri = g_uri_parse(config->proxy_url, G_URI_FLAGS_NONE, NULL);
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      if (proxy_uri) {
        g_uri_unref(proxy_uri);
      }
      return rc;
    }
    if (proxy_uri) {
      g_object_set(ctx->session, "proxy-uri", proxy_uri, NULL);
      g_uri_unref(proxy_uri);
    } else {
      LOG_DEBUG(
          "http_libsoup3_config_apply: Error g_uri_parse failed for proxy");
    }
  } else {
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
    g_object_set(ctx->session, "proxy-uri", NULL, NULL);
  }

  if (config->proxy_username) {
    if (ctx->config.proxy_username) {
      free(ctx->config.proxy_username);
      ctx->config.proxy_username = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_username,
                                  &ctx->config.proxy_username);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_username) {
    free(ctx->config.proxy_username);
    ctx->config.proxy_username = NULL;
  }

  if (config->proxy_password) {
    if (ctx->config.proxy_password) {
      free(ctx->config.proxy_password);
      ctx->config.proxy_password = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_password,
                                  &ctx->config.proxy_password);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_password) {
    free(ctx->config.proxy_password);
    ctx->config.proxy_password = NULL;
  }

  if (config->cookie_jar) {
    ctx->cookie_jar = config->cookie_jar;
  } else {
    ctx->cookie_jar = NULL;
  }

  LOG_DEBUG("http_libsoup3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HttpMethod enum to method string.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out Pointer to receive string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error get_method_string(enum HttpMethod method,
                                                    const char **out) {
  if (!out) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (method) {
  case HTTP_GET:
    *out = "GET";
    break;
  case HTTP_POST:
    *out = "POST";
    break;
  case HTTP_PUT:
    *out = "PUT";
    break;
  case HTTP_DELETE:
    *out = "DELETE";
    break;
  case HTTP_HEAD:
    *out = "HEAD";
    break;
  case HTTP_PATCH:
    *out = "PATCH";
    break;
  case HTTP_QUERY:
    *out = "QUERY";
    break;
  case HTTP_OPTIONS:
    *out = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out = "TRACE";
    break;
  case HTTP_CONNECT:
    *out = "CONNECT";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error
c_abstract_http_test_libsoup3_method_str(enum HttpMethod method,
                                         const char **out);

/**
 * @brief Expose get_method_string for testing.
 *
 * @param[in] method HTTP method.
 * @param[out] out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
c_abstract_http_test_libsoup3_method_str(enum HttpMethod method,
                                         const char **out) {
  return get_method_string(method, out);
}
#endif

/**
 * @brief Send an HTTP request using libsoup3.
 *
 * @param[in] ctx Transport context.
 * @param[in] req Request structure.
 * @param[out] res Pointer to receive response object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libsoup3_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  SoupMessage *msg = NULL;
  SoupMessageHeaders *req_headers = NULL;
  GBytes *body_bytes = NULL;
  GBytes *resp_bytes = NULL;
  GError *error = NULL;
  struct HttpResponse *new_res = NULL;
  size_t i;
  void *payload = NULL;
  size_t payload_len = 0;
  const char *method_str = NULL;
  gsize resp_len = 0;
  gconstpointer resp_data = NULL;
  char *response_body_copy = NULL;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_libsoup3_send: Entering");
  if (!ctx || !ctx->session || !req || !res) {
    LOG_DEBUG("http_libsoup3_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  payload = req->body;
  payload_len = req->body_len;

  if (req->parts.count > 0 && !payload) {
    LOG_DEBUG(
        "http_libsoup3_send: Error EINVAL (multipart parts with no payload)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = get_method_string(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libsoup3_send: Error get_method_string failed %d", (int)rc);
    return rc;
  }

  msg = soup_message_new(method_str, req->url);
  if (!msg) {
    LOG_DEBUG("http_libsoup3_send: Error soup_message_new failed (bad URL?)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  req_headers = soup_message_get_request_headers(msg);
  for (i = 0; i < req->headers.count; ++i) {
    soup_message_headers_append(req_headers, req->headers.headers[i].key,
                                req->headers.headers[i].value);
  }

  if (payload && payload_len > 0) {
    body_bytes = g_bytes_new(payload, payload_len);
    soup_message_set_request_body_from_bytes(msg, NULL, body_bytes);
    g_bytes_unref(body_bytes);
  } else if (req->read_chunk) {
    char buf[4096];
    size_t out_read;
    GByteArray *byte_array = g_byte_array_new();

    for (;;) {
      int chunk_rc = req->read_chunk(req->read_chunk_user_data, buf,
                                     sizeof(buf), &out_read);
      if (chunk_rc != 0) {
        g_byte_array_free(byte_array, TRUE);
        rc = C_ABSTRACT_HTTP_ERR_IO;
        goto cleanup;
      }
      if (out_read == 0) {
        break;
      }
      g_byte_array_append(byte_array, (const guint8 *)buf, (guint)out_read);
    }

    body_bytes = g_byte_array_free_to_bytes(byte_array);
    soup_message_set_request_body_from_bytes(msg, NULL, body_bytes);
    g_bytes_unref(body_bytes);
  }

  resp_bytes = soup_session_send_and_read(ctx->session, msg, NULL, &error);

  if (error) {
    LOG_DEBUG("http_libsoup3_send: Error g_error %d: %s", error->code,
              error->message);
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
      rc = (enum c_abstract_http_error)ECONNREFUSED;
    } else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
      rc = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    } else {
      rc = C_ABSTRACT_HTTP_ERR_IO;
    }
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_res_alloc_fail) {
    new_res = NULL;
  } else
#endif
  {
    new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!new_res) {
    LOG_DEBUG("http_libsoup3_send: Error ENOMEM allocating new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_res_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(new_res);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libsoup3_send: Error http_response_init failed with %d",
              (int)rc);
    free(new_res);
    new_res = NULL;
    goto cleanup;
  }

  new_res->status_code = (int)soup_message_get_status(msg);

  if (resp_bytes) {
    resp_data = g_bytes_get_data(resp_bytes, &resp_len);
    if (resp_data && resp_len > 0) {
      if (req->on_chunk) {
        int chunk_rc =
            req->on_chunk(req->on_chunk_user_data, resp_data, resp_len);
        if (chunk_rc != 0) {
          LOG_DEBUG("http_libsoup3_send: Error on_chunk failed %d", chunk_rc);
          rc = (enum c_abstract_http_error)ECANCELED;
          free(new_res);
          new_res = NULL;
          goto cleanup;
        }
        new_res->body = NULL;
        new_res->body_len = 0;
      } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_libsoup3_body_alloc_fail) {
          response_body_copy = NULL;
        } else
#endif
        {
          response_body_copy = (char *)malloc(resp_len + 1);
        }
        if (!response_body_copy) {
          LOG_DEBUG(
              "http_libsoup3_send: Error ENOMEM allocating response_body_copy");
          free(new_res);
          new_res = NULL;
          rc = C_ABSTRACT_HTTP_ERR_NOMEM;
          goto cleanup;
        }
        memcpy(response_body_copy, resp_data, resp_len);
        response_body_copy[resp_len] = '\0';
        new_res->body = response_body_copy;
        new_res->body_len = resp_len;
      }
    }
  }

  *res = new_res;

cleanup:
  if (error) {
    g_error_free(error);
  }
  if (resp_bytes) {
    g_bytes_unref(resp_bytes);
  }
  if (msg) {
    g_object_unref(msg);
  }

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libsoup3_send: Success");
  } else {
    LOG_DEBUG("http_libsoup3_send: Error returning %d", (int)rc);
  }
  return rc;
}
