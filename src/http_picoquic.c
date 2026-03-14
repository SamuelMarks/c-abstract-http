/**
 * @file http_picoquic.c
 * @brief Implementation of the Picoquic backend for HTTP/3.
 *
 * Maps Abstract Network Interface requests to Picoquic (and h3zero).
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_picoquic.h>
#include <c_abstract_http/str.h>
/* clang-format on */

#ifdef C_ABSTRACT_HTTP_USE_PICOQUIC

/* Forward declarations to avoid complex header inclusions */
typedef struct st_picoquic_quic_t picoquic_quic_t;
typedef struct st_picoquic_cnx_t picoquic_cnx_t;
struct sockaddr;

extern picoquic_quic_t *
picoquic_create(uint32_t nb_connections, char const *cert_file_name,
                char const *key_file_name, char const *cert_root_file_name,
                char const *default_alpn, void *default_callback_fn,
                void *default_callback_ctx, void *connection_id_callback,
                void *connection_id_callback_ctx, uint8_t reset_seed[16],
                uint64_t current_time, uint64_t *p_simulated_time,
                char const *ticket_file_name,
                const uint8_t *ticket_encryption_key,
                size_t ticket_encryption_key_length);

extern void picoquic_free(picoquic_quic_t *quic);
extern picoquic_cnx_t *
picoquic_create_cnx(picoquic_quic_t *quic, picoquic_cnx_t *cnx_id_alloc,
                    struct sockaddr *addr, uint64_t start_time,
                    uint32_t preferred_version, char const *sni,
                    char const *alpn, char const *client_mode, uint16_t length);
extern void picoquic_delete_cnx(picoquic_cnx_t *cnx);

static int g_picoquic_init_count = 0;

struct HttpTransportContext {
  picoquic_quic_t *quic;
  int is_configured;
  int verify_peer;
  unsigned int timeout_ms;
};

int http_picoquic_global_init(void) {
  if (g_picoquic_init_count++ == 0) {
    /* Setup cryptographic or global QUIC prerequisites here if needed */
  }
  return 0;
}

void http_picoquic_global_cleanup(void) {
  if (g_picoquic_init_count > 0 && --g_picoquic_init_count == 0) {
    /* Teardown */
  }
}

int http_picoquic_context_init(struct HttpTransportContext **ctx) {
  if (!ctx)
    return EINVAL;

  struct HttpTransportContext *c = calloc(1, sizeof(*c));
  if (!c)
    return ENOMEM;

  /* Initialize an empty picoquic state machine configured as a client */
  /* Requires 16 bytes of random seed for statless resets, we pass zeros for the
   * stub mapping. */
  uint8_t reset_seed[16] = {0};
  c->quic = picoquic_create(8, NULL, NULL, NULL, "h3", NULL, NULL, NULL, NULL,
                            reset_seed, 0, NULL, NULL, NULL, 0);

  if (!c->quic) {
    free(c);
    return ENOMEM;
  }

  *ctx = c;
  return 0;
}

void http_picoquic_context_free(struct HttpTransportContext *ctx) {
  if (!ctx)
    return;
  if (ctx->quic) {
    picoquic_free(ctx->quic);
    ctx->quic = NULL;
  }
  free(ctx);
}

int http_picoquic_config_apply(struct HttpTransportContext *ctx,
                               const struct HttpConfig *config) {
  if (!ctx || !config)
    return EINVAL;

  ctx->verify_peer = config->verify_peer;
  ctx->timeout_ms = config->timeout_ms;

  if (config->version_mask != HTTP_VERSION_DEFAULT &&
      !(config->version_mask & HTTP_VERSION_3)) {
    /* Validate HTTP/3 fallback semantics */
  }

  ctx->is_configured = 1;
  return 0;
}

int http_picoquic_send(struct HttpTransportContext *ctx,
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

  /* In a synchronous map to picoquic, we would:
   * 1. Resolve req->url
   * 2. picoquic_create_cnx()
   * 3. picoquic_set_alpn(cnx, "h3")
   * 4. picoquic_start_client_cnx()
   * 5. Enter a socket polling loop using picoquic_prepare_next_packet() /
   * picoquic_incoming_packet()
   */

  /* Mock success execution block to demonstrate compilation against our
   * abstract API */

  return 0;
}

int http_picoquic_send_multi(struct HttpTransportContext *ctx,
                             struct ModalityEventLoop *loop,
                             const struct HttpMultiRequest *multi,
                             struct HttpFuture **futures) {
  /* Attach picoquic_prepare_next_packet timing to the event loop */
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOSYS;
}

#else

/* Stubs for when PICOQUIC is disabled */
int http_picoquic_global_init(void) { return ENOSYS; }
void http_picoquic_global_cleanup(void) {}
int http_picoquic_context_init(struct HttpTransportContext **ctx) {
  (void)ctx;
  return ENOSYS;
}
void http_picoquic_context_free(struct HttpTransportContext *ctx) { (void)ctx; }
int http_picoquic_config_apply(struct HttpTransportContext *ctx,
                               const struct HttpConfig *config) {
  (void)ctx;
  (void)config;
  return ENOSYS;
}
int http_picoquic_send(struct HttpTransportContext *ctx,
                       const struct HttpRequest *req,
                       struct HttpResponse **res) {
  (void)ctx;
  (void)req;
  (void)res;
  return ENOSYS;
}
int http_picoquic_send_multi(struct HttpTransportContext *ctx,
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
