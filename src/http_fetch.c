
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fetch.h>

#include <c_abstract_http/http_fetch.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  struct HttpConfig config;
};

enum c_abstract_http_error http_fetch_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error http_fetch_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_fetch_context_init(struct HttpTransportContext **const ctx) {
  LOG_DEBUG("http_fetch_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_fetch_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *ctx = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_fetch_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != 0) {
    LOG_DEBUG("http_fetch_context_init: Error http_config_init failed with %d",
              rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_fetch_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_fetch_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_fetch_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_fetch_context_free: Exiting");
}

enum c_abstract_http_error
http_fetch_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  LOG_DEBUG("http_fetch_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_fetch_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config = *config;
  LOG_DEBUG("http_fetch_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int map_fetch_error(int err) {
  switch (err) {
  case FETCH_OK:
    return C_ABSTRACT_HTTP_SUCCESS;
  case FETCH_TIMEOUT:
    return C_ABSTRACT_HTTP_ERR_TIMEOUT;
  case FETCH_DOWN:
  case FETCH_NETWORK:
    return ECONNREFUSED;
  case FETCH_RESOLV:
    return EHOSTUNREACH;
  case FETCH_MEMORY:
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  default:
    return C_ABSTRACT_HTTP_ERR_IO;
  }
}

enum c_abstract_http_error http_fetch_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **const res) {
  struct url *u;
  FILE *f;
  const char *method_str = "GET";
  char buf[4096];
  size_t bytes_read;
  struct HttpResponse *new_res = NULL;
  char *tmp = NULL;

  LOG_DEBUG("http_fetch_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_fetch_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  u = fetchParseURL(req->url);
  if (!u) {
    LOG_DEBUG("http_fetch_send: Error fetchParseURL failed");
    return C_ABSTRACT_HTTP_ERR_INVAL;
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
  case HTTP_HEAD:
    method_str = "HEAD";
    break;
  case HTTP_PATCH:
    method_str = "PATCH";
    break;
  case HTTP_QUERY:
    method_str = "QUERY";
    break;
  }

  /* Environment variables for proxy/user-agent */
  if (ctx->config.user_agent) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_USER_AGENT", ctx->config.user_agent);
#else
    setenv("HTTP_USER_AGENT", ctx->config.user_agent, 1);
#endif
  }

  /* Flags e.g. 'd' for direct, etc. For now "" */
  f = fetchReqHTTP(u, method_str, "", NULL,
                   req->body ? (const char *)req->body : NULL);

  fetchFreeURL(u);

  if (!f) {
    rc = map_fetch_error(fetchLastErrCode);
    LOG_DEBUG("http_fetch_send: Error fetchReqHTTP failed, rc=%d", rc);
    return rc;
  }

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    LOG_DEBUG("http_fetch_send: Error ENOMEM allocating new_res");
    fclose(f);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_response_init(new_res);
  if (rc != 0) {
    LOG_DEBUG("http_fetch_send: Error http_response_init failed with %d", rc);
    free(new_res);
    fclose(f);
    return rc;
  }

  /* libfetch doesn't easily give us the HTTP status code on success, but it
   * usually means 200 */
  new_res->status_code = 200;

  while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (req->on_chunk) {
      rc = req->on_chunk(req->on_chunk_user_data, buf, bytes_read);
      if (rc != 0) {
        LOG_DEBUG("http_fetch_send: Error on_chunk failed with %d", rc);
        break;
      }
    } else {
      tmp = (char *)realloc((void *)new_res->body,
                            new_res->body_len + bytes_read + 1);
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

  if (rc != 0) {
    LOG_DEBUG("http_fetch_send: Error returning %d", rc);
    http_response_free(new_res);
    free(new_res);
    *res = NULL;
    return rc;
  }

  *res = new_res;
  LOG_DEBUG("http_fetch_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error http_fetch_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_fetch_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    int rc = http_fetch_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
