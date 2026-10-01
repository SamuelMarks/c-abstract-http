/* clang-format off */
#if !defined(_WIN32)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_msh3.h>
#include "c_abstract_http/log.h"
#include "str.h"
#include <c_abstract_http/thread_pool.h>

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_MSH3)
#include <msh3.h>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#else
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

* / enum c_abstract_http_error http_msh3_global_init(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  if (!g_msh3_mutex) {
    {
      rc = abstract_http_mutex_init(&g_msh3_mutex);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  { rc = abstract_http_mutex_lock(g_msh3_mutex); }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
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
  {
    enum c_abstract_http_error u_rc = abstract_http_mutex_unlock(g_msh3_mutex);
    if (u_rc != C_ABSTRACT_HTTP_SUCCESS && rc == C_ABSTRACT_HTTP_SUCCESS) {
      rc = u_rc;
    }
  }
  return rc;
}

/**
 * @brief Clean up global MsH3 state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_msh3_global_cleanup(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  if (!g_msh3_mutex) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  { rc = abstract_http_mutex_lock(g_msh3_mutex); }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  if (--g_msh3_init_count == 0) {
    MsH3ApiClose(g_msh3_api);
    g_msh3_api = NULL;
#if defined(_WIN32)
    WSACleanup();
#endif
  }
  rc = abstract_http_mutex_unlock(g_msh3_mutex);
  if (g_msh3_init_count == 0) {
    abstract_http_mutex_free(g_msh3_mutex);
    g_msh3_mutex = NULL;
  }
  return rc;
}

/**
 * @brief Initialize a new MsH3 transport context.
 *
 * @param[out] ctx Double pointer to receive allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_msh3_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;
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

  { rc = http_config_init(&c->base_config); }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_context_init: Error http_config_init failed with %d",
              (int)rc);
    free(c);
    return rc;
  }

  c->secure = 1;

  *ctx = c;
  LOG_DEBUG("http_msh3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free MsH3 transport context.
 *
 * @param[in] ctx The context to free. Satisfies NULL safety.
 */
enum c_abstract_http_error
http_msh3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_msh3_context_free: Entering");
  if (ctx) {
    if (ctx->config) {
      MsH3ConfigurationClose(ctx->config);
      ctx->config = NULL;
    }
    {
      enum c_abstract_http_error rch = http_config_free(&ctx->base_config);
      if (rch != C_ABSTRACT_HTTP_SUCCESS)
        return rch;
    }
    free(ctx);
  }
  LOG_DEBUG("http_msh3_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Apply configuration settings to MsH3 context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config The configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_msh3_config_apply(struct HttpTransportContext *ctx,
                       const struct HttpConfig *config) {
  MSH3_SETTINGS settings;
  MSH3_CREDENTIAL_CONFIG cred;
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

  memset(&cred, 0, sizeof(cred));
  cred.Type = MSH3_CREDENTIAL_TYPE_NONE;
  if (!config->verify_peer) {
    cred.Flags = MSH3_CREDENTIAL_FLAG_CLIENT |
                 MSH3_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
  } else {
    cred.Flags = MSH3_CREDENTIAL_FLAG_CLIENT;
  }
  if (MsH3ConfigurationLoadCredential(ctx->config, &cred) != 0) {
    /* ignore failure? */
  }

  ctx->base_config.timeout_ms = config->timeout_ms;
  ctx->base_config.verify_peer = config->verify_peer;
  ctx->base_config.verify_host = config->verify_host;
  ctx->base_config.follow_redirects = config->follow_redirects;

  LOG_DEBUG("http_msh3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Internal context for MsH3 callbacks */
struct msh3_req_ctx {
  /** @brief Output response */
  struct HttpResponse *res;
  /** @brief Synchronization mutex */
  struct AbstractHttpMutex *mutex;
  /** @brief Synchronization condition variable */
  struct AbstractHttpCond *cond;
  /** @brief Completion flag */
  int is_complete;
  /** @brief Error code */
  enum c_abstract_http_error error_code;
};

static MSH3_STATUS MSH3_CALL msh3_request_cb(MSH3_REQUEST *req, void *ctx,
                                             MSH3_REQUEST_EVENT *ev) {
  struct msh3_req_ctx *rctx = (struct msh3_req_ctx *)ctx;
  char *nstr = NULL;
  char *vstr = NULL;
  void *new_body = NULL;
  const char *name = NULL;
  const char *val = NULL;
  size_t namelen = 0;
  size_t vallen = 0;
  size_t dlen = 0;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  if (!req) {
    return MSH3_STATUS_SUCCESS;
  }

  if (!rctx || !ev) {
    return MSH3_STATUS_SUCCESS;
  }

  switch (ev->Type) {
  case MSH3_REQUEST_EVENT_HEADER_RECEIVED: {
    if (!ev->HEADER_RECEIVED.Header) {
      break;
    }
    name = ev->HEADER_RECEIVED.Header->Name;
    namelen = ev->HEADER_RECEIVED.Header->NameLength;
    val = ev->HEADER_RECEIVED.Header->Value;
    vallen = ev->HEADER_RECEIVED.Header->ValueLength;

    if (namelen == 7 && strncmp(name, ":status", 7) == 0) {
      {
        nstr = (char *)malloc(vallen + 1);
      }
      if (nstr) {
        memcpy(nstr, val, vallen);
        nstr[vallen] = '\0';
        rctx->res->status_code = atoi(nstr);
        free(nstr);
      } else {
        LOG_DEBUG("msh3_request_cb: Error ENOMEM parsing status");
        rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    } else {
      {
        nstr = (char *)malloc(namelen + 1);
        vstr = (char *)malloc(vallen + 1);
      }
      if (nstr && vstr) {
        memcpy(nstr, name, namelen);
        nstr[namelen] = '\0';
        memcpy(vstr, val, vallen);
        vstr[vallen] = '\0';
        { rc = http_headers_add(&rctx->res->headers, nstr, vstr); }
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("msh3_request_cb: Error http_headers_add failed with %d",
                    (int)rc);
        }
      } else {
        LOG_DEBUG("msh3_request_cb: Error ENOMEM copying headers");
      }
      if (nstr) {
        free(nstr);
      }
      if (vstr) {
        free(vstr);
      }
    }
    break;
  }
  case MSH3_REQUEST_EVENT_DATA_RECEIVED: {
    dlen = ev->DATA_RECEIVED.Length;
    if (dlen > 0) {
      {
        new_body = realloc(rctx->res->body, rctx->res->body_len + dlen + 1);
      }
      if (new_body) {
        rctx->res->body = new_body;
        memcpy((char *)rctx->res->body + rctx->res->body_len,
               ev->DATA_RECEIVED.Data, dlen);
        rctx->res->body_len += dlen;
        ((char *)rctx->res->body)[rctx->res->body_len] = '\0';
      } else {
        LOG_DEBUG("msh3_request_cb: Error ENOMEM on realloc");
        rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
    break;
  }
  case MSH3_REQUEST_EVENT_SHUTDOWN_COMPLETE: {
    rc = abstract_http_mutex_lock(rctx->mutex);
  }
    if (rc == C_ABSTRACT_HTTP_SUCCESS) {
      {
        rctx->is_complete = 1;
      }
      if (ev->SHUTDOWN_COMPLETE.ConnectionClosedRemotely ||
          ev->SHUTDOWN_COMPLETE.ConnectionErrorCode != 0) {
        rctx->error_code = C_ABSTRACT_HTTP_ERR_IO;
      }
      {
        enum c_abstract_http_error cond_rc =
            abstract_http_cond_signal(rctx->cond);
        if (cond_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("msh3_request_cb: abstract_http_cond_signal failed");
        }
      }
      {
        enum c_abstract_http_error cond_rc =
            abstract_http_mutex_unlock(rctx->mutex);
        if (cond_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("msh3_request_cb: abstract_http_mutex_unlock failed");
        }
      }
    } else {
      LOG_DEBUG(
          "msh3_request_cb: Error abstract_http_mutex_lock failed with %d",
          (int)rc);
    }
    break;
  default:
    break;
  }
  return MSH3_STATUS_SUCCESS;
}

static MSH3_STATUS MSH3_CALL msh3_conn_cb(MSH3_CONNECTION *conn, void *ctx,
                                          MSH3_CONNECTION_EVENT *ev) {
  if (!conn || !ctx || !ev) {
    return MSH3_STATUS_SUCCESS;
  }
  return MSH3_STATUS_SUCCESS;
}

static enum c_abstract_http_error parse_url(const char *url, char **host,
                                            char **port, char **path,
                                            char **scheme) {
  const char *p;
  const char *h;
  const char *slash;
  const char *colon;
  const char *port_start;
  size_t host_len;
  size_t port_len;
  size_t path_len;
  size_t scheme_len;

  p = strstr(url, "://");
  if (!p) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  scheme_len = (size_t)(p - url);
  { *scheme = (char *)malloc(scheme_len + 1); }
  if (!*scheme) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(*scheme, url, scheme_len);
  (*scheme)[scheme_len] = '\0';

  p += 3;
  h = p;
  slash = strchr(h, '/');
  colon = strchr(h, ':');

  port_start = NULL;
  host_len = 0;

  if (colon && (!slash || colon < slash)) {
    host_len = (size_t)(colon - h);
    port_start = colon + 1;
  } else if (slash) {
    host_len = (size_t)(slash - h);
  } else {
    host_len = strlen(h);
  }

  { *host = (char *)malloc(host_len + 1); }
  if (!*host) {
    free(*scheme);
    *scheme = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(*host, h, host_len);
  (*host)[host_len] = '\0';

  if (port_start) {
    port_len = slash ? (size_t)(slash - port_start) : strlen(port_start);
    { *port = (char *)malloc(port_len + 1); }
    if (!*port) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(*port, port_start, port_len);
    (*port)[port_len] = '\0';
  } else {
    if (strcmp(*scheme, "https") == 0) {
      {
        *port = (char *)malloc(4);
      }
      if (!*port) {
        free(*scheme);
        *scheme = NULL;
        free(*host);
        *host = NULL;
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(*port, 4, "443");
#else
      strcpy(*port, "443");
#endif
    } else {
      { *port = (char *)malloc(3); }
      if (!*port) {
        free(*scheme);
        *scheme = NULL;
        free(*host);
        *host = NULL;
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(*port, 3, "80");
#else
      strcpy(*port, "80");
#endif
    }
  }

  if (slash) {
    path_len = strlen(slash);
    { *path = (char *)malloc(path_len + 1); }
    if (!*path) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      free(*port);
      *port = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*path, path_len + 1, slash);
#else
    strcpy(*path, slash);
#endif
  } else {
    { *path = (char *)malloc(2); }
    if (!*path) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      free(*port);
      *port = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*path, 2, "/");
#else
    strcpy(*path, "/");
#endif
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Synchronously send HTTP request via MsH3.
 *
 * @param[in] ctx Transport context.
 * @param[in] req HTTP request.
 * @param[out] res Double pointer to receive response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_msh3_send(struct HttpTransportContext *ctx,
                                          const struct HttpRequest *req,
                                          struct HttpResponse **res) {
  struct msh3_req_ctx rctx;
  MSH3_CONNECTION *conn = NULL;
  MSH3_ADDR addr;
  MSH3_REQUEST *mreq = NULL;
  MSH3_HEADER headers[4];
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  char *host = NULL;
  char *port_str = NULL;
  char *path = NULL;
  char *scheme = NULL;
  char authority[256];
  const char *method_str = "GET";
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  enum c_abstract_http_error msh3_status = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_msh3_send: Entering");
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_msh3_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = NULL;

  rc = parse_url(req->url, &host, &port_str, &path, &scheme);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_send: Error parse_url failed with %d", (int)rc);
    return rc;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(authority, sizeof(authority), "%s:%s", host, port_str);
#else
  sprintf(authority, "%s:%s", host, port_str);
#endif

  { *res = (struct HttpResponse *)calloc(1, sizeof(**res)); }
  if (!*res) {
    LOG_DEBUG("http_msh3_send: Error ENOMEM");
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  { rc = http_response_init(*res); }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_send: Error http_response_init failed with %d",
              (int)rc);
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    free(*res);
    *res = NULL;
    return rc;
  }
  (*res)->status_code = 500;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  {
    rc = (enum c_abstract_http_error)getaddrinfo(host, port_str, &hints,
                                                 &result);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_send: Error getaddrinfo failed for host '%s'", host);
    free(host);
    free(port_str);
    free(path);
    free(scheme);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  memset(&addr, 0, sizeof(addr));
  if (result) {
    if (result->ai_addrlen > 0) {
      memcpy(&addr, result->ai_addr, (size_t)result->ai_addrlen);
    }
    freeaddrinfo(result);
  }

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

  if (!MsH3ConnectionStart(conn, ctx->config, host, &addr)) {
    /* MSH3 doesn't typically return an error code from start but let's check it
     */
  }

  memset(&rctx, 0, sizeof(rctx));
  rctx.res = *res;
  rctx.error_code = C_ABSTRACT_HTTP_SUCCESS;

  { rc = abstract_http_mutex_init(&rctx.mutex); }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_send: Error abstract_http_mutex_init failed with %d",
              (int)rc);
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

  { rc = abstract_http_cond_init(&rctx.cond); }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_send: Error abstract_http_cond_init failed with %d",
              (int)rc);
    abstract_http_mutex_free(rctx.mutex);
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

  switch (req->method) {
  case HTTP_GET:
    method_str = "GET";
    break;
  case HTTP_POST:
    method_str = "POST";
    break;
  case HTTP_PUT:
    method_str = "PUT";
    break;
  case HTTP_DELETE:
    method_str = "DELETE";
    break;
  case HTTP_PATCH:
    method_str = "PATCH";
    break;
  case HTTP_HEAD:
    method_str = "HEAD";
    break;
  case HTTP_OPTIONS:
    method_str = "OPTIONS";
    break;
  default:
    method_str = "GET";
    break;
  }

  headers[0].Name = ":method";
  headers[0].NameLength = 7;
  headers[0].Value = method_str;
  headers[0].ValueLength = strlen(method_str);

  headers[1].Name = ":path";
  headers[1].NameLength = 5;
  headers[1].Value = path;
  headers[1].ValueLength = strlen(path);

  headers[2].Name = ":scheme";
  headers[2].NameLength = 7;
  headers[2].Value = scheme;
  headers[2].ValueLength = strlen(scheme);

  headers[3].Name = ":authority";
  headers[3].NameLength = 10;
  headers[3].Value = authority;
  headers[3].ValueLength = strlen(authority);

  mreq = MsH3RequestOpen(conn, msh3_request_cb, &rctx, MSH3_REQUEST_FLAG_NONE);
  if (mreq) {
    if (!MsH3RequestSend(mreq, MSH3_REQUEST_SEND_FLAG_FIN, headers, 4, NULL)) {
      LOG_DEBUG("http_msh3_send: Error MsH3RequestSend failed");
      rctx.error_code = C_ABSTRACT_HTTP_ERR_IO;
    } else {
      { rc = abstract_http_mutex_lock(rctx.mutex); }
      if (rc == C_ABSTRACT_HTTP_SUCCESS) {
        while (!rctx.is_complete) {
          rc = abstract_http_cond_wait(rctx.cond, rctx.mutex);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            LOG_DEBUG(
                "http_msh3_send: Error abstract_http_cond_wait failed with %d",
                (int)rc);
            rctx.error_code = rc;
            break;
          }
        }
        {
          enum c_abstract_http_error cond_rc =
              abstract_http_mutex_unlock(rctx.mutex);
          if (cond_rc != C_ABSTRACT_HTTP_SUCCESS &&
              rctx.error_code == C_ABSTRACT_HTTP_SUCCESS) {
            rctx.error_code = cond_rc;
          }
        }
      } else {
        LOG_DEBUG(
            "http_msh3_send: Error abstract_http_mutex_lock failed with %d",
            (int)rc);
        rctx.error_code = rc;
      }
    }

    msh3_status = rctx.error_code;
    MsH3RequestClose(mreq);
  } else {
    LOG_DEBUG("http_msh3_send: Error MsH3RequestOpen failed");
    msh3_status = C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  MsH3ConnectionClose(conn);
  abstract_http_cond_free(rctx.cond);
  abstract_http_mutex_free(rctx.mutex);

  free(host);
  free(port_str);
  free(path);
  free(scheme);

  if (msh3_status != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_msh3_send: Error returning %d", (int)msh3_status);
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return msh3_status;
  }

  LOG_DEBUG("http_msh3_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Asynchronously send multi-requests via MsH3.
 *
 * @param[in] ctx Transport context.
 * @param[in] loop Event loop.
 * @param[in] multi Multi-request structure.
 * @param[out] futures Array of futures.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_msh3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  if (loop) {
    /* unused */
  }

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_msh3_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    rc = http_msh3_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
