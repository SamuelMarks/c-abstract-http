/**
 * @file http_msh3.c
 * @brief Implementation of the MsH3 backend for HTTP/3.
 *
 * Handles HTTP requests via MsH3 natively, enabling HTTP/3 support across
 * all requested modalities.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_msh3.h>
#include <c_abstract_http/str.h>
#include <c_abstract_http/thread_pool.h>

#ifdef C_ABSTRACT_HTTP_USE_MSH3
#include <msh3.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#endif
/* clang-format on */

#ifdef C_ABSTRACT_HTTP_USE_MSH3

static MSH3_API *g_msh3_api = NULL;
static int g_msh3_init_count = 0;
static struct CddMutex *g_msh3_mutex = NULL;

struct HttpTransportContext {
  MSH3_CONFIGURATION *config;
  int secure;
};

int http_msh3_global_init(void) {
  int rc = 0;
  if (!g_msh3_mutex) {
    cdd_mutex_init(&g_msh3_mutex);
  }
  cdd_mutex_lock(g_msh3_mutex);
  if (g_msh3_init_count++ == 0) {
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    g_msh3_api = MsH3ApiOpen();
    if (!g_msh3_api) {
      g_msh3_init_count--;
      rc = ENOMEM;
    }
  }
  cdd_mutex_unlock(g_msh3_mutex);
  return rc;
}

void http_msh3_global_cleanup(void) {
  if (!g_msh3_mutex)
    return;
  cdd_mutex_lock(g_msh3_mutex);
  if (g_msh3_init_count > 0 && --g_msh3_init_count == 0) {
    if (g_msh3_api) {
      MsH3ApiClose(g_msh3_api);
      g_msh3_api = NULL;
    }
#if defined(_WIN32)
    WSACleanup();
#endif
  }
  cdd_mutex_unlock(g_msh3_mutex);
}

int http_msh3_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  if (!ctx)
    return EINVAL;

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c)
    return ENOMEM;

  c->secure = 1;

  *ctx = c;
  return 0;
}

void http_msh3_context_free(struct HttpTransportContext *ctx) {
  if (!ctx)
    return;
  if (ctx->config) {
    MsH3ConfigurationClose(ctx->config);
    ctx->config = NULL;
  }
  free(ctx);
}

int http_msh3_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  MSH3_SETTINGS settings;
  if (!ctx || !config)
    return EINVAL;

  if (ctx->config) {
    MsH3ConfigurationClose(ctx->config);
    ctx->config = NULL;
  }

  memset(&settings, 0, sizeof(settings));
  ctx->config = MsH3ConfigurationOpen(g_msh3_api, &settings, sizeof(settings));
  if (!ctx->config)
    return ENOMEM;

  if (config->verify_peer == 0 || config->verify_host == 0) {
    MSH3_CREDENTIAL_CONFIG cred;
    memset(&cred, 0, sizeof(cred));
    cred.Type = MSH3_CREDENTIAL_TYPE_NONE;
    cred.Flags = MSH3_CREDENTIAL_FLAG_CLIENT |
                 MSH3_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    MsH3ConfigurationLoadCredential(ctx->config, &cred);
  } else {
    MSH3_CREDENTIAL_CONFIG cred;
    memset(&cred, 0, sizeof(cred));
    cred.Type = MSH3_CREDENTIAL_TYPE_NONE;
    cred.Flags = MSH3_CREDENTIAL_FLAG_CLIENT;
    MsH3ConfigurationLoadCredential(ctx->config, &cred);
  }

  return 0;
}

/* Internal context for MSH3 callbacks */
struct msh3_req_ctx {
  struct HttpResponse *res;
  struct CddMutex *mutex;
  struct CddCond *cond;
  int is_complete;
  int error_code;
};

static MSH3_STATUS MSH3_CALL msh3_request_cb(MSH3_REQUEST *req, void *ctx,
                                             MSH3_REQUEST_EVENT *ev) {
  struct msh3_req_ctx *rctx = (struct msh3_req_ctx *)ctx;
  (void)req;

  switch (ev->Type) {
  case MSH3_REQUEST_EVENT_HEADER_RECEIVED: {
    const char *name = ev->HEADER_RECEIVED.Header->Name;
    size_t namelen = ev->HEADER_RECEIVED.Header->NameLength;
    const char *val = ev->HEADER_RECEIVED.Header->Value;
    size_t vallen = ev->HEADER_RECEIVED.Header->ValueLength;
    char *nstr, *vstr;

    if (namelen == 7 && strncmp(name, ":status", 7) == 0) {
      nstr = (char *)malloc(vallen + 1);
      if (nstr) {
        memcpy(nstr, val, vallen);
        nstr[vallen] = '\0';
        rctx->res->status_code = atoi(nstr);
        free(nstr);
      }
    } else {
      nstr = (char *)malloc(namelen + 1);
      vstr = (char *)malloc(vallen + 1);
      if (nstr && vstr) {
        memcpy(nstr, name, namelen);
        nstr[namelen] = '\0';
        memcpy(vstr, val, vallen);
        vstr[vallen] = '\0';
        http_headers_add(&rctx->res->headers, nstr, vstr);
      }
      if (nstr)
        free(nstr);
      if (vstr)
        free(vstr);
    }
    break;
  }
  case MSH3_REQUEST_EVENT_DATA_RECEIVED: {
    size_t dlen = ev->DATA_RECEIVED.Length;
    if (dlen > 0) {
      void *new_body = realloc(rctx->res->body, rctx->res->body_len + dlen);
      if (new_body) {
        rctx->res->body = new_body;
        memcpy((char *)rctx->res->body + rctx->res->body_len,
               ev->DATA_RECEIVED.Data, dlen);
        rctx->res->body_len += dlen;
      }
    }
    break;
  }
  case MSH3_REQUEST_EVENT_SHUTDOWN_COMPLETE:
    cdd_mutex_lock(rctx->mutex);
    rctx->is_complete = 1;
    if (ev->SHUTDOWN_COMPLETE.ConnectionClosedRemotely ||
        ev->SHUTDOWN_COMPLETE.ConnectionErrorCode != 0) {
      rctx->error_code = ECONNREFUSED;
    }
    cdd_cond_signal(rctx->cond);
    cdd_mutex_unlock(rctx->mutex);
    break;
  default:
    break;
  }
  return MSH3_STATUS_SUCCESS;
}

static MSH3_STATUS MSH3_CALL msh3_conn_cb(MSH3_CONNECTION *conn, void *ctx,
                                          MSH3_CONNECTION_EVENT *ev) {
  (void)conn;
  (void)ctx;
  (void)ev;
  return MSH3_STATUS_SUCCESS;
}

static int parse_url(const char *url, char **host, char **port, char **path,
                     char **scheme) {
  const char *p = strstr(url, "://");
  const char *h, *slash, *colon, *port_start;
  size_t host_len;
  if (!p)
    return EINVAL;

  *scheme = (char *)malloc(p - url + 1);
  if (!*scheme)
    return ENOMEM;
  memcpy(*scheme, url, p - url);
  (*scheme)[p - url] = '\0';

  p += 3;
  h = p;
  slash = strchr(h, '/');
  colon = strchr(h, ':');

  port_start = NULL;
  host_len = 0;

  if (colon && (!slash || colon < slash)) {
    host_len = colon - h;
    port_start = colon + 1;
  } else if (slash) {
    host_len = slash - h;
  } else {
    host_len = strlen(h);
  }

  *host = (char *)malloc(host_len + 1);
  if (!*host) {
    free(*scheme);
    return ENOMEM;
  }
  memcpy(*host, h, host_len);
  (*host)[host_len] = '\0';

  if (port_start) {
    size_t port_len = slash ? (size_t)(slash - port_start) : strlen(port_start);
    *port = (char *)malloc(port_len + 1);
    if (!*port) {
      free(*scheme);
      free(*host);
      return ENOMEM;
    }
    memcpy(*port, port_start, port_len);
    (*port)[port_len] = '\0';
  } else {
    if (strcmp(*scheme, "https") == 0) {
      *port = (char *)malloc(4);
#if defined(_MSC_VER)
      strcpy_s(*port, 4, "443");
#else
      strcpy(*port, "443");
#endif
    } else {
      *port = (char *)malloc(3);
#if defined(_MSC_VER)
      strcpy_s(*port, 3, "80");
#else
      strcpy(*port, "80");
#endif
    }
  }

  if (slash) {
    size_t path_len = strlen(slash);
    *path = (char *)malloc(path_len + 1);
#if defined(_MSC_VER)
    strcpy_s(*path, path_len + 1, slash);
#else
    strcpy(*path, slash);
#endif
  } else {
    *path = (char *)malloc(2);
#if defined(_MSC_VER)
    strcpy_s(*path, 2, "/");
#else
    strcpy(*path, "/");
#endif
  }
  return 0;
}

int http_msh3_send(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  struct msh3_req_ctx rctx;
  MSH3_CONNECTION *conn;
  MSH3_ADDR addr;
  MSH3_REQUEST *mreq;
  MSH3_HEADER headers[4];
  struct addrinfo hints, *result;
  char *host = NULL, *port_str = NULL, *path = NULL, *scheme = NULL;
  char authority[256];
  const char *method_str = "GET";
  int rc;
  int msh3_status = 0;

  if (!ctx || !req || !res || !req->url)
    return EINVAL;

  if (parse_url(req->url, &host, &port_str, &path, &scheme) != 0) {
    return EINVAL;
  }

  snprintf(authority, sizeof(authority), "%s:%s", host, port_str);

  *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  if (!*res) {
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    return ENOMEM;
  }

  http_response_init(*res);
  (*res)->status_code = 500; /* default failure */

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  rc = getaddrinfo(host, port_str, &hints, &result);
  if (rc != 0) {
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return EHOSTUNREACH;
  }

  memset(&addr, 0, sizeof(addr));
  if (result->ai_family == AF_INET) {
    memcpy(&addr.Ipv4, result->ai_addr, sizeof(struct sockaddr_in));
  } else if (result->ai_family == AF_INET6) {
    memcpy(&addr.Ipv6, result->ai_addr, sizeof(struct sockaddr_in6));
  }
  freeaddrinfo(result);

  conn = MsH3ConnectionOpen(g_msh3_api, msh3_conn_cb, NULL);
  if (!conn) {
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return EIO;
  }

  MsH3ConnectionStart(conn, ctx->config, host, &addr);

  memset(&rctx, 0, sizeof(rctx));
  rctx.res = *res;
  cdd_mutex_init(&rctx.mutex);
  cdd_cond_init(&rctx.cond);

  mreq = MsH3RequestOpen(conn, msh3_request_cb, &rctx, MSH3_REQUEST_FLAG_NONE);
  if (mreq) {
    if (req->method == HTTP_POST)
      method_str = "POST";
    else if (req->method == HTTP_PUT)
      method_str = "PUT";
    else if (req->method == HTTP_DELETE)
      method_str = "DELETE";
    else if (req->method == HTTP_PATCH)
      method_str = "PATCH";

    headers[0].Name = ":method";
    headers[0].NameLength = 7;
    headers[0].Value = method_str;
    headers[0].ValueLength = strlen(method_str);

    headers[1].Name = ":scheme";
    headers[1].NameLength = 7;
    headers[1].Value = scheme;
    headers[1].ValueLength = strlen(scheme);

    headers[2].Name = ":authority";
    headers[2].NameLength = 10;
    headers[2].Value = authority;
    headers[2].ValueLength = strlen(authority);

    headers[3].Name = ":path";
    headers[3].NameLength = 5;
    headers[3].Value = path;
    headers[3].ValueLength = strlen(path);

    if (!MsH3RequestSend(mreq, MSH3_REQUEST_SEND_FLAG_FIN, headers, 4,
                         req->body, (uint32_t)req->body_len, rctx.res)) {
      rctx.error_code = EIO;
      rctx.is_complete = 1;
    } else {
      cdd_mutex_lock(rctx.mutex);
      while (!rctx.is_complete) {
        cdd_cond_wait(rctx.cond, rctx.mutex);
      }
      cdd_mutex_unlock(rctx.mutex);
    }

    msh3_status = rctx.error_code;
    MsH3RequestClose(mreq);
  } else {
    msh3_status = ENOMEM;
  }

  MsH3ConnectionClose(conn);
  cdd_cond_free(rctx.cond);
  cdd_mutex_free(rctx.mutex);

  free(host);
  free(port_str);
  free(path);
  free(scheme);

  if (msh3_status != 0) {
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return msh3_status;
  }

  return 0;
}

int http_msh3_send_multi(struct HttpTransportContext *ctx,
                         struct ModalityEventLoop *loop,
                         const struct HttpMultiRequest *multi,
                         struct HttpFuture **futures) {
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOSYS;
}

#else

/* Stubs for when MSH3 is disabled */
int http_msh3_global_init(void) { return ENOSYS; }
void http_msh3_global_cleanup(void) {}
int http_msh3_context_init(struct HttpTransportContext **ctx) {
  (void)ctx;
  return ENOSYS;
}
void http_msh3_context_free(struct HttpTransportContext *ctx) { (void)ctx; }
int http_msh3_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  (void)ctx;
  (void)config;
  return ENOSYS;
}
int http_msh3_send(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  (void)ctx;
  (void)req;
  (void)res;
  return ENOSYS;
}
int http_msh3_send_multi(struct HttpTransportContext *ctx,
                         struct ModalityEventLoop *loop,
                         const struct HttpMultiRequest *multi,
                         struct HttpFuture **futures) {
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOSYS;
}

#endif
