
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fetch.h>

#include <c_abstract_http/http_fetch.h>
#include "c_abstract_http/log.h"
#include "functions/parse/str.h"
/* clang-format on */

struct HttpTransportContext {
  struct HttpConfig config;
};

int http_fetch_global_init(void) { return 0; }
void http_fetch_global_cleanup(void) {}

int http_fetch_context_init(struct HttpTransportContext **const ctx) {
  int rc;
  LOG_DEBUG("http_fetch_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_fetch_context_init: Error EINVAL");
    return EINVAL;
  }
  *ctx = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_fetch_context_init: Error ENOMEM");
    return ENOMEM;
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
  return 0;
}

void http_fetch_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_fetch_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_fetch_context_free: Exiting");
}

int http_fetch_config_apply(struct HttpTransportContext *ctx,
                            const struct HttpConfig *config) {
  LOG_DEBUG("http_fetch_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_fetch_config_apply: Error EINVAL");
    return EINVAL;
  }
  ctx->config = *config;
  LOG_DEBUG("http_fetch_config_apply: Success");
  return 0;
}

static int map_fetch_error(int err) {
  switch (err) {
  case FETCH_OK:
    return 0;
  case FETCH_TIMEOUT:
    return ETIMEDOUT;
  case FETCH_DOWN:
  case FETCH_NETWORK:
    return ECONNREFUSED;
  case FETCH_RESOLV:
    return EHOSTUNREACH;
  case FETCH_MEMORY:
    return ENOMEM;
  default:
    return EIO;
  }
}

int http_fetch_send(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req,
                    struct HttpResponse **const res) {
  struct url *u;
  FILE *f;
  const char *method_str = "GET";
  int rc = 0;
  char buf[4096];
  size_t bytes_read;
  struct HttpResponse *new_res = NULL;
  char *tmp = NULL;

  LOG_DEBUG("http_fetch_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_fetch_send: Error EINVAL");
    return EINVAL;
  }

  u = fetchParseURL(req->url);
  if (!u) {
    LOG_DEBUG("http_fetch_send: Error fetchParseURL failed");
    return EINVAL;
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
    return ENOMEM;
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
        rc = ENOMEM;
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
  return 0;
}
int http_fetch_send_multi(struct HttpTransportContext *ctx,
                          struct ModalityEventLoop *loop,
                          const struct HttpMultiRequest *multi,
                          struct HttpFuture **futures) {
  size_t i;
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_fetch_send_multi: Error EINVAL");
    return EINVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    int rc = http_fetch_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }
  return 0;
}
