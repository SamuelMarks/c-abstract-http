/**
 * @file http_libevent.h
 * @brief libevent implementation of the Abstract Network Interface (ANI).
 *
 * Provides functions to instantiate a transport context backed by `libevent`
 * (`evhttp`). Handles the mapping between the generic `HttpClient` and
 * `HttpRequest` structures and `evhttp_connection` / `evhttp_request` state.
 *
 * Includes support for:
 * - Reference-counted global initialization.
 * - Configuration application (Timeouts, OpenSSL bufferevent TLS, Proxy).
 * - Comprehensive error code mapping.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_LIBEVENT_H
#define C_ABSTRACT_HTTP_HTTP_LIBEVENT_H

/* clang-format off */
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Initialize the global libevent environment safely.
 *
 * Uses an internal reference counter to ensure global socket/library setup is
 * performed once on the first invocation and torn down only on the last.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libevent_global_init(void);

/**
 * @brief Decrement the global initialization reference count.
 *
 * Tears down global state when the reference count reaches zero.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libevent_global_cleanup(void);

/**
 * @brief Create a new libevent-backed transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or ENOMEM/EIO error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libevent_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free the transport context.
 * Cleans up the internal libevent handle and any cached configuration.
 *
 * @param[in] ctx The context to free. Safe to pass NULL.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libevent_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to the libevent transport context.
 *
 * Stores abstract `HttpConfig` settings (timeouts, TLS verification/pinning,
 * proxy, user-agent, redirects) for subsequent `evhttp` connections.
 *
 * @param[in] ctx The transport context.
 * @param[in] config The configuration structure to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libevent_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config);

/**
 * @brief Synchronous send implementation for `libevent`.
 * Matches `http_send_fn` signature.
 *
 * Dispatches an `evhttp_request` over an `event_base` connection (with optional
 * OpenSSL `bufferevent` TLS wrapping, redirect following, and streaming).
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the newly allocated response
 * object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or a mapped error code on
 * failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_libevent_send(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res);

/**
 * @brief Asynchronous multi-request dispatcher for libevent.
 *
 * Submits concurrent `evhttp_request` operations onto a shared `event_base` and
 * bridges non-blocking `event_base_loop(EVLOOP_NONBLOCK)` ticks onto
 * `ModalityEventLoop`.
 *
 * @param[in] ctx The transport context.
 * @param[in,out] loop Optional event loop to drive and wake on completion.
 * @param[in] multi Batch of HTTP requests to execute concurrently.
 * @param[out] futures Array of futures populated with responses/errors.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_libevent_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_LIBEVENT_H */
