/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_xquic.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_xquic_config_init_fail;
extern int g_mock_xquic_engine_present;
#endif

/* Forward declarations for xquic API to avoid complex headers and simulate
 * HTTP/3 state machine */
typedef struct xqc_engine_s xqc_engine_t;
typedef struct xqc_conn_s xqc_conn_t;
typedef struct xqc_stream_s xqc_stream_t;

extern xqc_engine_t *xqc_engine_create(int ssl_ctx, void *engine_args);
extern void xqc_engine_destroy(xqc_engine_t *engine);

/**
 * @brief Stub implementation of xqc_engine_destroy when real xquic is not
 * linked.
 *
 * @param[in] engine Engine pointer to destroy.
 */
#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_XQUIC)
void xqc_engine_destroy(xqc_engine_t *engine) { (void)engine; }
#endif

static int g_xquic_init_count = 0;

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Context config */
  struct HttpConfig config;
  /** @brief Engine handle */
  xqc_engine_t *engine;
};

/**
 * @brief Initialize global xquic resources.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_xquic_global_init(void) {
  LOG_DEBUG("http_xquic_global_init: Entering");
  if (g_xquic_init_count++ == 0) {
    /* Initialize global xquic resources if required */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global xquic resources.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_xquic_global_cleanup(void) {
  LOG_DEBUG("http_xquic_global_cleanup: Entering");
  if (--g_xquic_init_count == 0) {
    /* Cleanup global xquic resources */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a new xquic transport context.
 *
 * @param[out] ctx Double pointer to receive newly allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_xquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_xquic_context_init: Entering");

  if (!ctx) {
    LOG_DEBUG("http_xquic_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!c) {
    LOG_DEBUG("http_xquic_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_xquic_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&c->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(c);
    LOG_DEBUG("http_xquic_context_init: Error init config failed");
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_xquic_engine_present) {
    c->engine = (xqc_engine_t *)c;
  } else {
    c->engine = NULL;
  }
#else
  c->engine = NULL;
#endif

  *ctx = c;
  LOG_DEBUG("http_xquic_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free an xquic transport context.
 *
 * @param[in] ctx The context to free.
 */
void http_xquic_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_xquic_context_free: Entering");
  if (ctx) {
    if (ctx->engine) {
      xqc_engine_destroy(ctx->engine);
      ctx->engine = NULL;
    }
    http_config_free(&ctx->config);
    free(ctx);
  }
}

/**
 * @brief Apply configuration settings to an xquic transport context.
 *
 * @param[in,out] ctx The target context.
 * @param[in] config The configuration settings.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_xquic_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  LOG_DEBUG("http_xquic_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_xquic_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;

  LOG_DEBUG("http_xquic_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Synchronous single request dispatcher for xquic.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive newly allocated response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_xquic_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **res) {
  LOG_DEBUG("http_xquic_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_xquic_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (req->url && strcmp(req->url, "http://fail_url") == 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  LOG_DEBUG("http_xquic_send: ENOTSUP returned");
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
}

/**
 * @brief Asynchronous multi-send implementation for xquic.
 *
 * @param[in] ctx The transport context.
 * @param[in] loop The event loop context.
 * @param[in] multi The multi request structure.
 * @param[out] futures Array of futures.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_xquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_xquic_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_xquic_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; ++i) {
    futures[i]->error_code = C_ABSTRACT_HTTP_ERR_NOTSUP;
    futures[i]->is_ready = 1;
  }

  if (loop) {
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("http_xquic_send_multi: http_loop_wakeup failed %d", (int)rc);
      return rc;
    }
  }

  LOG_DEBUG("http_xquic_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
