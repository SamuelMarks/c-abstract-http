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
#include "crypto_utils.h"

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

  if (config->tls_version_mask != 0 &&
      (config->tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  {
    long eff_ms = config->timeout_ms;
    if (eff_ms <= 0 && config->connect_timeout_ms > eff_ms) {
      eff_ms = config->connect_timeout_ms;
    }
    if (eff_ms <= 0 && config->read_timeout_ms > eff_ms) {
      eff_ms = config->read_timeout_ms;
    }
    if (eff_ms <= 0 && config->write_timeout_ms > eff_ms) {
      eff_ms = config->write_timeout_ms;
    }
    if (eff_ms > 0) {
      g_object_set(ctx->session, "timeout", (guint)((eff_ms + 999L) / 1000L),
                   NULL);
    }
    if (config->read_timeout_ms > 0) {
      g_object_set(ctx->session, "idle-timeout",
                   (guint)((config->read_timeout_ms + 999L) / 1000L), NULL);
    }
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;
  ctx->config.tls_version_mask = config->tls_version_mask;

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

/**
 * @brief Build a Basic Proxy-Authorization header value ("Basic <base64>").
 * @param user Proxy username.
 * @param pass Optional proxy password.
 * @param out_val Newly allocated header value string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
soup_build_proxy_auth(const char *user, const char *pass, char **out_val) {
  const char *pw = pass ? pass : "";
  size_t ulen = strlen(user);
  size_t plen = strlen(pw);
  char *raw;
  char *b64 = NULL;
  size_t b64_len = 0;
  char *hdr;
  enum c_abstract_http_error rc;

  raw = (char *)malloc(ulen + 1 + plen + 1);
  if (!raw) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(raw, user, ulen);
  raw[ulen] = ':';
  memcpy(raw + ulen + 1, pw, plen);
  raw[ulen + 1 + plen] = '\0';

  rc = base64_encode((const unsigned char *)raw, ulen + 1 + plen, &b64,
                     &b64_len);
  free(raw);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  hdr = (char *)malloc(6 + b64_len + 1);
  if (!hdr) {
    free(b64);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(hdr, "Basic ", 6);
  memcpy(hdr + 6, b64, b64_len + 1);
  free(b64);
  *out_val = hdr;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Construct and populate a `SoupMessage` for an `HttpRequest`.
 * @param ctx Transport context.
 * @param req Input HTTP request.
 * @param out_msg Output `SoupMessage` pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
soup_build_message(const struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, SoupMessage **out_msg) {
  SoupMessage *msg = NULL;
  SoupMessageHeaders *req_headers = NULL;
  GBytes *body_bytes = NULL;
  const char *method_str = NULL;
  size_t i;
  enum c_abstract_http_error rc;

  if (req->parts.count > 0 && !req->body) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc = get_method_string(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  msg = soup_message_new(method_str, req->url);
  if (!msg) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!ctx->config.follow_redirects) {
    soup_message_set_flags(msg, SOUP_MESSAGE_NO_REDIRECT);
  }

  req_headers = soup_message_get_request_headers(msg);
  for (i = 0; i < req->headers.count; ++i) {
    soup_message_headers_append(req_headers, req->headers.headers[i].key,
                                req->headers.headers[i].value);
  }
  if (ctx->config.proxy_url && ctx->config.proxy_username) {
    char *auth_val = NULL;
    rc = soup_build_proxy_auth(ctx->config.proxy_username,
                               ctx->config.proxy_password, &auth_val);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      g_object_unref(msg);
      return rc;
    }
    soup_message_headers_append(req_headers, "Proxy-Authorization", auth_val);
    free(auth_val);
  }

  if (req->body) {
    if (req->body_len > 0) {
      body_bytes = g_bytes_new(req->body, req->body_len);
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
        return C_ABSTRACT_HTTP_ERR_IO;
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

  *out_msg = msg;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse `SoupMessage` + `GBytes` / `GError` into an `HttpResponse`.
 * @param msg Completed `SoupMessage` (consumed/unreffed by this function).
 * @param resp_bytes Response payload `GBytes` (consumed/unreffed).
 * @param error Optional `GError` (consumed/freed).
 * @param req Originating HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
soup_finalize_result(SoupMessage *msg, GBytes *resp_bytes, GError *error,
                     const struct HttpRequest *req, struct HttpResponse **res) {
  struct HttpResponse *new_res = NULL;
  gsize resp_len = 0;
  gconstpointer resp_data = NULL;
  char *response_body_copy = NULL;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  if (error) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED)) {
      rc = C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    } else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
      rc = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    } else {
      rc = C_ABSTRACT_HTTP_ERR_IO;
    }
    g_error_free(error);
    g_bytes_unref(resp_bytes);
    g_object_unref(msg);
    return rc;
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
    g_bytes_unref(resp_bytes);
    g_object_unref(msg);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_res_init_fail) {
    free(new_res);
    g_bytes_unref(resp_bytes);
    g_object_unref(msg);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(new_res);
    g_bytes_unref(resp_bytes);
    g_object_unref(msg);
    return rc;
  }

  new_res->status_code = (int)soup_message_get_status(msg);
  resp_data = g_bytes_get_data(resp_bytes, &resp_len);

  if (req->on_chunk) {
    enum c_abstract_http_error chunk_rc =
        req->on_chunk(req->on_chunk_user_data, resp_data, resp_len);
    g_bytes_unref(resp_bytes);
    g_object_unref(msg);
    if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
      enum c_abstract_http_error free_rc = http_response_free(new_res);
      free(new_res);
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return free_rc;
      }
      return C_ABSTRACT_HTTP_ERR_CANCELED;
    }
    new_res->body = NULL;
    new_res->body_len = 0;
    *res = new_res;
    return C_ABSTRACT_HTTP_SUCCESS;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libsoup3_body_alloc_fail) {
    response_body_copy = NULL;
  } else
#endif
  {
    response_body_copy = (char *)malloc(resp_len + 1);
  }
  if (!response_body_copy) {
    enum c_abstract_http_error free_rc = http_response_free(new_res);
    free(new_res);
    g_bytes_unref(resp_bytes);
    g_object_unref(msg);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc
                                              : C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(response_body_copy, resp_data, resp_len);
  response_body_copy[resp_len] = '\0';
  new_res->body = response_body_copy;
  new_res->body_len = resp_len;

  g_bytes_unref(resp_bytes);
  g_object_unref(msg);
  *res = new_res;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Send an HTTP request using libsoup3. */
enum c_abstract_http_error http_libsoup3_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  SoupMessage *msg = NULL;
  GBytes *resp_bytes = NULL;
  GError *error = NULL;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !ctx->session || !req || !res) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = soup_build_message(ctx, req, &msg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  resp_bytes = soup_session_send_and_read(ctx->session, msg, NULL, &error);
  return soup_finalize_result(msg, resp_bytes, error, req, res);
}

/** @brief Shared batch state for `http_libsoup3_send_multi` */
struct soup_multi_batch {
  /** @brief Active SoupSession */
  SoupSession *session;
  /** @brief Non-zero when caller supplied an external loop */
  int caller_loop;
  /** @brief Number of still-pending async operations */
  size_t remaining;
  /** @brief First error encountered */
  enum c_abstract_http_error first_err;
};

/** @brief Per-request state for `soup_session_send_and_read_async` */
struct soup_multi_slot {
  /** @brief Parent batch coordinator */
  struct soup_multi_batch *batch;
  /** @brief Originating HTTP request */
  const struct HttpRequest *req;
  /** @brief In-flight SoupMessage */
  SoupMessage *msg;
  /** @brief Target future handle */
  struct HttpFuture *future;
};

/**
 * @brief Finalize an async `SoupSession` completion result.
 * @param slot Completed slot.
 * @param async_res GLib async result token.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
soup_async_ready_impl(struct soup_multi_slot *slot, GAsyncResult *async_res) {
  GError *error = NULL;
  GBytes *resp_bytes = soup_session_send_and_read_finish(slot->batch->session,
                                                         async_res, &error);
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc =
      soup_finalize_result(slot->msg, resp_bytes, error, slot->req, &res);

  slot->msg = NULL;
  slot->future->response = res;
  slot->future->error_code = rc;
  slot->future->is_ready = 1;
  if (rc != C_ABSTRACT_HTTP_SUCCESS &&
      slot->batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
    slot->batch->first_err = rc;
  }
  if (slot->batch->remaining > 0) {
    slot->batch->remaining--;
  }
  return rc;
}

/**
 * @brief GLib `GAsyncReadyCallback` trampoline for
 * `soup_session_send_and_read_async`.
 * @param source_object Originating `SoupSession`.
 * @param res Async result handle.
 * @param user_data Slot pointer (`struct soup_multi_slot *`).
 */
static void soup_async_ready_cb(void *source_object, GAsyncResult *res,
                                void *user_data) {
  struct soup_multi_slot *slot = (struct soup_multi_slot *)user_data;
  enum c_abstract_http_error rc;
  (void)source_object;
  rc = soup_async_ready_impl(slot, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("soup_async_ready_cb: error %d", (int)rc);
  }
}

/**
 * @brief `ModalityEventLoop` timer callback pumping `GMainContext` iterations.
 * @param loop Active event loop.
 * @param timer_id Fired timer ID.
 * @param user_data Pointer to `struct soup_multi_batch`.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop control error.
 */
static enum c_abstract_http_error
soup_pump_gmain_cb(struct ModalityEventLoop *loop, int timer_id,
                   void *user_data) {
  struct soup_multi_batch *batch = (struct soup_multi_batch *)user_data;
  GMainContext *gctx = g_main_context_default();
  (void)timer_id;

  while (g_main_context_iteration(gctx, FALSE)) {
    /* Drain ready GLib main context events */
  }
  if (batch->remaining > 0) {
    int next_id = 0;
    return http_loop_add_timer(loop, 1, soup_pump_gmain_cb, batch, &next_id);
  }
  if (batch->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Dispatch concurrent requests via soup_session_send_and_read_async +
 * GMainContext. */
enum c_abstract_http_error http_libsoup3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop;
  struct soup_multi_batch batch;
  struct soup_multi_slot *slots;
  size_t i;
  int timer_id = 0;
  enum c_abstract_http_error rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !ctx->session || !multi || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  slots = (struct soup_multi_slot *)calloc(multi->count, sizeof(*slots));
  if (!slots) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (loop) {
    active_loop = loop;
  } else {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slots);
      return rc;
    }
    active_loop = owned_loop;
  }

  memset(&batch, 0, sizeof(batch));
  batch.session = ctx->session;
  batch.caller_loop = (loop != NULL) ? 1 : 0;
  batch.remaining = multi->count;
  batch.first_err = C_ABSTRACT_HTTP_SUCCESS;

  for (i = 0; i < multi->count; ++i) {
    slots[i].batch = &batch;
    slots[i].req = multi->requests[i];
    slots[i].future = futures[i];
    rc = soup_build_message(ctx, slots[i].req, &slots[i].msg);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      slots[i].future->response = NULL;
      slots[i].future->error_code = rc;
      slots[i].future->is_ready = 1;
      batch.remaining--;
      if (batch.first_err == C_ABSTRACT_HTTP_SUCCESS) {
        batch.first_err = rc;
      }
      continue;
    }
    soup_session_send_and_read_async(ctx->session, slots[i].msg,
                                     G_PRIORITY_DEFAULT, NULL,
                                     soup_async_ready_cb, &slots[i]);
  }

  if (batch.remaining > 0) {
    rc = http_loop_add_timer(active_loop, 0, soup_pump_gmain_cb, &batch,
                             &timer_id);
    (void)timer_id;
    if (rc == C_ABSTRACT_HTTP_SUCCESS) {
      rc = http_loop_run(active_loop);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS &&
        batch.first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch.first_err = rc;
    }
  }

  for (i = 0; i < multi->count; ++i) {
    if (slots[i].msg) {
      g_object_unref(slots[i].msg);
    }
  }
  free(slots);

  if (owned_loop) {
    enum c_abstract_http_error lrc = http_loop_free(owned_loop);
    if (lrc != C_ABSTRACT_HTTP_SUCCESS &&
        batch.first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch.first_err = lrc;
    }
  } else if (loop) {
    enum c_abstract_http_error urc = abstract_http_event_loop_test_unstop(loop);
    enum c_abstract_http_error wrc = http_loop_wakeup(loop);
    if (urc != C_ABSTRACT_HTTP_SUCCESS &&
        batch.first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch.first_err = urc;
    }
    if (wrc != C_ABSTRACT_HTTP_SUCCESS &&
        batch.first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch.first_err = wrc;
    }
  }

  return batch.first_err;
}
