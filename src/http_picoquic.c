
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_picoquic.h>
#include "c_abstract_http/log.h"
#include "functions/parse/str.h"
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
  struct HttpConfig config;
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
  struct HttpTransportContext *c;
  int rc;
  LOG_DEBUG("http_picoquic_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_picoquic_context_init: Error EINVAL");
    return EINVAL;
  }

  c = calloc(1, sizeof(*c));
  if (!c) {
    LOG_DEBUG("http_picoquic_context_init: Error ENOMEM");
    return ENOMEM;
  }

  rc = http_config_init(&c->config);
  if (rc != 0) {
    LOG_DEBUG(
        "http_picoquic_context_init: Error http_config_init failed with %d",
        rc);
    free(c);
    return rc;
  }

  /* Initialize an empty picoquic state machine configured as a client */
  /* Requires 16 bytes of random seed for statless resets, we pass zeros for the
   * mock mapping. */
  uint8_t reset_seed[16] = {0};
  c->quic = picoquic_create(8, NULL, NULL, NULL, "h3", NULL, NULL, NULL, NULL,
                            reset_seed, 0, NULL, NULL, NULL, 0);

  if (!c->quic) {
    LOG_DEBUG("http_picoquic_context_init: Error picoquic_create failed");
    http_config_free(&c->config);
    free(c);
    return ENOMEM;
  }

  *ctx = c;
  LOG_DEBUG("http_picoquic_context_init: Success");
  return 0;
}

void http_picoquic_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_picoquic_context_free: Entering");
  if (!ctx) {
    LOG_DEBUG("http_picoquic_context_free: Exiting early (ctx is NULL)");
    return;
  }
  if (ctx->quic) {
    picoquic_free(ctx->quic);
    ctx->quic = NULL;
  }
  http_config_free(&ctx->config);
  free(ctx);
  LOG_DEBUG("http_picoquic_context_free: Exiting");
}

int http_picoquic_config_apply(struct HttpTransportContext *ctx,
                               const struct HttpConfig *config) {
  LOG_DEBUG("http_picoquic_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_picoquic_config_apply: Error EINVAL");
    return EINVAL;
  }

  ctx->verify_peer = config->verify_peer;
  ctx->timeout_ms = config->timeout_ms;

  if (config->version_mask != HTTP_VERSION_DEFAULT &&
      !(config->version_mask & HTTP_VERSION_3)) {
    /* Validate HTTP/3 fallback semantics */
  }

  ctx->config = *config;
  ctx->is_configured = 1;
  LOG_DEBUG("http_picoquic_config_apply: Success");
  return 0;
}

int http_picoquic_send(struct HttpTransportContext *ctx,
                       const struct HttpRequest *req,
                       struct HttpResponse **res) {
  int rc;
  LOG_DEBUG("http_picoquic_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_picoquic_send: Error EINVAL");
    return EINVAL;
  }

  if (!ctx->is_configured) {
    LOG_DEBUG("http_picoquic_send: Error EINVAL (not configured)");
    return EINVAL;
  }

  *res = calloc(1, sizeof(**res));
  if (!*res) {
    LOG_DEBUG("http_picoquic_send: Error ENOMEM");
    return ENOMEM;
  }

  rc = http_response_init(*res);
  if (rc != 0) {
    LOG_DEBUG("http_picoquic_send: Error http_response_init failed with %d",
              rc);
    free(*res);
    *res = NULL;
    return rc;
  }
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

  LOG_DEBUG("http_picoquic_send: Success (simulated)");
  return 0;
}

int http_picoquic_send_multi(struct HttpTransportContext *ctx,
                             struct ModalityEventLoop *loop,
                             const struct HttpMultiRequest *multi,
                             struct HttpFuture **futures) {
  size_t i;
  /* Attach picoquic_prepare_next_packet timing to the event loop */
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_picoquic_send_multi: Error EINVAL");
    return EINVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    int rc = http_picoquic_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }
  return 0;
}

#endif
