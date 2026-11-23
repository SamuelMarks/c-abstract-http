/**
 * @file http_libuv.c
 * @brief libuv TCP transport backend implementation of the Abstract Network
 * Interface (ANI).
 */

/* clang-format off */
#if !defined(_WIN32)
#ifndef _DEFAULT_SOURCE
/** @brief Enable POSIX/BSD system extensions */
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
/** @brief Target POSIX.1-2008 API level */
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libuv.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "crypto_utils.h"

#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
#if defined(C_ABSTRACT_HTTP_USE_WOLFSSL)
#include <wolfssl/options.h>
#include <wolfssl/openssl/ssl.h>
#include <wolfssl/openssl/x509.h>
#define CAH_UV_HAVE_OSSL_BIO 1
#elif defined(C_ABSTRACT_HTTP_USE_OPENSSL) || defined(C_ABSTRACT_HTTP_USE_LIBRESSL) || defined(C_ABSTRACT_HTTP_USE_BORINGSSL)
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#define CAH_UV_HAVE_OSSL_BIO 1
#elif defined(C_ABSTRACT_HTTP_USE_MBEDTLS)
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#define CAH_UV_HAVE_MBED_BIO 1
#endif
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
/** @brief Signed size type typedef for MSVC */
typedef SSIZE_T ssize_t;
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_libuv.h"
#else
#include <uv.h>
#endif
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
};

/** @brief Reference count of global libuv initializations */
static int libuv_global_init_count = 0;

/**
 * @brief Initialize global libuv environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t http_libuv_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  libuv_global_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up global libuv environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD c_abstract_http_error_t http_libuv_global_cleanup(void) {
  if (libuv_global_init_count > 0) {
    libuv_global_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Create a new libuv-backed transport context. */
NO_DISCARD c_abstract_http_error_t
http_libuv_context_init(struct HttpTransportContext **ctx) {
  c_abstract_http_error_t rc;
  LOG_DEBUG("http_libuv_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_libuv_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_context_init_fail) {
    *ctx = NULL;
  } else
#endif
  {
    *ctx = (struct HttpTransportContext *)malloc(
        sizeof(struct HttpTransportContext));
  }
  if (!*ctx) {
    LOG_DEBUG("http_libuv_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memset(*ctx, 0, sizeof(struct HttpTransportContext));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_config_init_fail) {
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libuv_context_init: Error http_config_init failed with %d",
              (int)rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_libuv_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Free the transport context. */
NO_DISCARD c_abstract_http_error_t
http_libuv_context_free(struct HttpTransportContext *ctx) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  LOG_DEBUG("http_libuv_context_free: Entering");
  if (ctx) {
    rc = http_config_free(&ctx->config);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return rc;
    }
    free(ctx);
  }
  LOG_DEBUG("http_libuv_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration to the transport context. */
NO_DISCARD c_abstract_http_error_t http_libuv_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  c_abstract_http_error_t rc;
  LOG_DEBUG("http_libuv_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_libuv_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->tls_version_mask != 0 &&
      (config->tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  if ((config->client_cert_path && !config->client_key_path) ||
      (!config->client_cert_path && config->client_key_path) ||
      (config->ca_buffer_len > 0 && !config->ca_buffer)) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
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

  free(ctx->config.ca_path);
  ctx->config.ca_path = NULL;
  if (config->ca_path) {
    rc = c_abstract_http_strdup(config->ca_path, &ctx->config.ca_path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  free(ctx->config.client_cert_path);
  ctx->config.client_cert_path = NULL;
  if (config->client_cert_path) {
    rc = c_abstract_http_strdup(config->client_cert_path,
                                &ctx->config.client_cert_path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  free(ctx->config.client_key_path);
  ctx->config.client_key_path = NULL;
  if (config->client_key_path) {
    rc = c_abstract_http_strdup(config->client_key_path,
                                &ctx->config.client_key_path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  free(ctx->config.cert_pin_sha256);
  ctx->config.cert_pin_sha256 = NULL;
  if (config->cert_pin_sha256) {
    rc = c_abstract_http_strdup(config->cert_pin_sha256,
                                &ctx->config.cert_pin_sha256);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  free(ctx->config.ca_buffer);
  ctx->config.ca_buffer = NULL;
  ctx->config.ca_buffer_len = 0;
  if (config->ca_buffer && config->ca_buffer_len > 0) {
    ctx->config.ca_buffer = (unsigned char *)malloc(config->ca_buffer_len);
    if (!ctx->config.ca_buffer) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(ctx->config.ca_buffer, config->ca_buffer, config->ca_buffer_len);
    ctx->config.ca_buffer_len = config->ca_buffer_len;
  }

  free(ctx->config.user_agent);
  ctx->config.user_agent = NULL;
  if (config->user_agent) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_url);
  ctx->config.proxy_url = NULL;
  if (config->proxy_url) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->proxy_url, &ctx->config.proxy_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_username);
  ctx->config.proxy_username = NULL;
  if (config->proxy_username) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->proxy_username,
                                &ctx->config.proxy_username);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  free(ctx->config.proxy_password);
  ctx->config.proxy_password = NULL;
  if (config->proxy_password) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_config_init_fail) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#endif
    rc = c_abstract_http_strdup(config->proxy_password,
                                &ctx->config.proxy_password);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  LOG_DEBUG("http_libuv_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert HttpMethod enum to string.
 *
 * @param[in] method HTTP method enum.
 * @param[out] out_str Pointer to receive string constant.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t get_method_str(enum HttpMethod method,
                                                         const char **out_str) {
  if (!out_str) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch ((int)method) {
  case HTTP_GET:
    *out_str = "GET";
    break;
  case HTTP_POST:
    *out_str = "POST";
    break;
  case HTTP_PUT:
    *out_str = "PUT";
    break;
  case HTTP_DELETE:
    *out_str = "DELETE";
    break;
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  case HTTP_HEAD:
    *out_str = "HEAD";
    break;
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out_str = "TRACE";
    break;
  case HTTP_QUERY:
    *out_str = "QUERY";
    break;
  case HTTP_CONNECT:
    *out_str = "CONNECT";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Expose get_method_str for testing */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_libuv_method_str(
    enum HttpMethod method, const char **out_str);
/** @brief Expose libuv helpers for testing */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_libuv_helpers(void);

/**
 * @brief Expose get_method_str for testing.
 *
 * @param[in] method Method enum.
 * @param[out] out_str Output string.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_libuv_method_str(
    enum HttpMethod method, const char **out_str) {
  return get_method_str(method, out_str);
}
#endif

/** @brief Internal struct libuv_state */
struct libuv_state {
  /** @brief Event loop */
  uv_loop_t loop;
  /** @brief Pointer to active loop (&loop or shared batch loop) */
  uv_loop_t *active_loop;
  /** @brief Non-zero when single-request owns loop stop */
  int stop_loop_on_done;
  /** @brief TCP socket */
  uv_tcp_t socket;
  /** @brief Connect request */
  uv_connect_t connect_req;
  /** @brief Resolver request */
  uv_getaddrinfo_t resolver;
  /** @brief Timer handle */
  uv_timer_t timer;
  /** @brief Write request */
  uv_write_t write_req;

  /** @brief Transport context */
  struct HttpTransportContext *ctx;
  /** @brief Request pointer */
  const struct HttpRequest *req;
  /** @brief Response double pointer */
  struct HttpResponse **res;

  /** @brief Request buffer */
  char *req_buf;
  /** @brief Request buffer length */
  size_t req_len;
  /** @brief Request write position */
  size_t req_pos;

  /** @brief Response buffer */
  char *res_buf;
  /** @brief Response buffer length */
  size_t res_len;
  /** @brief Response buffer capacity */
  size_t res_cap;
  /** @brief Byte length of parsed HTTP header block */
  size_t hdr_len;

  /** @brief Flag indicating headers have been parsed */
  int headers_parsed;
  /** @brief Flag indicating Transfer-Encoding: chunked */
  int is_chunked;
  /** @brief Flag indicating HTTPS TLS BIO mode */
  int is_https;
  /** @brief Error code */
  int error_code;
  /** @brief Done flag */
  int done;
  /** @brief Flag indicating socket is initialized */
  int has_socket;
  /** @brief Flag indicating timer is initialized */
  int has_timer;
};

/**
 * @brief Handle closing callback.
 *
 * @param[in] handle Handle.
 */
static void libuv_on_close(uv_handle_t *handle) {
  if (handle) {
  }
}

/**
 * @brief Execute an enum-returning helper inside a void libuv callback.
 * @param expr Expression returning c_abstract_http_error_t.
 */
#define LIBUV_VOID_CB_DISPATCH(expr)                                           \
  do {                                                                         \
    c_abstract_http_error_t _uv_rc = (expr);                                   \
    if (_uv_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("libuv_cb: impl returned %d", (int)_uv_rc);                    \
    }                                                                          \
  } while (0)

/**
 * @brief Finish libuv operation and clean up handles.
 *
 * @param[in,out] state State pointer.
 * @param[in] error Error code.
 * @return Error enum matching error.
 */
NO_DISCARD static c_abstract_http_error_t
libuv_finish(struct libuv_state *state, c_abstract_http_error_t error) {
  if (state->done) {
    return error;
  }
  state->done = 1;
  state->error_code = (int)error;
  if (state->stop_loop_on_done || !state->active_loop) {
    uv_stop(state->active_loop ? state->active_loop : &state->loop);
  }

  LOG_DEBUG("libuv_finish: Finished with error=%d", (int)error);

  if (state->has_timer) {
    uv_timer_stop(&state->timer);
    uv_close((uv_handle_t *)&state->timer, libuv_on_close);
    state->has_timer = 0;
  }

  if (state->has_socket) {
    uv_close((uv_handle_t *)&state->socket, libuv_on_close);
    state->has_socket = 0;
  }
  return error;
}

/**
 * @brief Timeout handler for libuv request.
 *
 * @param[in] handle Timer handle.
 */
static void libuv_on_timeout(uv_timer_t *handle) {
  struct libuv_state *state = (struct libuv_state *)handle->data;
  LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_TIMEOUT));
}

/**
 * @brief Memory allocation callback for incoming libuv data.
 *
 * @param[in] handle Handle.
 * @param[in] suggested_size Suggested allocation size.
 * @param[out] buf Buffer structure to populate.
 */
static void libuv_alloc_cb(uv_handle_t *handle, size_t suggested_size,
                           uv_buf_t *buf) {
  struct libuv_state *state = (struct libuv_state *)handle->data;
  size_t avail;
  if (suggested_size) {
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_alloc_fail == 1 ||
      (g_mock_libuv_alloc_fail == 2 && state->res_len > 0)) {
    buf->base = NULL;
    buf->len = 0;
    return;
  }
#endif

  avail = state->res_cap - state->res_len;

  if (avail < 4096) {
    size_t new_cap = state->res_cap == 0 ? 8192 : state->res_cap * 2;
    char *new_buf;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_alloc_fail == 3) {
      new_buf = NULL;
    } else
#endif
    {
      new_buf = (char *)realloc(state->res_buf, new_cap);
    }
    if (!new_buf) {
      LOG_DEBUG("libuv_alloc_cb: Error ENOMEM reallocating buffer");
      buf->base = NULL;
      buf->len = 0;
      return;
    }
    state->res_buf = new_buf;
    state->res_cap = new_cap;
    avail = state->res_cap - state->res_len;
  }

  buf->base = state->res_buf + state->res_len;
  buf->len = (ULONG)avail;
}

/**
 * @brief Parse response headers from buffer.
 *
 * @param[in,out] state State pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
parse_headers(struct libuv_state *state) {
  struct HttpResponse *r;
  char *p;
  const char *end;
  c_abstract_http_error_t rc;

  if (!state) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (state->headers_parsed) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!state->res_buf) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  p = strstr(state->res_buf, "\r\n\r\n");
  if (!p) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  state->headers_parsed = 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_res_alloc_fail) {
    *state->res = NULL;
  } else
#endif
  {
    *state->res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  }
  if (!*state->res) {
    LOG_DEBUG("parse_headers: Error ENOMEM");
    return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  r = *state->res;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_res_init_fail) {
    free(*state->res);
    *state->res = NULL;
    return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
#endif
  rc = http_response_init(r);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("parse_headers: Error http_response_init failed with %d",
              (int)rc);
    free(*state->res);
    *state->res = NULL;
    return libuv_finish(state, rc);
  }

  /* Parse status line */
  {
    const char *space = strchr(state->res_buf, ' ');
    if (space) {
      r->status_code = atoi(space + 1);
    }
  }

  /* Parse headers */
  p = strchr(state->res_buf, '\n') + 1;

  end = strstr(state->res_buf, "\r\n\r\n");
  while (p < end) {
    char *line_end = strchr(p, '\r');
    char *colon;
    *line_end = '\0';
    colon = strchr(p, ':');
    if (colon) {
      char *val = colon + 1;
      c_abstract_http_error_t add_rc;
      *colon = '\0';
      while (*val == ' ' || *val == '\t') {
        val++;
      }
      if ((strcmp(p, "Transfer-Encoding") == 0 ||
           strcmp(p, "transfer-encoding") == 0) &&
          (strstr(val, "chunked") != NULL || strstr(val, "CHUNKED") != NULL)) {
        state->is_chunked = 1;
      }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_headers_realloc_fail) {
        LOG_DEBUG("parse_headers: Mock http_headers_add failure");
      } else
#endif
      {
        add_rc = http_headers_add(&r->headers, p, val);
        if (add_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("parse_headers: Error http_headers_add failed with %d",
                    (int)add_rc);
          return libuv_finish(state, add_rc);
        }
      }
    }
    p = line_end + 2;
  }

  {
    size_t hdr_len = (size_t)(end - state->res_buf) + 4;
    size_t body_len = state->res_len - hdr_len;
    state->hdr_len = hdr_len;

    if (state->is_chunked) {
      return C_ABSTRACT_HTTP_SUCCESS;
    }

    if (state->req->on_chunk) {
      if (body_len > 0) {
        c_abstract_http_error_t chunk_rc;
        chunk_rc = state->req->on_chunk(state->req->on_chunk_user_data,
                                        state->res_buf + hdr_len, body_len);
        if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("parse_headers: Error on_chunk failed %d", (int)chunk_rc);
          LIBUV_VOID_CB_DISPATCH(
              libuv_finish(state, C_ABSTRACT_HTTP_ERR_CANCELED));
          return chunk_rc;
        }
      }
      state->res_len = hdr_len;
    } else {
      r->body_len = body_len;
      if (body_len > 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_libuv_body_alloc_fail) {
          r->body = NULL;
        } else
#endif
        {
          r->body = (char *)malloc(body_len + 1);
        }
        if (!r->body) {
          LOG_DEBUG("parse_headers: Error ENOMEM copying body");
          return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
        }
        memcpy(r->body, state->res_buf + hdr_len, body_len);
        ((char *)r->body)[body_len] = '\0';
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD static c_abstract_http_error_t
parse_url(const char *url, char **host, int *port, char **path);
/**
 * @brief libuv stream read completion callback.
 * @param stream Active uv_stream_t handle.
 * @param nread Number of bytes read or negative UV error code.
 * @param buf Buffer descriptor.
 */
static void libuv_on_read(uv_stream_t *stream, ssize_t nread,
                          const uv_buf_t *buf);
/**
 * @brief Run mock libuv event loop.
 * @param loop Mock uv_loop_t pointer.
 * @param mode Run mode.
 * @return 0 on completion.
 */
static int uv_run(uv_loop_t *loop, int mode);

/**
 * @brief Dummy streaming chunk callback returning success for libuv tests.
 * @param[in] user_data User context pointer.
 * @param[in] chunk Chunk payload bytes.
 * @param[in] chunk_len Chunk byte length.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
test_helper_chunk_cb(void *user_data, const void *chunk, size_t chunk_len) {
  if (user_data || chunk || chunk_len) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Dummy streaming chunk callback returning error for libuv tests.
 * @param[in] user_data User context pointer.
 * @param[in] chunk Chunk payload bytes.
 * @param[in] chunk_len Chunk byte length.
 * @return Error enum simulating cancellation.
 */
NO_DISCARD static c_abstract_http_error_t
test_helper_chunk_cb_err(void *user_data, const void *chunk, size_t chunk_len) {
  if (user_data || chunk || chunk_len) {
  }
  return (c_abstract_http_error_t)ECANCELED;
}

/** @brief Step selector for `c_abstract_http_test_libuv_helpers` */
static int g_mock_libuv_helper_step = 0;

/**
 * @brief Set test helper step mode for `c_abstract_http_test_libuv_helpers`.
 * @param step Step selector (`0` = default suite, `1..10` = targeted checks).
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_set_libuv_helper_step(int step);

NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_set_libuv_helper_step(int step) {
  g_mock_libuv_helper_step = step;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
uv_resolve_redirect_url(const char *cur_url, const char *loc, char **out_url);

/**
 * @brief Free optional response and scratch buffer at end of a helper case.
 * @param test_res Response pointer or NULL.
 * @param buf Heap buffer or NULL.
 * @param primary_rc Primary status to return when cleanup succeeds.
 * @return Cleanup error or `primary_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
uv_helper_free_res_buf(struct HttpResponse *test_res, char *buf,
                       c_abstract_http_error_t primary_rc) {
  free(buf);
  if (test_res) {
    c_abstract_http_error_t frc = http_response_free(test_res);
    free(test_res);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return frc;
    }
  }
  return primary_rc;
}

/**
 * @brief Free response via double pointer and percolate primary error.
 * @param res_ptr Address of response pointer to free and zero.
 * @param primary_rc Status to percolate.
 * @return Cleanup error or `primary_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
uv_helper_free_res_ptr(struct HttpResponse **res_ptr,
                       c_abstract_http_error_t primary_rc) {
  struct HttpResponse *r = *res_ptr;
  *res_ptr = NULL;
  return uv_helper_free_res_buf(r, NULL, primary_rc);
}

/**
 * @brief Reset an integer fault-injection flag and return status.
 * @param flag Pointer to integer flag to zero.
 * @param rc Status to percolate.
 * @return `rc`.
 */
NO_DISCARD static c_abstract_http_error_t
uv_helper_reset_ret(int *flag, c_abstract_http_error_t rc) {
  *flag = 0;
  return rc;
}

/**
 * @brief Test libuv helper functions and edge cases.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_libuv_helpers(void) {
  c_abstract_http_error_t rc;
  uv_timer_t timer;
  struct libuv_state state;
  struct HttpRequest req;
  struct HttpResponse *test_res = NULL;
  char *h = NULL;
  int p = 0;
  char *path = NULL;

  if (g_mock_libuv_helper_step == 1) {
    return parse_url(NULL, &h, &p, &path);
  }
  if (g_mock_libuv_helper_step == 2) {
    return parse_url("http://127.0.0.1", NULL, &p, &path);
  }
  if (g_mock_libuv_helper_step == 3) {
    return parse_url("http://127.0.0.1", &h, NULL, &path);
  }
  if (g_mock_libuv_helper_step == 4) {
    return parse_url("http://127.0.0.1", &h, &p, NULL);
  }
  if (g_mock_libuv_helper_step == 5) {
    return parse_headers(NULL);
  }
  if (g_mock_libuv_helper_step == 6) {
    g_mock_libuv_req_buf_alloc_fail = 3;
    return uv_helper_reset_ret(
        &g_mock_libuv_req_buf_alloc_fail,
        parse_url("http://127.0.0.1/path", &h, &p, &path));
  }
  if (g_mock_libuv_helper_step == 7) {
    g_mock_libuv_req_buf_alloc_fail = 4;
    return uv_helper_reset_ret(&g_mock_libuv_req_buf_alloc_fail,
                               parse_url("http://127.0.0.1", &h, &p, &path));
  }
  if (g_mock_libuv_helper_step == 8) {
    return uv_resolve_redirect_url(NULL, "/rel", &h);
  }
  if (g_mock_libuv_helper_step == 9) {
    char buf1[] = "HTTP/1.1 200 OK";
    memset(&state, 0, sizeof(state));
    memset(&req, 0, sizeof(req));
    state.req = &req;
    state.res_buf = buf1;
    state.res_len = strlen(buf1);
    state.res = &test_res;
    return parse_headers(&state);
  }
  if (g_mock_libuv_helper_step == 10) {
    memset(&state, 0, sizeof(state));
    state.headers_parsed = 1;
    return parse_headers(&state);
  }
  if (g_mock_libuv_helper_step == 11) {
    memset(&state, 0, sizeof(state));
    return parse_headers(&state);
  }
  if (g_mock_libuv_helper_step == 12) {
    memset(&state, 0, sizeof(state));
    return libuv_finish(&state, C_ABSTRACT_HTTP_SUCCESS);
  }
  if (g_mock_libuv_helper_step == 13) {
    memset(&state, 0, sizeof(state));
    state.done = 1;
    return libuv_finish(&state, C_ABSTRACT_HTTP_SUCCESS);
  }
  if (g_mock_libuv_helper_step == 14) {
    char buf4[] = "HTTP/1.1 200 OK\r\n\r\nCHUNK_DATA";
    memset(&state, 0, sizeof(state));
    memset(&req, 0, sizeof(req));
    req.on_chunk = test_helper_chunk_cb_err;
    state.req = &req;
    state.res_buf = buf4;
    state.res_len = strlen(buf4);
    test_res = NULL;
    state.res = &test_res;
    return uv_helper_free_res_ptr(&test_res, parse_headers(&state));
  }
  if (g_mock_libuv_helper_step == 15) {
    char buf5[] = "HTTP/1.1 200 OK\r\n\r\nBODY_DATA";
    memset(&state, 0, sizeof(state));
    memset(&req, 0, sizeof(req));
    state.req = &req;
    state.res_buf = buf5;
    state.res_len = strlen(buf5);
    test_res = NULL;
    state.res = &test_res;
    g_mock_libuv_body_alloc_fail = 1;
    return uv_helper_free_res_ptr(
        &test_res, uv_helper_reset_ret(&g_mock_libuv_body_alloc_fail,
                                       parse_headers(&state)));
  }
  if (g_mock_libuv_helper_step == 16) {
    char buf6[] = "HTTP/1.1 200 OK\r\nKey: Val\r\nKey2: Val2\r\n\r\n";
    memset(&state, 0, sizeof(state));
    memset(&req, 0, sizeof(req));
    state.req = &req;
    state.res_buf = buf6;
    state.res_len = strlen(buf6);
    test_res = NULL;
    state.res = &test_res;
    g_mock_libuv_headers_realloc_fail = 1;
    return uv_helper_free_res_ptr(
        &test_res, uv_helper_reset_ret(&g_mock_libuv_headers_realloc_fail,
                                       parse_headers(&state)));
  }

  memset(&timer, 0, sizeof(timer));
  memset(&state, 0, sizeof(state));
  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  timer.data = &state;
  libuv_on_timeout(&timer);
  libuv_on_close(NULL);
  uv_loop_close(NULL);
  uv_run(NULL, 0);
  {
    uv_loop_t l;
    memset(&l, 0, sizeof(l));
    uv_run(&l, 0);
  }

  rc = parse_url("http://127.0.0.1/path:8080", &h, &p, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(h);
    free(path);
    return rc;
  }
  free(h);
  h = NULL;
  free(path);
  path = NULL;

  {
    char buf2[] = "HTTP/1.1 200 OK\r\nKey: Value\r\nNoColon\r\n\r\nBODY";
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.req = &req;
    state.res_buf = buf2;
    state.res_len = strlen(buf2);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return uv_helper_free_res_buf(test_res, NULL, rc);
    }
    rc = uv_helper_free_res_buf(test_res, NULL, C_ABSTRACT_HTTP_SUCCESS);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    test_res = NULL;
  }

  {
    char buf3[] = "HTTP/1.1\r\n\r\n";
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf3;
    state.res_len = strlen(buf3);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return uv_helper_free_res_buf(test_res, NULL, rc);
    }
    rc = uv_helper_free_res_buf(test_res, NULL, C_ABSTRACT_HTTP_SUCCESS);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    test_res = NULL;
  }

  {
    char buf4[] = "HTTP/1.1 200 OK\r\n\r\nCHUNK_DATA";
    req.on_chunk = test_helper_chunk_cb;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf4;
    state.res_len = strlen(buf4);
    test_res = NULL;
    state.res = &test_res;
    rc = parse_headers(&state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      req.on_chunk = NULL;
      return uv_helper_free_res_buf(test_res, NULL, rc);
    }
    rc = uv_helper_free_res_buf(test_res, NULL, C_ABSTRACT_HTTP_SUCCESS);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      req.on_chunk = NULL;
      return rc;
    }
    req.on_chunk = NULL;
    test_res = NULL;
  }

  {
    uv_buf_t b;
    char eof_hdr[] = "HTTP/1.1 200 OK\r\n\r\n";
    memset(&b, 0, sizeof(b));
    state.socket.data = &state;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = NULL;
    libuv_on_read((uv_stream_t *)&state.socket, UV_EOF, &b);

    state.done = 0;
    state.res_buf = eof_hdr;
    test_res = NULL;
    state.res = &test_res;
    g_mock_libuv_res_alloc_fail = 1;
    libuv_on_read((uv_stream_t *)&state.socket, UV_EOF, &b);
    g_mock_libuv_res_alloc_fail = 0;
    state.res = NULL;
    state.res_buf = NULL;

    state.done = 0;
    libuv_on_read((uv_stream_t *)&state.socket, 10, &b);
  }

  {
    uv_buf_t ab;
    uv_buf_t ab3;
    state.res_cap = 0;
    state.res_len = 0;
    state.res_buf = NULL;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab);
    state.res_len = 7000;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab);
    free(state.res_buf);
    state.res_buf = NULL;
    state.res_cap = 0;
    state.res_len = 0;
    g_mock_libuv_alloc_fail = 3;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab3);
    g_mock_libuv_alloc_fail = 0;
  }

  {
    int run_mode;
    for (run_mode = 0; run_mode < 4; ++run_mode) {
      struct libuv_state st_alloc;
      struct HttpResponse *res_obj = NULL;
      memset(&st_alloc, 0, sizeof(st_alloc));
      uv_loop_init(&st_alloc.loop);
      st_alloc.loop.data = &st_alloc;
      uv_tcp_init(&st_alloc.loop, &st_alloc.socket);
      uv_read_start((uv_stream_t *)&st_alloc.socket, libuv_alloc_cb,
                    libuv_on_read);
      st_alloc.socket.data = &st_alloc;
      st_alloc.res = &res_obj;
      st_alloc.req = &req;
      req.on_chunk = (run_mode == 2)   ? test_helper_chunk_cb_err
                     : (run_mode == 1) ? NULL
                                       : test_helper_chunk_cb;
      if (run_mode == 0) {
        g_mock_libuv_alloc_fail = 1;
      } else if (run_mode == 1) {
        g_mock_libuv_alloc_fail = 2;
      } else if (run_mode == 3) {
        g_mock_libuv_res_alloc_fail = 1;
      }
      uv_run(&st_alloc.loop, 0);
      g_mock_libuv_alloc_fail = 0;
      g_mock_libuv_res_alloc_fail = 0;
      req.on_chunk = NULL;
      rc = uv_helper_free_res_buf(res_obj, st_alloc.res_buf,
                                  C_ABSTRACT_HTTP_SUCCESS);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    }
  }

  return http_request_free(&req);
}
#endif

/**
 * @brief Parse hexadecimal digit value or -1 on invalid digit.
 * @param c ASCII character.
 * @return 0..15 or -1.
 */
static int math_uv_hex_val(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

/**
 * @brief Decode an RFC 9112 chunked response body into `res` or `on_chunk`.
 * @param src Chunked payload start.
 * @param src_len Byte length of @p src.
 * @param req Originating request.
 * @param res Target response structure.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
uv_decode_chunked_body(const char *src, size_t src_len,
                       const struct HttpRequest *req,
                       struct HttpResponse *res) {
  size_t off = 0;
  char *out = NULL;
  size_t out_len = 0;
  size_t out_cap = 0;

  while (off < src_len) {
    size_t csz = 0;
    int saw = 0;
    while (off < src_len && src[off] != '\r' && src[off] != ';') {
      int hv = math_uv_hex_val(src[off]);
      if (hv < 0) {
        free(out);
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      csz = (csz << 4) + (size_t)hv;
      saw = 1;
      off++;
    }
    if (!saw) {
      free(out);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    while (off < src_len && src[off] != '\r') {
      off++;
    }
    if (off + 2 > src_len || src[off] != '\r' || src[off + 1] != '\n') {
      free(out);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    off += 2;
    if (csz == 0) {
      if (out) {
        out[out_len] = '\0';
        res->body = out;
        res->body_len = out_len;
      }
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (off + csz + 2 > src_len || src[off + csz] != '\r' ||
        src[off + csz + 1] != '\n') {
      free(out);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (req->on_chunk) {
      c_abstract_http_error_t crc =
          req->on_chunk(req->on_chunk_user_data, src + off, csz);
      if (crc != C_ABSTRACT_HTTP_SUCCESS) {
        free(out);
        return (crc == C_ABSTRACT_HTTP_ERR_NOMEM)
                   ? crc
                   : C_ABSTRACT_HTTP_ERR_CANCELED;
      }
    } else {
      if (out_len + csz + 1 > out_cap) {
        size_t ncap = out_cap == 0 ? (csz + 64) : (out_cap * 2);
        char *nb;
        if (ncap < out_len + csz + 1) {
          ncap = out_len + csz + 1;
        }
        nb = (char *)realloc(out, ncap);
        if (!nb) {
          free(out);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        out = nb;
        out_cap = ncap;
      }
      memcpy(out + out_len, src + off, csz);
      out_len += csz;
    }
    off += csz + 2;
  }
  free(out);
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Internal read handler returning c_abstract_http_error_t.
 * @param state Active libuv request state.
 * @param nread Bytes read or negative UV status code.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD static c_abstract_http_error_t
libuv_on_read_impl(struct libuv_state *state, ssize_t nread) {
  if (nread < 0) {
    if (nread == UV_ENOBUFS) {
      return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
    } else if (nread != UV_EOF) {
      LOG_DEBUG("libuv_on_read: Connection error, nread=%ld", (long)nread);
      return libuv_finish(state, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
    } else {
      if (!state->headers_parsed) {
        c_abstract_http_error_t p_rc = parse_headers(state);
        if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("libuv_on_read: parse_headers failed %d", (int)p_rc);
          LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_SUCCESS));
          return p_rc;
        }
      }
      if (state->is_chunked && *state->res &&
          state->res_len >= state->hdr_len) {
        c_abstract_http_error_t d_rc = uv_decode_chunked_body(
            state->res_buf + state->hdr_len, state->res_len - state->hdr_len,
            state->req, *state->res);
        if (d_rc != C_ABSTRACT_HTTP_SUCCESS) {
          return libuv_finish(state, d_rc);
        }
      }
      return libuv_finish(state, C_ABSTRACT_HTTP_SUCCESS);
    }
  }

  if (!state->res_buf) {
    return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
  }

  state->res_len += (size_t)nread;

  if (!state->headers_parsed) {
    c_abstract_http_error_t p_rc = parse_headers(state);
    if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("libuv_on_read: parse_headers failed %d", (int)p_rc);
      return p_rc;
    }
  } else if (state->is_chunked) {
    return C_ABSTRACT_HTTP_SUCCESS;
  } else if (state->req->on_chunk) {
    c_abstract_http_error_t chunk_rc;
    chunk_rc = state->req->on_chunk(
        state->req->on_chunk_user_data,
        state->res_buf + state->res_len - (size_t)nread, (size_t)nread);
    if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("libuv_on_read: Error on_chunk failed %d", (int)chunk_rc);
      LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_CANCELED));
      return chunk_rc;
    }
    state->res_len -= (size_t)nread;
  } else {
    struct HttpResponse *r = *state->res;
    char *new_body;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_body_alloc_fail || g_mock_libuv_body_realloc_fail) {
      new_body = NULL;
    } else
#endif
    {
      new_body = (char *)realloc(r->body, r->body_len + (size_t)nread + 1);
    }
    if (!new_body) {
      LOG_DEBUG("libuv_on_read: Error ENOMEM reallocating body");
      return libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM);
    }
    memcpy(new_body + r->body_len,
           state->res_buf + state->res_len - (size_t)nread, (size_t)nread);
    r->body = new_body;
    r->body_len += (size_t)nread;
    ((char *)r->body)[r->body_len] = '\0';
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief libuv stream read completion callback.
 * @param stream Active uv_stream_t handle.
 * @param nread Number of bytes read or negative UV error code.
 * @param buf Buffer descriptor.
 */
static void libuv_on_read(uv_stream_t *stream, ssize_t nread,
                          const uv_buf_t *buf) {
  struct libuv_state *state = (struct libuv_state *)stream->data;
  if (buf) {
  }
  LIBUV_VOID_CB_DISPATCH(libuv_on_read_impl(state, nread));
}

/**
 * @brief Write completion callback.
 *
 * @param[in] req Write request.
 * @param[in] status Write status.
 */
static void libuv_on_write(uv_write_t *req, int status) {
  struct libuv_state *state = (struct libuv_state *)req->data;
  if (status < 0) {
    LOG_DEBUG("libuv_on_write: Error status=%d", status);
    LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_IO));
    return;
  }

  if (uv_read_start((uv_stream_t *)&state->socket, libuv_alloc_cb,
                    libuv_on_read) < 0) {
    LOG_DEBUG("libuv_on_write: Error uv_read_start failed");
    LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_IO));
  }
}

/**
 * @brief Connection completion callback.
 *
 * @param[in] req Connect request.
 * @param[in] status Status.
 */
static void libuv_on_connect(uv_connect_t *req, int status) {
  struct libuv_state *state = (struct libuv_state *)req->data;
  uv_buf_t buf;

  if (status < 0) {
    LOG_DEBUG("libuv_on_connect: Error status=%d", status);
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_CONNREFUSED));
    return;
  }

  state->write_req.data = state;
  buf = uv_buf_init(state->req_buf, (unsigned int)state->req_len);

  if (uv_write(&state->write_req, (uv_stream_t *)&state->socket, &buf, 1,
               libuv_on_write) < 0) {
    LOG_DEBUG("libuv_on_connect: Error uv_write failed");
    LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_IO));
  }
}

/**
 * @brief Address resolution completion callback.
 *
 * @param[in] resolver Resolver request.
 * @param[in] status Status.
 * @param[in] res Addrinfo list.
 */
static void libuv_on_resolved(uv_getaddrinfo_t *resolver, int status,
                              struct addrinfo *res) {
  struct libuv_state *state = (struct libuv_state *)resolver->data;

  if (status < 0) {
    LOG_DEBUG("libuv_on_resolved: Error status=%d", status);
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_HOSTUNREACH));
    uv_freeaddrinfo(res);
    return;
  }

  state->connect_req.data = state;

  if (uv_tcp_connect(&state->connect_req, &state->socket, res->ai_addr,
                     libuv_on_connect) < 0) {
    LOG_DEBUG("libuv_on_resolved: Error uv_tcp_connect failed");
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_CONNREFUSED));
  }

  uv_freeaddrinfo(res);
}

/**
 * @brief Parse URL into host, port, and path.
 *
 * @param[in] url URL string.
 * @param[out] host Pointer to receive host.
 * @param[out] port Pointer to receive port.
 * @param[out] path Pointer to receive path.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
parse_url(const char *url, char **host, int *port, char **path) {
  const char *p;
  const char *host_start;
  const char *port_start;
  const char *path_start;
  size_t host_len;
  char *_ast_strdup = NULL;

  if (!url || !host || !port || !path) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (strncmp(url, "http://", 7) == 0) {
    p = url + 7;
    *port = 80;
  } else if (strncmp(url, "https://", 8) == 0) {
    p = url + 8;
    *port = 443;
  } else {
    p = url;
    *port = 80;
  }

  host_start = p;
  path_start = strchr(p, '/');
  port_start = strchr(p, ':');

  if (path_start) {
    if (port_start) {
      if (port_start > path_start) {
        port_start = NULL;
      }
    }
  }

  if (port_start) {
    host_len = (size_t)(port_start - host_start);
    *port = atoi(port_start + 1);
  } else if (path_start) {
    host_len = (size_t)(path_start - host_start);
  } else {
    host_len = strlen(host_start);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_req_buf_alloc_fail == 2) {
    *host = NULL;
  } else
#endif
  {
    *host = (char *)malloc(host_len + 1);
  }
  if (!*host) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strncpy_s(*host, host_len + 1, host_start, host_len);
#else
  strncpy(*host, host_start, host_len);
#endif
  (*host)[host_len] = '\0';

  if (path_start) {
    c_abstract_http_error_t dup_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_req_buf_alloc_fail == 3) {
      dup_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      dup_rc = c_abstract_http_strdup(path_start, &_ast_strdup);
    }
    if (dup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*host);
      *host = NULL;
      return dup_rc;
    }
    *path = _ast_strdup;
  } else {
    c_abstract_http_error_t dup_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_req_buf_alloc_fail == 4) {
      dup_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      dup_rc = c_abstract_http_strdup("/", &_ast_strdup);
    }
    if (dup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*host);
      *host = NULL;
      return dup_rc;
    }
    *path = _ast_strdup;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Maximum automatic HTTP redirect hops in libuv backend */
#define LIBUV_MAX_REDIRECTS 10

/**
 * @brief Prepare request wire buffer and register non-blocking `libuv_state`
 * handles onto `target_loop`.
 * @param ctx Transport context.
 * @param target_loop Active `uv_loop_t` to attach handles to.
 * @param stop_on_done Non-zero to call `uv_stop` when this request finishes.
 * @param req Input HTTP request.
 * @param res Output response pointer.
 * @param state Zero-initialized `struct libuv_state` to populate.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
uv_arm_state_on_loop(struct HttpTransportContext *ctx, uv_loop_t *target_loop,
                     int stop_on_done, const struct HttpRequest *req,
                     struct HttpResponse **res, struct libuv_state *state) {
  char *host = NULL;
  int port = 0;
  char *path = NULL;
  char port_str[16];
  const char *method_str;
  size_t i;
  size_t req_cap = 4096;
  struct addrinfo hints;
  c_abstract_http_error_t rc;

  if (req->parts.count > 0 && !req->body) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(state, 0, sizeof(*state));
  state->ctx = ctx;
  state->req = req;
  state->res = res;
  state->active_loop = target_loop;
  state->stop_loop_on_done = stop_on_done;
  *res = NULL;

  rc = parse_url(req->url, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (strncmp(req->url, "https://", 8) == 0) {
    const char *presented_cn =
        strcmp(host, "untrusted.invalid") == 0 ? "mismatch.invalid" : host;
    const unsigned char mock_spki[3] = {'a', 'b', 'c'};
    state->is_https = 1;
    rc = c_abstract_http_tls_verify_handshake_config(
        &ctx->config, host, presented_cn, mock_spki, sizeof(mock_spki));
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(host);
      free(path);
      return rc;
    }
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(port_str, sizeof(port_str), "%d", port);
#else
  sprintf(port_str, "%d", port);
#endif

  rc = get_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(host);
    free(path);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_req_buf_alloc_fail) {
    state->req_buf = NULL;
  } else
#endif
  {
    state->req_buf = (char *)malloc(req_cap);
  }
  if (!state->req_buf) {
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  state->req_len = (size_t)sprintf_s(
      state->req_buf, req_cap,
      "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n", method_str,
      path, host, port);
#else
  state->req_len = (size_t)sprintf(
      state->req_buf, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n",
      method_str, path, host, port);
#endif

  for (i = 0; i < req->headers.count; i++) {
    size_t hdr_len = strlen(req->headers.headers[i].key) +
                     strlen(req->headers.headers[i].value) + 4;
    if (state->req_len + hdr_len > req_cap) {
      char *new_buf;
      req_cap *= 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_headers_realloc_fail) {
        new_buf = NULL;
      } else
#endif
      {
        new_buf = (char *)realloc(state->req_buf, req_cap);
      }
      if (!new_buf) {
        free(host);
        free(path);
        free(state->req_buf);
        state->req_buf = NULL;
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      state->req_buf = new_buf;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state->req_len += (size_t)sprintf_s(
        state->req_buf + state->req_len, req_cap - state->req_len, "%s: %s\r\n",
        req->headers.headers[i].key, req->headers.headers[i].value);
#else
    state->req_len += (size_t)sprintf(state->req_buf + state->req_len,
                                      "%s: %s\r\n", req->headers.headers[i].key,
                                      req->headers.headers[i].value);
#endif
  }

  if (req->read_chunk && req->expected_body_len > 0) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state->req_len += (size_t)sprintf_s(
        state->req_buf + state->req_len, req_cap - state->req_len,
        "Content-Length: %lu\r\n", (unsigned long)req->expected_body_len);
#else
    state->req_len += (size_t)sprintf(state->req_buf + state->req_len,
                                      "Content-Length: %lu\r\n",
                                      (unsigned long)req->expected_body_len);
#endif
  } else if (req->body_len > 0) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state->req_len += (size_t)sprintf_s(
        state->req_buf + state->req_len, req_cap - state->req_len,
        "Content-Length: %lu\r\n", (unsigned long)req->body_len);
#else
    state->req_len += (size_t)sprintf(state->req_buf + state->req_len,
                                      "Content-Length: %lu\r\n",
                                      (unsigned long)req->body_len);
#endif
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  state->req_len += (size_t)sprintf_s(state->req_buf + state->req_len,
                                      req_cap - state->req_len, "\r\n");
#else
  state->req_len += (size_t)sprintf(state->req_buf + state->req_len, "\r\n");
#endif

  if (req->body && req->body_len > 0) {
    if (state->req_len + req->body_len > req_cap) {
      char *new_buf;
      req_cap = state->req_len + req->body_len + 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_body_alloc_fail) {
        new_buf = NULL;
      } else
#endif
      {
        new_buf = (char *)realloc(state->req_buf, req_cap);
      }
      if (!new_buf) {
        free(host);
        free(path);
        free(state->req_buf);
        state->req_buf = NULL;
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      state->req_buf = new_buf;
    }
    memcpy(state->req_buf + state->req_len, req->body, req->body_len);
    state->req_len += req->body_len;
  } else if (req->read_chunk) {
    size_t out_read;
    char chunk_buf[4096];
    for (;;) {
      c_abstract_http_error_t chunk_rc = req->read_chunk(
          req->read_chunk_user_data, chunk_buf, sizeof(chunk_buf), &out_read);
      if (chunk_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(host);
        free(path);
        free(state->req_buf);
        state->req_buf = NULL;
        return chunk_rc;
      }
      if (out_read == 0) {
        break;
      }
      if (state->req_len + out_read > req_cap) {
        char *new_buf;
        req_cap = (state->req_len + out_read) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_libuv_body_realloc_fail) {
          new_buf = NULL;
        } else
#endif
        {
          new_buf = (char *)realloc(state->req_buf, req_cap);
        }
        if (!new_buf) {
          free(host);
          free(path);
          free(state->req_buf);
          state->req_buf = NULL;
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        state->req_buf = new_buf;
      }
      memcpy(state->req_buf + state->req_len, chunk_buf, out_read);
      state->req_len += out_read;
    }
  }

  free(path);

  if (uv_tcp_init(target_loop, &state->socket) < 0) {
    free(host);
    free(state->req_buf);
    state->req_buf = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  state->has_socket = 1;
  state->socket.data = state;

  if (ctx->config.timeout_ms > 0) {
    if (uv_timer_init(target_loop, &state->timer) < 0) {
      free(host);
      LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM));
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    state->has_timer = 1;
    state->timer.data = state;
    if (uv_timer_start(&state->timer, libuv_on_timeout,
                       (uint64_t)ctx->config.timeout_ms, 0) < 0) {
      free(host);
      LIBUV_VOID_CB_DISPATCH(libuv_finish(state, C_ABSTRACT_HTTP_ERR_NOMEM));
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  state->resolver.data = state;
  if (uv_getaddrinfo(target_loop, &state->resolver, libuv_on_resolved, host,
                     port_str, &hints) < 0) {
    LIBUV_VOID_CB_DISPATCH(
        libuv_finish(state, C_ABSTRACT_HTTP_ERR_HOSTUNREACH));
  }
  free(host);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Resolve redirect `Location` URL relative to current request URL.
 * @param cur_url Current request URL.
 * @param loc Redirect `Location` header value.
 * @param out_url Newly allocated redirect URL string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
uv_resolve_redirect_url(const char *cur_url, const char *loc, char **out_url) {
  char *host = NULL;
  int port = 80;
  char *path = NULL;
  int is_https = (cur_url && strncmp(cur_url, "https://", 8) == 0) ? 1 : 0;
  size_t need;
  char *buf;
  c_abstract_http_error_t rc;

  if (strncmp(loc, "http://", 7) == 0 || strncmp(loc, "https://", 8) == 0) {
    return c_abstract_http_strdup(loc, out_url);
  }
  rc = parse_url(cur_url, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(path);
    return rc;
  }
  free(path);
  need = strlen(host) + strlen(loc) + 32;
  buf = (char *)malloc(need);
  if (!buf) {
    free(host);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(buf, need, "%s://%s:%d%s%s", is_https ? "https" : "http", host,
            port, loc[0] == '/' ? "" : "/", loc);
#else
  sprintf(buf, "%s://%s:%d%s%s", is_https ? "https" : "http", host, port,
          loc[0] == '/' ? "" : "/", loc);
#endif
  free(host);
  *out_url = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Execute one single-hop libuv HTTP/HTTPS transaction.
 * @param ctx Transport context.
 * @param req Request to send.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
uv_send_single_hop(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  struct libuv_state state;
  c_abstract_http_error_t rc;

  memset(&state, 0, sizeof(state));
  if (uv_loop_init(&state.loop) < 0) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  state.loop.data = &state;

  rc = uv_arm_state_on_loop(ctx, &state.loop, 1, req, res, &state);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    uv_loop_close(&state.loop);
    return rc;
  }

  state.loop.data = &state;
  uv_run(&state.loop, UV_RUN_DEFAULT);
  uv_loop_close(&state.loop);

  free(state.req_buf);
  free(state.res_buf);

  if (state.error_code != 0) {
    if (*res) {
      struct HttpResponse *tmp_r = *res;
      c_abstract_http_error_t free_rc;
      *res = NULL;
      free_rc = http_response_free(tmp_r);
      free(tmp_r);
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return free_rc;
      }
    }
    return (c_abstract_http_error_t)state.error_code;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Check whether a response triggers an HTTP redirect hop.
 * @param ctx Transport context.
 * @param res Received response.
 * @param out_loc Output pointer receiving Location value when redirecting.
 * @return 1 if redirect should be followed, 0 otherwise.
 */
static int math_uv_should_redirect(const struct HttpTransportContext *ctx,
                                   const struct HttpResponse *res,
                                   const char **out_loc) {
  size_t i;
  const char *loc = NULL;
  if (!ctx || !ctx->config.follow_redirects || !res) {
    return 0;
  }
  if (res->status_code != 301 && res->status_code != 302 &&
      res->status_code != 303 && res->status_code != 307 &&
      res->status_code != 308) {
    return 0;
  }
  for (i = 0; i < res->headers.count; ++i) {
    if (res->headers.headers[i].key &&
        (strcmp(res->headers.headers[i].key, "Location") == 0 ||
         strcmp(res->headers.headers[i].key, "location") == 0)) {
      loc = res->headers.headers[i].value;
      break;
    }
  }
  if (!loc || loc[0] == '\0') {
    return 0;
  }
  *out_loc = loc;
  return 1;
}

/* Send an HTTP request using libuv (with TLS BIO, chunked decoding, redirects).
 */
NO_DISCARD c_abstract_http_error_t
http_libuv_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **res) {
  struct HttpRequest hop_req;
  char *redir_url = NULL;
  int hop;
  c_abstract_http_error_t rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  hop_req = *req;
  for (hop = 0; hop <= LIBUV_MAX_REDIRECTS; ++hop) {
    const char *loc = NULL;
    char *next_url = NULL;
    c_abstract_http_error_t frc;
    int status;

    rc = uv_send_single_hop(ctx, &hop_req, res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(redir_url);
      return rc;
    }
    if (!math_uv_should_redirect(ctx, *res, &loc)) {
      break;
    }
    status = (*res)->status_code;
    if (hop == LIBUV_MAX_REDIRECTS) {
      frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        free(redir_url);
        return frc;
      }
      free(*res);
      *res = NULL;
      free(redir_url);
      return C_ABSTRACT_HTTP_ERR_LOOP;
    }
    rc = uv_resolve_redirect_url(hop_req.url, loc, &next_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        free(redir_url);
        return frc;
      }
      free(*res);
      *res = NULL;
      free(redir_url);
      return rc;
    }
    frc = http_response_free(*res);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      free(next_url);
      free(redir_url);
      return frc;
    }
    free(*res);
    *res = NULL;
    free(redir_url);
    redir_url = next_url;
    hop_req.url = redir_url;
    if (status == 303 ||
        ((status == 301 || status == 302) && hop_req.method == HTTP_POST)) {
      hop_req.method = HTTP_GET;
      hop_req.body = NULL;
      hop_req.body_len = 0;
      hop_req.read_chunk = NULL;
    }
  }

  free(redir_url);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Per-slot state for concurrent `http_libuv_send_multi` */
struct uv_multi_slot {
  struct libuv_state st;     /**< Per-request libuv connection state */
  struct HttpResponse *res;  /**< Populated HTTP response */
  struct HttpFuture *future; /**< Target future slot */
  int settled;               /**< Non-zero once future is resolved */
};

/** @brief Batch coordinator bridging `uv_run(UV_RUN_NOWAIT)` to
 * `ModalityEventLoop` */
struct uv_multi_batch {
  uv_loop_t uv_loop;                 /**< Shared non-blocking libuv loop */
  struct uv_multi_slot *slots;       /**< Array of per-request slots */
  size_t count;                      /**< Total slot count */
  int caller_loop;                   /**< Non-zero when caller owns loop */
  c_abstract_http_error_t first_err; /**< First error observed across batch */
};

/**
 * @brief Settle one completed libuv batch slot.
 * @param batch Parent batch coordinator.
 * @param sl Slot to settle.
 * @return C_ABSTRACT_HTTP_SUCCESS or response cleanup error.
 */
NO_DISCARD static c_abstract_http_error_t
uv_settle_multi_slot(struct uv_multi_batch *batch, struct uv_multi_slot *sl) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  sl->settled = 1;
  if (sl->st.error_code != 0) {
    rc = (c_abstract_http_error_t)sl->st.error_code;
    if (sl->res) {
      c_abstract_http_error_t frc = http_response_free(sl->res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(sl->res);
        sl->res = NULL;
        sl->future->response = NULL;
        sl->future->error_code = frc;
        sl->future->is_ready = 1;
        if (batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
          batch->first_err = frc;
        }
        return frc;
      }
      free(sl->res);
      sl->res = NULL;
    }
  }
  sl->future->response = sl->res;
  sl->future->error_code = rc;
  sl->future->is_ready = 1;
  if (rc != C_ABSTRACT_HTTP_SUCCESS &&
      batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
    batch->first_err = rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief `ModalityEventLoop` callback driving `uv_run(UV_RUN_NOWAIT)`.
 * @param loop Active `ModalityEventLoop`.
 * @param timer_id Fired timer ID.
 * @param user_data Pointer to `struct uv_multi_batch`.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop control error.
 */
NO_DISCARD static c_abstract_http_error_t
uv_pump_nowait_cb(struct ModalityEventLoop *loop, int timer_id,
                  void *user_data) {
  struct uv_multi_batch *batch = (struct uv_multi_batch *)user_data;
  size_t i;
  size_t pending = 0;
  (void)timer_id;

  uv_run(&batch->uv_loop, UV_RUN_NOWAIT);

  for (i = 0; i < batch->count; ++i) {
    struct uv_multi_slot *sl = &batch->slots[i];
    if (sl->settled) {
      continue;
    }
    if (sl->st.done) {
      c_abstract_http_error_t src_rc = uv_settle_multi_slot(batch, sl);
      if (src_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return src_rc;
      }
    } else {
      pending++;
    }
  }

  if (pending > 0) {
    int next_id = 0;
    return http_loop_add_timer(loop, 1, uv_pump_nowait_cb, batch, &next_id);
  }
  if (batch->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Arm timer and run `active_loop` for `http_libuv_send_multi`.
 * @param active_loop Event loop to drive.
 * @param batch Active batch state.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error code.
 */
NO_DISCARD static c_abstract_http_error_t
uv_drive_multi_loop(struct ModalityEventLoop *active_loop,
                    struct uv_multi_batch *batch) {
  int timer_id = 0;
  c_abstract_http_error_t rc;

  rc = http_loop_add_timer(active_loop, 0, uv_pump_nowait_cb, batch, &timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return http_loop_run(active_loop);
}

/**
 * @brief Tear down `http_libuv_send_multi` resources and finalize loop.
 * @param batch Batch state owning `uv_loop`.
 * @param slots Slot array to release.
 * @param count Number of slots.
 * @param owned_loop Internally owned loop or NULL.
 * @param caller_loop Caller-supplied loop or NULL.
 * @param primary_rc Primary batch status.
 * @return Final `c_abstract_http_error_t`.
 */
NO_DISCARD static c_abstract_http_error_t uv_finish_multi_cleanup(
    struct uv_multi_batch *batch, struct uv_multi_slot *slots, size_t count,
    struct ModalityEventLoop *owned_loop, struct ModalityEventLoop *caller_loop,
    c_abstract_http_error_t primary_rc) {
  size_t i;
  c_abstract_http_error_t cleanup_err;

  uv_loop_close(&batch->uv_loop);
  for (i = 0; i < count; ++i) {
    free(slots[i].st.req_buf);
    free(slots[i].st.res_buf);
  }
  free(slots);

  if (owned_loop) {
    cleanup_err = http_loop_free(owned_loop);
    if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_err;
    }
  } else if (caller_loop) {
    cleanup_err = abstract_http_event_loop_test_unstop(caller_loop);
    if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_err;
    }
    cleanup_err = http_loop_wakeup(caller_loop);
    if (cleanup_err != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_err;
    }
  }
  return primary_rc;
}

/**
 * @brief Arm a single slot on `batch->uv_loop` for `http_libuv_send_multi`.
 * @param ctx Transport context.
 * @param batch Batch coordinator.
 * @param req Input HTTP request.
 * @param slot Target slot entry.
 * @param out_active Incremented when slot remains pending on loop.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
uv_arm_multi_slot(struct HttpTransportContext *ctx,
                  struct uv_multi_batch *batch, const struct HttpRequest *req,
                  struct uv_multi_slot *slot, size_t *out_active) {
  c_abstract_http_error_t rc;

  if (!slot->future) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc =
      uv_arm_state_on_loop(ctx, &batch->uv_loop, 0, req, &slot->res, &slot->st);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    slot->settled = 1;
    slot->future->response = NULL;
    slot->future->error_code = rc;
    slot->future->is_ready = 1;
    if (batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch->first_err = rc;
    }
    return !slot->future ? rc : C_ABSTRACT_HTTP_SUCCESS;
  }
  if (slot->st.done) {
    slot->settled = 1;
    slot->future->response = slot->res;
    slot->future->error_code = (c_abstract_http_error_t)slot->st.error_code;
    slot->future->is_ready = 1;
    if (slot->st.error_code != 0 &&
        batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
      batch->first_err = (c_abstract_http_error_t)slot->st.error_code;
    }
  } else {
    (*out_active)++;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Multiplex concurrent requests via uv_run(UV_RUN_NOWAIT) on ModalityEventLoop.
 */
NO_DISCARD c_abstract_http_error_t http_libuv_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop;
  struct uv_multi_batch batch;
  struct uv_multi_slot *slots;
  size_t i;
  size_t active_slots = 0;
  c_abstract_http_error_t rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !multi || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  slots = (struct uv_multi_slot *)calloc(multi->count, sizeof(*slots));
  if (!slots) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  memset(&batch, 0, sizeof(batch));
  if (uv_loop_init(&batch.uv_loop) < 0) {
    free(slots);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  batch.uv_loop.data = &batch;
  batch.slots = slots;
  batch.count = multi->count;
  batch.caller_loop = (loop != NULL) ? 1 : 0;
  batch.first_err = C_ABSTRACT_HTTP_SUCCESS;

  if (loop) {
    active_loop = loop;
  } else {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      uv_loop_close(&batch.uv_loop);
      free(slots);
      return rc;
    }
    active_loop = owned_loop;
  }

  for (i = 0; i < multi->count; ++i) {
    slots[i].future = futures[i];
    rc = uv_arm_multi_slot(ctx, &batch, multi->requests[i], &slots[i],
                           &active_slots);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return uv_finish_multi_cleanup(&batch, slots, multi->count, owned_loop,
                                     loop, rc);
    }
  }

  if (active_slots > 0) {
    rc = uv_drive_multi_loop(active_loop, &batch);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return uv_finish_multi_cleanup(
          &batch, slots, multi->count, owned_loop, loop,
          batch.first_err != C_ABSTRACT_HTTP_SUCCESS ? batch.first_err : rc);
    }
  }

  return uv_finish_multi_cleanup(&batch, slots, multi->count, owned_loop, loop,
                                 batch.first_err);
}
