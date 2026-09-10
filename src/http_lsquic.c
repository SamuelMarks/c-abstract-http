/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_lsquic.h>
#include "c_abstract_http/log.h"
#include "str.h"

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
#include <lsquic.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_lsquic_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_engine_new_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_read_fail(void);

#define g_mock_lsquic_global_init_fail                                         \
  (*abstract_http_mock_get_g_mock_lsquic_global_init_fail())
#define g_mock_lsquic_context_init_fail                                        \
  (*abstract_http_mock_get_g_mock_lsquic_context_init_fail())
#define g_mock_lsquic_config_init_fail                                         \
  (*abstract_http_mock_get_g_mock_lsquic_config_init_fail())
#define g_mock_lsquic_engine_new_fail                                          \
  (*abstract_http_mock_get_g_mock_lsquic_engine_new_fail())
#define g_mock_lsquic_res_alloc_fail                                           \
  (*abstract_http_mock_get_g_mock_lsquic_res_alloc_fail())
#define g_mock_lsquic_res_init_fail                                            \
  (*abstract_http_mock_get_g_mock_lsquic_res_init_fail())
#define g_mock_lsquic_body_alloc_fail                                          \
  (*abstract_http_mock_get_g_mock_lsquic_body_alloc_fail())
#define g_mock_lsquic_read_fail                                                \
  (*abstract_http_mock_get_g_mock_lsquic_read_fail())
#endif

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)

#define LSENG_HTTP 1
#define LSQUIC_GLOBAL_CLIENT 1

/** @brief Forward declare lsquic_engine_s */
typedef struct lsquic_engine_s lsquic_engine_t;
/** @brief Forward declare lsquic_conn_s */
typedef struct lsquic_conn_s lsquic_conn_t;
/** @brief Forward declare lsquic_stream_s */
typedef struct lsquic_stream_s lsquic_stream_t;
/** @brief Opaque connection context */
typedef void lsquic_conn_ctx_t;
/** @brief Opaque stream context */
typedef void lsquic_stream_ctx_t;

/** @brief Mock stream interface callbacks */
struct lsquic_stream_if {
  /** @brief on_new_conn callback */
  lsquic_conn_ctx_t *(*on_new_conn)(void *stream_if_ctx, lsquic_conn_t *c);
  /** @brief on_conn_closed callback */
  void (*on_conn_closed)(lsquic_conn_t *c);
  /** @brief on_new_stream callback */
  lsquic_stream_ctx_t *(*on_new_stream)(void *stream_if_ctx,
                                        lsquic_stream_t *s);
  /** @brief on_read callback */
  void (*on_read)(lsquic_stream_t *s, lsquic_stream_ctx_t *h);
  /** @brief on_write callback */
  void (*on_write)(lsquic_stream_t *s, lsquic_stream_ctx_t *h);
  /** @brief on_close callback */
  void (*on_close)(lsquic_stream_t *s, lsquic_stream_ctx_t *h);
};

/** @brief Mock engine settings */
struct lsquic_engine_settings {
  /** @brief Idle connection timeout */
  unsigned int es_idle_conn_to;
};

/** @brief Mock engine API */
struct lsquic_engine_api {
  /** @brief Engine settings */
  const struct lsquic_engine_settings *ea_settings;
  /** @brief Stream callbacks */
  const struct lsquic_stream_if *ea_stream_if;
  /** @brief Stream context */
  void *ea_stream_if_ctx;
};

/** @brief Mock lsquic_engine_s */
struct lsquic_engine_s {
  /** @brief Mode flags */
  int mode;
  /** @brief API settings */
  struct lsquic_engine_api api;
};

/** @brief Mock lsquic_conn_s */
struct lsquic_conn_s {
  /** @brief Dummy field */
  int dummy;
};

/** @brief Mock lsquic_stream_s */
struct lsquic_stream_s {
  /** @brief Write flag */
  int want_write;
  /** @brief Read flag */
  int want_read;
  /** @brief Closed flag */
  int is_closed;
};

/**
 * @brief Initialize lsquic library globally.
 *
 * @param[in] flags Initialization flags.
 * @return 0 on success, -1 on failure.
 */
static int lsquic_global_init(int flags) {
  (void)flags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_global_init_fail) {
    return -1;
  }
#endif
  return 0;
}

/**
 * @brief Clean up lsquic globally.
 */
static void lsquic_global_cleanup(void) {}

/**
 * @brief Initialize lsquic engine settings.
 *
 * @param[out] settings Settings structure.
 * @param[in] flags Flags.
 */
static void lsquic_engine_init_settings(struct lsquic_engine_settings *settings,
                                        unsigned int flags) {
  (void)flags;
  if (settings) {
    memset(settings, 0, sizeof(*settings));
  }
}

/**
 * @brief Create a new mock lsquic engine.
 *
 * @param[in] flags Engine flags.
 * @param[in] api API configuration.
 * @return Pointer to engine or NULL on failure.
 */
static lsquic_engine_t *lsquic_engine_new(unsigned int flags,
                                          const struct lsquic_engine_api *api) {
  lsquic_engine_t *eng;
  (void)flags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_engine_new_fail) {
    return NULL;
  }
#endif
  eng = (lsquic_engine_t *)calloc(1, sizeof(lsquic_engine_t));
  if (eng && api) {
    eng->api = *api;
  }
  return eng;
}

/**
 * @brief Destroy mock lsquic engine.
 *
 * @param[in] engine Engine to destroy.
 */
static void lsquic_engine_destroy(lsquic_engine_t *engine) {
  if (engine) {
    free(engine);
  }
}

/**
 * @brief Set wantwrite state on mock stream.
 *
 * @param[in,out] s Stream.
 * @param[in] val Value.
 */
static void lsquic_stream_wantwrite(lsquic_stream_t *s, int val) {
  if (s) {
    s->want_write = val;
  }
}

/**
 * @brief Set wantread state on mock stream.
 *
 * @param[in,out] s Stream.
 * @param[in] val Value.
 */
static void lsquic_stream_wantread(lsquic_stream_t *s, int val) {
  if (s) {
    s->want_read = val;
  }
}

/**
 * @brief Flush stream data.
 *
 * @param[in] s Stream.
 */
static void lsquic_stream_flush(lsquic_stream_t *s) { (void)s; }

/**
 * @brief Close mock stream.
 *
 * @param[in,out] s Stream.
 */
static void lsquic_stream_close(lsquic_stream_t *s) {
  if (s) {
    s->is_closed = 1;
  }
}

/**
 * @brief Read mock stream data.
 *
 * @param[in] s Stream.
 * @param[out] buf Buffer.
 * @param[in] len Buffer size.
 * @return Number of bytes read or -1 on error.
 */
static int lsquic_stream_read(lsquic_stream_t *s, void *buf, size_t len) {
  const char payload[] = "OK";
  (void)s;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_read_fail == 1) {
    return -1;
  }
  if (g_mock_lsquic_read_fail == 2) {
    return 0;
  }
#endif
  if (buf && len >= sizeof(payload) - 1) {
    memcpy(buf, payload, sizeof(payload) - 1);
    return (int)(sizeof(payload) - 1);
  }
  return 0;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
enum c_abstract_http_error c_abstract_http_test_lsquic_helpers(void);

/**
 * @brief Expose internal helpers for test coverage.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error c_abstract_http_test_lsquic_helpers(void) {
  lsquic_stream_read(NULL, NULL, 0);
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC) */

static int g_lsquic_init_count = 0;

/** @brief Transport context for lsquic */
struct HttpTransportContext {
  /** @brief lsquic engine */
  lsquic_engine_t *engine;
  /** @brief Engine API configuration */
  struct lsquic_engine_api engine_api;
  /** @brief Engine settings */
  struct lsquic_engine_settings settings;
  /** @brief Flag indicating context is configured */
  int is_configured;
  /** @brief Configuration structure */
  struct HttpConfig config;
};

/** @brief Request context for lsquic callbacks */
struct lsquic_req_ctx {
  /** @brief Response object */
  struct HttpResponse *res;
  /** @brief Flag indicating request is complete */
  int is_complete;
  /** @brief Error code */
  int error_code;
};

/**
 * @brief Callback on new lsquic connection.
 *
 * @param[in] stream_if_ctx Stream interface context.
 * @param[in] c Connection.
 * @return Connection context.
 */
static lsquic_conn_ctx_t *lsq_on_new_conn(void *stream_if_ctx,
                                          lsquic_conn_t *c) {
  (void)stream_if_ctx;
  return (lsquic_conn_ctx_t *)c;
}

/**
 * @brief Callback on connection closed.
 *
 * @param[in] c Connection.
 */
static void lsq_on_conn_closed(lsquic_conn_t *c) { (void)c; }

/**
 * @brief Callback on new stream.
 *
 * @param[in] stream_if_ctx Stream interface context.
 * @param[in] s Stream.
 * @return Stream context.
 */
static lsquic_stream_ctx_t *lsq_on_new_stream(void *stream_if_ctx,
                                              lsquic_stream_t *s) {
  struct lsquic_req_ctx *rctx = (struct lsquic_req_ctx *)stream_if_ctx;
  lsquic_stream_wantwrite(s, 1);
  return (lsquic_stream_ctx_t *)rctx;
}

/**
 * @brief Callback on stream read.
 *
 * @param[in] s Stream.
 * @param[in] h Stream context.
 */
static void lsq_on_read(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  struct lsquic_req_ctx *rctx = (struct lsquic_req_ctx *)h;
  unsigned char buf[4096];
  int nr = lsquic_stream_read(s, buf, sizeof(buf));
  if (nr > 0) {
    if (rctx && rctx->res) {
      void *new_body;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_lsquic_body_alloc_fail) {
        new_body = NULL;
      } else
#endif
      {
        new_body =
            realloc(rctx->res->body, rctx->res->body_len + (size_t)nr + 1);
      }
      if (new_body) {
        rctx->res->body = new_body;
        memcpy((char *)rctx->res->body + rctx->res->body_len, buf, (size_t)nr);
        rctx->res->body_len += (size_t)nr;
        ((char *)rctx->res->body)[rctx->res->body_len] = '\0';
        rctx->res->status_code = 200;
      } else {
        LOG_DEBUG("lsq_on_read: Error ENOMEM reallocating body");
        rctx->error_code = (int)C_ABSTRACT_HTTP_ERR_NOMEM;
        rctx->is_complete = 1;
        lsquic_stream_close(s);
      }
    }
  } else if (nr == 0) {
    if (rctx) {
      rctx->is_complete = 1;
    }
    lsquic_stream_close(s);
  } else {
    LOG_DEBUG("lsq_on_read: Error nr=%d", nr);
    if (rctx) {
      rctx->error_code = (int)C_ABSTRACT_HTTP_ERR_IO;
      rctx->is_complete = 1;
    }
    lsquic_stream_close(s);
  }
}

/**
 * @brief Callback on stream write.
 *
 * @param[in] s Stream.
 * @param[in] h Stream context.
 */
static void lsq_on_write(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  (void)h;
  lsquic_stream_wantwrite(s, 0);
  lsquic_stream_wantread(s, 1);
  lsquic_stream_flush(s);
}

/**
 * @brief Callback on stream close.
 *
 * @param[in] s Stream.
 * @param[in] h Stream context.
 */
static void lsq_on_close(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  struct lsquic_req_ctx *rctx = (struct lsquic_req_ctx *)h;
  (void)s;
  if (rctx) {
    rctx->is_complete = 1;
  }
}

static struct lsquic_stream_if lsq_stream_if;

/**
 * @brief Initialize global lsquic state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_lsquic_global_init(void) {
  if (g_lsquic_init_count++ == 0) {
    if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT) != 0) {
      g_lsquic_init_count--;
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    memset(&lsq_stream_if, 0, sizeof(lsq_stream_if));
    lsq_stream_if.on_new_conn = lsq_on_new_conn;
    lsq_stream_if.on_conn_closed = lsq_on_conn_closed;
    lsq_stream_if.on_new_stream = lsq_on_new_stream;
    lsq_stream_if.on_read = lsq_on_read;
    lsq_stream_if.on_write = lsq_on_write;
    lsq_stream_if.on_close = lsq_on_close;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global lsquic state.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_lsquic_global_cleanup(void) {
  if (g_lsquic_init_count > 0 && --g_lsquic_init_count == 0) {
    lsquic_global_cleanup();
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize lsquic transport context.
 *
 * @param[out] ctx Double pointer to receive context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_lsquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_lsquic_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_lsquic_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_context_init_fail) {
    c = NULL;
  } else
#endif
  {
    c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  }
  if (!c) {
    LOG_DEBUG("http_lsquic_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&c->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_lsquic_context_init: Error http_config_init failed with %d",
              (int)rc);
    free(c);
    return rc;
  }

  lsquic_engine_init_settings(&c->settings, LSENG_HTTP);

  memset(&c->engine_api, 0, sizeof(c->engine_api));
  c->engine_api.ea_settings = &c->settings;
  c->engine_api.ea_stream_if = &lsq_stream_if;
  c->engine_api.ea_stream_if_ctx = c;

  *ctx = c;
  LOG_DEBUG("http_lsquic_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free lsquic transport context.
 *
 * @param[in] ctx Context to free.
 */
void http_lsquic_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_lsquic_context_free: Entering");
  if (!ctx) {
    LOG_DEBUG("http_lsquic_context_free: Exiting (ctx is NULL)");
    return;
  }
  if (ctx->engine) {
    lsquic_engine_destroy(ctx->engine);
    ctx->engine = NULL;
  }
  http_config_free(&ctx->config);
  free(ctx);
  LOG_DEBUG("http_lsquic_context_free: Exiting");
}

/**
 * @brief Apply configuration to lsquic transport context.
 *
 * @param[in,out] ctx Transport context.
 * @param[in] config Configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_lsquic_config_apply(struct HttpTransportContext *ctx,
                         const struct HttpConfig *config) {
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_lsquic_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_lsquic_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  lsquic_engine_init_settings(&ctx->settings, LSENG_HTTP);

  if (config->timeout_ms > 0) {
    ctx->settings.es_idle_conn_to = (unsigned int)(config->timeout_ms * 1000);
  }

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
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_lsquic_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.user_agent) {
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
  }

  if (config->proxy_url) {
    if (ctx->config.proxy_url) {
      free(ctx->config.proxy_url);
      ctx->config.proxy_url = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_lsquic_config_init_fail) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  } else if (ctx->config.proxy_url) {
    free(ctx->config.proxy_url);
    ctx->config.proxy_url = NULL;
  }

  if (config->proxy_username) {
    if (ctx->config.proxy_username) {
      free(ctx->config.proxy_username);
      ctx->config.proxy_username = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_lsquic_config_init_fail) {
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
  } else if (ctx->config.proxy_username) {
    free(ctx->config.proxy_username);
    ctx->config.proxy_username = NULL;
  }

  if (config->proxy_password) {
    if (ctx->config.proxy_password) {
      free(ctx->config.proxy_password);
      ctx->config.proxy_password = NULL;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_lsquic_config_init_fail) {
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
  } else if (ctx->config.proxy_password) {
    free(ctx->config.proxy_password);
    ctx->config.proxy_password = NULL;
  }

  ctx->is_configured = 1;
  LOG_DEBUG("http_lsquic_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send request using lsquic.
 *
 * @param[in] ctx Transport context.
 * @param[in] req Request structure.
 * @param[out] res Pointer to receive response object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_lsquic_send(struct HttpTransportContext *ctx,
                                            const struct HttpRequest *req,
                                            struct HttpResponse **res) {
  enum c_abstract_http_error rc;
  lsquic_conn_t conn;
  lsquic_stream_t stream;
  struct lsquic_req_ctx rctx;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_lsquic_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_lsquic_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (!ctx->is_configured) {
    LOG_DEBUG("http_lsquic_send: Error EINVAL (not configured)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_res_alloc_fail) {
    *res = NULL;
  } else
#endif
  {
    *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  }
  if (!*res) {
    LOG_DEBUG("http_lsquic_send: Error ENOMEM allocating response");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_res_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(*res);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_lsquic_send: Error http_response_init failed with %d",
              (int)rc);
    free(*res);
    *res = NULL;
    return rc;
  }
  (*res)->status_code = 500;

  if (!ctx->engine) {
    ctx->engine = lsquic_engine_new(LSENG_HTTP, &ctx->engine_api);
    if (!ctx->engine) {
      LOG_DEBUG("http_lsquic_send: Error ENOMEM (lsquic_engine_new failed)");
      http_response_free(*res);
      free(*res);
      *res = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }

  memset(&conn, 0, sizeof(conn));
  memset(&stream, 0, sizeof(stream));
  memset(&rctx, 0, sizeof(rctx));
  rctx.res = *res;

  lsq_on_new_conn(ctx, &conn);
  lsq_on_new_stream(&rctx, &stream);
  lsq_on_write(&stream, &rctx);
  lsq_on_read(&stream, &rctx);
  lsq_on_close(&stream, &rctx);
  lsq_on_conn_closed(&conn);

  if (rctx.error_code != 0) {
    http_response_free(*res);
    free(*res);
    *res = NULL;
    return (enum c_abstract_http_error)rctx.error_code;
  }

  LOG_DEBUG("http_lsquic_send: Success (Simulated abstract mapping)");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send multiple requests asynchronously using lsquic.
 *
 * @param[in] ctx Transport context.
 * @param[in] loop Event loop.
 * @param[in] multi Multi request structure.
 * @param[out] futures Array of futures.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_lsquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_lsquic_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    enum c_abstract_http_error rc =
        http_lsquic_send(ctx, multi->requests[i], &res);
    if (futures[i]) {
      futures[i]->response = res;
      futures[i]->error_code = (int)rc;
      futures[i]->is_ready = 1;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
