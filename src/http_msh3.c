/**
 * @file http_msh3.c
 * @brief MsH3 implementation of the Abstract Network Interface (ANI).
 * @author Samuel Marks
 */

/* clang-format off */
#if !defined(_WIN32)
#ifndef _DEFAULT_SOURCE
/** @brief Enable default POSIX/BSD definitions */
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
/** @brief Enable POSIX.1-2008 definitions */
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <c_abstract_http/http_msh3.h>
#include "c_abstract_http/log.h"
#include <c_abstract_http/thread_pool.h>

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HAVE_REAL_MSH3)
#include "mock_stubs/mock_msh3.h"
#else
#include <msh3.h>
#endif
/* clang-format on */

/** @brief Reference count for global MsH3 initialization */
static int g_msh3_init_count = 0;
/** @brief Global initialization mutex for MsH3 */
static struct AbstractHttpMutex *g_msh3_mutex = NULL;
/** @brief Shared MsH3 API instance */
static MSH3_API *g_msh3_api = NULL;

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief MsH3 configuration handle */
  MSH3_CONFIGURATION *config;
  /** @brief Secure connection flag */
  int secure;
  /** @brief Base configuration settings */
  struct HttpConfig base_config;
};

/** @brief Internal context for MsH3 callbacks */
struct msh3_req_ctx {
  /** @brief Originating request (for streaming on_chunk callback) */
  const struct HttpRequest *req;
  /** @brief Optional cookie jar for Set-Cookie storage */
  struct HttpCookieJar *cookie_jar;
  /** @brief Output response */
  struct HttpResponse *res;
  /** @brief Synchronization mutex */
  struct AbstractHttpMutex *mutex;
  /** @brief Synchronization condition variable */
  struct AbstractHttpCond *cond;
  /** @brief Completion flag */
  int is_complete;
  /** @brief Error code */
  c_abstract_http_error_t error_code;
};

/** @brief In-flight multiplexed MsH3 stream slot */
struct msh3_multi_slot {
  /** @brief MsH3 connection handle */
  MSH3_CONNECTION *conn;
  /** @brief MsH3 request stream handle */
  MSH3_REQUEST *mreq;
  /** @brief Per-stream callback state */
  struct msh3_req_ctx rctx;
  /** @brief Parsed host string */
  char *host;
  /** @brief Parsed port string */
  char *port_str;
  /** @brief Parsed path string */
  char *path;
  /** @brief Parsed scheme string */
  char *scheme;
  /** @brief Allocated header array */
  MSH3_HEADER *hdrs;
  /** @brief Allocated Cookie header value buffer */
  char *cookie_buf;
  /** @brief Temporary flattened multipart request copy */
  struct HttpRequest flat_req;
  /** @brief Non-zero when flat_req holds flattened multipart resources */
  int flattened;
  /** @brief Non-zero when stream was successfully opened and sent */
  int active;
};

/**
 * @brief Case-insensitive ASCII string comparison.
 * @param a First NUL-terminated string.
 * @param b Second NUL-terminated string.
 * @return 0 when equal ignoring ASCII case, non-zero otherwise.
 */
static int math_msh3_strcasecmp(const char *a, const char *b) {
  unsigned char ca;
  unsigned char cb;
  while (*a && *b) {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (unsigned char)(ca + ('a' - 'A'));
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (unsigned char)(cb + ('a' - 'A'));
    }
    if (ca != cb) {
      return (int)ca - (int)cb;
    }
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* Initialize the global MsH3 state. */
NO_DISCARD c_abstract_http_error_t http_msh3_global_init(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  if (!g_msh3_mutex) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_msh3_mutex_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = abstract_http_mutex_init(&g_msh3_mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_global_lock_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  rc = abstract_http_mutex_lock(g_msh3_mutex);
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
    c_abstract_http_error_t u_rc = abstract_http_mutex_unlock(g_msh3_mutex);
    if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      return u_rc;
    }
  }
  return rc;
}

/* Clean up global MsH3 state. */
NO_DISCARD c_abstract_http_error_t http_msh3_global_cleanup(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  c_abstract_http_error_t unlock_rc;
  if (!g_msh3_mutex) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_global_lock_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  rc = abstract_http_mutex_lock(g_msh3_mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  if (g_msh3_init_count > 0 && --g_msh3_init_count == 0) {
    MsH3ApiClose(g_msh3_api);
    g_msh3_api = NULL;
#if defined(_WIN32)
    WSACleanup();
#endif
  }
  if (g_msh3_init_count == 0) {
    struct AbstractHttpMutex *m = g_msh3_mutex;
    c_abstract_http_error_t free_rc;
    g_msh3_mutex = NULL;
    unlock_rc = abstract_http_mutex_unlock(m);
    if (unlock_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free_rc = abstract_http_mutex_free(m);
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return free_rc;
      }
      return unlock_rc;
    }
    return abstract_http_mutex_free(m);
  }
  return abstract_http_mutex_unlock(g_msh3_mutex);
}

/**
 * @brief Initialize an `HttpConfig` with optional OOM fault injection.
 * @param cfg Target `HttpConfig` to initialize.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_init_cfg_checked(struct HttpConfig *cfg) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_config_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  return http_config_init(cfg);
}

/* Initialize a new MsH3 transport context. */
NO_DISCARD c_abstract_http_error_t
http_msh3_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  c_abstract_http_error_t rc;
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

  rc = msh3_init_cfg_checked(&c->base_config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(c);
    return rc;
  }

  c->secure = 1;
  *ctx = c;
  LOG_DEBUG("http_msh3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free MsH3 transport context. */
NO_DISCARD c_abstract_http_error_t
http_msh3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_msh3_context_free: Entering");
  if (ctx) {
    c_abstract_http_error_t rch;
    if (ctx->config) {
      MsH3ConfigurationClose(ctx->config);
      ctx->config = NULL;
    }
    rch = http_config_free(&ctx->base_config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return rch;
    }
    free(ctx);
  }
  LOG_DEBUG("http_msh3_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration settings to MsH3 context. */
NO_DISCARD c_abstract_http_error_t http_msh3_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  MSH3_SETTINGS settings;
  MSH3_CREDENTIAL_CONFIG cred;
  c_abstract_http_error_t rc;
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
  settings.IdleTimeoutMs = (uint64_t)config->timeout_ms;
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
    LOG_DEBUG("http_msh3_config_apply: Credential load returned non-zero");
  }

  free(ctx->base_config.user_agent);
  ctx->base_config.user_agent = NULL;
  if (config->user_agent) {
    rc = c_abstract_http_strdup(config->user_agent,
                                &ctx->base_config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      MsH3ConfigurationClose(ctx->config);
      ctx->config = NULL;
      return rc;
    }
  }

  ctx->base_config.timeout_ms = config->timeout_ms;
  ctx->base_config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->base_config.read_timeout_ms = config->read_timeout_ms;
  ctx->base_config.write_timeout_ms = config->write_timeout_ms;
  ctx->base_config.verify_peer = config->verify_peer;
  ctx->base_config.verify_host = config->verify_host;
  ctx->base_config.follow_redirects = config->follow_redirects;
  ctx->base_config.cookie_jar = config->cookie_jar;
  ctx->base_config.http3_fallback = config->http3_fallback;

  LOG_DEBUG("http_msh3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse a Set-Cookie header value and store it in the cookie jar.
 * @param jar Target cookie jar.
 * @param header_val Raw Set-Cookie header value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_store_set_cookie(struct HttpCookieJar *jar, const char *header_val) {
  const char *eq;
  const char *semi;
  size_t nlen;
  size_t vlen;
  char *cname;
  char *cval;
  c_abstract_http_error_t rc;

  eq = strchr(header_val, '=');
  if (!eq || eq == header_val) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  nlen = (size_t)(eq - header_val);
  semi = strchr(eq + 1, ';');
  vlen = semi ? (size_t)(semi - (eq + 1)) : strlen(eq + 1);

  cname = (char *)malloc(nlen + 1);
  if (!cname) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  cval = (char *)malloc(vlen + 1);
  if (!cval) {
    free(cname);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(cname, header_val, nlen);
  cname[nlen] = '\0';
  memcpy(cval, eq + 1, vlen);
  cval[vlen] = '\0';

  rc = http_cookie_jar_set(jar, cname, cval);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(cname);
    free(cval);
    return rc;
  }
  free(cname);
  free(cval);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Enum-returning implementation of the MsH3 per-request event callback.
 * @param req MsH3 request handle.
 * @param rctx Per-request synchronization and response state.
 * @param ev Incoming MsH3 request event.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_request_cb_impl(MSH3_REQUEST *req, struct msh3_req_ctx *rctx,
                     MSH3_REQUEST_EVENT *ev) {
  char *nstr = NULL;
  char *vstr = NULL;
  void *new_body = NULL;
  const char *name = NULL;
  const char *val = NULL;
  size_t namelen = 0;
  size_t vallen = 0;
  size_t dlen = 0;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

  if (!req || !rctx || !ev) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  switch (ev->Type) {
  case MSH3_REQUEST_EVENT_HEADER_RECEIVED:
    if (!ev->HEADER_RECEIVED.Header) {
      break;
    }
    name = ev->HEADER_RECEIVED.Header->Name;
    namelen = ev->HEADER_RECEIVED.Header->NameLength;
    val = ev->HEADER_RECEIVED.Header->Value;
    vallen = ev->HEADER_RECEIVED.Header->ValueLength;

    if (namelen == 7 && strncmp(name, ":status", 7) == 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_msh3_header_alloc_fail == 1) {
        rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
        break;
      }
#endif
      nstr = (char *)malloc(vallen + 1);
      if (nstr) {
        memcpy(nstr, val, vallen);
        nstr[vallen] = '\0';
        rctx->res->status_code = atoi(nstr);
        free(nstr);
      } else {
        rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_msh3_header_alloc_fail == 2) {
        rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
        break;
      }
#endif
      nstr = (char *)malloc(namelen + 1);
      vstr = (char *)malloc(vallen + 1);
      if (nstr && vstr) {
        memcpy(nstr, name, namelen);
        nstr[namelen] = '\0';
        memcpy(vstr, val, vallen);
        vstr[vallen] = '\0';
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_msh3_header_add_fail) {
          free(nstr);
          free(vstr);
          rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
          break;
        }
#endif
        rc = http_headers_add(&rctx->res->headers, nstr, vstr);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          rctx->error_code = rc;
          free(nstr);
          free(vstr);
          return rc;
        }
        if (rctx->cookie_jar && math_msh3_strcasecmp(nstr, "Set-Cookie") == 0) {
          rc = msh3_store_set_cookie(rctx->cookie_jar, vstr);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            rctx->error_code = rc;
            free(nstr);
            free(vstr);
            return rc;
          }
        }
      } else {
        rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      free(nstr);
      free(vstr);
    }
    break;
  case MSH3_REQUEST_EVENT_DATA_RECEIVED:
    dlen = ev->DATA_RECEIVED.Length;
    if (dlen > 0) {
      if (rctx->req && rctx->req->on_chunk) {
        rc = rctx->req->on_chunk(rctx->req->on_chunk_user_data,
                                 ev->DATA_RECEIVED.Data, dlen);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          rctx->error_code = rc;
          return rc;
        }
      } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_msh3_body_realloc_fail) {
          rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
          break;
        }
#endif
        new_body = realloc(rctx->res->body, rctx->res->body_len + dlen + 1);
        if (new_body) {
          rctx->res->body = new_body;
          memcpy((char *)rctx->res->body + rctx->res->body_len,
                 ev->DATA_RECEIVED.Data, dlen);
          rctx->res->body_len += dlen;
          ((char *)rctx->res->body)[rctx->res->body_len] = '\0';
        } else {
          rctx->error_code = C_ABSTRACT_HTTP_ERR_NOMEM;
        }
      }
    }
    break;
  case MSH3_REQUEST_EVENT_SHUTDOWN_COMPLETE:
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_msh3_cb_mutex_lock_fail) {
      rctx->is_complete = 1;
      rctx->error_code = C_ABSTRACT_HTTP_ERR_IO;
      break;
    }
#endif
    rc = abstract_http_mutex_lock(rctx->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      rctx->is_complete = 1;
      rctx->error_code = rc;
      return rc;
    }
    rctx->is_complete = 1;
    if (ev->SHUTDOWN_COMPLETE.ConnectionClosedRemotely ||
        ev->SHUTDOWN_COMPLETE.ConnectionErrorCode != 0) {
      rctx->error_code = C_ABSTRACT_HTTP_ERR_IO;
    }
    {
      c_abstract_http_error_t sig_rc;
      c_abstract_http_error_t unl_rc;
      sig_rc = abstract_http_cond_signal(rctx->cond);
      if (sig_rc != C_ABSTRACT_HTTP_SUCCESS) {
        unl_rc = abstract_http_mutex_unlock(rctx->mutex);
        if (unl_rc != C_ABSTRACT_HTTP_SUCCESS) {
          rctx->error_code = unl_rc;
          return unl_rc;
        }
        rctx->error_code = sig_rc;
        return sig_rc;
      }
      unl_rc = abstract_http_mutex_unlock(rctx->mutex);
      if (unl_rc != C_ABSTRACT_HTTP_SUCCESS) {
        rctx->error_code = unl_rc;
        return unl_rc;
      }
    }
    break;
  default:
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Dispatch an enum-returning MsH3 helper inside an MSH3_STATUS ABI
 * callback.
 * @param expr Expression returning `c_abstract_http_error_t`.
 */
#define MSH3_CB_DISPATCH(expr)                                                 \
  do {                                                                         \
    c_abstract_http_error_t _cb_rc = (expr);                                   \
    if (_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("msh3_request_cb: helper returned %d", (int)_cb_rc);           \
    }                                                                          \
  } while (0)

/**
 * @brief MsH3 per-request event ABI callback.
 * @param req MsH3 request handle.
 * @param ctx Pointer to struct msh3_req_ctx.
 * @param ev Incoming MsH3 request event.
 * @return MSH3_STATUS_SUCCESS.
 */
static MSH3_STATUS MSH3_CALL msh3_request_cb(MSH3_REQUEST *req, void *ctx,
                                             MSH3_REQUEST_EVENT *ev) {
  MSH3_CB_DISPATCH(msh3_request_cb_impl(req, (struct msh3_req_ctx *)ctx, ev));
  return MSH3_STATUS_SUCCESS;
}

/**
 * @brief MsH3 connection event callback.
 * @param conn MsH3 connection handle.
 * @param ctx Callback context.
 * @param ev Incoming connection event.
 * @return MSH3_STATUS_SUCCESS.
 */
static MSH3_STATUS MSH3_CALL msh3_conn_cb(MSH3_CONNECTION *conn, void *ctx,
                                          MSH3_CONNECTION_EVENT *ev) {
  if (!conn || !ctx || !ev) {
    return MSH3_STATUS_SUCCESS;
  }
  return MSH3_STATUS_SUCCESS;
}

/**
 * @brief Allocate a string buffer with optional parse_url step fault injection.
 * @param cap Allocation size in bytes.
 * @param step 1-based allocation step index.
 * @param out Output pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t msh3_url_alloc(size_t cap, int step,
                                                         char **out) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_parse_url_alloc_fail == step) {
    *out = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#else
  (void)step;
#endif
  *out = (char *)malloc(cap);
  if (!*out) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Split an HTTP(S) URL into scheme, host, port, and path strings.
 * @param url Input URL string.
 * @param host Newly allocated host string.
 * @param port Newly allocated port string.
 * @param path Newly allocated path string.
 * @param scheme Newly allocated scheme string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t parse_url(const char *url,
                                                    char **host, char **port,
                                                    char **path,
                                                    char **scheme) {
  const char *p;
  const char *h;
  const char *slash;
  const char *colon;
  const char *port_start = NULL;
  size_t host_len = 0;
  size_t port_len;
  size_t path_len;
  size_t scheme_len;
  c_abstract_http_error_t rc;

  p = strstr(url, "://");
  if (!p) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  scheme_len = (size_t)(p - url);
  rc = msh3_url_alloc(scheme_len + 1, 1, scheme);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  memcpy(*scheme, url, scheme_len);
  (*scheme)[scheme_len] = '\0';

  p += 3;
  h = p;
  slash = strchr(h, '/');
  colon = strchr(h, ':');

  if (colon && (!slash || colon < slash)) {
    host_len = (size_t)(colon - h);
    port_start = colon + 1;
  } else if (slash) {
    host_len = (size_t)(slash - h);
  } else {
    host_len = strlen(h);
  }

  rc = msh3_url_alloc(host_len + 1, 2, host);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*scheme);
    *scheme = NULL;
    return rc;
  }
  memcpy(*host, h, host_len);
  (*host)[host_len] = '\0';

  if (port_start) {
    port_len = slash ? (size_t)(slash - port_start) : strlen(port_start);
    rc = msh3_url_alloc(port_len + 1, 3, port);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      return rc;
    }
    memcpy(*port, port_start, port_len);
    (*port)[port_len] = '\0';
  } else if (strcmp(*scheme, "https") == 0) {
    rc = msh3_url_alloc(4, 4, port);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      return rc;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*port, 4, "443");
#else
    strcpy(*port, "443");
#endif
  } else {
    rc = msh3_url_alloc(3, 5, port);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      return rc;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*port, 3, "80");
#else
    strcpy(*port, "80");
#endif
  }

  if (slash) {
    path_len = strlen(slash);
    rc = msh3_url_alloc(path_len + 1, 6, path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      free(*port);
      *port = NULL;
      return rc;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*path, path_len + 1, slash);
#else
    strcpy(*path, slash);
#endif
  } else {
    rc = msh3_url_alloc(2, 7, path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*scheme);
      *scheme = NULL;
      free(*host);
      *host = NULL;
      free(*port);
      *port = NULL;
      return rc;
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
 * @brief Map an HttpMethod enum to its HTTP/3 :method string.
 * @param method HTTP method enum.
 * @param out_str Output pointer receiving static method token.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_method_str(enum HttpMethod method, const char **out_str) {
  if ((int)method < 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (method) {
  case HTTP_GET:
    *out_str = "GET";
    break;
  case HTTP_POST:
    *out_str = "POST";
    break;
  case HTTP_PUT:
    *out_str = "PUT";
    break;
  case HTTP_DELETE:
    *out_str = "DELETE";
    break;
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  case HTTP_HEAD:
    *out_str = "HEAD";
    break;
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  default:
    *out_str = "GET";
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Flatten multipart parts into a temporary request copy when needed.
 * @param req Input caller request.
 * @param flat_req Scratch request structure to populate if parts are present.
 * @param out_flattened Set to 1 if @p flat_req owns flattened resources.
 * @param out_eff Output pointer to the effective request to transmit.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_prepare_effective_request(const struct HttpRequest *req,
                               struct HttpRequest *flat_req, int *out_flattened,
                               const struct HttpRequest **out_eff) {
  size_t i;
  c_abstract_http_error_t rc;

  *out_flattened = 0;
  *out_eff = req;

  if (req->parts.count == 0 || req->body || req->read_chunk) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!req->parts.parts) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(flat_req, 0, sizeof(*flat_req));
  flat_req->method = req->method;
  flat_req->url = req->url;
  flat_req->parts = req->parts;
  flat_req->on_chunk = req->on_chunk;
  flat_req->on_chunk_user_data = req->on_chunk_user_data;

  rc = http_headers_init(&flat_req->headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  for (i = 0; i < req->headers.count; ++i) {
    rc = http_headers_add(&flat_req->headers, req->headers.headers[i].key,
                          req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t frc = http_headers_free(&flat_req->headers);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        return frc;
      }
      return rc;
    }
  }
  rc = http_request_flatten_parts(flat_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t frc;
    free(flat_req->body);
    flat_req->body = NULL;
    frc = http_headers_free(&flat_req->headers);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return frc;
    }
    return rc;
  }
  *out_flattened = 1;
  *out_eff = flat_req;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Release flattened temporary request resources if active.
 * @param flat_req Scratch request structure.
 * @param flattened Non-zero when @p flat_req owns allocated headers/body.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_cleanup_effective_request(struct HttpRequest *flat_req, int flattened) {
  if (!flattened) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  free(flat_req->body);
  flat_req->body = NULL;
  return http_headers_free(&flat_req->headers);
}

/**
 * @brief Build the HTTP/3 MSH3_HEADER array including custom headers, UA, and
 * cookies.
 * @param ctx Transport context.
 * @param eff_req Effective HTTP request.
 * @param method_str HTTP method string.
 * @param path Request path.
 * @param scheme Request scheme.
 * @param authority Request authority string.
 * @param out_hdrs Output pointer receiving allocated MSH3_HEADER array.
 * @param out_count Output header count.
 * @param out_cookie_buf Output pointer receiving allocated Cookie value buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_build_headers(const struct HttpTransportContext *ctx,
                   const struct HttpRequest *eff_req, const char *method_str,
                   const char *path, const char *scheme, const char *authority,
                   MSH3_HEADER **out_hdrs, size_t *out_count,
                   char **out_cookie_buf) {
  MSH3_HEADER *hdrs;
  size_t cap;
  size_t idx = 0;
  size_t i;
  int has_ua = 0;
  int has_cookie = 0;

  *out_hdrs = NULL;
  *out_count = 0;
  *out_cookie_buf = NULL;

  for (i = 0; i < eff_req->headers.count; ++i) {
    if (math_msh3_strcasecmp(eff_req->headers.headers[i].key, "user-agent") ==
        0) {
      has_ua = 1;
    } else if (math_msh3_strcasecmp(eff_req->headers.headers[i].key,
                                    "cookie") == 0) {
      has_cookie = 1;
    }
  }

  cap = 4 + eff_req->headers.count + 2;
  hdrs = (MSH3_HEADER *)calloc(cap, sizeof(MSH3_HEADER));
  if (!hdrs) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  hdrs[idx].Name = ":method";
  hdrs[idx].NameLength = 7;
  hdrs[idx].Value = method_str;
  hdrs[idx].ValueLength = strlen(method_str);
  idx++;

  hdrs[idx].Name = ":path";
  hdrs[idx].NameLength = 5;
  hdrs[idx].Value = path;
  hdrs[idx].ValueLength = strlen(path);
  idx++;

  hdrs[idx].Name = ":scheme";
  hdrs[idx].NameLength = 7;
  hdrs[idx].Value = scheme;
  hdrs[idx].ValueLength = strlen(scheme);
  idx++;

  hdrs[idx].Name = ":authority";
  hdrs[idx].NameLength = 10;
  hdrs[idx].Value = authority;
  hdrs[idx].ValueLength = strlen(authority);
  idx++;

  for (i = 0; i < eff_req->headers.count; ++i) {
    hdrs[idx].Name = eff_req->headers.headers[i].key;
    hdrs[idx].NameLength = strlen(eff_req->headers.headers[i].key);
    hdrs[idx].Value = eff_req->headers.headers[i].value;
    hdrs[idx].ValueLength = strlen(eff_req->headers.headers[i].value);
    idx++;
  }

  if (!has_ua && ctx->base_config.user_agent) {
    hdrs[idx].Name = "user-agent";
    hdrs[idx].NameLength = 10;
    hdrs[idx].Value = ctx->base_config.user_agent;
    hdrs[idx].ValueLength = strlen(ctx->base_config.user_agent);
    idx++;
  }

  if (!has_cookie && ctx->base_config.cookie_jar &&
      ctx->base_config.cookie_jar->count > 0) {
    size_t total_len = 1;
    char *cbuf;
    char *p;
    for (i = 0; i < ctx->base_config.cookie_jar->count; ++i) {
      total_len += strlen(ctx->base_config.cookie_jar->cookies[i].name) + 1 +
                   strlen(ctx->base_config.cookie_jar->cookies[i].value) + 2;
    }
    cbuf = (char *)malloc(total_len);
    if (!cbuf) {
      free(hdrs);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    p = cbuf;
    for (i = 0; i < ctx->base_config.cookie_jar->count; ++i) {
      size_t nl = strlen(ctx->base_config.cookie_jar->cookies[i].name);
      size_t vl = strlen(ctx->base_config.cookie_jar->cookies[i].value);
      if (i > 0) {
        *p++ = ';';
        *p++ = ' ';
      }
      memcpy(p, ctx->base_config.cookie_jar->cookies[i].name, nl);
      p += nl;
      *p++ = '=';
      memcpy(p, ctx->base_config.cookie_jar->cookies[i].value, vl);
      p += vl;
    }
    *p = '\0';
    *out_cookie_buf = cbuf;

    hdrs[idx].Name = "cookie";
    hdrs[idx].NameLength = 6;
    hdrs[idx].Value = cbuf;
    hdrs[idx].ValueLength = (size_t)(p - cbuf);
    idx++;
  }

  *out_hdrs = hdrs;
  *out_count = idx;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Transmit headers and body/read_chunk stream over an open MsH3 request.
 * @param mreq Open MsH3 request handle.
 * @param eff_req Effective HTTP request.
 * @param hdrs Encoded MSH3_HEADER array.
 * @param hdr_count Number of headers in @p hdrs.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_send_stream_payload(MSH3_REQUEST *mreq, const struct HttpRequest *eff_req,
                         const MSH3_HEADER *hdrs, size_t hdr_count) {
  c_abstract_http_error_t rc;

  if (!eff_req->read_chunk) {
    const void *body_ptr =
        (eff_req->body && eff_req->body_len > 0) ? eff_req->body : NULL;
    uint32_t body_len = (eff_req->body && eff_req->body_len > 0)
                            ? (uint32_t)eff_req->body_len
                            : 0U;
    if (!MsH3RequestSend(mreq, MSH3_REQUEST_SEND_FLAG_FIN, hdrs, hdr_count,
                         body_ptr, body_len, NULL)) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  if (!MsH3RequestSend(mreq, MSH3_REQUEST_SEND_FLAG_NONE, hdrs, hdr_count, NULL,
                       0, NULL)) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  for (;;) {
    unsigned char chunk[4096];
    size_t nread = 0;
    rc = eff_req->read_chunk(eff_req->read_chunk_user_data, chunk,
                             sizeof(chunk), &nread);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    if (nread == 0) {
      if (!MsH3RequestSend(mreq, MSH3_REQUEST_SEND_FLAG_FIN, NULL, 0, NULL, 0,
                           NULL)) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      break;
    }
    if (!MsH3RequestSend(mreq, MSH3_REQUEST_SEND_FLAG_NONE, NULL, 0, chunk,
                         (uint32_t)nread, NULL)) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Resolve host/port into an MSH3_ADDR structure.
 * @param host Target hostname or IP literal.
 * @param port_str Target decimal port string.
 * @param out_addr Output MsH3 address buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_HOSTUNREACH.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_resolve_addr(const char *host, const char *port_str, MSH3_ADDR *out_addr) {
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  int s;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  memset(out_addr, 0, sizeof(*out_addr));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_getaddrinfo_fail) {
    return C_ABSTRACT_HTTP_ERR_HOSTUNREACH;
  }
  if (g_mock_msh3_getaddrinfo_null_result) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (strcmp(host, "127.0.0.1") == 0 || strcmp(host, "localhost") == 0) {
    if (!g_mock_msh3_zero_addrlen) {
      out_addr->raw[0] = 1;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif

  s = getaddrinfo(host, port_str, &hints, &result);
  if (s != 0) {
    return C_ABSTRACT_HTTP_ERR_HOSTUNREACH;
  }
  if (result) {
    size_t copy_len = sizeof(*out_addr) < (size_t)result->ai_addrlen
                          ? sizeof(*out_addr)
                          : (size_t)result->ai_addrlen;
    if (copy_len > 0) {
      memcpy(out_addr, result->ai_addr, copy_len);
    }
    freeaddrinfo(result);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free an allocated response on error and return the appropriate error.
 * @param res In/out response pointer.
 * @param orig_rc Error code to return if response cleanup succeeds.
 * @return Cleanup error code or @p orig_rc.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_cleanup_failed_res(struct HttpResponse **res,
                        c_abstract_http_error_t orig_rc) {
  if (*res) {
    struct HttpResponse *tmp = *res;
    c_abstract_http_error_t free_rc;
    *res = NULL;
    free_rc = http_response_free(tmp);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(tmp);
      return free_rc;
    }
    free(tmp);
  }
  return orig_rc;
}

/**
 * @brief Free remaining buffers and response in an MsH3 slot after sync
 * handles are closed.
 * @param slot Slot to finalize.
 * @param keep_res Non-zero to keep slot->rctx.res ownership on success.
 * @param status Accumulated status enum.
 * @return Final status enum.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_finalize_slot_buffers(struct msh3_multi_slot *slot, int keep_res,
                           c_abstract_http_error_t status) {
  free(slot->hdrs);
  slot->hdrs = NULL;
  free(slot->cookie_buf);
  slot->cookie_buf = NULL;
  free(slot->host);
  slot->host = NULL;
  free(slot->port_str);
  slot->port_str = NULL;
  free(slot->path);
  slot->path = NULL;
  free(slot->scheme);
  slot->scheme = NULL;
  slot->active = 0;

  if (!keep_res || status != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_cleanup_failed_res(&slot->rctx.res, status);
  }
  return status;
}

/**
 * @brief Clean up flattened effective request and finalize slot buffers.
 * @param slot Slot being closed.
 * @param flattened Non-zero if `slot->flat_req` owns flattened allocations.
 * @param keep_res Non-zero to preserve `slot->rctx.res` on success.
 * @param status Accumulated status enum.
 * @return Final status enum.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_close_slot_eff(struct msh3_multi_slot *slot, int flattened, int keep_res,
                    c_abstract_http_error_t status) {
  c_abstract_http_error_t tmp_rc;
  tmp_rc = msh3_cleanup_effective_request(&slot->flat_req, flattened);
  if (tmp_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_finalize_slot_buffers(
        slot, 0, status != C_ABSTRACT_HTTP_SUCCESS ? status : tmp_rc);
  }
  return msh3_finalize_slot_buffers(slot, keep_res, status);
}

/**
 * @brief Free slot mutex (if present) and delegate to `msh3_close_slot_eff`.
 * @param slot Slot being closed.
 * @param flattened Non-zero if `slot->flat_req` owns flattened allocations.
 * @param keep_res Non-zero to preserve `slot->rctx.res` on success.
 * @param status Accumulated status enum.
 * @return Final status enum.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_close_slot_mutex(struct msh3_multi_slot *slot, int flattened, int keep_res,
                      c_abstract_http_error_t status) {
  if (slot->rctx.mutex) {
    struct AbstractHttpMutex *m = slot->rctx.mutex;
    c_abstract_http_error_t tmp_rc;
    slot->rctx.mutex = NULL;
    tmp_rc = abstract_http_mutex_free(m);
    if (tmp_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return msh3_close_slot_eff(slot, flattened, 0,
                                 status != C_ABSTRACT_HTTP_SUCCESS ? status
                                                                   : tmp_rc);
    }
  }
  return msh3_close_slot_eff(slot, flattened, keep_res, status);
}

/**
 * @brief Release a slot's handles, buffers, and synchronization primitives.
 * @param slot Slot to release.
 * @param keep_res Non-zero to keep slot->rctx.res ownership for caller.
 * @param orig_rc Primary error code to preserve if cleanup succeeds.
 * @return Cleanup error code or @p orig_rc.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_close_slot(struct msh3_multi_slot *slot, int keep_res,
                c_abstract_http_error_t orig_rc) {
  int flattened = slot->flattened;

  if (slot->mreq) {
    MsH3RequestClose(slot->mreq);
    slot->mreq = NULL;
  }
  if (slot->conn) {
    MsH3ConnectionClose(slot->conn);
    slot->conn = NULL;
  }
  slot->flattened = 0;
  if (slot->rctx.cond) {
    struct AbstractHttpCond *c = slot->rctx.cond;
    c_abstract_http_error_t tmp_rc;
    slot->rctx.cond = NULL;
    tmp_rc = abstract_http_cond_free(c);
    if (tmp_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return msh3_close_slot_mutex(slot, flattened, 0,
                                   orig_rc != C_ABSTRACT_HTTP_SUCCESS ? orig_rc
                                                                      : tmp_rc);
    }
  }
  return msh3_close_slot_mutex(slot, flattened, keep_res, orig_rc);
}

/**
 * @brief Free temporary `default_cfg` and propagate primary or cleanup error to
 * `msh3_close_slot`.
 * @param default_cfg Temporary config to free.
 * @param slot Slot to close on error.
 * @param apply_rc Result of `http_msh3_config_apply`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_finish_default_cfg(struct HttpConfig *default_cfg,
                        struct msh3_multi_slot *slot,
                        c_abstract_http_error_t apply_rc) {
  c_abstract_http_error_t cfg_free_rc;
  cfg_free_rc = http_config_free(default_cfg);
  if (cfg_free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(
        slot, 0, apply_rc != C_ABSTRACT_HTTP_SUCCESS ? apply_rc : cfg_free_rc);
  }
  if (apply_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, apply_rc);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Open and initiate an MsH3 request stream slot without blocking on
 * completion.
 * @param ctx Transport context.
 * @param req HTTP request to dispatch.
 * @param slot Zero-initialized slot structure to populate.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_open_slot(struct HttpTransportContext *ctx, const struct HttpRequest *req,
               struct msh3_multi_slot *slot) {
  MSH3_ADDR addr;
  char authority[256];
  const char *method_str = "GET";
  const struct HttpRequest *eff_req = NULL;
  size_t hdr_count = 0;
  c_abstract_http_error_t rc;

  if (!req || !req->url) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = parse_url(req->url, &slot->host, &slot->port_str, &slot->path,
                 &slot->scheme);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = msh3_prepare_effective_request(req, &slot->flat_req, &slot->flattened,
                                      &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

  if (!ctx->config) {
    struct HttpConfig default_cfg;
    rc = msh3_init_cfg_checked(&default_cfg);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return msh3_close_slot(slot, 0, rc);
    }
    rc = http_msh3_config_apply(ctx, &default_cfg);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return msh3_finish_default_cfg(&default_cfg, slot, rc);
    }
    rc = msh3_finish_default_cfg(&default_cfg, slot, C_ABSTRACT_HTTP_SUCCESS);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_res_alloc_fail) {
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#endif
  slot->rctx.res = (struct HttpResponse *)calloc(1, sizeof(*slot->rctx.res));
  if (!slot->rctx.res) {
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_res_init_fail) {
    free(slot->rctx.res);
    slot->rctx.res = NULL;
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#endif
  rc = http_response_init(slot->rctx.res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(slot->rctx.res);
    slot->rctx.res = NULL;
    return msh3_close_slot(slot, 0, rc);
  }

  slot->rctx.req = eff_req;
  slot->rctx.cookie_jar = ctx->base_config.cookie_jar;
  slot->rctx.is_complete = 0;
  slot->rctx.error_code = C_ABSTRACT_HTTP_SUCCESS;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_mutex_init_fail) {
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#endif
  rc = abstract_http_mutex_init(&slot->rctx.mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_cond_init_fail) {
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#endif
  rc = abstract_http_cond_init(&slot->rctx.cond);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

  rc = msh3_resolve_addr(slot->host, slot->port_str, &addr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(authority, sizeof(authority), "%s:%s", slot->host, slot->port_str);
#else
  sprintf(authority, "%s:%s", slot->host, slot->port_str);
#endif

  rc = msh3_method_str(eff_req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

  rc =
      msh3_build_headers(ctx, eff_req, method_str, slot->path, slot->scheme,
                         authority, &slot->hdrs, &hdr_count, &slot->cookie_buf);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

  slot->conn = MsH3ConnectionOpen(g_msh3_api, msh3_conn_cb, &slot->rctx);
  if (!slot->conn) {
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  MsH3ConnectionStart(slot->conn, ctx->config, slot->host, &addr);

  slot->mreq = MsH3RequestOpen(slot->conn, msh3_request_cb, &slot->rctx,
                               MSH3_REQUEST_FLAG_NONE);
  if (!slot->mreq) {
    return msh3_close_slot(slot, 0, C_ABSTRACT_HTTP_ERR_NOMEM);
  }

  rc = msh3_send_stream_payload(slot->mreq, eff_req, slot->hdrs, hdr_count);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }

  slot->active = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Wait for an in-flight MsH3 request stream slot to complete.
 * @param slot Active stream slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_await_slot(struct msh3_multi_slot *slot) {
  c_abstract_http_error_t rc;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_msh3_send_lock_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (g_mock_msh3_cond_wait_loop) {
    slot->rctx.is_complete = 0;
  }
#endif
  rc = abstract_http_mutex_lock(slot->rctx.mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  while (!slot->rctx.is_complete) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_msh3_cond_wait_fail) {
      c_abstract_http_error_t unl_rc =
          abstract_http_mutex_unlock(slot->rctx.mutex);
      if (unl_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return unl_rc;
      }
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (g_mock_msh3_cond_wait_loop == 1) {
      slot->rctx.is_complete = 1;
      break;
    }
#endif
    rc = abstract_http_cond_wait(slot->rctx.cond, slot->rctx.mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t unl_rc =
          abstract_http_mutex_unlock(slot->rctx.mutex);
      if (unl_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return unl_rc;
      }
      return rc;
    }
  }
  rc = abstract_http_mutex_unlock(slot->rctx.mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return slot->rctx.error_code;
}

/**
 * @brief Await and close an MsH3 request slot, transferring response on
 * success.
 * @param slot Active stream slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_finish_slot(struct msh3_multi_slot *slot) {
  c_abstract_http_error_t rc;
  rc = msh3_await_slot(slot);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return msh3_close_slot(slot, 0, rc);
  }
  return msh3_close_slot(slot, 1, C_ABSTRACT_HTTP_SUCCESS);
}

/* Synchronously send HTTP request via MsH3. */
NO_DISCARD c_abstract_http_error_t
http_msh3_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
               struct HttpResponse **res) {
  struct msh3_multi_slot slot;
  c_abstract_http_error_t rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_msh3_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = NULL;
  memset(&slot, 0, sizeof(slot));

  rc = msh3_open_slot(ctx, req, &slot);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = msh3_finish_slot(&slot);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  *res = slot.rctx.res;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Drain and close opened MsH3 slots starting from `start_idx`.
 * @param slots Array of active slots.
 * @param start_idx Starting index in `slots`.
 * @param opened Number of opened slots.
 * @param futures Array of futures receiving slot results.
 * @return C_ABSTRACT_HTTP_SUCCESS or failure enum.
 */
NO_DISCARD static c_abstract_http_error_t
msh3_drain_slots_from(struct msh3_multi_slot *slots, size_t start_idx,
                      size_t opened, struct HttpFuture **futures) {
  size_t i;
  for (i = start_idx; i < opened; ++i) {
    c_abstract_http_error_t wait_rc;
    wait_rc = msh3_finish_slot(&slots[i]);
    if (wait_rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t rest_rc;
      futures[i]->response = NULL;
      futures[i]->error_code = wait_rc;
      futures[i]->is_ready = 1;
      rest_rc = msh3_drain_slots_from(slots, i + 1, opened, futures);
      if (rest_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rest_rc;
      }
      return wait_rc;
    }
    futures[i]->response = slots[i].rctx.res;
    futures[i]->error_code = C_ABSTRACT_HTTP_SUCCESS;
    futures[i]->is_ready = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Asynchronously multiplex multi-requests concurrently via MsH3RequestOpen. */
NO_DISCARD c_abstract_http_error_t http_msh3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct msh3_multi_slot *slots = NULL;
  size_t i;
  size_t opened = 0;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  c_abstract_http_error_t drain_rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_msh3_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  slots = (struct msh3_multi_slot *)calloc(multi->count, sizeof(*slots));
  if (!slots) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  /* Phase 1: Open and transmit all MsH3 request streams concurrently */
  for (i = 0; i < multi->count; ++i) {
    rc = msh3_open_slot(ctx, multi->requests[i], &slots[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      futures[i]->response = NULL;
      futures[i]->error_code = rc;
      futures[i]->is_ready = 1;
      goto await_opened;
    }
    opened++;
  }

await_opened:;
  /* Phase 2: Await completion and collect responses for all active streams */
  drain_rc = msh3_drain_slots_from(slots, 0, opened, futures);
  if (drain_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(slots);
    return rc != C_ABSTRACT_HTTP_SUCCESS ? rc : drain_rc;
  }

  free(slots);

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (loop) {
    return http_loop_wakeup(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
