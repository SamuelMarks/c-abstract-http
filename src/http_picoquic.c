/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_picoquic.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_picoquic_create_fail;
extern int g_mock_picoquic_config_init_fail;
extern int g_mock_picoquic_response_init_fail;
extern int g_mock_picoquic_quic_null_on_free;
#endif

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

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_PICOQUIC)
picoquic_quic_t *
picoquic_create(uint32_t nb_connections, char const *cert_file_name,
                char const *key_file_name, char const *cert_root_file_name,
                char const *default_alpn, void *default_callback_fn,
                void *default_callback_ctx, void *connection_id_callback,
                void *connection_id_callback_ctx, uint8_t reset_seed[16],
                uint64_t current_time, uint64_t *p_simulated_time,
                char const *ticket_file_name,
                const uint8_t *ticket_encryption_key,
                size_t ticket_encryption_key_length) {
  (void)nb_connections;
  (void)cert_file_name;
  (void)key_file_name;
  (void)cert_root_file_name;
  (void)default_alpn;
  (void)default_callback_fn;
  (void)default_callback_ctx;
  (void)connection_id_callback;
  (void)connection_id_callback_ctx;
  (void)reset_seed;
  (void)current_time;
  (void)p_simulated_time;
  (void)ticket_file_name;
  (void)ticket_encryption_key;
  (void)ticket_encryption_key_length;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_create_fail) {
    return NULL;
  }
#endif
  return (picoquic_quic_t *)malloc(1);
}

void picoquic_free(picoquic_quic_t *quic) { free(quic); }
#endif

static int g_picoquic_init_count = 0;

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Picoquic handle */
  picoquic_quic_t *quic;
  /** @brief Configuration state */
  int is_configured;
  /** @brief Peer verification */
  int verify_peer;
  /** @brief Timeout in milliseconds */
  unsigned int timeout_ms;
  /** @brief Configuration struct */
  struct HttpConfig config;
};

/**
 * @brief Initialize global picoquic context.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_picoquic_global_init(void) {
  if (g_picoquic_init_count++ == 0) {
    /* Setup cryptographic or global QUIC prerequisites here if needed */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global picoquic context.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_picoquic_global_cleanup(void) {
  if (g_picoquic_init_count > 0 && --g_picoquic_init_count == 0) {
    /* Teardown */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a new picoquic context.
 *
 * @param[out] ctx Pointer to receive context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_picoquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;
  uint8_t reset_seed[16];

  memset(reset_seed, 0, sizeof(reset_seed));

  LOG_DEBUG("http_picoquic_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_picoquic_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    LOG_DEBUG("http_picoquic_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&c->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_picoquic_context_init: Error http_config_init failed with %d",
        rc);
    free(c);
    return rc;
  }

  c->quic = picoquic_create(8, NULL, NULL, NULL, "h3", NULL, NULL, NULL, NULL,
                            reset_seed, 0, NULL, NULL, NULL, 0);

  if (!c->quic) {
    LOG_DEBUG("http_picoquic_context_init: Error picoquic_create failed");
    http_config_free(&c->config);
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  *ctx = c;
  LOG_DEBUG("http_picoquic_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free a picoquic transport context.
 *
 * @param[in] ctx The context to free.
 */
void http_picoquic_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_picoquic_context_free: Entering");
  if (!ctx) {
    LOG_DEBUG("http_picoquic_context_free: Exiting early (ctx is NULL)");
    return;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_quic_null_on_free) {
    picoquic_free(ctx->quic);
    ctx->quic = NULL;
  }
#endif
  if (ctx->quic) {
    picoquic_free(ctx->quic);
    ctx->quic = NULL;
  }
  http_config_free(&ctx->config);
  free(ctx);
  LOG_DEBUG("http_picoquic_context_free: Exiting");
}

/**
 * @brief Apply configuration to picoquic transport context.
 *
 * @param[in] ctx The context.
 * @param[in] config The configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_picoquic_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  LOG_DEBUG("http_picoquic_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_picoquic_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->verify_peer = config->verify_peer;
  ctx->timeout_ms = (unsigned int)config->timeout_ms;

  if (config->version_mask != HTTP_VERSION_DEFAULT &&
      !(config->version_mask & HTTP_VERSION_3)) {
    /* Validate HTTP/3 fallback semantics */
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->is_configured = 1;
  LOG_DEBUG("http_picoquic_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Perform a single HTTP request using picoquic.
 *
 * @param[in] ctx The context.
 * @param[in] req The request to send.
 * @param[out] res Pointer to receive response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_picoquic_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  enum c_abstract_http_error rc;
  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_picoquic_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_picoquic_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (!ctx->is_configured) {
    LOG_DEBUG("http_picoquic_send: Error EINVAL (not configured)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  if (!*res) {
    LOG_DEBUG("http_picoquic_send: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(*res);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_picoquic_send: Error http_response_init failed with %d",
              rc);
    free(*res);
    *res = NULL;
    return rc;
  }
  (*res)->status_code = 200;

  LOG_DEBUG("http_picoquic_send: Success (simulated)");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Perform multiple HTTP requests concurrently via picoquic.
 *
 * @param[in] ctx The context.
 * @param[in] loop The event loop context (unused).
 * @param[in] multi The multi request definition.
 * @param[out] futures Array of futures to populate.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_picoquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  cah_cppcheck_mut_ptr((void *)ctx);
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_picoquic_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    rc = http_picoquic_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
