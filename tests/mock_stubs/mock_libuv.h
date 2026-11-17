/**
 * @file mock_libuv.h
 * @brief Internal test-only libuv C-library stubs for OOM and fault injection.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_LIBUV_H
#define C_ABSTRACT_HTTP_TEST_MOCK_LIBUV_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include <c_abstract_http/http_types.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif
/* clang-format on */

extern int g_mock_libuv_global_init_fail;
extern int g_mock_libuv_context_init_fail;
extern int g_mock_libuv_config_init_fail;
extern int g_mock_libuv_req_buf_alloc_fail;
extern int g_mock_libuv_res_alloc_fail;
extern int g_mock_libuv_res_init_fail;
extern int g_mock_libuv_body_alloc_fail;
extern int g_mock_libuv_body_realloc_fail;
extern int g_mock_libuv_headers_realloc_fail;
extern int g_mock_libuv_addrinfo_fail;
extern int g_mock_libuv_connect_fail;
extern int g_mock_libuv_write_fail;
extern int g_mock_libuv_read_fail;
extern int g_mock_libuv_alloc_fail;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)

#define UV_RUN_DEFAULT 0
#define UV_EOF (-4095)
#define UV_ENOBUFS (-4060)

typedef unsigned long ULONG;

typedef struct uv_buf_s {
  char *base;
  size_t len;
} uv_buf_t;

struct uv_handle_s;
struct uv_stream_s;
struct uv_tcp_s;
struct uv_connect_s;
struct uv_getaddrinfo_s;
struct uv_timer_s;
struct uv_write_s;
struct uv_loop_s;

typedef struct uv_handle_s uv_handle_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_loop_s uv_loop_t;

typedef void (*uv_close_cb)(uv_handle_t *handle);
typedef void (*uv_alloc_cb)(uv_handle_t *handle, size_t suggested_size,
                            uv_buf_t *buf);
typedef void (*uv_read_cb)(uv_stream_t *stream, ssize_t nread,
                           const uv_buf_t *buf);
typedef void (*uv_write_cb)(struct uv_write_s *req, int status);
typedef void (*uv_connect_cb)(struct uv_connect_s *req, int status);
typedef void (*uv_getaddrinfo_cb)(struct uv_getaddrinfo_s *resolver, int status,
                                  struct addrinfo *res);
typedef void (*uv_timer_cb)(struct uv_timer_s *handle);

struct uv_handle_s {
  void *data;
  uv_loop_t *loop;
  int is_closing;
};

struct uv_stream_s {
  void *data;
  uv_loop_t *loop;
  int is_closing;
  uv_read_cb read_cb;
  uv_alloc_cb alloc_cb;
};

typedef struct uv_tcp_s {
  void *data;
  uv_loop_t *loop;
  int is_closing;
  uv_read_cb read_cb;
  uv_alloc_cb alloc_cb;
  uv_connect_cb connect_cb;
  struct uv_connect_s *connect_req;
  struct uv_write_s *write_req;
} uv_tcp_t;

typedef struct uv_connect_s {
  void *data;
} uv_connect_t;

typedef struct uv_getaddrinfo_s {
  void *data;
  uv_getaddrinfo_cb cb;
  char port_str[16];
} uv_getaddrinfo_t;

typedef struct uv_timer_s {
  void *data;
  uv_loop_t *loop;
  int is_closing;
  uv_timer_cb timer_cb;
  int active;
} uv_timer_t;

typedef struct uv_write_s {
  void *data;
  uv_write_cb write_cb;
} uv_write_t;

struct uv_loop_s {
  void *data;
  int stop;
  uv_tcp_t *tcp;
  uv_getaddrinfo_t *resolver;
};

static uv_buf_t uv_buf_init(char *base, unsigned int len) {
  uv_buf_t buf;
  buf.base = base;
  buf.len = (size_t)len;
  return buf;
}

static int uv_loop_init(uv_loop_t *loop) {
  memset(loop, 0, sizeof(*loop));
  return 0;
}

static int uv_loop_close(uv_loop_t *loop) {
  (void)loop;
  return 0;
}

static void uv_stop(uv_loop_t *loop) {
  if (loop) {
    loop->stop = 1;
  }
}

static int uv_tcp_init(uv_loop_t *loop, uv_tcp_t *tcp) {
  memset(tcp, 0, sizeof(*tcp));
  tcp->loop = loop;
  if (loop) {
    loop->tcp = tcp;
  }
  return 0;
}

static int uv_timer_init(uv_loop_t *loop, uv_timer_t *timer) {
  memset(timer, 0, sizeof(*timer));
  timer->loop = loop;
  return 0;
}

static int uv_timer_start(uv_timer_t *timer, uv_timer_cb cb, uint64_t timeout,
                          uint64_t repeat) {
  (void)timeout;
  (void)repeat;
  timer->timer_cb = cb;
  timer->active = 1;
  return 0;
}

static int uv_timer_stop(uv_timer_t *timer) {
  timer->active = 0;
  return 0;
}

static void uv_close(uv_handle_t *handle, uv_close_cb close_cb) {
  handle->is_closing = 1;
  close_cb(handle);
}

static void uv_freeaddrinfo(struct addrinfo *ai) {
  if (ai) {
    free(ai->ai_addr);
    free(ai);
  }
}

static int uv_getaddrinfo(uv_loop_t *loop, uv_getaddrinfo_t *resolver,
                          uv_getaddrinfo_cb cb, const char *node,
                          const char *service, const struct addrinfo *hints) {
  (void)node;
  (void)hints;
  if (g_mock_libuv_addrinfo_fail == 2) {
    return -1;
  }
  resolver->cb = cb;
  if (loop) {
    loop->resolver = resolver;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strncpy_s(resolver->port_str, sizeof(resolver->port_str), service,
            sizeof(resolver->port_str) - 1);
#else
  strncpy(resolver->port_str, service, sizeof(resolver->port_str) - 1);
#endif
  resolver->port_str[sizeof(resolver->port_str) - 1] = '\0';
  return 0;
}

static int uv_tcp_connect(uv_connect_t *req, uv_tcp_t *handle,
                          const struct sockaddr *addr, uv_connect_cb cb) {
  (void)addr;
  if (g_mock_libuv_connect_fail == 2) {
    return -1;
  }
  handle->connect_cb = cb;
  handle->connect_req = req;
  return 0;
}

static int uv_write(uv_write_t *req, uv_stream_t *handle, const uv_buf_t bufs[],
                    unsigned int nbufs, uv_write_cb cb) {
  uv_tcp_t *tcp = (uv_tcp_t *)handle;
  (void)bufs;
  (void)nbufs;
  if (g_mock_libuv_write_fail == 2) {
    return -1;
  }
  req->write_cb = cb;
  tcp->write_req = req;
  return 0;
}

static int uv_read_start(uv_stream_t *stream, uv_alloc_cb alloc_cb,
                         uv_read_cb read_cb) {
  if (g_mock_libuv_read_fail == 2) {
    return -1;
  }
  stream->alloc_cb = alloc_cb;
  stream->read_cb = read_cb;
  if (stream->loop) {
    stream->loop->tcp = (uv_tcp_t *)stream;
  }
  return 0;
}

static int uv_run(uv_loop_t *loop, int mode) {
  struct addrinfo *ai;
  uv_tcp_t *tcp;
  uv_getaddrinfo_t *resolver;
  (void)mode;

  if (!loop || !loop->data) {
    return 0;
  }
  tcp = loop->tcp;
  resolver = loop->resolver;

  if (resolver && resolver->cb) {
    ai = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
    ai->ai_addr = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
    if (g_mock_libuv_addrinfo_fail) {
      uv_freeaddrinfo(ai);
      resolver->cb(resolver, -1, NULL);
      return 0;
    }
    resolver->cb(resolver, 0, ai);
  }

  if (tcp && tcp->connect_cb && tcp->connect_req) {
    int port = resolver ? atoi(resolver->port_str) : 80;
    if (g_mock_libuv_connect_fail || port == 59999) {
      tcp->connect_cb(tcp->connect_req, -1);
      return 0;
    }
    tcp->connect_cb(tcp->connect_req, 0);
  }

  if (tcp && tcp->write_req && tcp->write_req->write_cb) {
    if (g_mock_libuv_write_fail) {
      tcp->write_req->write_cb(tcp->write_req, -1);
      return 0;
    }
    tcp->write_req->write_cb(tcp->write_req, 0);
  }

  if (tcp && tcp->read_cb && !loop->stop) {
    uv_buf_t buf;
    static const char resp_headers[] = {
        'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ', '2', '0', '0',
        ' ', 'O', 'K', 13,  10,  'H', 'e', 'a', 'd', 'e', 'r', ':',
        ' ', 'V', 'a', 'l', 'u', 'e', 13,  10,  13,  10,  0};
    const char body_chunk[] = "OK";
    memset(&buf, 0, sizeof(buf));
    if (g_mock_libuv_read_fail) {
      tcp->read_cb((uv_stream_t *)tcp, -1, &buf);
      return 0;
    }
    tcp->alloc_cb((uv_handle_t *)tcp, sizeof(resp_headers) - 1, &buf);
    if (buf.base) {
      memcpy(buf.base, resp_headers, sizeof(resp_headers) - 1);
      tcp->read_cb((uv_stream_t *)tcp, (ssize_t)(sizeof(resp_headers) - 1),
                   &buf);
    } else {
      tcp->read_cb((uv_stream_t *)tcp, UV_ENOBUFS, &buf);
      return 0;
    }
    if (!loop->stop) {
      tcp->alloc_cb((uv_handle_t *)tcp, sizeof(body_chunk) - 1, &buf);
      if (buf.base) {
        memcpy(buf.base, body_chunk, sizeof(body_chunk) - 1);
        tcp->read_cb((uv_stream_t *)tcp, (ssize_t)(sizeof(body_chunk) - 1),
                     &buf);
      } else {
        tcp->read_cb((uv_stream_t *)tcp, UV_ENOBUFS, &buf);
        return 0;
      }
    }
    if (!loop->stop) {
      tcp->read_cb((uv_stream_t *)tcp, UV_EOF, &buf);
    }
  }

  return 0;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_LIBUV_H */
