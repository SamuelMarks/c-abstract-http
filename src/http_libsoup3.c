
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defines for C89 string functions if missing */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define strdup _strdup
#endif
#else
#endif

#include <libsoup/soup.h>

#include <c_abstract_http/http_libsoup3.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief session (variable) of struct HttpTransportContext */
  SoupSession *session;
  struct HttpCookieJar *cookie_jar;
  struct HttpConfig config;
};

static int g_libsoup3_init_count = 0;

enum c_abstract_http_error http_libsoup3_global_init(void) {
  if (g_libsoup3_init_count == 0) {
    /* glib/gio handles its own initialization mostly, but if we needed anything
     * it goes here */
  }
  g_libsoup3_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_libsoup3_global_cleanup(void) {
  if (g_libsoup3_init_count > 0) {
    g_libsoup3_init_count--;
    if (g_libsoup3_init_count == 0) {
      /* Cleanup if needed */
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
}

enum c_abstract_http_error
http_libsoup3_context_init(struct HttpTransportContext **const ctx) {
  LOG_DEBUG("http_libsoup3_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libsoup3_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_libsoup3_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != 0) {
    LOG_DEBUG(
        "http_libsoup3_context_init: Error http_config_init failed with %d",
        rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->session = soup_session_new();
  if (!(*ctx)->session) {
    LOG_DEBUG("http_libsoup3_context_init: Error soup_session_new failed");
    http_config_free(&(*ctx)->config);
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  (*ctx)->cookie_jar = NULL;

  LOG_DEBUG("http_libsoup3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_libsoup3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libsoup3_context_free: Entering");
  if (ctx) {
    if (ctx->session) {
      g_object_unref(ctx->session);
    }
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_libsoup3_context_free: Exiting");
}

enum c_abstract_http_error
http_libsoup3_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  LOG_DEBUG("http_libsoup3_config_apply: Entering");
  if (!ctx || !ctx->session || !config) {
    LOG_DEBUG("http_libsoup3_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (config->timeout_ms > 0) {
    g_object_set(ctx->session, "timeout", (guint)(config->timeout_ms / 1000),
                 NULL);
  }

  if (config->user_agent) {
    g_object_set(ctx->session, "user-agent", config->user_agent, NULL);
  }

  if (config->proxy_url) {
    GUri *proxy_uri = g_uri_parse(config->proxy_url, G_URI_FLAGS_NONE, NULL);
    if (proxy_uri) {
      g_object_set(ctx->session, "proxy-uri", proxy_uri, NULL);
      g_uri_unref(proxy_uri);
    } else {
      LOG_DEBUG(
          "http_libsoup3_config_apply: Error g_uri_parse failed for proxy");
    }
  } else {
    g_object_set(ctx->session, "proxy-uri", NULL, NULL);
  }

  /* verify_peer / verify_host usually handled via tls-database or signals,
   * skipping complex TLS config for now */

  if (config->cookie_jar) {
    ctx->cookie_jar = config->cookie_jar;
  } else {
    ctx->cookie_jar = NULL;
  }

  ctx->config = *config;
  LOG_DEBUG("http_libsoup3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int math_get_method_string(enum HttpMethod method, const char **out) {
  if (!out)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  switch (method) {
  case HTTP_GET:
    *out = "GET";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_POST:
    *out = "POST";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_PUT:
    *out = "PUT";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_DELETE:
    *out = "DELETE";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_HEAD:
    *out = "HEAD";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_PATCH:
    *out = "PATCH";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_QUERY:
    *out = "QUERY";
    return C_ABSTRACT_HTTP_SUCCESS;
  default:
    *out = "GET";
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

enum c_abstract_http_error http_libsoup3_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **const res) {
  SoupMessage *msg = NULL;
  SoupMessageHeaders *req_headers = NULL;
  GBytes *body_bytes = NULL;
  GBytes *resp_bytes = NULL;
  GError *error = NULL;
  struct HttpResponse *new_res = NULL;
  size_t i;
  void *payload = NULL;
  size_t payload_len = 0;
  const char *method_str;
  gsize resp_len = 0;
  gconstpointer resp_data = NULL;
  char *response_body_copy = NULL;

  LOG_DEBUG("http_libsoup3_send: Entering");
  if (!ctx || !ctx->session || !req || !res) {
    LOG_DEBUG("http_libsoup3_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  payload = req->body;
  payload_len = req->body_len;

  if (req->parts.count > 0 && !payload) {
    LOG_DEBUG(
        "http_libsoup3_send: Error EINVAL (multipart parts with no payload)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = math_get_method_string(req->method, &method_str);
  if (rc != 0) {
    LOG_DEBUG("http_libsoup3_send: Error math_get_method_string failed %d", rc);
    return rc;
  }
  msg = soup_message_new(method_str, req->url);
  if (!msg) {
    LOG_DEBUG("http_libsoup3_send: Error soup_message_new failed (bad URL?)");
    return C_ABSTRACT_HTTP_ERR_INVAL; /* Bad URL likely */
  }

  req_headers = soup_message_get_request_headers(msg);
  for (i = 0; i < req->headers.count; ++i) {
    soup_message_headers_append(req_headers, req->headers.headers[i].key,
                                req->headers.headers[i].value);
  }

  if (payload && payload_len > 0) {
    /* Note: if chunked upload is needed, we would use
       soup_message_set_request_body with an input stream. For now, simple byte
       buffer upload. */
    body_bytes = g_bytes_new(payload, payload_len);
    soup_message_set_request_body_from_bytes(msg, NULL, body_bytes);
    g_bytes_unref(body_bytes);
  } else if (req->read_chunk) {
    /* Fallback for chunked upload without input stream: not fully supported in
     * this simplified binding */
    LOG_DEBUG("http_libsoup3_send: Error ENOTSUP (chunked upload stream)");
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup;
  }

  resp_bytes = soup_session_send_and_read(ctx->session, msg, NULL, &error);

  if (error) {
    LOG_DEBUG("http_libsoup3_send: Error g_error %d: %s", error->code,
              error->message);
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
      rc = ECONNREFUSED;
    } else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
      rc = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    } else {
      rc = C_ABSTRACT_HTTP_ERR_IO;
    }
    goto cleanup;
  }

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    LOG_DEBUG("http_libsoup3_send: Error ENOMEM allocating new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  rc = http_response_init(new_res);
  if (rc != 0) {
    LOG_DEBUG("http_libsoup3_send: Error http_response_init failed with %d",
              rc);
    free(new_res);
    new_res = NULL;
    goto cleanup;
  }

  new_res->status_code = soup_message_get_status(msg);

  if (resp_bytes) {
    resp_data = g_bytes_get_data(resp_bytes, &resp_len);
    if (resp_data && resp_len > 0) {
      if (req->on_chunk) {
        /* If streaming was requested, simulate it here via callback since we
         * fetched everything */
        int chunk_rc =
            req->on_chunk(req->on_chunk_user_data, resp_data, resp_len);
        if (chunk_rc != 0) {
          LOG_DEBUG("http_libsoup3_send: Error on_chunk failed %d", chunk_rc);
          rc = ECANCELED; /* User aborted */
          free(new_res);
          new_res = NULL;
          goto cleanup;
        }
        /* When chunked, no body is kept */
        new_res->body = NULL;
        new_res->body_len = 0;
      } else {
        response_body_copy = (char *)malloc(resp_len + 1);
        if (!response_body_copy) {
          LOG_DEBUG(
              "http_libsoup3_send: Error ENOMEM allocating response_body_copy");
          free(new_res);
          new_res = NULL;
          rc = C_ABSTRACT_HTTP_ERR_NOMEM;
          goto cleanup;
        }
        memcpy(response_body_copy, resp_data, resp_len);
        response_body_copy[resp_len] = '\0';
        new_res->body = response_body_copy;
        new_res->body_len = resp_len;
      }
    }
  }

  *res = new_res;

cleanup:
  if (error)
    g_error_free(error);
  if (resp_bytes)
    g_bytes_unref(resp_bytes);
  if (msg)
    g_object_unref(msg);

  if (rc == 0) {
    LOG_DEBUG("http_libsoup3_send: Success");
  } else {
    LOG_DEBUG("http_libsoup3_send: Error returning %d", rc);
  }
  return rc;
}
