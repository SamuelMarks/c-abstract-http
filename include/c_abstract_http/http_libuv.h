/**
 * @file http_libuv.h
 * @brief libuv implementation of the Abstract Network Interface (ANI).
 *
 * Provides functions to instantiate a transport context backed by `libuv`
 * (`uv_tcp_t` + HTTP/1.1 framing and optional OpenSSL/mbedTLS memory BIOs).
 *
 * Includes support for:
 * - Reference-counted global initialization.
 * - Configuration application (Timeouts, TLS BIO verification/pinning, Proxy).
 * - Comprehensive error code mapping.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_LIBUV_H
#define C_ABSTRACT_HTTP_HTTP_LIBUV_H

/* clang-format off */
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Initialize the global libuv environment safely.
 *
 * Uses an internal reference counter to ensure global initialization runs once
 * on the first invocation and tears down only on the last.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libuv_global_init(void);

/**
 * @brief Decrement the global initialization reference count.
 *
 * Tears down global state when the reference count reaches zero.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libuv_global_cleanup(void);

/**
 * @brief Create a new libuv-backed transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or ENOMEM/EIO error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libuv_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free the transport context.
 * Cleans up the internal libuv loop/context and any cached configuration.
 *
 * @param[in] ctx The context to free. Safe to pass NULL.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libuv_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to the libuv transport context.
 *
 * Stores abstract `HttpConfig` settings (timeouts, TLS verification/pinning,
 * proxy, user-agent, redirects) for subsequent `uv_tcp_t` requests.
 *
 * @param[in] ctx The transport context.
 * @param[in] config The configuration structure to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_libuv_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config);

/**
 * @brief Synchronous send implementation for `libuv`.
 * Matches `http_send_fn` signature.
 *
 * Executes an HTTP/1.1 request over `uv_tcp_t` (with optional TLS memory-BIO
 * wrapping, HTTP proxy / `CONNECT` tunneling, redirect following, and chunked
 * streaming).
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the newly allocated response
 * object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or a mapped error code on
 * failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libuv_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **res);

/**
 * @brief Asynchronous multi-request dispatcher for libuv.
 *
 * Multiplexes concurrent TCP/TLS-BIO requests on a shared `uv_loop_t` bridged
 * with `ModalityEventLoop` via `uv_run(UV_RUN_NOWAIT)`.
 *
 * @param[in] ctx The transport context.
 * @param[in,out] loop Optional event loop to drive and wake on completion.
 * @param[in] multi Batch of HTTP requests to execute concurrently.
 * @param[out] futures Array of futures populated with responses/errors.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_libuv_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_LIBUV_H */
