/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libevent.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "str.h"

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_libevent_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_base_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_conn_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_req_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_make_req_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_buf_add_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_empty_body(void);

#define g_mock_libevent_global_init_fail                                       \
  (*abstract_http_mock_get_g_mock_libevent_global_init_fail())
#define g_mock_libevent_context_init_fail                                      \
  (*abstract_http_mock_get_g_mock_libevent_context_init_fail())
#define g_mock_libevent_config_init_fail                                       \
  (*abstract_http_mock_get_g_mock_libevent_config_init_fail())
#define g_mock_libevent_base_new_fail                                          \
  (*abstract_http_mock_get_g_mock_libevent_base_new_fail())
#define g_mock_libevent_conn_new_fail                                          \
  (*abstract_http_mock_get_g_mock_libevent_conn_new_fail())
#define g_mock_libevent_req_new_fail                                           \
  (*abstract_http_mock_get_g_mock_libevent_req_new_fail())
#define g_mock_libevent_make_req_fail                                          \
  (*abstract_http_mock_get_g_mock_libevent_make_req_fail())
#define g_mock_libevent_res_alloc_fail                                         \
  (*abstract_http_mock_get_g_mock_libevent_res_alloc_fail())
#define g_mock_libevent_res_init_fail                                          \
  (*abstract_http_mock_get_g_mock_libevent_res_init_fail())
#define g_mock_libevent_body_alloc_fail                                        \
  (*abstract_http_mock_get_g_mock_libevent_body_alloc_fail())
#define g_mock_libevent_buf_add_fail                                           \
  (*abstract_http_mock_get_g_mock_libevent_buf_add_fail())
#define g_mock_libevent_empty_body                                             \
  (*abstract_http_mock_get_g_mock_libevent_empty_body())
#endif

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)

#define EVHTTP_REQ_GET (1 << 0)
#define EVHTTP_REQ_POST (1 << 1)
#define EVHTTP_REQ_HEAD (1 << 2)
#define EVHTTP_REQ_PUT (1 << 3)
#define EVHTTP_REQ_DELETE (1 << 4)
#define EVHTTP_REQ_OPTIONS (1 << 5)
#define EVHTTP_REQ_TRACE (1 << 6)
#define EVHTTP_REQ_CONNECT (1 << 7)
#define EVHTTP_REQ_PATCH (1 << 8)

/** @brief Mock evbuffer structure */
struct evbuffer {
  /** @brief Buffer data pointer */
  char *data;
  /** @brief Current data length */
  size_t len;
  /** @brief Allocated capacity */
  size_t cap;
};

/** @brief Mock key-value entry for headers */
struct evkeyval {
  /** @brief Header key */
  char *key;
  /** @brief Header value */
  char *val;
};

/** @brief Mock key-value queue for headers */
struct evkeyvalq {
  /** @brief Header entries */
  struct evkeyval entries[32];
  /** @brief Number of header entries */
  size_t count;
};

struct evhttp_request;

/** @brief Mock callback type for request completion */
typedef void (*evhttp_request_cb_t)(struct evhttp_request *, void *);
/** @brief Mock callback type for chunk processing */
typedef void (*evhttp_chunk_cb_t)(struct evhttp_request *, void *);

/** @brief Mock evhttp_request structure */
struct evhttp_request {
  /** @brief Done callback */
  evhttp_request_cb_t cb;
  /** @brief Done callback argument */
  void *cb_arg;
  /** @brief Chunk callback */
  evhttp_chunk_cb_t chunk_cb;
  /** @brief Output headers */
  struct evkeyvalq output_headers;
  /** @brief Output buffer */
  struct evbuffer output_buffer;
  /** @brief Input buffer */
  struct evbuffer input_buffer;
  /** @brief HTTP response code */
  int response_code;
};

struct event_base;

/** @brief Mock evhttp_connection structure */
struct evhttp_connection {
  /** @brief Parent event base */
  struct event_base *base;
  /** @brief Remote host name */
  char host[256];
  /** @brief Remote port */
  int port;
  /** @brief Timeout in seconds */
  long timeout_sec;
};

/** @brief Mock event_base structure */
struct event_base {
  /** @brief Flag indicating loop break */
  int loop_break;
  /** @brief Associated connection */
  struct evhttp_connection *conn;
  /** @brief Associated request */
  struct evhttp_request *req;
  /** @brief HTTP method command */
  int cmd;
  /** @brief Request URI path */
  char path[1024];
};

/**
 * @brief Create a new mock event base.
 *
 * @return Pointer to newly allocated event_base or NULL on failure.
 */
static struct event_base *event_base_new(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_base_new_fail) {
    return NULL;
  }
#endif
  return (struct event_base *)calloc(1, sizeof(struct event_base));
}

/**
 * @brief Free a mock event base.
 *
 * @param[in] base Base to free.
 */
static void event_base_free(struct event_base *base) {
  if (base) {
    free(base);
  }
}

/**
 * @brief Signal loopbreak on mock event base.
 *
 * @param[in] base Base to signal.
 */
static void event_base_loopbreak(struct event_base *base) {
  if (base) {
    base->loop_break = 1;
  }
}

/**
 * @brief Create a new mock evhttp connection.
 *
 * @param[in] base Event base.
 * @param[in] dnsbase DNS base placeholder.
 * @param[in] address Remote address.
 * @param[in] port Remote port.
 * @return Pointer to connection or NULL on failure.
 */
static struct evhttp_connection *
evhttp_connection_base_new(struct event_base *base, void *dnsbase,
                           const char *address, unsigned short port) {
  struct evhttp_connection *conn;
  (void)dnsbase;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_conn_new_fail) {
    conn = NULL;
  } else
#endif
  {
    conn =
        (struct evhttp_connection *)calloc(1, sizeof(struct evhttp_connection));
  }
  if (!conn) {
    return NULL;
  }
  conn->base = base;
  if (address) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strncpy_s(conn->host, sizeof(conn->host), address, sizeof(conn->host) - 1);
#else
    strncpy(conn->host, address, sizeof(conn->host) - 1);
#endif
    conn->host[sizeof(conn->host) - 1] = '\0';
  }
  conn->port = (int)port;
  return conn;
}

/**
 * @brief Set connection timeout on mock evhttp connection.
 *
 * @param[in] conn Connection.
 * @param[in] timeout_in_secs Timeout in seconds.
 */
static void evhttp_connection_set_timeout(struct evhttp_connection *conn,
                                          int timeout_in_secs) {
  if (conn) {
    conn->timeout_sec = (long)timeout_in_secs;
  }
}

/**
 * @brief Internal helper to free evbuffer data.
 *
 * @param[in] buf Buffer to free.
 */
static void evbuffer_free_internal(struct evbuffer *buf) {
  if (buf->data) {
    free(buf->data);
    buf->data = NULL;
  }
  buf->len = 0;
  buf->cap = 0;
}

/**
 * @brief Free a mock evhttp connection.
 *
 * @param[in] conn Connection to free.
 */
static void evhttp_connection_free(struct evhttp_connection *conn) {
  if (conn) {
    free(conn);
  }
}

/**
 * @brief Allocate a new mock evhttp request.
 *
 * @param[in] cb Callback invoked on request completion.
 * @param[in] arg User data passed to callback.
 * @return Pointer to request or NULL on failure.
 */
static struct evhttp_request *evhttp_request_new(evhttp_request_cb_t cb,
                                                 void *arg) {
  struct evhttp_request *req;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_req_new_fail) {
    req = NULL;
  } else
#endif
  {
    req = (struct evhttp_request *)calloc(1, sizeof(struct evhttp_request));
  }
  if (!req) {
    return NULL;
  }
  req->cb = cb;
  req->cb_arg = arg;
  return req;
}

/**
 * @brief Free a mock evhttp request.
 *
 * @param[in] req Request to free.
 */
static void evhttp_request_free(struct evhttp_request *req) {
  size_t i;
  if (req) {
    for (i = 0; i < req->output_headers.count; ++i) {
      if (req->output_headers.entries[i].key) {
        free(req->output_headers.entries[i].key);
      }
      if (req->output_headers.entries[i].val) {
        free(req->output_headers.entries[i].val);
      }
    }
    evbuffer_free_internal(&req->output_buffer);
    evbuffer_free_internal(&req->input_buffer);
    free(req);
  }
}

/**
 * @brief Set chunked callback on mock evhttp request.
 *
 * @param[in] req Request.
 * @param[in] cb Chunk callback.
 */
static void evhttp_request_set_chunked_cb(struct evhttp_request *req,
                                          evhttp_chunk_cb_t cb) {
  if (req) {
    req->chunk_cb = cb;
  }
}

/**
 * @brief Get output headers from mock evhttp request.
 *
 * @param[in] req Request.
 * @return Pointer to output headers queue.
 */
static struct evkeyvalq *
evhttp_request_get_output_headers(struct evhttp_request *req) {
  return &req->output_headers;
}

/**
 * @brief Get output buffer from mock evhttp request.
 *
 * @param[in] req Request.
 * @return Pointer to output buffer.
 */
static struct evbuffer *
evhttp_request_get_output_buffer(struct evhttp_request *req) {
  return &req->output_buffer;
}

/**
 * @brief Get input buffer from mock evhttp request.
 *
 * @param[in] req Request.
 * @return Pointer to input buffer.
 */
static struct evbuffer *
evhttp_request_get_input_buffer(struct evhttp_request *req) {
  return &req->input_buffer;
}

/**
 * @brief Get response status code from mock evhttp request.
 *
 * @param[in] req Request.
 * @return HTTP status code.
 */
static int evhttp_request_get_response_code(const struct evhttp_request *req) {
  return req ? req->response_code : 0;
}

/**
 * @brief Add header key-value pair to mock headers queue.
 *
 * @param[in,out] headers Headers queue.
 * @param[in] key Header name.
 * @param[in] val Header value.
 * @return 0 on success, -1 on failure.
 */
static int evhttp_add_header(struct evkeyvalq *headers, const char *key,
                             const char *val) {
  char *_ast_strdup_k = NULL;
  char *_ast_strdup_v = NULL;
  enum c_abstract_http_error rck, rcv;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_buf_add_fail) {
    return -1;
  }
#endif
  if (!headers || !key || !val || headers->count >= 32) {
    return -1;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_res_init_fail == 1) {
    rck = C_ABSTRACT_HTTP_ERR_NOMEM;
    rcv = C_ABSTRACT_HTTP_SUCCESS;
  } else if (g_mock_libevent_res_init_fail == 2) {
    rck = c_abstract_http_strdup(key, &_ast_strdup_k);
    rcv = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rck = c_abstract_http_strdup(key, &_ast_strdup_k);
    rcv = c_abstract_http_strdup(val, &_ast_strdup_v);
  }
  if (rck != C_ABSTRACT_HTTP_SUCCESS || rcv != C_ABSTRACT_HTTP_SUCCESS) {
    if (_ast_strdup_k) {
      free(_ast_strdup_k);
    }
    return -1;
  }
  headers->entries[headers->count].key = _ast_strdup_k;
  headers->entries[headers->count].val = _ast_strdup_v;
  headers->count++;
  return 0;
}

/**
 * @brief Append data to mock evbuffer.
 *
 * @param[in,out] buf Buffer.
 * @param[in] data Data to append.
 * @param[in] datlen Length of data.
 * @return 0 on success, -1 on failure.
 */
static int evbuffer_add(struct evbuffer *buf, const void *data, size_t datlen) {
  char *new_data;
  size_t new_cap;
  if (!buf || !data || datlen == 0) {
    return 0;
  }
  if (buf->len + datlen > buf->cap) {
    new_cap = buf->cap == 0 ? (datlen < 128 ? 128 : datlen * 2)
                            : (buf->cap + datlen) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_buf_add_fail) {
      new_data = NULL;
    } else
#endif
    {
      new_data = (char *)realloc(buf->data, new_cap);
    }
    if (!new_data) {
      return -1;
    }
    buf->data = new_data;
    buf->cap = new_cap;
  }
  memcpy(buf->data + buf->len, data, datlen);
  buf->len += datlen;
  return 0;
}

/**
 * @brief Get length of data stored in mock evbuffer.
 *
 * @param[in] buf Buffer.
 * @return Length in bytes.
 */
static size_t evbuffer_get_length(const struct evbuffer *buf) {
  return buf ? buf->len : 0;
}

/**
 * @brief Remove and copy bytes from mock evbuffer.
 *
 * @param[in,out] buf Buffer.
 * @param[out] data_out Destination buffer.
 * @param[in] datlen Number of bytes to remove.
 * @return Number of bytes removed.
 */
static int evbuffer_remove(struct evbuffer *buf, void *data_out,
                           size_t datlen) {
  size_t to_copy;
  if (!buf || !data_out || datlen == 0 || buf->len == 0) {
    return 0;
  }
  to_copy = datlen < buf->len ? datlen : buf->len;
  memcpy(data_out, buf->data, to_copy);
  if (to_copy < buf->len) {
    memmove(buf->data, buf->data + to_copy, buf->len - to_copy);
  }
  buf->len -= to_copy;
  return (int)to_copy;
}

/**
 * @brief Initiate request on mock evhttp connection.
 *
 * @param[in] conn Connection.
 * @param[in] req Request.
 * @param[in] type HTTP method type.
 * @param[in] uri URI path.
 * @return 0 on success, -1 on failure.
 */
static int evhttp_make_request(struct evhttp_connection *conn,
                               struct evhttp_request *req, int type,
                               const char *uri) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_make_req_fail) {
    return -1;
  }
#endif
  if (!conn || !req || !conn->base) {
    return -1;
  }
  conn->base->conn = conn;
  conn->base->req = req;
  conn->base->cmd = type;
  if (uri) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strncpy_s(conn->base->path, sizeof(conn->base->path), uri,
              sizeof(conn->base->path) - 1);
#else
    strncpy(conn->base->path, uri, sizeof(conn->base->path) - 1);
#endif
    conn->base->path[sizeof(conn->base->path) - 1] = '\0';
  }
  return 0;
}

/**
 * @brief Run mock event base dispatch.
 *
 * @param[in] base Event base.
 * @return 0 on success, -1 on failure.
 */
static int event_base_dispatch(struct event_base *base) {
  const char mock_payload[] = "OK";
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_empty_body == 2) {
    return 0;
  }
#endif
  if (!base || !base->req) {
    return -1;
  }
  if (base->conn && base->conn->port == 59999) {
    if (base->req->cb) {
      base->req->cb(NULL, base->req->cb_arg);
    }
    return 0;
  }
  if (base->req->chunk_cb) {
    evbuffer_add(&base->req->input_buffer, mock_payload,
                 sizeof(mock_payload) - 1);
    base->req->chunk_cb(base->req, base->req->cb_arg);
    if (base->loop_break) {
      if (base->req->cb) {
        base->req->cb(base->req, base->req->cb_arg);
      }
      return 0;
    }
  } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (!g_mock_libevent_empty_body)
#endif
    {
      evbuffer_add(&base->req->input_buffer, mock_payload,
                   sizeof(mock_payload) - 1);
    }
  }
  base->req->response_code = 200;
  if (base->req->cb) {
    base->req->cb(base->req, base->req->cb_arg);
  }
  return 0;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT) */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
};

static int libevent_global_init_count = 0;

/**
 * @brief Initialize the global libevent environment safely.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libevent_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  libevent_global_init_count++;
#ifdef _WIN32
  if (libevent_global_init_count == 1) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up the global libevent environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libevent_global_cleanup(void) {
  if (libevent_global_init_count > 0) {
    libevent_global_init_count--;
#ifdef _WIN32
    if (libevent_global_init_count == 0) {
      WSACleanup();
    }
#endif
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Create a new libevent-backed transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_libevent_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_libevent_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libevent_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_context_init_fail) {
    *ctx = NULL;
  } else
#endif
  {
    *ctx = (struct HttpTransportContext *)malloc(
        sizeof(struct HttpTransportContext));
  }
  if (!*ctx) {
    LOG_DEBUG("http_libevent_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(*ctx, 0, sizeof(struct HttpTransportContext));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&(*ctx)->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_libevent_context_init: Error http_config_init failed with %d",
        (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_libevent_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free the transport context.
 *
 * @param[in] ctx The context to free. Safe to pass NULL.
 */
void http_libevent_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libevent_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_libevent_context_free: Exiting");
}

/**
 * @brief Apply configuration settings to the transport context.
 *
 * @param[in] ctx The transport context.
 * @param[in] config The configuration structure to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_libevent_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  enum c_abstract_http_error rc;
  if (!ctx || !config) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
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
    if (g_mock_libevent_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.user_agent) {
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
  }

  if (config->proxy_url) {
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_url) {
    free(ctx->config.proxy_url);
    ctx->config.proxy_url = NULL;
  }

  if (config->proxy_username) {
    if (ctx->config.proxy_username) {
      free(ctx->config.proxy_username);
      ctx->config.proxy_username = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_config_init_fail) {
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
    if (g_mock_libevent_config_init_fail) {
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

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HttpMethod enum to libevent request command.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_cmd Pointer to receive libevent method command.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error get_method_cmd(enum HttpMethod method,
                                                 int *out_cmd) {
  if (!out_cmd) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (method) {
  case HTTP_GET:
    *out_cmd = EVHTTP_REQ_GET;
    break;
  case HTTP_POST:
    *out_cmd = EVHTTP_REQ_POST;
    break;
  case HTTP_PUT:
    *out_cmd = EVHTTP_REQ_PUT;
    break;
  case HTTP_DELETE:
    *out_cmd = EVHTTP_REQ_DELETE;
    break;
  case HTTP_PATCH:
    *out_cmd = EVHTTP_REQ_PATCH;
    break;
  case HTTP_HEAD:
    *out_cmd = EVHTTP_REQ_HEAD;
    break;
  case HTTP_OPTIONS:
    *out_cmd = EVHTTP_REQ_OPTIONS;
    break;
  case HTTP_TRACE:
    *out_cmd = EVHTTP_REQ_TRACE;
    break;
  case HTTP_CONNECT:
    *out_cmd = EVHTTP_REQ_CONNECT;
    break;
  default:
    *out_cmd = EVHTTP_REQ_GET;
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd);
enum c_abstract_http_error c_abstract_http_test_libevent_evbuffer(void);
enum c_abstract_http_error c_abstract_http_test_libevent_add_header_edge(void);

/**
 * @brief Expose get_method_cmd for testing.
 *
 * @param[in] method HTTP method.
 * @param[out] out_cmd Output command.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
c_abstract_http_test_libevent_method_cmd(enum HttpMethod method, int *out_cmd) {
  return get_method_cmd(method, out_cmd);
}

/**
 * @brief Test evbuffer edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
enum c_abstract_http_error c_abstract_http_test_libevent_evbuffer(void) {
  struct evbuffer buf;
  struct evbuffer buf2;
  char out[16];
  memset(&buf, 0, sizeof(buf));
  memset(&buf2, 0, sizeof(buf2));
  evbuffer_add(NULL, "a", 1);
  evbuffer_add(&buf, NULL, 1);
  evbuffer_add(&buf, "a", 0);
  evbuffer_remove(NULL, out, 1);
  evbuffer_remove(&buf, NULL, 1);
  evbuffer_remove(&buf, out, 0);
  evbuffer_get_length(NULL);

  evbuffer_add(&buf, "hello", 5);
  evbuffer_add(&buf,
               "12345678901234567890123456789012345678901234567890123456789012"
               "34567890123456789012345678901234567890123456789012345678901234"
               "567890",
               130);
  evbuffer_remove(&buf, out, 2);
  evbuffer_remove(&buf, out, 10);
  evbuffer_free_internal(&buf);

  evbuffer_add(&buf2,
               "12345678901234567890123456789012345678901234567890123456789012"
               "34567890123456789012345678901234567890123456789012345678901234"
               "567890",
               130);
  evbuffer_free_internal(&buf2);

  evhttp_make_request(NULL, NULL, 0, NULL);
  event_base_dispatch(NULL);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Test evhttp_add_header edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
enum c_abstract_http_error c_abstract_http_test_libevent_add_header_edge(void) {
  struct evkeyvalq q;
  size_t i;
  memset(&q, 0, sizeof(q));
  evhttp_add_header(NULL, "k", "v");
  evhttp_add_header(&q, NULL, "v");
  evhttp_add_header(&q, "k", NULL);
  g_mock_libevent_res_init_fail = 1;
  evhttp_add_header(&q, "k", "v");
  g_mock_libevent_res_init_fail = 2;
  evhttp_add_header(&q, "k", "v");
  g_mock_libevent_res_init_fail = 0;
  for (i = 0; i < 32; i++) {
    evhttp_add_header(&q, "k", "v");
  }
  evhttp_add_header(&q, "k", "v");
  for (i = 0; i < q.count; i++) {
    free(q.entries[i].key);
    free(q.entries[i].val);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/** @brief Internal struct libevent_state */
struct libevent_state {
  /** @brief Event base */
  struct event_base *base;
  /** @brief evhttp connection */
  struct evhttp_connection *conn;
  /** @brief Request pointer */
  const struct HttpRequest *req;
  /** @brief Response double pointer */
  struct HttpResponse **res;
  /** @brief Error code captured during execution */
  int error_code;
  /** @brief Flag indicating completion */
  int done;
};

/**
 * @brief Request completion callback for libevent.
 *
 * @param[in] req_ev evhttp request.
 * @param[in] arg Pointer to libevent_state.
 */
static void http_request_done(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state;
  state = (struct libevent_state *)arg;
  state->done = 1;

  if (state->error_code != 0) {
    LOG_DEBUG("http_request_done: Aborted with error %d", state->error_code);
    return;
  }

  if (!req_ev) {
    LOG_DEBUG("http_request_done: Request failed (req_ev NULL)");
    state->error_code = ECONNREFUSED;
    return;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libevent_res_alloc_fail) {
    *state->res = NULL;
  } else
#endif
  {
    *state->res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!*state->res) {
    LOG_DEBUG("http_request_done: Error ENOMEM allocating response");
    state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
    return;
  }

  {
    enum c_abstract_http_error rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_res_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = http_response_init(*state->res);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("http_request_done: Error http_response_init failed with %d",
                (int)rc);
      free(*state->res);
      *state->res = NULL;
      state->error_code = (int)rc;
      return;
    }
  }

  (*state->res)->status_code = evhttp_request_get_response_code(req_ev);

  {
    struct evbuffer *evb = evhttp_request_get_input_buffer(req_ev);
    size_t len = evbuffer_get_length(evb);
    if (len > 0) {
      char *body_char;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libevent_body_alloc_fail) {
        body_char = NULL;
      } else
#endif
      {
        body_char = (char *)malloc(len + 1);
      }
      if (body_char) {
        evbuffer_remove(evb, body_char, len);
        body_char[len] = '\0';
        (*state->res)->body = body_char;
        (*state->res)->body_len = len;
      } else {
        LOG_DEBUG("http_request_done: Error ENOMEM allocating body");
        state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
  }
}

/**
 * @brief Chunk callback for libevent stream responses.
 *
 * @param[in] req_ev evhttp request.
 * @param[in] arg Pointer to libevent_state.
 */
static void http_chunked_cb(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state;
  struct evbuffer *evb;
  size_t len;

  state = (struct libevent_state *)arg;
  evb = evhttp_request_get_input_buffer(req_ev);
  len = evbuffer_get_length(evb);

  if (len > 0 && state->req->on_chunk) {
    char *buf;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libevent_body_alloc_fail) {
      buf = NULL;
    } else
#endif
    {
      buf = (char *)malloc(len);
    }
    if (!buf) {
      state->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
      event_base_loopbreak(state->base);
      return;
    }
    evbuffer_remove(evb, buf, len);
    {
      int rc = state->req->on_chunk(state->req->on_chunk_user_data, buf, len);
      if (rc != (int)C_ABSTRACT_HTTP_SUCCESS) {
        state->error_code = rc;
        event_base_loopbreak(state->base);
      }
    }
    free(buf);
  }
}

/**
 * @brief Send an HTTP request using libevent.
 *
 * @param[in] ctx Transport context.
 * @param[in] req Request structure.
 * @param[out] res Pointer to receive response object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libevent_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  struct libevent_state state;
  struct evhttp_request *req_ev;
  char host[256];
  int port;
  const char *p;
  const char *path;
  struct evkeyvalq *output_headers;
  enum c_abstract_http_error rc;
  int method_cmd;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_libevent_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_libevent_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(&state, 0, sizeof(state));
  memset(host, 0, sizeof(host));
  port = 80;
  path = "/";
  state.req = req;
  state.res = res;
  state.error_code = 0;

  if (strncmp(req->url, "http://", 7) == 0) {
    p = req->url + 7;
  } else {
    p = req->url;
  }

  {
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
      size_t len = (size_t)(colon - p);
      if (len < sizeof(host)) {
        memcpy(host, p, len);
        host[len] = '\0';
      }
      port = atoi(colon + 1);
      if (slash) {
        path = slash;
      }
    } else if (slash) {
      size_t len = (size_t)(slash - p);
      if (len < sizeof(host)) {
        memcpy(host, p, len);
        host[len] = '\0';
      }
      path = slash;
    } else {
      size_t len = strlen(p);
      if (len < sizeof(host)) {
        memcpy(host, p, len);
        host[len] = '\0';
      }
    }
  }

  state.base = event_base_new();
  if (!state.base) {
    LOG_DEBUG("http_libevent_send: Error ENOMEM (event_base_new failed)");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  state.conn =
      evhttp_connection_base_new(state.base, NULL, host, (unsigned short)port);
  if (!state.conn) {
    LOG_DEBUG(
        "http_libevent_send: Error ENOMEM (evhttp_connection_base_new failed)");
    event_base_free(state.base);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (ctx->config.timeout_ms > 0) {
    evhttp_connection_set_timeout(state.conn,
                                  (int)(ctx->config.timeout_ms / 1000));
  }

  req_ev = evhttp_request_new(http_request_done, &state);
  if (!req_ev) {
    LOG_DEBUG("http_libevent_send: Error ENOMEM (evhttp_request_new failed)");
    evhttp_connection_free(state.conn);
    event_base_free(state.base);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (req->on_chunk) {
    evhttp_request_set_chunked_cb(req_ev, http_chunked_cb);
  }

  output_headers = evhttp_request_get_output_headers(req_ev);
  if (evhttp_add_header(output_headers, "Host", host) != 0) {
    LOG_DEBUG("http_libevent_send: Error evhttp_add_header failed");
  }
  if (evhttp_add_header(output_headers, "Connection", "close") != 0) {
    LOG_DEBUG("http_libevent_send: Error evhttp_add_header failed");
  }

  if (req->headers.count > 0) {
    size_t i;
    for (i = 0; i < req->headers.count; i++) {
      if (evhttp_add_header(output_headers, req->headers.headers[i].key,
                            req->headers.headers[i].value) != 0) {
        LOG_DEBUG("http_libevent_send: Error evhttp_add_header failed for "
                  "custom header");
      }
    }
  }

  if (req->body && req->body_len > 0) {
    struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
    if (evbuffer_add(evb, req->body, req->body_len) != 0) {
      LOG_DEBUG("http_libevent_send: Error evbuffer_add failed");
    }
  } else if (req->read_chunk) {
    struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
    char buf[4096];
    size_t read_bytes = 0;
    for (;;) {
      int r = req->read_chunk(req->read_chunk_user_data, buf, sizeof(buf),
                              &read_bytes);
      if (r != 0 || read_bytes == 0) {
        break;
      }
      if (evbuffer_add(evb, buf, read_bytes) != 0) {
        LOG_DEBUG("http_libevent_send: Error evbuffer_add failed during "
                  "chunk read");
      }
    }
  }

  get_method_cmd(req->method, &method_cmd);
  if (evhttp_make_request(state.conn, req_ev, method_cmd, path) != 0) {
    LOG_DEBUG("http_libevent_send: Error evhttp_make_request failed");
#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
    evhttp_request_free(req_ev);
#endif
    evhttp_connection_free(state.conn);
    event_base_free(state.base);
    return (enum c_abstract_http_error)ECONNREFUSED;
  }

  event_base_dispatch(state.base);

  if (state.error_code != 0) {
    rc = (enum c_abstract_http_error)state.error_code;
  } else if (!*res) {
    rc = (enum c_abstract_http_error)ECONNREFUSED;
  } else {
    rc = C_ABSTRACT_HTTP_SUCCESS;
  }

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBEVENT)
  evhttp_request_free(req_ev);
#endif
  evhttp_connection_free(state.conn);
  event_base_free(state.base);

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libevent_send: Success");
  } else {
    LOG_DEBUG("http_libevent_send: Error returning %d", (int)rc);
  }
  return rc;
}
