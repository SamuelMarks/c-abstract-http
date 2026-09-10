/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_nghttp3.h>
#include "c_abstract_http/log.h"
#include "str.h"

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
#include <nghttp3/nghttp3.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_nghttp3_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_conn_new_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_response_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_send_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_loop_wakeup_fail(void);

#define g_mock_nghttp3_global_init_fail                                        \
  (*abstract_http_mock_get_g_mock_nghttp3_global_init_fail())
#define g_mock_nghttp3_context_init_fail                                       \
  (*abstract_http_mock_get_g_mock_nghttp3_context_init_fail())
#define g_mock_nghttp3_config_init_fail                                        \
  (*abstract_http_mock_get_g_mock_nghttp3_config_init_fail())
#define g_mock_nghttp3_conn_new_fail                                           \
  (*abstract_http_mock_get_g_mock_nghttp3_conn_new_fail())
#define g_mock_nghttp3_response_alloc_fail                                     \
  (*abstract_http_mock_get_g_mock_nghttp3_response_alloc_fail())
#define g_mock_nghttp3_response_init_fail                                      \
  (*abstract_http_mock_get_g_mock_nghttp3_response_init_fail())
#define g_mock_nghttp3_send_fail                                               \
  (*abstract_http_mock_get_g_mock_nghttp3_send_fail())
#define g_mock_nghttp3_loop_wakeup_fail                                        \
  (*abstract_http_mock_get_g_mock_nghttp3_loop_wakeup_fail())
#endif

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
/** @brief nghttp3_conn forward typedef */
typedef struct nghttp3_conn_s nghttp3_conn;
/** @brief nghttp3_mem forward typedef */
typedef struct nghttp3_mem_s nghttp3_mem;
/** @brief nghttp3_rcbuf forward typedef */
typedef struct nghttp3_rcbuf_s nghttp3_rcbuf;

/**
 * @brief nghttp3_settings mock structure.
 */
typedef struct nghttp3_settings_s {
  /** @brief Dummy field */
  int dummy;
} nghttp3_settings;

/**
 * @brief nghttp3_callbacks mock structure.
 */
typedef struct nghttp3_callbacks_s {
  /** @brief acked_stream_data callback */
  int (*acked_stream_data)(nghttp3_conn *conn, int64_t stream_id,
                           uint64_t datalen, void *conn_user_data,
                           void *stream_user_data);
  /** @brief stream_close callback */
  int (*stream_close)(nghttp3_conn *conn, int64_t stream_id,
                      uint64_t app_error_code, void *conn_user_data,
                      void *stream_user_data);
  /** @brief recv_data callback */
  int (*recv_data)(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *conn_user_data,
                   void *stream_user_data);
  /** @brief deferred_consume callback */
  int (*deferred_consume)(nghttp3_conn *conn, int64_t stream_id,
                          size_t consumed, void *conn_user_data,
                          void *stream_user_data);
  /** @brief begin_headers callback */
  int (*begin_headers)(nghttp3_conn *conn, int64_t stream_id,
                       void *conn_user_data, void *stream_user_data);
  /** @brief recv_header callback */
  int (*recv_header)(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                     nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                     void *conn_user_data, void *stream_user_data);
  /** @brief end_headers callback */
  int (*end_headers)(nghttp3_conn *conn, int64_t stream_id, int fin,
                     void *conn_user_data, void *stream_user_data);
} nghttp3_callbacks;

/** @brief nghttp3 connection structure stub */
struct nghttp3_conn_s {
  /** @brief Dummy field */
  int dummy;
};

/**
 * @brief Mock for nghttp3_settings_default.
 *
 * @param[out] settings Settings to initialize.
 */
static void nghttp3_settings_default(nghttp3_settings *settings) {
  settings->dummy = 0;
}

/**
 * @brief Mock for nghttp3_mem_default.
 *
 * @return NULL pointer.
 */
static nghttp3_mem *nghttp3_mem_default(void) { return NULL; }

/**
 * @brief Mock for nghttp3_conn_client_new.
 *
 * @param[out] pconn Pointer to receive connection.
 * @param[in] callbacks Callbacks structure.
 * @param[in] settings Settings structure.
 * @param[in] mem Memory allocator.
 * @param[in] user_data User context data.
 * @return 0 on success, non-zero on failure.
 */
static int nghttp3_conn_client_new(nghttp3_conn **pconn,
                                   const nghttp3_callbacks *callbacks,
                                   const nghttp3_settings *settings,
                                   const nghttp3_mem *mem, void *user_data) {
  (void)callbacks;
  (void)settings;
  (void)mem;
  (void)user_data;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_conn_new_fail) {
    return -1;
  }
#endif
  *pconn = (nghttp3_conn *)calloc(1, sizeof(nghttp3_conn));
  return 0;
}

/**
 * @brief Mock for nghttp3_conn_del.
 *
 * @param[in] conn Connection to free.
 */
static void nghttp3_conn_del(nghttp3_conn *conn) { free(conn); }
#endif

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
  (void)conn;
  (void)stream_id;
  (void)datalen;
  (void)conn_user_data;
  (void)stream_user_data;
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
  (void)conn;
  (void)stream_id;
  (void)app_error_code;
  (void)conn_user_data;
  (void)stream_user_data;
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
  (void)conn;
  (void)stream_id;
  (void)data;
  (void)datalen;
  (void)conn_user_data;
  (void)stream_user_data;
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
  (void)conn;
  (void)stream_id;
  (void)consumed;
  (void)conn_user_data;
  (void)stream_user_data;
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
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  (void)stream_user_data;
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
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)name;
  (void)value;
  (void)flags;
  (void)conn_user_data;
  (void)stream_user_data;
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
  (void)conn;
  (void)stream_id;
  (void)fin;
  (void)conn_user_data;
  (void)stream_user_data;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error c_abstract_http_test_nghttp3_callbacks(void);

/**
 * @brief Test hook to invoke nghttp3 callbacks directly.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_nghttp3_callbacks(void) {
  acked_stream_data(NULL, 0, 0, NULL, NULL);
  stream_close(NULL, 0, 0, NULL, NULL);
  recv_data(NULL, 0, NULL, 0, NULL, NULL);
  deferred_consume(NULL, 0, 0, NULL, NULL);
  begin_headers(NULL, 0, NULL, NULL);
  recv_header(NULL, 0, 0, NULL, NULL, 0, NULL, NULL);
  end_headers(NULL, 0, 0, NULL, NULL);
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Initialize the global nghttp3 API state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_nghttp3_global_init(void) {
  LOG_DEBUG("http_nghttp3_global_init: Entering");
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    LOG_DEBUG("http_nghttp3_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&c->config);
  }

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
void http_nghttp3_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_nghttp3_context_free: Entering");
  if (!ctx) {
    LOG_DEBUG("http_nghttp3_context_free: Exiting early (ctx is NULL)");
    return;
  }
  nghttp3_conn_del(ctx->conn);
  ctx->conn = NULL;
  http_config_free(&ctx->config);
  free(ctx);
  LOG_DEBUG("http_nghttp3_context_free: Exiting");
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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_response_alloc_fail) {
    new_res = NULL;
  } else
#endif
  {
    new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }

  if (!new_res) {
    LOG_DEBUG("http_nghttp3_send: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(new_res);
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_nghttp3_send: Error http_response_init failed with %d",
              (int)rc);
    free(new_res);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_send_fail) {
    http_response_free(new_res);
    free(new_res);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  new_res->status_code = 200;

  if (req->on_chunk) {
    const char dummy_chunk[23] = "HTTP/3 simulated chunk";
    rc = (enum c_abstract_http_error)req->on_chunk(
        req->on_chunk_user_data, dummy_chunk, sizeof(dummy_chunk));
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      http_response_free(new_res);
      free(new_res);
      return rc;
    }
  } else {
    char *body_copy;
    body_copy = NULL;
    rc = c_abstract_http_strdup("HTTP/3 simulated response body", &body_copy);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      http_response_free(new_res);
      free(new_res);
      return rc;
    }
    new_res->body = body_copy;
    new_res->body_len = strlen(body_copy);
  }

  *res = new_res;
  LOG_DEBUG("http_nghttp3_send: Success (simulated)");
  return C_ABSTRACT_HTTP_SUCCESS;
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
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  if (loop) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_nghttp3_loop_wakeup_fail) {
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
