/* clang-format off */
#include <c_abstract_http/http_raw.h>
#include "c_abstract_http/log.h"
#include "str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS) || defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS) || 1

#if defined(_WIN32)
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#define RAW_CLOSESOCKET(s) closesocket(s)
typedef SOCKET raw_socket_t;
#define RAW_INVALID_SOCKET INVALID_SOCKET
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#define RAW_CLOSESOCKET(s) close(s)
typedef int raw_socket_t;
#define RAW_INVALID_SOCKET (-1)
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_raw_send_fail;
extern int g_mock_raw_connect_fail;
extern int g_mock_raw_gethostbyname_fail;
extern int g_mock_raw_nonblocking_fail;
extern int g_mock_raw_blocking_fail;
extern int g_mock_raw_realloc_fail;
extern int g_mock_raw_response_init_fail;
#endif

enum c_abstract_http_error http_raw_global_init(void) {
#if defined(_WIN32)
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_raw_global_cleanup(void) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

struct RawCtx {
  long timeout_ms;
  struct HttpConfig config;
};

enum c_abstract_http_error
http_raw_context_init(struct HttpTransportContext **ctx) {
  struct RawCtx *c;
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_raw_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_raw_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  c = (struct RawCtx *)calloc(1, sizeof(struct RawCtx));
  if (!c) {
    LOG_DEBUG("http_raw_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_raw_context_init: Error http_config_init failed with %d",
              rc);
    free(c);
    return rc;
  }

  c->timeout_ms = 30000L;
  *ctx = (struct HttpTransportContext *)c;
  LOG_DEBUG("http_raw_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_raw_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_raw_context_free: Entering");
  if (ctx) {
    http_config_free(&((struct RawCtx *)ctx)->config);
    free(ctx);
  }
  LOG_DEBUG("http_raw_context_free: Exiting");
}

enum c_abstract_http_error
http_raw_config_apply(struct HttpTransportContext *ctx,
                      const struct HttpConfig *config) {
  struct RawCtx *c = (struct RawCtx *)ctx;
  if (!c || !config)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (config->timeout_ms > 0)
    c->timeout_ms = config->timeout_ms;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert a 16-bit integer from host to network byte order.
 * @param hostshort Value in host byte order.
 * @return Value in network byte order.
 */
static unsigned short raw_htons(unsigned short hostshort) {
  unsigned short out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)(hostshort >> 8);
  p[1] = (unsigned char)(hostshort & 0xFF);
  return out;
}

/**
 * @brief Configure socket to be non-blocking.
 * @param sock Socket descriptor.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
static enum c_abstract_http_error make_socket_nonblocking(raw_socket_t sock) {
#if defined(_WIN32)
  u_long mode = 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_nonblocking_fail == 1 || g_mock_raw_nonblocking_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (ioctlsocket(sock, (long)FIONBIO, &mode) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  int flags;
  int setfl_res;
  flags = fcntl(sock, F_GETFL, 0);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_nonblocking_fail == 1)
    flags = -1;
#endif
  if (flags < 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  setfl_res = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_nonblocking_fail == 2)
    setfl_res = -1;
#endif
  if (setfl_res < 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Configure socket to be blocking.
 * @param sock Socket descriptor.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
static enum c_abstract_http_error make_socket_blocking(raw_socket_t sock) {
#if defined(_WIN32)
  u_long mode = 0;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_blocking_fail == 1 || g_mock_raw_blocking_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (ioctlsocket(sock, (long)FIONBIO, &mode) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  int flags;
  int setfl_res;
  flags = fcntl(sock, F_GETFL, 0);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_blocking_fail == 1)
    flags = -1;
#endif
  if (flags < 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  setfl_res = fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_blocking_fail == 2)
    setfl_res = -1;
#endif
  if (setfl_res < 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Parse a URL into host, port, and path components.
 * @param url Input URL string.
 * @param host Output pointer to allocated host string.
 * @param port Output pointer to port number.
 * @param path Output pointer to allocated path string.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
static enum c_abstract_http_error parse_url(const char *url, char **host,
                                            int *port, char **path) {
  const char *p;
  const char *port_start;
  const char *path_start;
  size_t host_len;

  if (!url)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (strncmp(url, "http://", 7) == 0) {
    p = url + 7;
    *port = 80;
  } else if (strncmp(url, "https://", 8) == 0) {
    p = url + 8;
    *port = 443;
  } else {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  port_start = strchr(p, ':');
  path_start = strchr(p, '/');

  if (path_start && port_start && port_start > path_start) {
    port_start = NULL;
  }

  if (port_start) {
    host_len = (size_t)(port_start - p);
    *port = atoi(port_start + 1);
  } else if (path_start) {
    host_len = (size_t)(path_start - p);
  } else {
    host_len = strlen(p);
  }

  *host = (char *)malloc(host_len + 1);
  if (!*host)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memcpy(*host, p, host_len);
  (*host)[host_len] = '\0';

  if (path_start) {
    size_t path_len = strlen(path_start);
    *path = (char *)malloc(path_len + 1);
    if (!*path) {
      free(*host);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(*path, path_start, path_len + 1);
  } else {
    *path = (char *)malloc(2);
    if (!*path) {
      free(*host);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    strcpy_s(*path, 2, "/");
#else
    /* Standard POSIX / GCC Path */
    strcpy(*path, "/");
#endif
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_raw_send(struct HttpTransportContext *ctx,
                                         const struct HttpRequest *req,
                                         struct HttpResponse **res) {
  raw_socket_t sock = RAW_INVALID_SOCKET;
  char *host = NULL;
  int port = 80;
  char *path = NULL;
  struct hostent *he;
  struct sockaddr_in addr;
  char *request_buf = NULL;
  size_t req_len = 0;
  size_t req_cap = 1024;
  char *p;
  size_t len;
  char recv_buf[8192];
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  int rc_send = C_ABSTRACT_HTTP_SUCCESS;
  size_t body_len = 0;
  char *body = NULL;
  size_t body_cap = 0;
  int conn_res = 0;

  (void)ctx;
  LOG_DEBUG("http_raw_send: Entering");
  if (!req || !res) {
    LOG_DEBUG("http_raw_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if ((rc = parse_url(req->url, &host, &port, &path)) !=
      C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_gethostbyname_fail)
    he = NULL;
  else
#endif
    he = gethostbyname(host);
  if (!he) {
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == RAW_INVALID_SOCKET) {
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = raw_htons((unsigned short)port);
  memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

  if ((rc = make_socket_nonblocking(sock)) != C_ABSTRACT_HTTP_SUCCESS) {
    free(host);
    free(path);
    RAW_CLOSESOCKET(sock);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_connect_fail == 1) {
    RAW_CLOSESOCKET(sock);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  conn_res = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_connect_fail == 2) {
    conn_res = -1;
#if defined(_WIN32)
    WSASetLastError(WSAECONNREFUSED);
#else
    errno = ECONNREFUSED;
#endif
  } else if (g_mock_raw_connect_fail == 3) {
    conn_res = 0;
  }
#endif
  if (conn_res < 0) {
#if defined(_WIN32)
    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
    if (errno != EINPROGRESS)
#endif
    {
      RAW_CLOSESOCKET(sock);
      free(host);
      free(path);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  {
    fd_set fdset;
    struct timeval tv;
    int res_sel;
    struct RawCtx *rctx = (struct RawCtx *)ctx;

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = rctx->timeout_ms / 1000;
    tv.tv_usec = (int)((rctx->timeout_ms % 1000) * 1000);

    res_sel = select((int)(sock + 1), NULL, &fdset, NULL, &tv);
    if (res_sel <= 0) {
      RAW_CLOSESOCKET(sock);
      free(host);
      free(path);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  if ((rc = make_socket_blocking(sock)) != C_ABSTRACT_HTTP_SUCCESS) {
    RAW_CLOSESOCKET(sock);
    free(host);
    free(path);
    return rc;
  }

  request_buf = (char *)malloc(req_cap);
  if (!request_buf) {
    RAW_CLOSESOCKET(sock);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  {
    const char *method_str = "GET";
    if (req->method == HTTP_POST)
      method_str = "POST";
    else if (req->method == HTTP_PUT)
      method_str = "PUT";
    else if (req->method == HTTP_DELETE)
      method_str = "DELETE";
    else if (req->method == HTTP_PATCH)
      method_str = "PATCH";
    else if (req->method == HTTP_HEAD)
      method_str = "HEAD";
    else if (req->method == HTTP_OPTIONS)
      method_str = "OPTIONS";

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    sprintf_s(request_buf, req_cap,
              "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n", method_str,
              path, host);
#else
    /* Standard POSIX / GCC Path */
    sprintf(request_buf, "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n",
            method_str, path, host);
#endif
    req_len = strlen(request_buf);
  }
  free(host);
  free(path);

  {
    size_t i;
    for (i = 0; i < req->headers.count; ++i) {
      size_t line_len = strlen(req->headers.headers[i].key) +
                        strlen(req->headers.headers[i].value) + 4;
      if (req_len + line_len >= req_cap) {
        char *new_buf;
        req_cap *= 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_raw_realloc_fail == 1)
          new_buf = NULL;
        else
#endif
          new_buf = (char *)realloc(request_buf, req_cap);
        if (!new_buf) {
          free(request_buf);
          RAW_CLOSESOCKET(sock);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        request_buf = new_buf;
      }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      /* MSVC Safe Path */
      sprintf_s(request_buf + req_len, req_cap - req_len, "%s: %s\r\n",
                req->headers.headers[i].key, req->headers.headers[i].value);
#else
      /* Standard POSIX / GCC Path */
      sprintf(request_buf + req_len, "%s: %s\r\n", req->headers.headers[i].key,
              req->headers.headers[i].value);
#endif
      req_len += strlen(request_buf + req_len);
    }
  }

  if (req->body && req->body_len > 0) {
    char len_buf[64];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    sprintf_s(len_buf, sizeof(len_buf), "Content-Length: %lu\r\n\r\n",
              (unsigned long)req->body_len);
#else
    /* Standard POSIX / GCC Path */
    sprintf(len_buf, "Content-Length: %lu\r\n\r\n",
            (unsigned long)req->body_len);
#endif
    if (req_len + strlen(len_buf) + req->body_len >= req_cap) {
      char *new_buf;
      req_cap = req_len + strlen(len_buf) + req->body_len + 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_raw_realloc_fail == 2)
        new_buf = NULL;
      else
#endif
        new_buf = (char *)realloc(request_buf, req_cap);
      if (!new_buf) {
        free(request_buf);
        RAW_CLOSESOCKET(sock);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      request_buf = new_buf;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    strcat_s(request_buf, req_cap, len_buf);
#else
    /* Standard POSIX / GCC Path */
    strcat(request_buf, len_buf);
#endif
    req_len += strlen(len_buf);
    memcpy(request_buf + req_len, req->body, req->body_len);
    req_len += req->body_len;
  } else {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    strcat_s(request_buf, req_cap, "\r\n");
#else
    /* Standard POSIX / GCC Path */
    strcat(request_buf, "\r\n");
#endif
    req_len += 2;
  }

  p = request_buf;
  len = req_len;
  while (len > 0) {
    long s_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_raw_send_fail) {
      rc_send = C_ABSTRACT_HTTP_ERR_IO;
      break;
    }
#endif
#if defined(_WIN32)
    s_rc = (long)send(sock, p, (int)len, 0);
#else
    s_rc = (long)send(sock, p, len, 0);
#endif
    if (s_rc <= 0) {
      rc_send = C_ABSTRACT_HTTP_ERR_IO;
      break;
    }
    p += (size_t)s_rc;
    len -= (size_t)s_rc;
  }

  free(request_buf);

  if (rc_send != C_ABSTRACT_HTTP_SUCCESS) {
    RAW_CLOSESOCKET(sock);
    return rc_send;
  }

  *res = calloc(1, sizeof(struct HttpResponse));
  if (!*res) {
    RAW_CLOSESOCKET(sock);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  {
    enum c_abstract_http_error rc_init = http_response_init(*res);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_raw_response_init_fail)
      rc_init = C_ABSTRACT_HTTP_ERR_NOMEM;
#endif
    if (rc_init != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      RAW_CLOSESOCKET(sock);
      return rc_init;
    }
  }

  body_cap = 8192;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_realloc_fail == 4)
    body = NULL;
  else
#endif
    body = (char *)malloc(body_cap);
  if (!body) {
    http_response_free(*res);
    free(*res);
    *res = NULL;
    RAW_CLOSESOCKET(sock);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  for (;;) {
    int r_rc;
    fd_set fdset;
    struct timeval tv;
    int res_sel;
    struct RawCtx *rctx = (struct RawCtx *)ctx;

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = rctx->timeout_ms / 1000;
    tv.tv_usec = (int)((rctx->timeout_ms % 1000) * 1000);

    res_sel = select((int)(sock + 1), &fdset, NULL, NULL, &tv);
    if (res_sel <= 0) {
      rc_send = C_ABSTRACT_HTTP_ERR_IO;
      break;
    }

    r_rc = (int)recv(sock, recv_buf, sizeof(recv_buf), 0);
    if (r_rc < 0) {
      rc_send = C_ABSTRACT_HTTP_ERR_IO;
      break;
    } else if (r_rc == 0) {
      break;
    }

    if (req->on_chunk) {
      int cb_rc =
          req->on_chunk(req->on_chunk_user_data, recv_buf, (size_t)r_rc);
      if (cb_rc != 0) {
        rc_send = cb_rc;
        break;
      }
    } else {
      if (body_len + (size_t)r_rc + 1 > body_cap
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
          || g_mock_raw_realloc_fail == 3
#endif
      ) {
        char *new_body;
        body_cap = (body_len + (size_t)r_rc + 1) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_raw_realloc_fail == 3)
          new_body = NULL;
        else
#endif
          new_body = (char *)realloc(body, body_cap);
        if (!new_body) {
          rc_send = C_ABSTRACT_HTTP_ERR_NOMEM;
          break;
        }
        body = new_body;
      }
      memcpy(body + body_len, recv_buf, (size_t)r_rc);
      body_len += (size_t)r_rc;
    }
  }

  RAW_CLOSESOCKET(sock);

  if (rc_send != C_ABSTRACT_HTTP_SUCCESS) {
    http_response_free(*res);
    free(*res);
    *res = NULL;
    free(body);
    return rc_send;
  }

  /* Very rudimentary HTTP parser */
  if (!req->on_chunk && body_len > 0) {
    char *header_end;
    body[body_len] = '\0';
    header_end = strstr(body, "\r\n\r\n");
    if (header_end) {
      char *p_nl;
      size_t header_len = (size_t)(header_end - body) + 4;
      size_t actual_body_len = body_len - header_len;

      /* parse status */
      if (strncmp(body, "HTTP/1.", 7) == 0) {
        (*res)->status_code = atoi(body + 9);
      } else {
        (*res)->status_code = 200;
      }

      /* copy body down */
      if (actual_body_len > 0) {
        char *real_body;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_raw_realloc_fail == 6)
          real_body = NULL;
        else
#endif
          real_body = (char *)malloc(actual_body_len + 1);
        if (real_body) {
          memcpy(real_body, body + header_len, actual_body_len);
          real_body[actual_body_len] = '\0';
          (*res)->body = real_body;
          (*res)->body_len = actual_body_len;
        }
      }

      /* We don't parse full headers here for the rudimentary raw fallback */
      p_nl = strstr(body, "\r\n");
      if (p_nl < header_end) {
        p_nl += 2;
        while (p_nl < header_end) {
          char *colon;
          char *next_nl = strstr(p_nl, "\r\n");
          colon = strchr(p_nl, ':');
          if (colon && colon < next_nl) {
            *colon = '\0';
            *next_nl = '\0';
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
            if (g_mock_raw_realloc_fail == 5)
              rc = C_ABSTRACT_HTTP_ERR_NOMEM;
            else
#endif
              rc = http_headers_add(&(*res)->headers, p_nl, colon + 1);
            if (rc != C_ABSTRACT_HTTP_SUCCESS) {
              http_response_free(*res);
              free(*res);
              *res = NULL;
              free(body);
              return rc;
            }
          }
          p_nl = next_nl + 2;
        }
      }
    } else {
      /* no headers? */
      (*res)->status_code = 200;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_raw_realloc_fail == 7)
        (*res)->body = NULL;
      else
#endif
        (*res)->body = malloc(body_len + 1);
      if ((*res)->body) {
        memcpy((*res)->body, body, body_len);
        ((char *)(*res)->body)[body_len] = '\0';
        (*res)->body_len = body_len;
      }
    }
  }

  free(body);

  LOG_DEBUG("http_raw_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_raw_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *reqs, struct HttpFuture **future) {
  size_t i;
  cah_cppcheck_mut_ptr((void *)ctx);
  (void)loop;

  if (!ctx || !reqs || !future) {
    LOG_DEBUG("http_raw_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < reqs->count; i++) {
    struct HttpResponse *res = NULL;
    enum c_abstract_http_error rc = http_raw_send(ctx, reqs->requests[i], &res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      future[i]->response = res;
      future[i]->error_code = rc;
      future[i]->is_ready = 1;
      return rc;
    } else {
      future[i]->response = res;
      future[i]->error_code = rc;
      future[i]->is_ready = 1;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif
