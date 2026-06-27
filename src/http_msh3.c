
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_msh3.h>
#include "c_abstract_http/log.h"
#include "str.h"
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
  struct HttpConfig base_config;
};

enum c_abstract_http_error http_msh3_global_init(void) {
  if (!g_msh3_mutex) {
    rc = cdd_mutex_init(&g_msh3_mutex);
    if (rc != 0)
      return rc;
  }
  rc = cdd_mutex_lock(g_msh3_mutex);
  if (rc != 0)
    return rc;
  if (g_msh3_init_count++ == 0) {
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    g_msh3_api = MsH3ApiOpen();
    if (!g_msh3_api) {
      g_msh3_init_count--;
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }
  cdd_mutex_unlock(g_msh3_mutex);
  return rc;
}

void http_msh3_global_cleanup(void) {
  if (!g_msh3_mutex)
    return;
  if (cdd_mutex_lock(g_msh3_mutex) != 0)
    return;
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

enum c_abstract_http_error
http_msh3_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  LOG_DEBUG("http_msh3_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_msh3_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    LOG_DEBUG("http_msh3_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&c->base_config);
  if (rc != 0) {
    LOG_DEBUG("http_msh3_context_init: Error http_config_init failed with %d",
              rc);
    free(c);
    return rc;
  }

  c->secure = 1;

  *ctx = c;
  LOG_DEBUG("http_msh3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_msh3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_msh3_context_free: Entering");
  if (!ctx) {
    LOG_DEBUG("http_msh3_context_free: Exiting early (ctx == NULL)");
    return;
  }
  if (ctx->config) {
    MsH3ConfigurationClose(ctx->config);
    ctx->config = NULL;
  }
  http_config_free(&ctx->base_config);
  free(ctx);
  LOG_DEBUG("http_msh3_context_free: Exiting");
}

enum c_abstract_http_error
http_msh3_config_apply(struct HttpTransportContext *ctx,
                       const struct HttpConfig *config) {
  MSH3_SETTINGS settings;
  LOG_DEBUG("http_msh3_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_msh3_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (ctx->config) {
    MsH3ConfigurationClose(ctx->config);
    ctx->config = NULL;
  }

  memset(&settings, 0, sizeof(settings));
  ctx->config = MsH3ConfigurationOpen(g_msh3_api, &settings, sizeof(settings));
  if (!ctx->config) {
    LOG_DEBUG(
        "http_msh3_config_apply: Error ENOMEM (MsH3ConfigurationOpen failed)");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

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

  ctx->base_config.cookie_jar = config->cookie_jar;
  LOG_DEBUG("http_msh3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
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

    if (namelen == 7 && strncmp(name, ":status", 7) == 0) {
      nstr = (char *)malloc(vallen + 1);
      if (nstr) {
        memcpy(nstr, val, vallen);
        nstr[vallen] = '\0';
        rctx->res->status_code = atoi(nstr);
        free(nstr);
      } else {
        LOG_DEBUG("msh3_request_cb: Error ENOMEM parsing status");
      }
    } else {
      nstr = (char *)malloc(namelen + 1);
      vstr = (char *)malloc(vallen + 1);
      if (nstr && vstr) {
        memcpy(nstr, name, namelen);
        nstr[namelen] = '\0';
        memcpy(vstr, val, vallen);
        vstr[vallen] = '\0';
        rc = http_headers_add(&rctx->res->headers, nstr, vstr);
        if (rc != 0) {
          LOG_DEBUG("msh3_request_cb: Error http_headers_add failed with %d",
                    rc);
        }
      } else {
        LOG_DEBUG("msh3_request_cb: Error ENOMEM copying headers");
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
      } else {
        LOG_DEBUG("msh3_request_cb: Error ENOMEM on realloc");
        rctx->error_code = ENOMEM;
      }
    }
    break;
  }
  case MSH3_REQUEST_EVENT_SHUTDOWN_COMPLETE:
    rc = cdd_mutex_lock(rctx->mutex);
    if (rc == 0) {
      rctx->is_complete = 1;
      if (ev->SHUTDOWN_COMPLETE.ConnectionClosedRemotely ||
          ev->SHUTDOWN_COMPLETE.ConnectionErrorCode != 0) {
        rctx->error_code = ECONNREFUSED;
      }
      rc = cdd_cond_signal(rctx->cond);
      if (rc != 0) {
        LOG_DEBUG("msh3_request_cb: Error cdd_cond_signal failed with %d", rc);
      }
      rc = cdd_mutex_unlock(rctx->mutex);
      if (rc != 0) {
        LOG_DEBUG("msh3_request_cb: Error cdd_mutex_unlock failed with %d", rc);
      }
    } else {
      LOG_DEBUG("msh3_request_cb: Error cdd_mutex_lock failed with %d", rc);
    }
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
    return C_ABSTRACT_HTTP_ERR_INVAL;

  *scheme = (char *)malloc(p - url + 1);
  if (!*scheme)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
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
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(*host, h, host_len);
  (*host)[host_len] = '\0';

  if (port_start) {
    size_t port_len = slash ? (size_t)(slash - port_start) : strlen(port_start);
    *port = (char *)malloc(port_len + 1);
    if (!*port) {
      free(*scheme);
      free(*host);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(*port, port_start, port_len);
    (*port)[port_len] = '\0';
  } else {
    if (strcmp(*scheme, "https") == 0) {
      *port = (char *)malloc(4);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(*port, 4, "443");
#else
      strcpy(*port, "443");
#endif
    } else {
      *port = (char *)malloc(3);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(*port, 3, "80");
#else
      strcpy(*port, "80");
#endif
    }
  }

  if (slash) {
    size_t path_len = strlen(slash);
    *path = (char *)malloc(path_len + 1);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*path, path_len + 1, slash);
#else
    strcpy(*path, slash);
#endif
  } else {
    *path = (char *)malloc(2);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*path, 2, "/");
#else
    strcpy(*path, "/");
#endif
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_msh3_send(const struct HttpTransportContext *ctx,
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
  int msh3_status = 0;

  LOG_DEBUG("http_msh3_send: Entering");
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_msh3_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = parse_url(req->url, &host, &port_str, &path, &scheme);
  if (rc != 0) {
    LOG_DEBUG("http_msh3_send: Error parse_url failed with %d", rc);
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(authority, sizeof(authority), "%s:%s", host, port_str);
#else
  sprintf(authority, "%s:%s", host, port_str);
#endif

  *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  if (!*res) {
    LOG_DEBUG("http_msh3_send: Error ENOMEM");
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_response_init(*res);
  if (rc != 0) {
    LOG_DEBUG("http_msh3_send: Error http_response_init failed with %d", rc);
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    free(*res);
    *res = NULL;
    return rc;
  }
  (*res)->status_code = 500; /* default failure */

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  rc = getaddrinfo(host, port_str, &hints, &result);
  if (rc != 0) {
    LOG_DEBUG("http_msh3_send: Error getaddrinfo failed for host '%s'", host);
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
    LOG_DEBUG("http_msh3_send: Error MsH3ConnectionOpen failed");
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  MsH3ConnectionStart(conn, ctx->config, host, &addr);

  memset(&rctx, 0, sizeof(rctx));
  rctx.res = *res;
  rc = cdd_mutex_init(&rctx.mutex);
  if (rc != 0) {
    LOG_DEBUG("http_msh3_send: Error cdd_mutex_init failed with %d", rc);
    MsH3ConnectionClose(conn);
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return rc;
  }
  rc = cdd_cond_init(&rctx.cond);
  if (rc != 0) {
    LOG_DEBUG("http_msh3_send: Error cdd_cond_init failed with %d", rc);
    cdd_mutex_destroy(&rctx.mutex);
    MsH3ConnectionClose(conn);
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return rc;
  }

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
      LOG_DEBUG("http_msh3_send: Error MsH3RequestSend failed");
      rctx.error_code = EIO;
      rctx.is_complete = 1;
    } else {
      rc = cdd_mutex_lock(rctx.mutex);
      if (rc == 0) {
        while (!rctx.is_complete) {
          rc = cdd_cond_wait(rctx.cond, rctx.mutex);
          if (rc != 0) {
            LOG_DEBUG("http_msh3_send: Error cdd_cond_wait failed with %d", rc);
            break;
          }
        }
        rc = cdd_mutex_unlock(rctx.mutex);
        if (rc != 0) {
          LOG_DEBUG("http_msh3_send: Error cdd_mutex_unlock failed with %d",
                    rc);
        }
      } else {
        LOG_DEBUG("http_msh3_send: Error cdd_mutex_lock failed with %d", rc);
        rctx.error_code = rc;
      }
    }

    msh3_status = rctx.error_code;
    MsH3RequestClose(mreq);
  } else {
    LOG_DEBUG("http_msh3_send: Error MsH3RequestOpen failed");
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
    LOG_DEBUG("http_msh3_send: Error returning %d", msh3_status);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return msh3_status;
  }

  LOG_DEBUG("http_msh3_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_msh3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_msh3_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    int rc = http_msh3_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif
