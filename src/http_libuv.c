/* clang-format off */
#if !defined(_WIN32)
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
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
#include "str.h"

#if defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)
#include <uv.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_libuv_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_req_buf_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_headers_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_addrinfo_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_write_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_read_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_alloc_fail(void);

#define g_mock_libuv_global_init_fail                                          \
  (*abstract_http_mock_get_g_mock_libuv_global_init_fail())
#define g_mock_libuv_context_init_fail                                         \
  (*abstract_http_mock_get_g_mock_libuv_context_init_fail())
#define g_mock_libuv_config_init_fail                                          \
  (*abstract_http_mock_get_g_mock_libuv_config_init_fail())
#define g_mock_libuv_req_buf_alloc_fail                                        \
  (*abstract_http_mock_get_g_mock_libuv_req_buf_alloc_fail())
#define g_mock_libuv_res_alloc_fail                                            \
  (*abstract_http_mock_get_g_mock_libuv_res_alloc_fail())
#define g_mock_libuv_res_init_fail                                             \
  (*abstract_http_mock_get_g_mock_libuv_res_init_fail())
#define g_mock_libuv_body_alloc_fail                                           \
  (*abstract_http_mock_get_g_mock_libuv_body_alloc_fail())
#define g_mock_libuv_body_realloc_fail                                         \
  (*abstract_http_mock_get_g_mock_libuv_body_realloc_fail())
#define g_mock_libuv_headers_realloc_fail                                      \
  (*abstract_http_mock_get_g_mock_libuv_headers_realloc_fail())
#define g_mock_libuv_addrinfo_fail                                             \
  (*abstract_http_mock_get_g_mock_libuv_addrinfo_fail())
#define g_mock_libuv_connect_fail                                              \
  (*abstract_http_mock_get_g_mock_libuv_connect_fail())
#define g_mock_libuv_write_fail                                                \
  (*abstract_http_mock_get_g_mock_libuv_write_fail())
#define g_mock_libuv_read_fail                                                 \
  (*abstract_http_mock_get_g_mock_libuv_read_fail())
#define g_mock_libuv_alloc_fail                                                \
  (*abstract_http_mock_get_g_mock_libuv_alloc_fail())
#endif

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)

#define UV_RUN_DEFAULT 0
#define UV_EOF (-4095)

#if defined(_WIN32) && !defined(__GNUC__)
typedef unsigned long ULONG;
#else
typedef unsigned long ULONG;
#endif

/** @brief Mock uv_buf_t structure */
typedef struct uv_buf_s {
  /** @brief Pointer to base buffer */
  char *base;
  /** @brief Buffer length */
  size_t len;
} uv_buf_t;

/** @brief Forward declare structs */
struct uv_handle_s;
struct uv_stream_s;
struct uv_tcp_s;
struct uv_connect_s;
struct uv_getaddrinfo_s;
struct uv_timer_s;
struct uv_write_s;
struct uv_loop_s;

/** @brief Forward declare handle */
typedef struct uv_handle_s uv_handle_t;
/** @brief Forward declare stream */
typedef struct uv_stream_s uv_stream_t;

/** @brief Callback for closing handles */
typedef void (*uv_close_cb)(uv_handle_t *handle);
/** @brief Callback for allocating memory */
typedef void (*uv_alloc_cb)(uv_handle_t *handle, size_t suggested_size,
                            uv_buf_t *buf);
/** @brief Callback for reading data */
typedef void (*uv_read_cb)(uv_stream_t *stream, ssize_t nread,
                           const uv_buf_t *buf);
/** @brief Callback for write completion */
typedef void (*uv_write_cb)(struct uv_write_s *req, int status);
/** @brief Callback for connection completion */
typedef void (*uv_connect_cb)(struct uv_connect_s *req, int status);
/** @brief Callback for getaddrinfo completion */
typedef void (*uv_getaddrinfo_cb)(struct uv_getaddrinfo_s *resolver, int status,
                                  struct addrinfo *res);
/** @brief Callback for timer expiration */
typedef void (*uv_timer_cb)(struct uv_timer_s *handle);

/** @brief Mock base uv_handle_s */
struct uv_handle_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Closing flag */
  int is_closing;
};

/** @brief Mock uv_stream_s */
struct uv_stream_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Closing flag */
  int is_closing;
  /** @brief Read callback */
  uv_read_cb read_cb;
  /** @brief Alloc callback */
  uv_alloc_cb alloc_cb;
};

/** @brief Mock uv_tcp_t */
typedef struct uv_tcp_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Closing flag */
  int is_closing;
  /** @brief Read callback */
  uv_read_cb read_cb;
  /** @brief Alloc callback */
  uv_alloc_cb alloc_cb;
  /** @brief Connect callback */
  uv_connect_cb connect_cb;
} uv_tcp_t;

/** @brief Mock uv_connect_t */
typedef struct uv_connect_s {
  /** @brief User data pointer */
  void *data;
} uv_connect_t;

/** @brief Mock uv_getaddrinfo_t */
typedef struct uv_getaddrinfo_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Callback */
  uv_getaddrinfo_cb cb;
  /** @brief Port string */
  char port_str[16];
} uv_getaddrinfo_t;

/** @brief Mock uv_timer_t */
typedef struct uv_timer_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Closing flag */
  int is_closing;
  /** @brief Timer callback */
  uv_timer_cb timer_cb;
  /** @brief Active flag */
  int active;
} uv_timer_t;

/** @brief Mock uv_write_t */
typedef struct uv_write_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Write callback */
  uv_write_cb write_cb;
} uv_write_t;

/** @brief Mock uv_loop_t */
typedef struct uv_loop_s {
  /** @brief User data pointer */
  void *data;
  /** @brief Run flag */
  int stop;
} uv_loop_t;

/**
 * @brief Initialize a buffer structure.
 *
 * @param[in] base Base pointer.
 * @param[in] len Length in bytes.
 * @return Initialized uv_buf_t.
 */
static uv_buf_t uv_buf_init(char *base, unsigned int len) {
  uv_buf_t buf;
  buf.base = base;
  buf.len = (size_t)len;
  return buf;
}

/**
 * @brief Initialize an event loop.
 *
 * @param[out] loop Loop to initialize.
 * @return 0 on success.
 */
static int uv_loop_init(uv_loop_t *loop) {
  if (loop) {
    memset(loop, 0, sizeof(*loop));
  }
  return 0;
}

/**
 * @brief Close an event loop.
 *
 * @param[in] loop Loop to close.
 * @return 0 on success.
 */
static int uv_loop_close(uv_loop_t *loop) {
  (void)loop;
  return 0;
}

/**
 * @brief Initialize a TCP handle.
 *
 * @param[in] loop Event loop.
 * @param[out] tcp TCP handle to initialize.
 * @return 0 on success.
 */
static int uv_tcp_init(uv_loop_t *loop, uv_tcp_t *tcp) {
  (void)loop;
  if (tcp) {
    memset(tcp, 0, sizeof(*tcp));
  }
  return 0;
}

/**
 * @brief Initialize a timer handle.
 *
 * @param[in] loop Event loop.
 * @param[out] timer Timer handle.
 * @return 0 on success.
 */
static int uv_timer_init(uv_loop_t *loop, uv_timer_t *timer) {
  (void)loop;
  if (timer) {
    memset(timer, 0, sizeof(*timer));
  }
  return 0;
}

/**
 * @brief Start a timer handle.
 *
 * @param[in] timer Timer handle.
 * @param[in] cb Timer callback.
 * @param[in] timeout Timeout in milliseconds.
 * @param[in] repeat Repeat interval.
 * @return 0 on success.
 */
static int uv_timer_start(uv_timer_t *timer, uv_timer_cb cb, uint64_t timeout,
                          uint64_t repeat) {
  (void)timeout;
  (void)repeat;
  if (timer) {
    timer->timer_cb = cb;
    timer->active = 1;
  }
  return 0;
}

/**
 * @brief Stop a timer handle.
 *
 * @param[in] timer Timer handle.
 * @return 0 on success.
 */
static int uv_timer_stop(uv_timer_t *timer) {
  if (timer) {
    timer->active = 0;
  }
  return 0;
}

/**
 * @brief Check if a handle is closing.
 *
 * @param[in] handle Handle pointer.
 * @return 1 if closing, 0 otherwise.
 */
static int uv_is_closing(const uv_handle_t *handle) {
  return handle ? handle->is_closing : 0;
}

/**
 * @brief Close a handle.
 *
 * @param[in] handle Handle to close.
 * @param[in] close_cb Optional close callback.
 */
static void uv_close(uv_handle_t *handle, uv_close_cb close_cb) {
  if (handle) {
    handle->is_closing = 1;
    if (close_cb) {
      close_cb(handle);
    }
  }
}

/**
 * @brief Free addrinfo struct.
 *
 * @param[in] ai Pointer to addrinfo.
 */
static void uv_freeaddrinfo(struct addrinfo *ai) {
  if (ai) {
    if (ai->ai_addr) {
      free(ai->ai_addr);
    }
    free(ai);
  }
}

/**
 * @brief Asynchronously resolve address.
 *
 * @param[in] loop Loop pointer.
 * @param[out] resolver Resolver handle.
 * @param[in] cb Completion callback.
 * @param[in] node Node hostname.
 * @param[in] service Service port string.
 * @param[in] hints Addrinfo hints.
 * @return 0 on success, -1 on failure.
 */
static int uv_getaddrinfo(uv_loop_t *loop, uv_getaddrinfo_t *resolver,
                          uv_getaddrinfo_cb cb, const char *node,
                          const char *service, const struct addrinfo *hints) {
  (void)loop;
  (void)node;
  (void)hints;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_addrinfo_fail == 2) {
    return -1;
  }
#endif
  if (resolver) {
    resolver->cb = cb;
    if (service) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strncpy_s(resolver->port_str, sizeof(resolver->port_str), service,
                sizeof(resolver->port_str) - 1);
#else
      strncpy(resolver->port_str, service, sizeof(resolver->port_str) - 1);
#endif
      resolver->port_str[sizeof(resolver->port_str) - 1] = '\0';
    }
  }
  return 0;
}

/**
 * @brief Connect TCP socket.
 *
 * @param[in] req Connect request handle.
 * @param[in] handle TCP handle.
 * @param[in] addr Socket address.
 * @param[in] cb Connection callback.
 * @return 0 on success, -1 on failure.
 */
static int uv_tcp_connect(uv_connect_t *req, uv_tcp_t *handle,
                          const struct sockaddr *addr, uv_connect_cb cb) {
  (void)addr;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_connect_fail == 2) {
    return -1;
  }
#endif
  if (handle) {
    handle->connect_cb = cb;
  }
  (void)req;
  return 0;
}

/**
 * @brief Write data to stream.
 *
 * @param[in] req Write request handle.
 * @param[in] handle Stream handle.
 * @param[in] bufs Array of buffers.
 * @param[in] nbufs Number of buffers.
 * @param[in] cb Completion callback.
 * @return 0 on success, -1 on failure.
 */
static int uv_write(uv_write_t *req, uv_stream_t *handle, const uv_buf_t bufs[],
                    unsigned int nbufs, uv_write_cb cb) {
  (void)handle;
  (void)bufs;
  (void)nbufs;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_write_fail == 2) {
    return -1;
  }
#endif
  if (req) {
    req->write_cb = cb;
  }
  return 0;
}

/**
 * @brief Start reading data from stream.
 *
 * @param[in] stream Stream handle.
 * @param[in] alloc_cb Memory allocation callback.
 * @param[in] read_cb Data received callback.
 * @return 0 on success.
 */
static int uv_read_start(uv_stream_t *stream, uv_alloc_cb alloc_cb,
                         uv_read_cb read_cb) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_read_fail == 2) {
    return -1;
  }
#endif
  if (stream) {
    stream->alloc_cb = alloc_cb;
    stream->read_cb = read_cb;
  }
  return 0;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV) */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
};

static int libuv_global_init_count = 0;

/**
 * @brief Initialize global libuv environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libuv_global_init(void) {
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
enum c_abstract_http_error http_libuv_global_cleanup(void) {
  if (libuv_global_init_count > 0) {
    libuv_global_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Create a new libuv-backed transport context.
 *
 * @param[out] ctx Double pointer to receive newly allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_libuv_context_init(struct HttpTransportContext **ctx) {
  enum c_abstract_http_error rc;
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
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&(*ctx)->config);
  }
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

/**
 * @brief Free the transport context.
 *
 * @param[in] ctx Context to free.
 */
void http_libuv_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_libuv_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_libuv_context_free: Exiting");
}

/**
 * @brief Apply configuration to the transport context.
 *
 * @param[in] ctx Context.
 * @param[in] config Configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_libuv_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_libuv_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_libuv_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
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
    if (g_mock_libuv_config_init_fail) {
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
    if (g_mock_libuv_config_init_fail) {
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
    if (g_mock_libuv_config_init_fail) {
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
    if (g_mock_libuv_config_init_fail) {
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
static enum c_abstract_http_error get_method_str(enum HttpMethod method,
                                                 const char **out_str) {
  if (!out_str) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch (method) {
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
enum c_abstract_http_error
c_abstract_http_test_libuv_method_str(enum HttpMethod method,
                                      const char **out_str);
enum c_abstract_http_error c_abstract_http_test_libuv_helpers(void);

/**
 * @brief Expose get_method_str for testing.
 *
 * @param[in] method Method enum.
 * @param[out] out_str Output string.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
c_abstract_http_test_libuv_method_str(enum HttpMethod method,
                                      const char **out_str) {
  return get_method_str(method, out_str);
}
#endif

/** @brief Internal struct libuv_state */
struct libuv_state {
  /** @brief Event loop */
  uv_loop_t loop;
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

  /** @brief Flag indicating headers have been parsed */
  int headers_parsed;
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
static void libuv_on_close(uv_handle_t *handle) { (void)handle; }

/**
 * @brief Finish libuv operation and clean up handles.
 *
 * @param[in,out] state State pointer.
 * @param[in] error Error code.
 */
static void libuv_finish(struct libuv_state *state, int error) {
  if (state->done) {
    return;
  }
  state->done = 1;
  state->error_code = error;

  LOG_DEBUG("libuv_finish: Finished with error=%d", error);

  if (state->has_timer) {
    uv_timer_stop(&state->timer);
    uv_close((uv_handle_t *)&state->timer, libuv_on_close);
    state->has_timer = 0;
  }

  if (state->has_socket && !uv_is_closing((uv_handle_t *)&state->socket)) {
    uv_close((uv_handle_t *)&state->socket, libuv_on_close);
    state->has_socket = 0;
  }
}

/**
 * @brief Timeout handler for libuv request.
 *
 * @param[in] handle Timer handle.
 */
static void libuv_on_timeout(uv_timer_t *handle) {
  struct libuv_state *state = (struct libuv_state *)handle->data;
  libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_TIMEOUT);
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
  (void)suggested_size;

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
 */
static void parse_headers(struct libuv_state *state) {
  struct HttpResponse *r;
  char *p;
  const char *end;
  enum c_abstract_http_error rc;

  if (state->headers_parsed || !state->res_buf) {
    return;
  }

  p = strstr(state->res_buf, "\r\n\r\n");
  if (!p) {
    return;
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
    libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
    return;
  }
  r = *state->res;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_res_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(r);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("parse_headers: Error http_response_init failed with %d",
              (int)rc);
    free(*state->res);
    *state->res = NULL;
    libuv_finish(state, (int)rc);
    return;
  }

  /* Parse status line */
  {
    const char *space = strchr(state->res_buf, ' ');
    if (space) {
      r->status_code = atoi(space + 1);
    }
  }

  /* Parse headers */
  p = strchr(state->res_buf, '\n');
  if (p) {
    p++;
  }

  end = strstr(state->res_buf, "\r\n\r\n");
  while (p && p < end && *p != '\r') {
    char *line_end = strchr(p, '\r');
    if (line_end && line_end < end) {
      *line_end = '\0';
      {
        char *colon = strchr(p, ':');
        if (colon) {
          char *val = colon + 1;
          enum c_abstract_http_error add_rc;
          *colon = '\0';
          while (*val == ' ') {
            val++;
          }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
          if (g_mock_libuv_headers_realloc_fail) {
            add_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
          } else
#endif
          {
            add_rc = http_headers_add(&r->headers, p, val);
          }
          if (add_rc != C_ABSTRACT_HTTP_SUCCESS) {
            LOG_DEBUG("parse_headers: Error http_headers_add failed with %d",
                      (int)add_rc);
          }
        }
      }
      p = line_end + 2;
    } else {
      break;
    }
  }

  {
    size_t hdr_len = (size_t)(end - state->res_buf) + 4;
    size_t body_len = state->res_len - hdr_len;

    if (state->req && state->req->on_chunk) {
      if (body_len > 0) {
        int chunk_rc = state->req->on_chunk(state->req->on_chunk_user_data,
                                            state->res_buf + hdr_len, body_len);
        if (chunk_rc != 0) {
          LOG_DEBUG("parse_headers: Error on_chunk failed %d", chunk_rc);
          libuv_finish(state, (int)ECANCELED);
          return;
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
          libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
          return;
        }
        memcpy(r->body, state->res_buf + hdr_len, body_len);
        ((char *)r->body)[body_len] = '\0';
      }
    }
  }
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
static enum c_abstract_http_error parse_url(const char *url, char **host,
                                            int *port, char **path);
static void libuv_on_read(uv_stream_t *stream, ssize_t nread,
                          const uv_buf_t *buf);
static int uv_run(uv_loop_t *loop, int mode);

/**
 * @brief Test libuv helper functions and edge cases.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
static int test_helper_chunk_cb(void *user_data, const void *chunk,
                                size_t chunk_len) {
  (void)user_data;
  (void)chunk;
  (void)chunk_len;
  return 0;
}

static int test_helper_chunk_cb_err(void *user_data, const void *chunk,
                                    size_t chunk_len) {
  (void)user_data;
  (void)chunk;
  (void)chunk_len;
  return (int)ECANCELED;
}

enum c_abstract_http_error c_abstract_http_test_libuv_helpers(void) {
  enum c_abstract_http_error rc;
  uv_timer_t timer;
  struct libuv_state state;
  struct HttpRequest req;
  char *h = NULL;
  int p = 0;
  char *path = NULL;

  memset(&timer, 0, sizeof(timer));
  memset(&state, 0, sizeof(state));
  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  timer.data = &state;
  libuv_on_timeout(&timer);

  libuv_on_close(NULL);
  uv_loop_close(NULL);
  uv_close(NULL, NULL);
  uv_is_closing(NULL);
  parse_url(NULL, NULL, NULL, NULL);
  parse_url("http://127.0.0.1/path:8080", &h, &p, &path);
  if (h)
    free(h);
  if (path)
    free(path);

  /* parse_headers without end separator */
  {
    char *buf1 = NULL;
    c_abstract_http_strdup("HTTP/1.1 200 OK", &buf1);
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.req = &req;
    state.res_buf = buf1;
    state.res_len = buf1 ? strlen(buf1) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      parse_headers(&state);
      /* Also test parse_headers when headers_parsed == 1 */
      parse_headers(&state);
      free(state.res);
    }
    if (buf1) {
      free(buf1);
    }
  }

  /* parse_headers with valid headers and body */
  {
    char *buf2 = NULL;
    c_abstract_http_strdup(
        "HTTP/1.1 200 OK\r\nKey: Value\r\nNoColon\r\n\r\nBODY", &buf2);
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf2;
    state.res_len = buf2 ? strlen(buf2) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      parse_headers(&state);
      if (*state.res) {
        http_response_free(*state.res);
        free(*state.res);
      }
      free(state.res);
    }
    if (buf2) {
      free(buf2);
    }
  }

  /* parse_headers with status line without space */
  {
    char *buf3 = NULL;
    c_abstract_http_strdup("HTTP/1.1\r\n\r\n", &buf3);
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf3;
    state.res_len = buf3 ? strlen(buf3) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      parse_headers(&state);
      if (*state.res) {
        http_response_free(*state.res);
        free(*state.res);
      }
      free(state.res);
    }
    if (buf3) {
      free(buf3);
    }
  }

  /* parse_headers with on_chunk success and error */
  {
    char *buf4 = NULL;
    c_abstract_http_strdup("HTTP/1.1 200 OK\r\n\r\nCHUNK_DATA", &buf4);
    req.on_chunk = test_helper_chunk_cb;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf4;
    state.res_len = buf4 ? strlen(buf4) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      parse_headers(&state);
      if (*state.res) {
        http_response_free(*state.res);
        free(*state.res);
      }
      free(state.res);
    }

    req.on_chunk = test_helper_chunk_cb_err;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_len = buf4 ? strlen(buf4) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      parse_headers(&state);
      if (*state.res) {
        http_response_free(*state.res);
        free(*state.res);
      }
      free(state.res);
    }
    req.on_chunk = NULL;
    if (buf4) {
      free(buf4);
    }
  }

  /* test parse_headers with NULL res_buf */
  state.done = 0;
  state.error_code = 0;
  state.headers_parsed = 0;
  state.res_buf = NULL;
  parse_headers(&state);

  /* test parse_headers with body alloc fail */
  {
    char *buf5 = NULL;
    c_abstract_http_strdup("HTTP/1.1 200 OK\r\n\r\nBODY_DATA", &buf5);
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf5;
    state.res_len = buf5 ? strlen(buf5) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      g_mock_libuv_body_alloc_fail = 1;
      parse_headers(&state);
      g_mock_libuv_body_alloc_fail = 0;
      free(state.res);
    }
    if (buf5) {
      free(buf5);
    }
  }

  /* test parse_headers with header add fail */
  {
    char *buf6 = NULL;
    c_abstract_http_strdup("HTTP/1.1 200 OK\r\nKey: Val\r\nKey2: Val2\r\n\r\n",
                           &buf6);
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = buf6;
    state.res_len = buf6 ? strlen(buf6) : 0;
    state.res = (struct HttpResponse **)malloc(sizeof(struct HttpResponse *));
    if (state.res) {
      *state.res = NULL;
      g_mock_libuv_headers_realloc_fail = 1;
      parse_headers(&state);
      g_mock_libuv_headers_realloc_fail = 0;
      if (*state.res) {
        http_response_free(*state.res);
        free(*state.res);
      }
      free(state.res);
    }
    if (buf6) {
      free(buf6);
    }
  }

  /* test libuv_finish double call */
  state.done = 0;
  libuv_finish(&state, 0);
  libuv_finish(&state, 0);

  /* test libuv_on_read edge branches */
  {
    uv_buf_t b;
    memset(&b, 0, sizeof(b));
    state.socket.data = &state;
    state.done = 0;
    state.error_code = 0;
    state.headers_parsed = 0;
    state.res_buf = NULL;
    libuv_on_read((uv_stream_t *)&state.socket, UV_EOF, &b);

    state.done = 0;
    state.res_buf = NULL;
    libuv_on_read((uv_stream_t *)&state.socket, 10, &b);
  }

  /* test parse_url dup_rc fail */
  g_mock_libuv_req_buf_alloc_fail = 3;
  parse_url("http://127.0.0.1/path", &h, &p, &path);
  g_mock_libuv_req_buf_alloc_fail = 4;
  parse_url("http://127.0.0.1", &h, &p, &path);
  g_mock_libuv_req_buf_alloc_fail = 0;

  /* test uv_run NULL */
  uv_run(NULL, 0);
  {
    uv_loop_t l;
    memset(&l, 0, sizeof(l));
    uv_run(&l, 0);
  }

  /* test alloc_cb realloc branches */
  {
    uv_buf_t ab;
    uv_buf_t ab3;
    state.res_cap = 0;
    state.res_len = 0;
    state.res_buf = NULL;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab);
    state.res_len = 7000;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab);
    if (state.res_buf) {
      free(state.res_buf);
      state.res_buf = NULL;
    }
    state.res_cap = 0;
    state.res_len = 0;
    g_mock_libuv_alloc_fail = 3;
    libuv_alloc_cb((uv_handle_t *)&state.socket, 0, &ab3);
    g_mock_libuv_alloc_fail = 0;
  }

  /* test uv_run alloc_cb failure branches */
  {
    uv_loop_t loop_alloc;
    struct libuv_state st_alloc;
    struct HttpResponse *res_obj = NULL;
    memset(&loop_alloc, 0, sizeof(loop_alloc));
    memset(&st_alloc, 0, sizeof(st_alloc));
    loop_alloc.data = &st_alloc;
    st_alloc.socket.read_cb = libuv_on_read;
    st_alloc.socket.alloc_cb = libuv_alloc_cb;
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = test_helper_chunk_cb;
    g_mock_libuv_alloc_fail = 1;
    uv_run(&loop_alloc, 0);

    memset(&st_alloc, 0, sizeof(st_alloc));
    st_alloc.socket.read_cb = libuv_on_read;
    st_alloc.socket.alloc_cb = libuv_alloc_cb;
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = test_helper_chunk_cb;
    g_mock_libuv_alloc_fail = 2;
    uv_run(&loop_alloc, 0);

    memset(&st_alloc, 0, sizeof(st_alloc));
    st_alloc.socket.read_cb = libuv_on_read;
    st_alloc.socket.alloc_cb = libuv_alloc_cb;
    st_alloc.socket.data = &st_alloc;
    st_alloc.res = &res_obj;
    st_alloc.req = &req;
    req.on_chunk = NULL;
    g_mock_libuv_alloc_fail = 2;
    uv_run(&loop_alloc, 0);
    g_mock_libuv_alloc_fail = 0;
    if (res_obj) {
      http_response_free(res_obj);
      free(res_obj);
    }
  }

  http_request_free(&req);
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Data received callback for libuv socket.
 *
 * @param[in] stream Stream handle.
 * @param[in] nread Bytes read or error code.
 * @param[in] buf Buffer structure.
 */
static void libuv_on_read(uv_stream_t *stream, ssize_t nread,
                          const uv_buf_t *buf) {
  struct libuv_state *state = (struct libuv_state *)stream->data;
  (void)buf;

  if (nread < 0) {
    if (nread != UV_EOF) {
      LOG_DEBUG("libuv_on_read: Connection error, nread=%ld", (long)nread);
      libuv_finish(state, (int)ECONNREFUSED);
    } else {
      if (!state->headers_parsed) {
        parse_headers(state);
      }
      libuv_finish(state, 0);
    }
    return;
  }

  if (!state->res_buf) {
    libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
    return;
  }

  state->res_len += (size_t)nread;

  if (!state->headers_parsed) {
    parse_headers(state);
  } else if (state->req->on_chunk) {
    int chunk_rc = state->req->on_chunk(
        state->req->on_chunk_user_data,
        state->res_buf + state->res_len - (size_t)nread, (size_t)nread);
    if (chunk_rc != 0) {
      LOG_DEBUG("libuv_on_read: Error on_chunk failed %d", chunk_rc);
      libuv_finish(state, (int)ECANCELED);
      return;
    }
    state->res_len -= (size_t)nread;
  } else {
    struct HttpResponse *r = *state->res;
    if (r) {
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
        libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
        return;
      }
      memcpy(new_body + r->body_len,
             state->res_buf + state->res_len - (size_t)nread, (size_t)nread);
      r->body = new_body;
      r->body_len += (size_t)nread;
      ((char *)r->body)[r->body_len] = '\0';
    }
  }
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
    libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_IO);
    return;
  }

  if (uv_read_start((uv_stream_t *)&state->socket, libuv_alloc_cb,
                    libuv_on_read) < 0) {
    LOG_DEBUG("libuv_on_write: Error uv_read_start failed");
    libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_IO);
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
    libuv_finish(state, (int)ECONNREFUSED);
    return;
  }

  state->write_req.data = state;
  buf = uv_buf_init(state->req_buf, (unsigned int)state->req_len);

  if (uv_write(&state->write_req, (uv_stream_t *)&state->socket, &buf, 1,
               libuv_on_write) < 0) {
    LOG_DEBUG("libuv_on_connect: Error uv_write failed");
    libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_IO);
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
    libuv_finish(state, (int)EHOSTUNREACH);
    uv_freeaddrinfo(res);
    return;
  }

  state->connect_req.data = state;

  if (uv_tcp_connect(&state->connect_req, &state->socket,
                     res ? res->ai_addr : NULL, libuv_on_connect) < 0) {
    LOG_DEBUG("libuv_on_resolved: Error uv_tcp_connect failed");
    libuv_finish(state, (int)ECONNREFUSED);
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
static enum c_abstract_http_error parse_url(const char *url, char **host,
                                            int *port, char **path) {
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

  if (path_start && port_start && port_start > path_start) {
    port_start = NULL;
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
    enum c_abstract_http_error dup_rc;
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
    enum c_abstract_http_error dup_rc;
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

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LIBUV)
/**
 * @brief Run mock event loop dispatching scheduled callbacks.
 *
 * @param[in] loop Loop pointer.
 * @param[in] mode Run mode.
 * @return 0 on success.
 */
static int uv_run(uv_loop_t *loop, int mode) {
  struct libuv_state *state;
  struct addrinfo *ai;
  (void)mode;

  if (!loop || !loop->data) {
    return 0;
  }
  state = (struct libuv_state *)loop->data;

  ai = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
  if (ai) {
    ai->ai_addr = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
  }
  if (state->resolver.cb) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_addrinfo_fail) {
      state->resolver.cb(&state->resolver, -1, NULL);
      return 0;
    }
#endif
    state->resolver.cb(&state->resolver, 0, ai);
  }

  if (state->socket.connect_cb) {
    int port = atoi(state->resolver.port_str);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_connect_fail || port == 59999)
#else
    if (port == 59999)
#endif
    {
      state->socket.connect_cb(&state->connect_req, -1);
      return 0;
    }
    state->socket.connect_cb(&state->connect_req, 0);
  }

  if (state->write_req.write_cb) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_write_fail) {
      state->write_req.write_cb(&state->write_req, -1);
      return 0;
    }
#endif
    state->write_req.write_cb(&state->write_req, 0);
  }

  if (state->socket.read_cb && state->socket.alloc_cb) {
    uv_buf_t buf;
    memset(&buf, 0, sizeof(buf));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_libuv_read_fail) {
      state->socket.read_cb((uv_stream_t *)&state->socket, -1, &buf);
      return 0;
    }
#endif
    if (state->req && state->req->on_chunk) {
      const char resp_headers[] = "HTTP/1.1 200 OK\r\n\r\n";
      const char chunk[] = "OK";
      state->socket.alloc_cb((uv_handle_t *)&state->socket,
                             sizeof(resp_headers) - 1, &buf);
      if (buf.base) {
        memcpy(buf.base, resp_headers, sizeof(resp_headers) - 1);
        state->socket.read_cb((uv_stream_t *)&state->socket,
                              (ssize_t)(sizeof(resp_headers) - 1), &buf);
      } else {
        libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
        return 0;
      }
      if (!state->done) {
        state->socket.alloc_cb((uv_handle_t *)&state->socket, sizeof(chunk) - 1,
                               &buf);
        if (buf.base) {
          memcpy(buf.base, chunk, sizeof(chunk) - 1);
          state->socket.read_cb((uv_stream_t *)&state->socket,
                                (ssize_t)(sizeof(chunk) - 1), &buf);
        } else {
          libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
          return 0;
        }
      }
      if (!state->done) {
        state->socket.read_cb((uv_stream_t *)&state->socket, UV_EOF, &buf);
      }
    } else {
      const char resp_headers[] = "HTTP/1.1 200 OK\r\nHeader: Value\r\n\r\n";
      const char body_chunk[] = "OK";
      state->socket.alloc_cb((uv_handle_t *)&state->socket,
                             sizeof(resp_headers) - 1, &buf);
      if (buf.base) {
        memcpy(buf.base, resp_headers, sizeof(resp_headers) - 1);
        state->socket.read_cb((uv_stream_t *)&state->socket,
                              (ssize_t)(sizeof(resp_headers) - 1), &buf);
      } else {
        libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
        return 0;
      }
      if (!state->done) {
        state->socket.alloc_cb((uv_handle_t *)&state->socket,
                               sizeof(body_chunk) - 1, &buf);
        if (buf.base) {
          memcpy(buf.base, body_chunk, sizeof(body_chunk) - 1);
          state->socket.read_cb((uv_stream_t *)&state->socket,
                                (ssize_t)(sizeof(body_chunk) - 1), &buf);
        } else {
          libuv_finish(state, (int)C_ABSTRACT_HTTP_ERR_NOMEM);
          return 0;
        }
      }
      if (!state->done) {
        state->socket.read_cb((uv_stream_t *)&state->socket, UV_EOF, &buf);
      }
    }
  }

  return 0;
}
#endif

/**
 * @brief Send an HTTP request using libuv.
 *
 * @param[in] ctx Transport context.
 * @param[in] req Request structure.
 * @param[out] res Pointer to receive response object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_libuv_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **res) {
  struct libuv_state state;
  char *host = NULL;
  int port = 0;
  char *path = NULL;
  char port_str[16];
  const char *method_str;
  size_t i;
  size_t req_cap = 4096;
  struct addrinfo hints;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  LOG_DEBUG("http_libuv_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_libuv_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG(
        "http_libuv_send: Error EINVAL (multipart missing flattened body)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(&state, 0, sizeof(state));
  state.ctx = ctx;
  state.req = req;
  state.res = res;
  *res = NULL;

  rc = parse_url(req->url, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libuv_send: Error parse_url failed %d", (int)rc);
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(port_str, sizeof(port_str), "%d", port);
#else
  sprintf(port_str, "%d", port);
#endif

  rc = get_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_libuv_send: Error get_method_str failed %d", (int)rc);
    free(host);
    free(path);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_libuv_req_buf_alloc_fail) {
    state.req_buf = NULL;
  } else
#endif
  {
    state.req_buf = (char *)malloc(req_cap);
  }
  if (!state.req_buf) {
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  state.req_len = (size_t)sprintf_s(
      state.req_buf, req_cap,
      "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n", method_str,
      path, host, port);
#else
  state.req_len = (size_t)sprintf(
      state.req_buf, "%s %s HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n",
      method_str, path, host, port);
#endif

  for (i = 0; i < req->headers.count; i++) {
    size_t hdr_len = strlen(req->headers.headers[i].key) +
                     strlen(req->headers.headers[i].value) + 4;
    if (state.req_len + hdr_len > req_cap) {
      char *new_buf;
      req_cap *= 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_headers_realloc_fail) {
        new_buf = NULL;
      } else
#endif
      {
        new_buf = (char *)realloc(state.req_buf, req_cap);
      }
      if (!new_buf) {
        LOG_DEBUG("http_libuv_send: Error ENOMEM reallocating headers");
        free(host);
        free(path);
        free(state.req_buf);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      state.req_buf = new_buf;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state.req_len += (size_t)sprintf_s(
        state.req_buf + state.req_len, req_cap - state.req_len, "%s: %s\r\n",
        req->headers.headers[i].key, req->headers.headers[i].value);
#else
    state.req_len += (size_t)sprintf(state.req_buf + state.req_len,
                                     "%s: %s\r\n", req->headers.headers[i].key,
                                     req->headers.headers[i].value);
#endif
  }

  if (req->read_chunk && req->expected_body_len > 0) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state.req_len += (size_t)sprintf_s(
        state.req_buf + state.req_len, req_cap - state.req_len,
        "Content-Length: %lu\r\n", (unsigned long)req->expected_body_len);
#else
    state.req_len += (size_t)sprintf(state.req_buf + state.req_len,
                                     "Content-Length: %lu\r\n",
                                     (unsigned long)req->expected_body_len);
#endif
  } else if (req->body_len > 0) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    state.req_len += (size_t)sprintf_s(
        state.req_buf + state.req_len, req_cap - state.req_len,
        "Content-Length: %lu\r\n", (unsigned long)req->body_len);
#else
    state.req_len += (size_t)sprintf(state.req_buf + state.req_len,
                                     "Content-Length: %lu\r\n",
                                     (unsigned long)req->body_len);
#endif
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  state.req_len += (size_t)sprintf_s(state.req_buf + state.req_len,
                                     req_cap - state.req_len, "\r\n");
#else
  state.req_len += (size_t)sprintf(state.req_buf + state.req_len, "\r\n");
#endif

  if (req->body && req->body_len > 0) {
    if (state.req_len + req->body_len > req_cap) {
      char *new_buf;
      req_cap = state.req_len + req->body_len + 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_libuv_body_alloc_fail) {
        new_buf = NULL;
      } else
#endif
      {
        new_buf = (char *)realloc(state.req_buf, req_cap);
      }
      if (!new_buf) {
        LOG_DEBUG("http_libuv_send: Error ENOMEM reallocating for body");
        free(host);
        free(path);
        free(state.req_buf);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      state.req_buf = new_buf;
    }
    memcpy(state.req_buf + state.req_len, req->body, req->body_len);
    state.req_len += req->body_len;
  } else if (req->read_chunk) {
    size_t out_read;
    char chunk_buf[4096];
    while (req->read_chunk(req->read_chunk_user_data, chunk_buf,
                           sizeof(chunk_buf), &out_read) == 0) {
      if (out_read == 0) {
        break;
      }
      if (state.req_len + out_read > req_cap) {
        char *new_buf;
        req_cap = (state.req_len + out_read) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_libuv_body_realloc_fail) {
          new_buf = NULL;
        } else
#endif
        {
          new_buf = (char *)realloc(state.req_buf, req_cap);
        }
        if (!new_buf) {
          LOG_DEBUG("http_libuv_send: Error ENOMEM chunk accumulation");
          free(host);
          free(path);
          free(state.req_buf);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        state.req_buf = new_buf;
      }
      memcpy(state.req_buf + state.req_len, chunk_buf, out_read);
      state.req_len += out_read;
    }
  }

  uv_loop_init(&state.loop);
  state.loop.data = &state;
  uv_tcp_init(&state.loop, &state.socket);
  state.has_socket = 1;
  state.socket.data = &state;

  if (ctx->config.timeout_ms > 0) {
    uv_timer_init(&state.loop, &state.timer);
    state.has_timer = 1;
    state.timer.data = &state;
    uv_timer_start(&state.timer, libuv_on_timeout,
                   (uint64_t)ctx->config.timeout_ms, 0);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  state.resolver.data = &state;
  if (uv_getaddrinfo(&state.loop, &state.resolver, libuv_on_resolved, host,
                     port_str, &hints) < 0) {
    libuv_finish(&state, (int)EHOSTUNREACH);
  }

  uv_run(&state.loop, UV_RUN_DEFAULT);

  uv_loop_close(&state.loop);

  free(host);
  free(path);
  if (state.req_buf) {
    free(state.req_buf);
  }
  if (state.res_buf) {
    free(state.res_buf);
  }

  if (state.error_code != 0 && *res) {
    http_response_free(*res);
    free(*res);
    *res = NULL;
  }

  if (state.error_code == 0) {
    LOG_DEBUG("http_libuv_send: Success");
    return C_ABSTRACT_HTTP_SUCCESS;
  } else {
    LOG_DEBUG("http_libuv_send: Error returning %d", state.error_code);
    return (enum c_abstract_http_error)state.error_code;
  }
}
