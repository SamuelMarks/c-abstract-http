/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_fetch.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "str.h"

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_FETCH)
#include <fetch.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_fetch_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_parse_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_req_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_err_code(void);
extern int *abstract_http_mock_get_g_mock_fetch_empty_body(void);
extern int *abstract_http_mock_get_g_mock_fetch_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_response_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_upload_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_upload_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail(void);

#define g_mock_fetch_global_init_fail                                          \
  (*abstract_http_mock_get_g_mock_fetch_global_init_fail())
#define g_mock_fetch_context_init_fail                                         \
  (*abstract_http_mock_get_g_mock_fetch_context_init_fail())
#define g_mock_fetch_config_init_fail                                          \
  (*abstract_http_mock_get_g_mock_fetch_config_init_fail())
#define g_mock_fetch_parse_fail                                                \
  (*abstract_http_mock_get_g_mock_fetch_parse_fail())
#define g_mock_fetch_req_fail (*abstract_http_mock_get_g_mock_fetch_req_fail())
#define g_mock_fetch_err_code (*abstract_http_mock_get_g_mock_fetch_err_code())
#define g_mock_fetch_empty_body                                                \
  (*abstract_http_mock_get_g_mock_fetch_empty_body())
#define g_mock_fetch_response_init_fail                                        \
  (*abstract_http_mock_get_g_mock_fetch_response_init_fail())
#define g_mock_fetch_response_alloc_fail                                       \
  (*abstract_http_mock_get_g_mock_fetch_response_alloc_fail())
#define g_mock_fetch_upload_alloc_fail                                         \
  (*abstract_http_mock_get_g_mock_fetch_upload_alloc_fail())
#define g_mock_fetch_upload_realloc_fail                                       \
  (*abstract_http_mock_get_g_mock_fetch_upload_realloc_fail())
#define g_mock_fetch_body_realloc_fail                                         \
  (*abstract_http_mock_get_g_mock_fetch_body_realloc_fail())
#define g_mock_fetch_loop_wakeup_fail                                          \
  (*abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail())
#endif

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_FETCH)
/**
 * @brief Libfetch URL structure stub.
 */
struct url {
  /** @brief Scheme */
  char scheme[32];
  /** @brief User */
  char user[64];
  /** @brief Password */
  char pwd[64];
  /** @brief Host */
  char host[256];
  /** @brief Port */
  int port;
  /** @brief Document */
  char doc[1024];
  /** @brief Offset */
  size_t offset;
  /** @brief Length */
  size_t length;
};

#define FETCH_OK 0
#define FETCH_TIMEOUT 1
#define FETCH_DOWN 2
#define FETCH_NETWORK 3
#define FETCH_RESOLV 4
#define FETCH_MEMORY 5

static int fetchLastErrCode = 0;

/**
 * @brief Parse URL string into libfetch url structure stub.
 *
 * @param[in] url_str URL string.
 * @return Pointer to allocated url structure, or NULL on failure.
 */
static struct url *fetchParseURL(const char *url_str) {
  struct url *u;
  if (strcmp(url_str, "invalid://url") == 0) {
    return NULL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_parse_fail) {
    return NULL;
  }
#endif
  u = (struct url *)calloc(1, sizeof(struct url));
  return u;
}

/**
 * @brief Free libfetch url structure stub.
 *
 * @param[in] u URL structure to free.
 */
static void fetchFreeURL(struct url *u) { free(u); }

/**
 * @brief Send HTTP request stub using libfetch API.
 *
 * @param[in] u URL structure.
 * @param[in] method HTTP method string.
 * @param[in] flags Flags string.
 * @param[in] content_type Content-Type header string.
 * @param[in] body Request body.
 * @return FILE stream pointer, or NULL on failure.
 */
static FILE *fetchReqHTTP(struct url *u, const char *method, const char *flags,
                          const char *content_type, const char *body) {
  FILE *f;
  (void)u;
  (void)method;
  (void)flags;
  (void)content_type;
  (void)body;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_req_fail) {
    fetchLastErrCode =
        g_mock_fetch_err_code ? g_mock_fetch_err_code : FETCH_DOWN;
    return NULL;
  }
  if (g_mock_fetch_empty_body) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    if (tmpfile_s(&f) != 0) {
      f = NULL;
    }
#else
    f = tmpfile();
#endif
    return f;
  }
#endif

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  if (tmpfile_s(&f) != 0) {
    f = NULL;
  }
#else
  f = tmpfile();
#endif
  fputs("HTTP/1.1 200 OK\r\n\r\nMock response body from libfetch", f);
  rewind(f);
  return f;
}
#endif

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Transport configuration */
  struct HttpConfig config;
};

static int g_fetch_init_count = 0;

/**
 * @brief Map libfetch error code to abstract HTTP error enum.
 *
 * @param[in] err Libfetch error code.
 * @param[out] out_err Pointer to receive abstract HTTP error enum.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error
map_fetch_error(int err, enum c_abstract_http_error *out_err) {
  if (!out_err) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (err) {
  case FETCH_OK:
    *out_err = C_ABSTRACT_HTTP_SUCCESS;
    break;
  case FETCH_TIMEOUT:
    *out_err = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    break;
  case FETCH_MEMORY:
    *out_err = C_ABSTRACT_HTTP_ERR_NOMEM;
    break;
  case FETCH_DOWN:
  case FETCH_NETWORK:
  case FETCH_RESOLV:
  default:
    *out_err = C_ABSTRACT_HTTP_ERR_IO;
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HTTP method enum to libfetch method string.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Pointer to receive method string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error get_fetch_method_str(enum HttpMethod method,
                                                       const char **out_str) {
  if (!out_str) {
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
  case HTTP_HEAD:
    *out_str = "HEAD";
    break;
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out_str = "TRACE";
    break;
  case HTTP_CONNECT:
    *out_str = "CONNECT";
    break;
  case HTTP_QUERY:
    *out_str = "QUERY";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error
c_abstract_http_test_fetch_map_error(int err,
                                     enum c_abstract_http_error *out_err);

enum c_abstract_http_error
c_abstract_http_test_fetch_method_str(enum HttpMethod method,
                                      const char **out_str);

/**
 * @brief Test hook to invoke map_fetch_error directly.
 *
 * @param[in] err Libfetch error code.
 * @param[out] out_err Mapped error enum pointer.
 * @return Error enum.
 */
enum c_abstract_http_error
c_abstract_http_test_fetch_map_error(int err,
                                     enum c_abstract_http_error *out_err) {
  return map_fetch_error(err, out_err);
}

/**
 * @brief Test hook to invoke get_fetch_method_str directly.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Method string pointer.
 * @return Error enum.
 */
enum c_abstract_http_error
c_abstract_http_test_fetch_method_str(enum HttpMethod method,
                                      const char **out_str) {
  return get_fetch_method_str(method, out_str);
}
#endif

/**
 * @brief Initialize global libfetch resources.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_fetch_global_init(void) {
  LOG_DEBUG("http_fetch_global_init: Entering");
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  if (g_fetch_init_count++ == 0) {
    /* Initialize global libfetch state */
  }
  LOG_DEBUG("http_fetch_global_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global libfetch resources.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_fetch_global_cleanup(void) {
  LOG_DEBUG("http_fetch_global_cleanup: Entering");
  if (g_fetch_init_count > 0 && --g_fetch_init_count == 0) {
    /* Clean up global libfetch state */
  }
  LOG_DEBUG("http_fetch_global_cleanup: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a new libfetch transport context.
 *
 * @param[out] ctx Double pointer to receive newly allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_fetch_context_init(struct HttpTransportContext **const ctx) {
  enum c_abstract_http_error rc;
  struct HttpTransportContext *c;

  LOG_DEBUG("http_fetch_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_fetch_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  c = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!c) {
    LOG_DEBUG("http_fetch_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&c->config);
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_context_init: Error http_config_init failed with %d",
              (int)rc);
    free(c);
    *ctx = NULL;
    return rc;
  }

  *ctx = c;
  LOG_DEBUG("http_fetch_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free a libfetch transport context.
 *
 * @param[in] ctx The context to free. Safe to pass NULL.
 */
void http_fetch_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_fetch_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_fetch_context_free: Exiting");
}

/**
 * @brief Apply configuration to libfetch transport context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config The configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_fetch_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_fetch_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_fetch_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;

  if (config->user_agent) {
    if (ctx->config.user_agent) {
      free(ctx->config.user_agent);
      ctx->config.user_agent = NULL;
    }
    rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else {
    if (ctx->config.user_agent) {
      free(ctx->config.user_agent);
      ctx->config.user_agent = NULL;
    }
  }

  if (config->proxy_url) {
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
    rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else {
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
  }

  LOG_DEBUG("http_fetch_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Perform a single HTTP request using libfetch.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the allocated response object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_fetch_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **const res) {
  struct url *u;
  FILE *f;
  const char *method_str;
  const char *body_ptr;
  char *upload_body;
  char buf[4096];
  size_t bytes_read;
  struct HttpResponse *new_res;
  char *tmp;
  enum c_abstract_http_error rc;

  method_str = "GET";
  body_ptr = NULL;
  upload_body = NULL;
  new_res = NULL;
  tmp = NULL;
  rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_fetch_send: Entering");
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_fetch_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = NULL;

  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG("http_fetch_send: Error EINVAL (multipart not flattened)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = get_fetch_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error invalid method");
    return rc;
  }

  u = fetchParseURL(req->url);
  if (!u) {
    LOG_DEBUG("http_fetch_send: Error fetchParseURL failed");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  /* Environment variables for proxy/user-agent */
  if (ctx->config.user_agent) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_USER_AGENT", ctx->config.user_agent);
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    setenv("HTTP_USER_AGENT", ctx->config.user_agent, 1);
#else
    {
      char env_buf[512];
      sprintf(env_buf, "HTTP_USER_AGENT=%s", ctx->config.user_agent);
      putenv(env_buf);
    }
#endif
  }

  if (ctx->config.proxy_url) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_PROXY", ctx->config.proxy_url);
#elif defined(_POSIX_C_SOURCE) || defined(__unix__) || defined(__APPLE__)
    setenv("HTTP_PROXY", ctx->config.proxy_url, 1);
#else
    {
      char env_buf[512];
      sprintf(env_buf, "HTTP_PROXY=%s", ctx->config.proxy_url);
      putenv(env_buf);
    }
#endif
  }

  if (req->read_chunk && !req->body) {
    size_t cap;
    size_t upload_len;
    size_t out_read;

    if (req->expected_body_len > 0) {
      cap = req->expected_body_len;
    } else {
      cap = 4096;
    }
    upload_len = 0;
    out_read = 0;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_fetch_upload_alloc_fail) {
      upload_body = NULL;
    } else
#endif
    {
      upload_body = (char *)malloc(cap + 1);
    }
    if (!upload_body) {
      LOG_DEBUG("http_fetch_send: Error ENOMEM (upload_body)");
      fetchFreeURL(u);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    while (1) {
      char chunk_buf[1024];
      int read_rc;

      read_rc = req->read_chunk(req->read_chunk_user_data, chunk_buf,
                                sizeof(chunk_buf), &out_read);
      if (read_rc != 0) {
        LOG_DEBUG("http_fetch_send: read_chunk aborted");
        free(upload_body);
        fetchFreeURL(u);
        return (enum c_abstract_http_error)read_rc;
      }
      if (out_read == 0) {
        break;
      }

      if (upload_len + out_read > cap) {
        char *new_buf;
        cap = (upload_len + out_read) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_fetch_upload_realloc_fail) {
          new_buf = NULL;
        } else
#endif
        {
          new_buf = (char *)realloc(upload_body, cap + 1);
        }
        if (!new_buf) {
          LOG_DEBUG("http_fetch_send: Error ENOMEM reallocating upload_body");
          free(upload_body);
          fetchFreeURL(u);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        upload_body = new_buf;
      }
      memcpy(upload_body + upload_len, chunk_buf, out_read);
      upload_len += out_read;
    }
    upload_body[upload_len] = '\0';
    body_ptr = upload_body;
  } else if (req->body) {
    body_ptr = (const char *)req->body;
  }

  /* Flags e.g. 'd' for direct, etc. For now "" */
  f = fetchReqHTTP(u, method_str, "", NULL, body_ptr);

  fetchFreeURL(u);

  if (!f) {
    map_fetch_error(fetchLastErrCode, &rc);
    LOG_DEBUG("http_fetch_send: Error fetchReqHTTP failed, rc=%d", (int)rc);
    if (upload_body) {
      free(upload_body);
    }
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_response_alloc_fail) {
    new_res = NULL;
  } else
#endif
  {
    new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!new_res) {
    LOG_DEBUG("http_fetch_send: Error ENOMEM allocating new_res");
    fclose(f);
    if (upload_body) {
      free(upload_body);
    }
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_fetch_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(new_res);
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error http_response_init failed with %d",
              (int)rc);
    free(new_res);
    fclose(f);
    if (upload_body) {
      free(upload_body);
    }
    return rc;
  }

  new_res->status_code = 200;

  while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (req->on_chunk) {
      rc = (enum c_abstract_http_error)req->on_chunk(req->on_chunk_user_data,
                                                     buf, bytes_read);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_fetch_send: Error on_chunk failed with %d", (int)rc);
        break;
      }
    } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_fetch_body_realloc_fail) {
        tmp = NULL;
      } else
#endif
      {
        tmp = (char *)realloc((void *)new_res->body,
                              new_res->body_len + bytes_read + 1);
      }
      if (!tmp) {
        LOG_DEBUG("http_fetch_send: Error ENOMEM reallocating body");
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        break;
      }
      memcpy(tmp + new_res->body_len, buf, bytes_read);
      new_res->body = tmp;
      new_res->body_len += bytes_read;
      ((char *)new_res->body)[new_res->body_len] = '\0';
    }
  }

  fclose(f);

  if (upload_body) {
    free(upload_body);
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_fetch_send: Error returning %d", (int)rc);
    http_response_free(new_res);
    free(new_res);
    *res = NULL;
    return rc;
  }

  *res = new_res;
  LOG_DEBUG("http_fetch_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Dispatch multiple HTTP requests using libfetch.
 *
 * @param[in] ctx Pointer to an initialized HttpTransportContext.
 * @param[in,out] loop Event loop to drive the execution.
 * @param[in] multi Multi-request specification.
 * @param[out] futures Array of returned HttpFuture handles.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_fetch_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_fetch_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res;
    res = NULL;
    rc = http_fetch_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  if (loop) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_fetch_loop_wakeup_fail) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
    } else
#endif
    {
      rc = http_loop_wakeup(loop);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}
