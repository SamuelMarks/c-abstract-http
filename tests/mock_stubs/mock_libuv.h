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
#define UV_RUN_NOWAIT 1
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
  char tx_preview[256];
} uv_tcp_t;

typedef struct uv_connect_s {
  void *data;
} uv_connect_t;

typedef struct uv_getaddrinfo_s {
  void *data;
  uv_getaddrinfo_cb cb;
  char port_str[16];
  int resolved;
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
  uv_tcp_t *tcps[32];
  size_t tcp_count;
  uv_getaddrinfo_t *resolvers[32];
  size_t resolver_count;
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
    if (loop->tcp_count < 32) {
      loop->tcps[loop->tcp_count++] = tcp;
    }
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
  resolver->resolved = 0;
  if (loop) {
    loop->resolver = resolver;
    if (loop->resolver_count < 32) {
      loop->resolvers[loop->resolver_count++] = resolver;
    }
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
  if (g_mock_libuv_write_fail == 2) {
    return -1;
  }
  if (nbufs > 0 && bufs && bufs[0].base && bufs[0].len > 0) {
    size_t clen = bufs[0].len < sizeof(tcp->tx_preview) - 1
                      ? bufs[0].len
                      : sizeof(tcp->tx_preview) - 1;
    memcpy(tcp->tx_preview, bufs[0].base, clen);
    tcp->tx_preview[clen] = '\0';
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

static void mock_uv_drive_tcp(uv_loop_t *loop, uv_tcp_t *tcp,
                              uv_getaddrinfo_t *resolver) {
  if (!tcp) {
    return;
  }
  if (tcp->connect_cb && tcp->connect_req) {
    uv_connect_cb ccb = tcp->connect_cb;
    uv_connect_t *creq = tcp->connect_req;
    int port = resolver ? atoi(resolver->port_str) : 80;
    tcp->connect_cb = NULL;
    if (g_mock_libuv_connect_fail || port == 59999) {
      ccb(creq, -1);
      return;
    }
    ccb(creq, 0);
  }

  if (tcp->write_req && tcp->write_req->write_cb) {
    uv_write_t *wreq = tcp->write_req;
    uv_write_cb wcb = wreq->write_cb;
    tcp->write_req = NULL;
    if (g_mock_libuv_write_fail) {
      wcb(wreq, -1);
      return;
    }
    wcb(wreq, 0);
  }

  if (tcp->read_cb && !tcp->is_closing && !loop->stop) {
    uv_buf_t buf;
    const char *hdr_str = "HTTP/1.1 200 OK\r\nHeader: Value\r\n\r\n";
    const char *body_str = "OK";
    size_t hlen;
    size_t blen;

    if (strstr(tcp->tx_preview, "/chunked-bad1") != NULL) {
      hdr_str = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
      body_str = "ZZ\r\n";
    } else if (strstr(tcp->tx_preview, "/chunked-bad2") != NULL) {
      hdr_str = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
      body_str = "\r\n";
    } else if (strstr(tcp->tx_preview, "/chunked-bad3") != NULL) {
      hdr_str = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
      body_str = "a\r\nshort";
    } else if (strstr(tcp->tx_preview, "/chunked-hex") != NULL) {
      hdr_str = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
      body_str = "a\r\n0123456789\r\nA\r\nabcdefghij\r\n0\r\n\r\n";
    } else if (strstr(tcp->tx_preview, "/chunked") != NULL) {
      hdr_str = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
      body_str = "5;ext\r\nHello\r\n7\r\n, libuv\r\n0\r\n\r\n";
    } else if (strstr(tcp->tx_preview, "/redir-once") != NULL) {
      hdr_str =
          "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:80/landed\r\n\r\n";
      body_str = "";
    } else if (strstr(tcp->tx_preview, "/redir-loop") != NULL) {
      hdr_str = "HTTP/1.1 302 Found\r\nLocation: /redir-loop\r\n\r\n";
      body_str = "";
    }
    hlen = strlen(hdr_str);
    blen = strlen(body_str);

    memset(&buf, 0, sizeof(buf));
    if (g_mock_libuv_read_fail) {
      tcp->read_cb((uv_stream_t *)tcp, -1, &buf);
      return;
    }
    tcp->alloc_cb((uv_handle_t *)tcp, hlen, &buf);
    if (buf.base) {
      memcpy(buf.base, hdr_str, hlen);
      tcp->read_cb((uv_stream_t *)tcp, (ssize_t)hlen, &buf);
    } else {
      tcp->read_cb((uv_stream_t *)tcp, UV_ENOBUFS, &buf);
      return;
    }
    if (!tcp->is_closing && !loop->stop && blen > 0) {
      tcp->alloc_cb((uv_handle_t *)tcp, blen, &buf);
      if (buf.base) {
        memcpy(buf.base, body_str, blen);
        tcp->read_cb((uv_stream_t *)tcp, (ssize_t)blen, &buf);
      } else {
        tcp->read_cb((uv_stream_t *)tcp, UV_ENOBUFS, &buf);
        return;
      }
    }
    if (!tcp->is_closing && !loop->stop) {
      tcp->read_cb((uv_stream_t *)tcp, UV_EOF, &buf);
    }
  }
}

static int uv_run(uv_loop_t *loop, int mode) {
  size_t i;
  (void)mode;

  if (!loop || !loop->data) {
    return 0;
  }

  for (i = 0; i < loop->resolver_count; ++i) {
    uv_getaddrinfo_t *r = loop->resolvers[i];
    if (r && r->cb && !r->resolved) {
      struct addrinfo *ai =
          (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
      r->resolved = 1;
      if (!ai || g_mock_libuv_addrinfo_fail) {
        uv_freeaddrinfo(ai);
        r->cb(r, -1, NULL);
        continue;
      }
      ai->ai_addr = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
      if (!ai->ai_addr) {
        uv_freeaddrinfo(ai);
        r->cb(r, -1, NULL);
        continue;
      }
      r->cb(r, 0, ai);
    }
  }

  if (loop->tcp_count > 0) {
    for (i = 0; i < loop->tcp_count; ++i) {
      uv_getaddrinfo_t *r =
          i < loop->resolver_count ? loop->resolvers[i] : loop->resolver;
      loop->stop = 0;
      mock_uv_drive_tcp(loop, loop->tcps[i], r);
    }
  } else if (loop->tcp) {
    mock_uv_drive_tcp(loop, loop->tcp, loop->resolver);
  }

  return 0;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_LIBUV_H */
