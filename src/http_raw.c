
/* clang-format off */
#include <c_abstract_http/http_raw.h>
#include "c_abstract_http/log.h"
#include "str.h"
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

/* Fallback headers for missing socket headers in strict DOS environments.
   A user compiling for DOS with Watt-32 or mTCP will provide these via
   external headers or compiler flags. We declare them weakly or just rely
   on standard POSIX signatures. */

#ifndef _WIN32
#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#if !defined(_MSC_VER)
#ifndef _MSC_VER
#include <unistd.h>
#endif
#endif
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
  struct HttpConfig config;
};

int http_raw_context_init(struct HttpTransportContext **ctx) {
  struct RawCtx *c;
  int rc;
  LOG_DEBUG("http_raw_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_raw_context_init: Error EINVAL");
    return EINVAL;
  }
  c = (struct RawCtx *)calloc(1, sizeof(struct RawCtx));
  if (!c) {
    LOG_DEBUG("http_raw_context_init: Error ENOMEM");
    return ENOMEM;
  }

  rc = http_config_init(&c->config);
  if (rc != 0) {
    LOG_DEBUG("http_raw_context_init: Error http_config_init failed with %d",
              rc);
    free(c);
    return rc;
  }

  c->timeout_ms = 30000;
  *ctx = (struct HttpTransportContext *)c;
  LOG_DEBUG("http_raw_context_init: Success");
  return 0;
}

void http_raw_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_raw_context_free: Entering");
  if (ctx) {
    http_config_free(&((struct RawCtx *)ctx)->config);
    free(ctx);
  }
  LOG_DEBUG("http_raw_context_free: Exiting");
}

int http_raw_send(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req, struct HttpResponse **res) {
  /* Manual fallback doing select/read/write/open/close */
  int sock = -1;
  char request_buf[1024];
  char *p;
  size_t len;

  (void)ctx;
  LOG_DEBUG("http_raw_send: Entering");
  if (!req || !res) {
    LOG_DEBUG("http_raw_send: Error EINVAL");
    return EINVAL;
  }

  /* Here you would resolve the hostname and open the socket */
  /* sock = raw_socket_connect(req->url, port); */

  if (sock < 0) {
    LOG_DEBUG("http_raw_send: Error ENOTSUP (socket not connected)");
    return ENOTSUP; /* Placeholder until user links specific TCP stack */
  }

  /* Build simple HTTP GET/POST */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
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
  (void)p;
  while (len > 0) {
#if defined(_WIN32)
    int rc = send(sock, p, (int)len, 0);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
    int rc = -1; /* DOS Watt-32 or mTCP would implement write/send here */
#else
    int rc = write(sock, p, len);
#endif
    if (rc <= 0) {
      LOG_DEBUG("http_raw_send: Error writing to socket");
      break;
    }
    p += rc;
    (void)p;
    len -= rc;
  }

  /* Read response */
  *res = calloc(1, sizeof(struct HttpResponse));
  if (*res) {
    int init_rc = http_response_init(*res);
    if (init_rc != 0) {
      LOG_DEBUG("http_raw_send: Error http_response_init failed with %d",
                init_rc);
      free(*res);
      *res = NULL;
#if defined(_WIN32)
      closesocket(sock);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
      /* no-op for dos compilation */
#else
      close(sock);
#endif
      return init_rc;
    }
    /* dummy response to satisfy compilation */
    (*res)->status_code = 200;
  } else {
    LOG_DEBUG("http_raw_send: Error ENOMEM");
#if defined(_WIN32)
    closesocket(sock);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
    /* no-op for dos compilation */
#else
    close(sock);
#endif
    return ENOMEM;
  }

#if defined(_WIN32)
  closesocket(sock);
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  /* no-op for dos compilation */
#else
  close(sock);
#endif

  LOG_DEBUG("http_raw_send: Success");
  return 0;
}

int http_raw_send_multi(struct HttpTransportContext *ctx,
                        struct ModalityEventLoop *loop,
                        const struct HttpMultiRequest *reqs,
                        struct HttpFuture **future) {
  size_t i;
  cah_cppcheck_mut_ptr((void *)ctx);
  (void)loop;

  if (!ctx || !reqs || !future) {
    LOG_DEBUG("http_raw_send_multi: Error EINVAL");
    return EINVAL;
  }

  for (i = 0; i < reqs->count; i++) {
    struct HttpResponse *res = NULL;
    int rc = http_raw_send(ctx, reqs->requests[i], &res);
    future[i]->response = res;
    future[i]->error_code = rc;
    future[i]->is_ready = 1;
  }
  return 0;
}

#endif
