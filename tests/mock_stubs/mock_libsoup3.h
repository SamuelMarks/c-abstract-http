/**
 * @file mock_libsoup3.h
 * @brief Internal test-only libsoup3/GLib C-library stubs for OOM and fault
 * injection.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_LIBSOUP3_H
#define C_ABSTRACT_HTTP_TEST_MOCK_LIBSOUP3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

extern int g_mock_libsoup3_global_init_fail;
extern int g_mock_libsoup3_context_init_fail;
extern int g_mock_libsoup3_config_init_fail;
extern int g_mock_libsoup3_session_new_fail;
extern int g_mock_libsoup3_msg_new_fail;
extern int g_mock_libsoup3_send_fail;
extern int g_mock_libsoup3_res_alloc_fail;
extern int g_mock_libsoup3_res_init_fail;
extern int g_mock_libsoup3_body_alloc_fail;
extern int g_mock_libsoup3_uri_parse_fail;

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
#define G_PRIORITY_DEFAULT 0
#define SOUP_MESSAGE_NO_REDIRECT (1U << 1)

typedef void *GAsyncResult;
typedef void (*GAsyncReadyCallback)(void *source_object, GAsyncResult *res,
                                    void *user_data);

/** @brief Mock GMainContext structure */
typedef struct GMainContext {
  int dummy;
} GMainContext;

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
  /** @brief GObject mock kind (2 = SoupMessage) */
  int obj_kind;
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
  /** @brief Message flags */
  unsigned int flags;
};
typedef struct SoupMessage SoupMessage;

struct mock_soup_async_item {
  SoupMessage *msg;
  GAsyncReadyCallback cb;
  void *user_data;
  int active;
};

/** @brief Mock SoupSession structure */
struct SoupSession {
  /** @brief GObject mock kind (1 = SoupSession) */
  int obj_kind;
  /** @brief Session timeout */
  guint timeout;
  /** @brief Session idle timeout */
  guint idle_timeout;
  /** @brief User agent string */
  char *user_agent;
  /** @brief Proxy URI */
  GUri *proxy_uri;
  /** @brief Queued async operations */
  struct mock_soup_async_item async_items[32];
  /** @brief Queued async count */
  size_t async_count;
};
typedef struct SoupSession SoupSession;

static SoupSession *g_mock_active_soup_session = NULL;

static SoupSession *soup_session_new(void) {
  SoupSession *s;
  if (g_mock_libsoup3_session_new_fail) {
    return NULL;
  }
  s = (SoupSession *)calloc(1, sizeof(SoupSession));
  if (s) {
    s->obj_kind = 1;
  }
  return s;
}

static SoupMessage *soup_message_new(const char *method, const char *uri) {
  SoupMessage *msg;
  if (g_mock_libsoup3_msg_new_fail == 1) {
    return NULL;
  }
  if (!uri) {
    return NULL;
  }
  msg = (SoupMessage *)calloc(1, sizeof(SoupMessage));
  if (g_mock_libsoup3_msg_new_fail == 2) {
    free(msg);
    msg = NULL;
  }
  if (!msg) {
    return NULL;
  }
  msg->obj_kind = 2;
  if (c_abstract_http_strdup(method, &msg->method) != C_ABSTRACT_HTTP_SUCCESS) {
    free(msg);
    return NULL;
  }
  if (c_abstract_http_strdup(uri, &msg->url) != C_ABSTRACT_HTTP_SUCCESS) {
    free(msg->method);
    free(msg);
    return NULL;
  }
  msg->status_code = 200;
  return msg;
}

static SoupMessageHeaders *soup_message_get_request_headers(SoupMessage *msg) {
  return &msg->headers;
}

static void soup_message_headers_append(SoupMessageHeaders *hdrs,
                                        const char *name, const char *value) {
  if (c_abstract_http_strdup(name, &hdrs->keys[hdrs->count]) !=
      C_ABSTRACT_HTTP_SUCCESS)
    return;
  if (c_abstract_http_strdup(value, &hdrs->values[hdrs->count]) !=
      C_ABSTRACT_HTTP_SUCCESS) {
    free(hdrs->keys[hdrs->count]);
    hdrs->keys[hdrs->count] = NULL;
    return;
  }
  hdrs->count++;
}

static GBytes *g_bytes_new(const void *data, size_t size) {
  GBytes *b = (GBytes *)calloc(1, sizeof(GBytes));
  if (!b || g_mock_libsoup3_msg_new_fail == 3) {
    free(b);
    return NULL;
  }
  b->data = malloc(size);
  if (!b->data) {
    free(b);
    return NULL;
  }
  memcpy(b->data, data, size);
  b->size = size;
  return b;
}

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

static void g_bytes_unref(GBytes *bytes) {
  if (bytes) {
    free(bytes->data);
    free(bytes);
  }
}

static void soup_message_set_request_body_from_bytes(SoupMessage *msg,
                                                     const char *content_type,
                                                     GBytes *bytes) {
  (void)content_type;
  msg->request_body = bytes;
}

static GByteArray *g_byte_array_new(void) {
  return (GByteArray *)calloc(1, sizeof(GByteArray));
}

static GByteArray *g_byte_array_append(GByteArray *arr, const guint8 *data,
                                       guint len) {
  if (arr->len + len > arr->cap) {
    size_t new_cap = arr->cap == 0 ? 128 : arr->cap * 2;
    while (new_cap < arr->len + len) {
      new_cap *= 2;
    }
    arr->data = (guint8 *)realloc(arr->data, new_cap);
    arr->cap = new_cap;
  }
  memcpy(arr->data + arr->len, data, len);
  arr->len += len;
  return arr;
}

static guint8 *g_byte_array_free(GByteArray *arr, gboolean free_segment) {
  if (arr) {
    if (free_segment && arr->data) {
      free(arr->data);
    }
    free(arr);
  }
  return NULL;
}

static GBytes *g_byte_array_free_to_bytes(GByteArray *arr) {
  GBytes *b;
  if (!arr) {
    return NULL;
  }
  b = (GBytes *)calloc(1, sizeof(GBytes));
  if (!b) {
    free(arr->data);
    free(arr);
    return NULL;
  }
  b->data = arr->data;
  b->size = arr->len;
  free(arr);
  return b;
}

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
    } else if (strcmp(prop, "idle-timeout") == 0) {
      session->idle_timeout = va_arg(args, guint);
    } else if (strcmp(prop, "user-agent") == 0) {
      const char *ua = va_arg(args, const char *);
      if (session->user_agent) {
        free(session->user_agent);
        session->user_agent = NULL;
      }
      if (ua) {
        if (c_abstract_http_strdup(ua, &session->user_agent) !=
            C_ABSTRACT_HTTP_SUCCESS) {
          va_end(args);
          return;
        }
      }
    } else if (strcmp(prop, "proxy-uri") == 0) {
      session->proxy_uri = va_arg(args, GUri *);
    } else {
      void *ignored_arg = va_arg(args, void *);
      if (ignored_arg) {
        /* property ignored */
      }
    }
    prop = va_arg(args, const char *);
  }
  va_end(args);
}

static void g_object_unref(void *obj) {
  if (obj) {
    int kind = *(int *)obj;
    if (kind == 1) {
      SoupSession *s = (SoupSession *)obj;
      if (g_mock_active_soup_session == s) {
        g_mock_active_soup_session = NULL;
      }
      free(s->user_agent);
      free(s);
    } else if (kind == 2) {
      SoupMessage *m = (SoupMessage *)obj;
      size_t i;
      for (i = 0; i < m->headers.count; ++i) {
        free(m->headers.keys[i]);
        free(m->headers.values[i]);
      }
      free(m->method);
      free(m->url);
      free(m);
    } else {
      free(obj);
    }
  }
}

static void g_error_free(GError *error) {
  if (error) {
    free(error->message);
    free(error);
  }
}

static gboolean g_error_matches(const GError *error, int domain, int code) {
  if (!error) {
    return FALSE;
  }
  return (error->domain == domain && error->code == code);
}

static GUri *g_uri_parse(const char *uri, int flags, GError **error) {
  GUri *u;
  (void)flags;
  (void)error;
  if (g_mock_libsoup3_uri_parse_fail) {
    return NULL;
  }
  if (!uri) {
    return NULL;
  }
  u = (GUri *)calloc(1, sizeof(GUri));
  if (!u) {
    return NULL;
  }
  if (c_abstract_http_strdup(uri, &u->uri_string) != C_ABSTRACT_HTTP_SUCCESS) {
    free(u);
    return NULL;
  }
  return u;
}

static void g_uri_unref(GUri *uri) {
  if (uri) {
    free(uri->uri_string);
    free(uri);
  }
}

static guint soup_message_get_status(const SoupMessage *msg) {
  return msg->status_code;
}

static void soup_message_set_flags(SoupMessage *msg, unsigned int flags) {
  if (msg) {
    msg->flags = flags;
  }
}

static GMainContext *g_main_context_default(void) {
  static GMainContext g_def_ctx = {0};
  return &g_def_ctx;
}

static void soup_session_send_and_read_async(SoupSession *session,
                                             SoupMessage *msg, int io_priority,
                                             void *cancellable,
                                             GAsyncReadyCallback callback,
                                             void *user_data) {
  struct mock_soup_async_item *it;
  (void)io_priority;
  (void)cancellable;
  if (!session || session->async_count >= 32) {
    return;
  }
  g_mock_active_soup_session = session;
  it = &session->async_items[session->async_count++];
  it->msg = msg;
  it->cb = callback;
  it->user_data = user_data;
  it->active = 1;
}

static GBytes *soup_session_send_and_read(SoupSession *session,
                                          SoupMessage *msg, void *cancellable,
                                          GError **error);

static GBytes *soup_session_send_and_read_finish(SoupSession *session,
                                                 GAsyncResult *result,
                                                 GError **error) {
  SoupMessage *msg = (SoupMessage *)result;
  return soup_session_send_and_read(session, msg, NULL, error);
}

static gboolean g_main_context_iteration(GMainContext *context,
                                         gboolean may_block) {
  size_t i;
  gboolean dispatched = FALSE;
  SoupSession *s = g_mock_active_soup_session;
  (void)context;
  (void)may_block;
  if (!s) {
    return FALSE;
  }
  for (i = 0; i < s->async_count; ++i) {
    if (s->async_items[i].active) {
      s->async_items[i].active = 0;
      dispatched = TRUE;
      if (s->async_items[i].cb) {
        s->async_items[i].cb(s, (GAsyncResult *)s->async_items[i].msg,
                             s->async_items[i].user_data);
      }
    }
  }
  s->async_count = 0;
  return dispatched;
}

static GBytes *soup_session_send_and_read(SoupSession *session,
                                          SoupMessage *msg, void *cancellable,
                                          GError **error) {
  enum c_abstract_http_error rc;
  (void)session;
  (void)cancellable;
  *error = NULL;
  if (g_mock_libsoup3_send_fail == 1) {
    *error = (GError *)calloc(1, sizeof(GError));
    (*error)->domain = G_IO_ERROR;
    (*error)->code = G_IO_ERROR_CONNECTION_REFUSED;
    rc = c_abstract_http_strdup("Connection refused", &(*error)->message);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return NULL;
    return NULL;
  }
  if (g_mock_libsoup3_send_fail == 2) {
    *error = (GError *)calloc(1, sizeof(GError));
    (*error)->domain = G_IO_ERROR;
    (*error)->code = G_IO_ERROR_TIMED_OUT;
    rc = c_abstract_http_strdup("Timed out", &(*error)->message);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return NULL;
    return NULL;
  }
  if (g_mock_libsoup3_send_fail == 3) {
    *error = (GError *)calloc(1, sizeof(GError));
    (*error)->domain = G_IO_ERROR;
    (*error)->code = 999;
    rc = c_abstract_http_strdup("Generic IO error", &(*error)->message);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return NULL;
    return NULL;
  }
  if (strstr(msg->url, "59999")) {
    *error = (GError *)calloc(1, sizeof(GError));
    (*error)->domain = G_IO_ERROR;
    (*error)->code = G_IO_ERROR_CONNECTION_REFUSED;
    rc = c_abstract_http_strdup("Connection refused", &(*error)->message);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return NULL;
    return NULL;
  }
  return g_bytes_new("OK", 2);
}

enum c_abstract_http_error c_abstract_http_test_libsoup3_helpers(void);
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
  g_bytes_unref(NULL);
  g_byte_array_free_to_bytes(NULL);
  {
    GByteArray *arr = g_byte_array_new();
    guint8 big[256];
    memset(big, 'X', sizeof(big));
    g_byte_array_append(arr, big, 10);
    g_byte_array_append(arr, big, 200);
    g_byte_array_append(arr, big, 10);
    g_byte_array_free(arr, TRUE);
  }
  {
    GByteArray *tmp = g_byte_array_new();
    tmp->data = (guint8 *)malloc(1);
    g_byte_array_free(tmp, TRUE);
    tmp = g_byte_array_new();
    g_byte_array_free(tmp, FALSE);
    g_byte_array_free(NULL, FALSE);
  }
  g_object_set(NULL, NULL);
  g_object_set(sess, NULL);
  g_object_set(sess, "user-agent", "agent", NULL);
  g_object_set(sess, "unknown", NULL, NULL);
  g_object_set(sess, "unknown", "val", NULL);
  g_object_unref(sess);
  g_object_unref(NULL);
  g_error_free(NULL);
  {
    GError err;
    memset(&err, 0, sizeof(err));
    err.domain = 1;
    err.code = 2;
    g_error_matches(NULL, 1, 2);
    g_error_matches(&err, 999, 2);
    g_error_matches(&err, 1, 999);
  }
  g_uri_parse(NULL, 0, NULL);
  g_uri_unref(NULL);
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBSOUP3) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_LIBSOUP3_H */
