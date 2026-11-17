/**
 * @file mock_libevent.h
 * @brief Internal test-only libevent/evhttp C-library stubs for OOM and fault
 * injection.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_LIBEVENT_H
#define C_ABSTRACT_HTTP_TEST_MOCK_LIBEVENT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

extern int g_mock_libevent_global_init_fail;
extern int g_mock_libevent_context_init_fail;
extern int g_mock_libevent_config_init_fail;
extern int g_mock_libevent_base_new_fail;
extern int g_mock_libevent_conn_new_fail;
extern int g_mock_libevent_req_new_fail;
extern int g_mock_libevent_make_req_fail;
extern int g_mock_libevent_res_alloc_fail;
extern int g_mock_libevent_res_init_fail;
extern int g_mock_libevent_body_alloc_fail;
extern int g_mock_libevent_buf_add_fail;
extern int g_mock_libevent_empty_body;

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

struct evbuffer {
  char *data;
  size_t len;
  size_t cap;
};

struct evkeyval {
  char *key;
  char *val;
};

struct evkeyvalq {
  struct evkeyval entries[32];
  size_t count;
};

struct evhttp_request;

typedef void (*evhttp_request_cb_t)(struct evhttp_request *, void *);
typedef void (*evhttp_chunk_cb_t)(struct evhttp_request *, void *);

struct evhttp_request {
  evhttp_request_cb_t cb;
  void *cb_arg;
  evhttp_chunk_cb_t chunk_cb;
  struct evkeyvalq output_headers;
  struct evbuffer output_buffer;
  struct evbuffer input_buffer;
  int response_code;
};

struct event_base;

struct evhttp_connection {
  struct event_base *base;
  char host[256];
  int port;
  long timeout_sec;
};

struct event_base {
  int loop_break;
  struct evhttp_connection *conn;
  struct evhttp_request *req;
  int cmd;
  char path[1024];
};

static struct event_base *event_base_new(void) {
  if (g_mock_libevent_base_new_fail) {
    return NULL;
  }
  return (struct event_base *)calloc(1, sizeof(struct event_base));
}

static void event_base_free(struct event_base *base) {
  if (base) {
    free(base);
  }
}

static void event_base_loopbreak(struct event_base *base) {
  if (base) {
    base->loop_break = 1;
  }
}

static struct evhttp_connection *
evhttp_connection_base_new(struct event_base *base, void *dnsbase,
                           const char *address, unsigned short port) {
  struct evhttp_connection *conn;
  (void)dnsbase;
  if (g_mock_libevent_conn_new_fail) {
    conn = NULL;
  } else {
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

static void evhttp_connection_set_timeout(struct evhttp_connection *conn,
                                          int timeout_in_secs) {
  if (conn) {
    conn->timeout_sec = (long)timeout_in_secs;
  }
}

static void evbuffer_free_internal(struct evbuffer *buf) {
  if (buf->data) {
    free(buf->data);
    buf->data = NULL;
  }
  buf->len = 0;
  buf->cap = 0;
}

static void evhttp_connection_free(struct evhttp_connection *conn) {
  if (conn) {
    free(conn);
  }
}

static struct evhttp_request *evhttp_request_new(evhttp_request_cb_t cb,
                                                 void *arg) {
  struct evhttp_request *req;
  if (g_mock_libevent_req_new_fail) {
    req = NULL;
  } else {
    req = (struct evhttp_request *)calloc(1, sizeof(struct evhttp_request));
  }
  if (!req) {
    return NULL;
  }
  req->cb = cb;
  req->cb_arg = arg;
  return req;
}

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

static void evhttp_request_set_chunked_cb(struct evhttp_request *req,
                                          evhttp_chunk_cb_t cb) {
  if (req) {
    req->chunk_cb = cb;
  }
}

static struct evkeyvalq *
evhttp_request_get_output_headers(struct evhttp_request *req) {
  return &req->output_headers;
}

static struct evbuffer *
evhttp_request_get_output_buffer(struct evhttp_request *req) {
  return &req->output_buffer;
}

static struct evbuffer *
evhttp_request_get_input_buffer(struct evhttp_request *req) {
  return &req->input_buffer;
}

static int evhttp_request_get_response_code(const struct evhttp_request *req) {
  return req ? req->response_code : 0;
}

static int evhttp_add_header(struct evkeyvalq *headers, const char *key,
                             const char *val) {
  char *_ast_strdup_k = NULL;
  char *_ast_strdup_v = NULL;
  enum c_abstract_http_error rck, rcv;
  if (g_mock_libevent_buf_add_fail) {
    return -1;
  }
  if (!headers || !key || !val || headers->count >= 32) {
    return -1;
  }
  if (g_mock_libevent_res_init_fail == 1) {
    rck = C_ABSTRACT_HTTP_ERR_NOMEM;
    rcv = C_ABSTRACT_HTTP_SUCCESS;
  } else if (g_mock_libevent_res_init_fail == 2) {
    rck = c_abstract_http_strdup(key, &_ast_strdup_k);
    rcv = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else {
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

static int evbuffer_add(struct evbuffer *buf, const void *data, size_t datlen) {
  char *new_data;
  size_t new_cap;
  if (!buf || !data || datlen == 0) {
    return 0;
  }
  if (buf->len + datlen > buf->cap) {
    new_cap = buf->cap == 0 ? (datlen < 128 ? 128 : datlen * 2)
                            : (buf->cap + datlen) * 2;
    if (g_mock_libevent_buf_add_fail) {
      new_data = NULL;
    } else {
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

static size_t evbuffer_get_length(const struct evbuffer *buf) {
  return buf ? buf->len : 0;
}

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

static int evhttp_make_request(struct evhttp_connection *conn,
                               struct evhttp_request *req, int type,
                               const char *uri) {
  if (g_mock_libevent_make_req_fail) {
    return -1;
  }
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

static int event_base_dispatch(struct event_base *base) {
  const char mock_payload[] = "OK";
  if (g_mock_libevent_empty_body == 2) {
    return 0;
  }
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
    if (!g_mock_libevent_empty_body) {
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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_LIBEVENT_H */
