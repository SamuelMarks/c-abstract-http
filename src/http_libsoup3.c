/**
 * @file http_libsoup3.c
 * @brief GNOME libsoup-3.0 transport backend implementation of the Abstract
 * Network Interface (ANI).
 */

/* clang-format off */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libsoup3.h>
#include "c_abstract_http/log.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_libsoup3.h"
#else
#include <libsoup/soup.h>
#endif
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief SoupSession pointer */
  SoupSession *session;
  /** @brief Cookie jar */
  struct HttpCookieJar *cookie_jar;
  /** @brief Configuration */
  struct HttpConfig config;
};

/** @brief Reference count of global libsoup3 initializations */
static int g_libsoup3_init_count = 0;

/**
 * @brief Initialize global libsoup3 state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libsoup3_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  g_libsoup3_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global libsoup3 state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_libsoup3_global_cleanup(void) {
  if (g_libsoup3_init_count > 0) {
    g_libsoup3_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Initialize libsoup3 transport context. */
enum c_abstract_http_error
http_libsoup3_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_libsoup3_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libsoup3_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_context_init_fail) {
    *ctx = NULL;
  } else
#endif
  {
    *ctx = (struct HttpTransportContext *)malloc(
        sizeof(struct HttpTransportContext));
  }
  if (!*ctx) {
    LOG_DEBUG("http_libsoup3_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_config_init_fail) {
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_libsoup3_context_init: Error http_config_init failed with %d",
        (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->session = soup_session_new();
  if (!(*ctx)->session) {
    enum c_abstract_http_error free_rc;
    LOG_DEBUG("http_libsoup3_context_init: Error soup_session_new failed");
    free_rc = http_config_free(&(*ctx)->config);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*ctx);
      *ctx = NULL;
      return free_rc;
    }
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  (*ctx)->cookie_jar = NULL;

  LOG_DEBUG("http_libsoup3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free libsoup3 transport context. */
enum c_abstract_http_error
http_libsoup3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libsoup3_context_free: Entering");
  if (ctx) {
    enum c_abstract_http_error rch;
    g_object_unref(ctx->session);
    ctx->session = NULL;
    rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    free(ctx);
  }
  LOG_DEBUG("http_libsoup3_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration to libsoup3 context. */
enum c_abstract_http_error
http_libsoup3_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_libsoup3_config_apply: Entering");
  if (!ctx || !ctx->session || !config) {
    LOG_DEBUG("http_libsoup3_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (config->timeout_ms > 0) {
    g_object_set(ctx->session, "timeout", (guint)(config->timeout_ms / 1000),
                 NULL);
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;

  free(ctx->config.user_agent);
  ctx->config.user_agent = NULL;
  if (config->user_agent) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    g_object_set(ctx->session, "user-agent", config->user_agent, NULL);
  } else {
    g_object_set(ctx->session, "user-agent", NULL, NULL);
  }

  free(ctx->config.proxy_url);
  ctx->config.proxy_url = NULL;
  if (config->proxy_url) {
    GUri *proxy_uri = g_uri_parse(config->proxy_url, G_URI_FLAGS_NONE, NULL);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      g_uri_unref(proxy_uri);
      return rc;
    }
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

  free(ctx->config.proxy_username);
  ctx->config.proxy_username = NULL;
  if (config->proxy_username) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_username,
                                  &ctx->config.proxy_username);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_password);
  ctx->config.proxy_password = NULL;
  if (config->proxy_password) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_password,
                                  &ctx->config.proxy_password);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  if (config->cookie_jar) {
    ctx->cookie_jar = config->cookie_jar;
  } else {
    ctx->cookie_jar = NULL;
  }

  LOG_DEBUG("http_libsoup3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HttpMethod enum to method string.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out Pointer to receive string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
static enum c_abstract_http_error get_method_string(enum HttpMethod method,
                                                    const char **out) {
  if (!out) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch ((int)method) {
  case HTTP_GET:
    *out = "GET";
    break;
  case HTTP_POST:
    *out = "POST";
    break;
  case HTTP_PUT:
    *out = "PUT";
    break;
  case HTTP_DELETE:
    *out = "DELETE";
    break;
  case HTTP_HEAD:
    *out = "HEAD";
    break;
  case HTTP_PATCH:
    *out = "PATCH";
    break;
  case HTTP_QUERY:
    *out = "QUERY";
    break;
  case HTTP_OPTIONS:
    *out = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out = "TRACE";
    break;
  case HTTP_CONNECT:
    *out = "CONNECT";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error
c_abstract_http_test_libsoup3_method_str(enum HttpMethod method,
                                         const char **out);

/**
 * @brief Expose get_method_string for testing.
 *
 * @param[in] method HTTP method.
 * @param[out] out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
c_abstract_http_test_libsoup3_method_str(enum HttpMethod method,
                                         const char **out) {
  return get_method_string(method, out);
}
#endif

/* Send an HTTP request using libsoup3. */
enum c_abstract_http_error http_libsoup3_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  SoupMessage *msg = NULL;
  SoupMessageHeaders *req_headers = NULL;
  GBytes *body_bytes = NULL;
  GBytes *resp_bytes = NULL;
  GError *error = NULL;
  struct HttpResponse *new_res = NULL;
  size_t i;
  void *payload = NULL;
  size_t payload_len = 0;
  const char *method_str = NULL;
  gsize resp_len = 0;
  gconstpointer resp_data = NULL;
  char *response_body_copy = NULL;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  cah_cppcheck_mut_ptr((void *)ctx);
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

  rc = get_method_string(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libsoup3_send: Error get_method_string failed %d", (int)rc);
    return rc;
  }

  msg = soup_message_new(method_str, req->url);
  if (!msg) {
    LOG_DEBUG("http_libsoup3_send: Error soup_message_new failed (bad URL?)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  req_headers = soup_message_get_request_headers(msg);
  for (i = 0; i < req->headers.count; ++i) {
    soup_message_headers_append(req_headers, req->headers.headers[i].key,
                                req->headers.headers[i].value);
  }

  if (payload) {
    if (payload_len > 0) {
      body_bytes = g_bytes_new(payload, payload_len);
      soup_message_set_request_body_from_bytes(msg, NULL, body_bytes);
      g_bytes_unref(body_bytes);
    }
  } else if (req->read_chunk) {
    char buf[4096];
    size_t out_read;
    GByteArray *byte_array = g_byte_array_new();

    for (;;) {
      enum c_abstract_http_error chunk_rc = req->read_chunk(
          req->read_chunk_user_data, buf, sizeof(buf), &out_read);
      if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
        g_byte_array_free(byte_array, TRUE);
        g_object_unref(msg);
        return (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) ? C_ABSTRACT_HTTP_ERR_IO
                                                     : chunk_rc;
      }
      if (out_read == 0) {
        break;
      }
      g_byte_array_append(byte_array, (const guint8 *)buf, (guint)out_read);
    }

    body_bytes = g_byte_array_free_to_bytes(byte_array);
    soup_message_set_request_body_from_bytes(msg, NULL, body_bytes);
    g_bytes_unref(body_bytes);
  }

  resp_bytes = soup_session_send_and_read(ctx->session, msg, NULL, &error);

  if (error) {
    LOG_DEBUG("http_libsoup3_send: Error g_error %d: %s", error->code,
              error->message);
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
      rc = C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    } else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
      rc = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    } else {
      rc = C_ABSTRACT_HTTP_ERR_IO;
    }
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_res_alloc_fail) {
    new_res = NULL;
  } else
#endif
  {
    new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!new_res) {
    LOG_DEBUG("http_libsoup3_send: Error ENOMEM allocating new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_res_init_fail) {
    free(new_res);
    new_res = NULL;
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
#endif
  rc = http_response_init(new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libsoup3_send: Error http_response_init failed with %d",
              (int)rc);
    free(new_res);
    new_res = NULL;
    goto cleanup;
  }

  new_res->status_code = (int)soup_message_get_status(msg);

  resp_data = g_bytes_get_data(resp_bytes, &resp_len);
  if (req->on_chunk) {
    enum c_abstract_http_error chunk_rc =
        req->on_chunk(req->on_chunk_user_data, resp_data, resp_len);
    if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
      enum c_abstract_http_error free_rc;
      LOG_DEBUG("http_libsoup3_send: Error on_chunk failed %d", (int)chunk_rc);
      free_rc = http_response_free(new_res);
      free(new_res);
      new_res = NULL;
      if (resp_bytes) {
        g_bytes_unref(resp_bytes);
      }
      if (msg) {
        g_object_unref(msg);
      }
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return free_rc;
      }
      return (chunk_rc != C_ABSTRACT_HTTP_SUCCESS)
                 ? C_ABSTRACT_HTTP_ERR_CANCELED
                 : chunk_rc;
    }
    new_res->body = NULL;
    new_res->body_len = 0;
  } else {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libsoup3_body_alloc_fail) {
      response_body_copy = NULL;
    } else
#endif
    {
      response_body_copy = (char *)malloc(resp_len + 1);
    }
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

  *res = new_res;

cleanup:
  if (error) {
    g_error_free(error);
  }
  g_bytes_unref(resp_bytes);
  g_object_unref(msg);

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libsoup3_send: Success");
  } else {
    LOG_DEBUG("http_libsoup3_send: Error returning %d", (int)rc);
  }
  return rc;
}
