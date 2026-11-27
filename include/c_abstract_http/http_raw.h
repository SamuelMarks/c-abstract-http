/**
 * @file http_raw.h
 * @brief Raw POSIX socket fallback implementation for HTTP client.
 */

#ifndef C_ABSTRACT_HTTP_HTTP_RAW_H
#define C_ABSTRACT_HTTP_HTTP_RAW_H

/* clang-format off */
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Initialize the global raw socket environment.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_raw_global_init(void);

/**
 * @brief Cleanup the global raw socket environment.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_raw_global_cleanup(void);

/**
 * @brief Initialize a new raw socket transport context.
 * @param ctx Pointer to the newly allocated context.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_raw_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free a raw socket transport context.
 * @param ctx Context to free.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_raw_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration to a raw socket transport context.
 * @param ctx Transport context.
 * @param config Configuration to apply.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_raw_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config);

/**
 * @brief Pluggable TLS session hooks for the raw socket HTTP backend.
 *
 * Allows callers to override or supply a TLS engine on any platform.
 * For non-blocking integration, `handshake`, `read`, and `write` set
 * `*out_want` to `0` on progress/completion, `1` when waiting for socket
 * readability, or `2` when waiting for socket writability.
 */
struct HttpRawTlsHooks {
  /** @brief Opaque provider context passed to `open` */
  void *provider_data;
  /** @brief Allocate and attach a TLS session to connected `sock_fd` */
  NO_DISCARD_FN_PTR c_abstract_http_error_t (*open)(
      void *provider_data, int sock_fd, const char *host,
      const struct HttpConfig *config, void **out_session);
  /** @brief Drive TLS client handshake (`*out_want`: 0=done, 1=read, 2=write)
   */
  NO_DISCARD_FN_PTR c_abstract_http_error_t (*handshake)(void *session,
                                                         int *out_want);
  /** @brief Read decrypted bytes (`*out_want`: 0=ok/eof, 1=read, 2=write) */
  NO_DISCARD_FN_PTR c_abstract_http_error_t (*read)(void *session, void *buf,
                                                    size_t cap, size_t *out_n,
                                                    int *out_want);
  /** @brief Write plaintext bytes (`*out_want`: 0=ok, 1=read, 2=write) */
  NO_DISCARD_FN_PTR c_abstract_http_error_t (*write)(void *session,
                                                     const void *buf,
                                                     size_t len, size_t *out_n,
                                                     int *out_want);
  /** @brief Shut down and free a TLS session */
  NO_DISCARD_FN_PTR c_abstract_http_error_t (*close)(void *session);
};

/**
 * @brief Register or clear custom TLS hooks on a raw socket transport context.
 * @param ctx Transport context.
 * @param hooks Optional custom TLS hooks to copy, or NULL to use native
 * OS/crypto TLS.
 * @return 0 on success, or C_ABSTRACT_HTTP_ERR_INVAL if @p ctx or required hook
 * pointers are invalid.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_raw_set_tls_hooks(
    struct HttpTransportContext *ctx, const struct HttpRawTlsHooks *hooks);

/**
 * @brief Send an HTTP request synchronously using raw sockets.
 * @param ctx Transport context.
 * @param req Request to send.
 * @param res Response to return.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_raw_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
              struct HttpResponse **res);

/**
 * @brief Send multiple HTTP requests asynchronously using raw sockets.
 * @param ctx Transport context.
 * @param loop Event loop.
 * @param reqs Multiple requests to send.
 * @param future Futures to return.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_raw_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *reqs, struct HttpFuture **future);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_RAW_H */
