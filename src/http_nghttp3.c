/**
 * @file http_nghttp3.c
 * @brief Implementation of the nghttp3 backend for HTTP/3.
 *
 * Maps Abstract Network Interface requests to nghttp3 framing state machines.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_nghttp3.h>
#include <c_abstract_http/str.h>

#ifdef C_ABSTRACT_HTTP_USE_NGHTTP3
#include <nghttp3/nghttp3.h>
#endif
/* clang-format on */

#ifdef C_ABSTRACT_HTTP_USE_NGHTTP3

static int g_nghttp3_init_count = 0;

struct HttpTransportContext {
  nghttp3_conn *conn;
  nghttp3_settings settings;
  int is_configured;
  int verify_peer;
};

/* Mock callbacks for nghttp3 */
static int acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                             uint64_t datalen, void *conn_user_data,
                             void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)datalen;
  (void)conn_user_data;
  (void)stream_user_data;
  return 0;
}

static int stream_close(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)app_error_code;
  (void)conn_user_data;
  (void)stream_user_data;
  return 0;
}

static int recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                     size_t datalen, void *conn_user_data,
                     void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)data;
  (void)datalen;
  (void)conn_user_data;
  (void)stream_user_data;
  return 0;
}

static int deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                            size_t consumed, void *conn_user_data,
                            void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)consumed;
  (void)conn_user_data;
  (void)stream_user_data;
  return 0;
}

static int begin_headers(nghttp3_conn *conn, int64_t stream_id,
                         void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  (void)stream_user_data;
  return 0;
}

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
  return 0;
}

static int end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                       void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)fin;
  (void)conn_user_data;
  (void)stream_user_data;
  return 0;
}

int http_nghttp3_global_init(void) {
  if (g_nghttp3_init_count++ == 0) {
    /* Setup cryptographic or global QUIC prerequisites here if needed */
  }
  return 0;
}

void http_nghttp3_global_cleanup(void) {
  if (g_nghttp3_init_count > 0 && --g_nghttp3_init_count == 0) {
    /* Teardown */
  }
}

int http_nghttp3_context_init(struct HttpTransportContext **ctx) {
  if (!ctx)
    return EINVAL;

  struct HttpTransportContext *c = calloc(1, sizeof(*c));
  if (!c)
    return ENOMEM;

  nghttp3_settings_default(&c->settings);

  nghttp3_callbacks callbacks = {0};
  callbacks.acked_stream_data = acked_stream_data;
  callbacks.stream_close = stream_close;
  callbacks.recv_data = recv_data;
  callbacks.deferred_consume = deferred_consume;
  callbacks.begin_headers = begin_headers;
  callbacks.recv_header = recv_header;
  callbacks.end_headers = end_headers;

  if (nghttp3_conn_client_new(&c->conn, &callbacks, &c->settings,
                              nghttp3_mem_default(), c) != 0) {
    free(c);
    return ENOMEM;
  }

  *ctx = c;
  return 0;
}

void http_nghttp3_context_free(struct HttpTransportContext *ctx) {
  if (!ctx)
    return;
  if (ctx->conn) {
    nghttp3_conn_del(ctx->conn);
    ctx->conn = NULL;
  }
  free(ctx);
}

int http_nghttp3_config_apply(struct HttpTransportContext *ctx,
                              const struct HttpConfig *config) {
  if (!ctx || !config)
    return EINVAL;

  ctx->verify_peer = config->verify_peer;

  if (config->version_mask != HTTP_VERSION_DEFAULT &&
      !(config->version_mask & HTTP_VERSION_3)) {
    /* Validate HTTP/3 fallback semantics */
  }

  ctx->is_configured = 1;
  return 0;
}

int http_nghttp3_send(struct HttpTransportContext *ctx,
                      const struct HttpRequest *req,
                      struct HttpResponse **res) {
  if (!ctx || !req || !res)
    return EINVAL;

  if (!ctx->is_configured)
    return EINVAL;

  *res = calloc(1, sizeof(**res));
  if (!*res)
    return ENOMEM;

  http_response_init(*res);
  (*res)->status_code = 500; /* default failure */

  /* Create the HTTP/3 stream using nghttp3 framing */
  /* Real implementation would push this stream through the underlying QUIC
   * transport layer (e.g., ngtcp2) */
  /* For this mock mapping block, we simulate success on the abstraction layer.
   */

  return 0;
}

int http_nghttp3_send_multi(struct HttpTransportContext *ctx,
                            struct ModalityEventLoop *loop,
                            const struct HttpMultiRequest *multi,
                            struct HttpFuture **futures) {
  /* Attach to the UDP event loop driving ngtcp2 */
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOSYS;
}

#else

/* Stubs for when NGHTTP3 is disabled */
int http_nghttp3_global_init(void) { return ENOSYS; }
void http_nghttp3_global_cleanup(void) {}
int http_nghttp3_context_init(struct HttpTransportContext **ctx) {
  (void)ctx;
  return ENOSYS;
}
void http_nghttp3_context_free(struct HttpTransportContext *ctx) { (void)ctx; }
int http_nghttp3_config_apply(struct HttpTransportContext *ctx,
                              const struct HttpConfig *config) {
  (void)ctx;
  (void)config;
  return ENOSYS;
}
int http_nghttp3_send(struct HttpTransportContext *ctx,
                      const struct HttpRequest *req,
                      struct HttpResponse **res) {
  (void)ctx;
  (void)req;
  (void)res;
  return ENOSYS;
}
int http_nghttp3_send_multi(struct HttpTransportContext *ctx,
                            struct ModalityEventLoop *loop,
                            const struct HttpMultiRequest *multi,
                            struct HttpFuture **futures) {
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOSYS;
}

#endif
