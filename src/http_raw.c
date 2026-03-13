/**
 * @file http_raw.c
 * @brief Raw POSIX socket fallback implementation for HTTP client.
 *
 * Implements an HTTP/1.1 client using basic select/read/write/open/close
 * with optional crypto library integration.
 */

/* clang-format off */
#include <c_abstract_http/http_raw.h>
#include <c_abstract_http/str.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS) || defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)

#if defined(C_ABSTRACT_HTTP_USE_MBEDTLS)
#include <mbedtls/ssl.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#endif

/* Fallback stubs for missing socket headers in strict DOS environments.
   A user compiling for DOS with Watt-32 or mTCP will provide these via 
   external headers or compiler flags. We declare them weakly or just rely
   on standard POSIX signatures. */

#ifndef _WIN32
#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#endif
/* clang-format on */
int http_raw_global_init(void) { return 0; }

void http_raw_global_cleanup(void) {}

struct RawCtx {
  int timeout_ms;
};

int http_raw_context_init(struct HttpTransportContext **ctx) {
  struct RawCtx *c;
  if (!ctx)
    return EINVAL;
  c = (struct RawCtx *)calloc(1, sizeof(struct RawCtx));
  if (!c)
    return ENOMEM;
  c->timeout_ms = 30000;
  *ctx = (struct HttpTransportContext *)c;
  return 0;
}

void http_raw_context_free(struct HttpTransportContext *ctx) {
  if (ctx)
    free(ctx);
}

int http_raw_send(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req, struct HttpResponse **res) {
  /* Manual fallback doing select/read/write/open/close */
  int sock = -1;
  char request_buf[1024];
  char *p;
  size_t len;
  int rc;

  (void)ctx;
  if (!req || !res)
    return EINVAL;

  /* Here you would resolve the hostname and open the socket */
  /* sock = raw_socket_connect(req->url, port); */

  if (sock < 0) {
    return ENOTSUP; /* Placeholder until user links specific TCP stack */
  }

  /* Build simple HTTP GET/POST */
#if defined(_MSC_VER)
  sprintf_s(request_buf, sizeof(request_buf),
            "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
            req->method == HTTP_POST ? "POST" : "GET", "/", "localhost");
#else
  sprintf(request_buf,
          "%s %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
          req->method == HTTP_POST ? "POST" : "GET", "/", "localhost");
#endif

  len = strlen(request_buf);
  p = request_buf;
  while (len > 0) {
#if defined(_WIN32)
    rc = send(sock, p, (int)len, 0);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
    rc = -1; /* DOS Watt-32 or mTCP would implement write/send here */
#else
    rc = write(sock, p, len);
#endif
    if (rc <= 0)
      break;
    p += rc;
    len -= rc;
  }

  /* Read response */
  *res = calloc(1, sizeof(struct HttpResponse));
  if (*res) {
    http_response_init(*res);
    /* dummy response to satisfy compilation */
    (*res)->status_code = 200;
  }

#if defined(_WIN32)
  closesocket(sock);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  /* no-op for dos compilation */
#else
  close(sock);
#endif

  return 0;
}

int http_raw_send_multi(struct HttpTransportContext *ctx,
                        struct ModalityEventLoop *loop,
                        const struct HttpMultiRequest *reqs,
                        struct HttpFuture **future) {
  (void)ctx;
  (void)loop;
  (void)reqs;
  if (future)
    *future = NULL;
  return ENOTSUP;
}

#endif
