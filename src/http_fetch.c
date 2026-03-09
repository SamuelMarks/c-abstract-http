/**
 * @file http_fetch.c
 * @brief Implementation of the Libfetch backend.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fetch.h>

#include <c_abstract_http/http_fetch.h>
#include <c_abstract_http/str.h>

/* clang-format on */

struct HttpTransportContext {
  struct HttpConfig *config;
};

int http_fetch_global_init(void) { return 0; }
void http_fetch_global_cleanup(void) {}

int http_fetch_context_init(struct HttpTransportContext **const ctx) {
  if (!ctx)
    return EINVAL;
  *ctx = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  return *ctx ? 0 : ENOMEM;
}

void http_fetch_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    if (ctx->config) {
      free(ctx->config);
    }
    free(ctx);
  }
}

int http_fetch_config_apply(struct HttpTransportContext *ctx,
                            const struct HttpConfig *config) {
  if (!ctx || !config)
    return EINVAL;
  /* Shallow copy config for now */
  if (!ctx->config)
    ctx->config = (struct HttpConfig *)malloc(sizeof(struct HttpConfig));
  if (ctx->config)
    memcpy(ctx->config, config, sizeof(struct HttpConfig));
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

  if (!ctx || !req || !res)
    return EINVAL;

  u = fetchParseURL(req->url);
  if (!u)
    return EINVAL;

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
  if (ctx->config && ctx->config->user_agent) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    _putenv_s("HTTP_USER_AGENT", ctx->config->user_agent);
#else
    setenv("HTTP_USER_AGENT", ctx->config->user_agent, 1);
#endif
  }

  /* Flags e.g. 'd' for direct, etc. For now "" */
  f = fetchReqHTTP(u, method_str, "", NULL,
                   req->body ? (const char *)req->body : NULL);

  fetchFreeURL(u);

  if (!f) {
    return map_fetch_error(fetchLastErrCode);
  }

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    fclose(f);
    return ENOMEM;
  }
  if (http_response_init(new_res) != 0) {
    free(new_res);
    fclose(f);
    return ENOMEM;
  }

  /* libfetch doesn't easily give us the HTTP status code on success, but it
   * usually means 200 */
  new_res->status_code = 200;

  while ((bytes_read = fread(buf, 1, sizeof(buf), f)) > 0) {
    if (req->on_chunk) {
      rc = req->on_chunk(req->on_chunk_user_data, buf, bytes_read);
      if (rc != 0)
        break;
    } else {
      tmp = (char *)realloc((void *)new_res->body,
                            new_res->body_len + bytes_read + 1);
      if (!tmp) {
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
    http_response_free(new_res);
    free(new_res);
    return rc;
  }

  *res = new_res;
  return 0;
}
int http_fetch_send_multi(struct HttpTransportContext *ctx,
                          struct ModalityEventLoop *loop,
                          const struct HttpMultiRequest *multi,
                          struct HttpFuture **futures) {
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOTSUP;
}
