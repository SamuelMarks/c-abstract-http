/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_nghttp3.h>
#include "c_abstract_http/log.h"
#include "str.h"

#include <nghttp3/nghttp3.h>
/* clang-format on */

static int g_nghttp3_init_count = 0;

/**
 * @brief Transport context for nghttp3.
 */
struct HttpTransportContext {
  /** @brief nghttp3 connection handle */
  nghttp3_conn *conn;
  /** @brief nghttp3 settings */
  nghttp3_settings settings;
  /** @brief Flag indicating if configuration has been applied */
  int is_configured;
  /** @brief Peer verification flag */
  int verify_peer;
  /** @brief Stored configuration */
  struct HttpConfig config;
};

/**
 * @brief Mock callback for acked stream data.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] datalen Data length.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                             uint64_t datalen, void *conn_user_data,
                             void *stream_user_data) {
  if (conn || stream_id || datalen || conn_user_data || stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock callback for stream closure.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] app_error_code Application error code.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int stream_close(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  if (conn || stream_id || app_error_code || conn_user_data ||
      stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock callback for received stream data.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] data Data buffer.
 * @param[in] datalen Data length.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                     size_t datalen, void *conn_user_data,
                     void *stream_user_data) {
  if (conn || stream_id || data || datalen || conn_user_data ||
      stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock callback for deferred consumption.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] consumed Number of bytes consumed.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                            size_t consumed, void *conn_user_data,
                            void *stream_user_data) {
  if (conn || stream_id || consumed || conn_user_data || stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock callback for beginning headers.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int begin_headers(nghttp3_conn *conn, int64_t stream_id,
                         void *conn_user_data, void *stream_user_data) {
  if (conn || stream_id || conn_user_data || stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock callback for received header.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] token Header token.
 * @param[in] name Header name.
 * @param[in] value Header value.
 * @param[in] flags Header flags.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                       nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                       void *conn_user_data, void *stream_user_data) {
  if (conn || stream_id || token || name || value || flags || conn_user_data ||
      stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock callback for ending headers.
 *
 * @param[in] conn nghttp3 connection.
 * @param[in] stream_id Stream ID.
 * @param[in] fin Fin flag.
 * @param[in] conn_user_data Connection user data.
 * @param[in] stream_user_data Stream user data.
 * @return 0 on success.
 */
static int end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                       void *conn_user_data, void *stream_user_data) {
  if (conn || stream_id || fin || conn_user_data || stream_user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize the global nghttp3 API state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_nghttp3_global_init(void) {
  LOG_DEBUG("http_nghttp3_global_init: Entering");
  if (g_nghttp3_init_count++ == 0) {
    /* Setup cryptographic or global QUIC prerequisites here if needed */
  }
  LOG_DEBUG("http_nghttp3_global_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up the global nghttp3 API state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_nghttp3_global_cleanup(void) {
  LOG_DEBUG("http_nghttp3_global_cleanup: Entering");
  if (g_nghttp3_init_count > 0 && --g_nghttp3_init_count == 0) {
    /* Teardown */
  }
  LOG_DEBUG("http_nghttp3_global_cleanup: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a new nghttp3 transport context.
 *
 * @param[out] ctx Double pointer to receive newly allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_nghttp3_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
  struct HttpTransportContext *c;
  nghttp3_callbacks callbacks;

  LOG_DEBUG("http_nghttp3_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_nghttp3_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    LOG_DEBUG("http_nghttp3_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  { rc = http_config_init(&c->config); }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_nghttp3_context_init: Error http_config_init failed with %d",
        (int)rc);
    free(c);
    *ctx = NULL;
    return rc;
  }

  nghttp3_settings_default(&c->settings);

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.acked_stream_data = acked_stream_data;
  callbacks.stream_close = stream_close;
  callbacks.recv_data = recv_data;
  callbacks.deferred_consume = deferred_consume;
  callbacks.begin_headers = begin_headers;
  callbacks.recv_header = recv_header;
  callbacks.end_headers = end_headers;

  if (nghttp3_conn_client_new(&c->conn, &callbacks, &c->settings,
                              nghttp3_mem_default(), c) != 0) {
    LOG_DEBUG(
        "http_nghttp3_context_init: Error nghttp3_conn_client_new failed");
    http_config_free(&c->config);
    free(c);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  *ctx = c;
  LOG_DEBUG("http_nghttp3_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free a nghttp3 transport context.
 *
 * @param[in] ctx The context to free. Safe to pass NULL.
 */
enum c_abstract_http_error
http_nghttp3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_nghttp3_context_free: Entering");
  if (!ctx) {
    LOG_DEBUG("http_nghttp3_context_free: Exiting early (ctx is NULL)");
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  nghttp3_conn_del(ctx->conn);
  ctx->conn = NULL;
  {
    enum c_abstract_http_error rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
  }
  free(ctx);
  LOG_DEBUG("http_nghttp3_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Apply configuration settings to an nghttp3 transport context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config The configuration settings.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_nghttp3_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_nghttp3_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_nghttp3_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->verify_peer = config->verify_peer;

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;

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

  ctx->is_configured = 1;
  LOG_DEBUG("http_nghttp3_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send a single request via nghttp3.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_nghttp3_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
  enum c_abstract_http_error rc;
  struct HttpResponse *new_res;

  new_res = NULL;
  rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_nghttp3_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_nghttp3_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = NULL;

  if (!ctx->is_configured) {
    LOG_DEBUG("http_nghttp3_send: Error EINVAL (not configured)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (ctx->conn) {
    nghttp3_info info;
    info.api_version = 0;
    info.version_str = NULL;
    info.version_num = 0;

    info = *nghttp3_version(0);
    if (info.version_num > 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  *res = NULL;
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Dispatch multiple HTTP requests using nghttp3.
 *
 * @param[in] ctx Pointer to an initialized HttpTransportContext.
 * @param[in,out] loop Event loop to drive execution.
 * @param[in] multi Multi-request specification.
 * @param[out] futures Array of returned HttpFuture handles.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_nghttp3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_nghttp3_send_multi: Entering");

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_nghttp3_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res;
    res = NULL;
    rc = http_nghttp3_send(ctx, multi->requests[i], &res);
    futures[i]->error_code = rc;
    futures[i]->response = res;
    futures[i]->is_ready = 1;
  }

  if (loop) {
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}
