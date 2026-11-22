/**
 * @file http_lsquic.h
 * @brief LiteSpeed QUIC (lsquic) HTTP/3 implementation of the Abstract Network
 * Interface (ANI).
 *
 * Provides functions to instantiate a transport context backed by LiteSpeed
 * QUIC (`lsquic`) for HTTP/3 over UDP QUIC with optional TCP fallback.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_LSQUIC_H
#define C_ABSTRACT_HTTP_HTTP_LSQUIC_H

/* clang-format off */
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Opaque transport context definition.
 */
struct HttpTransportContext;

/**
 * @brief Initialize global LiteSpeed QUIC library state.
 * Reference-counted; safe to call multiple times.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_global_init(void);

/**
 * @brief Clean up global LiteSpeed QUIC library state.
 * Automatically cleans up when the reference count drops to 0.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_global_cleanup(void);

/**
 * @brief Initialize a new LiteSpeed QUIC transport context.
 *
 * @param[out] ctx Double pointer to receive the newly allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or C_ABSTRACT_HTTP_ERR_NOMEM on
 * failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free a LiteSpeed QUIC transport context and its associated resources.
 *
 * @param[in] ctx The context to free.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to a LiteSpeed QUIC transport context.
 *
 * Maps abstract settings (timeouts, TLS verification, user_agent, MASQUE/proxy
 * settings, cookie jar, and HTTP/3 TCP fallback) to the lsquic engine context.
 *
 * @param[in,out] ctx The target context.
 * @param[in] config The configuration settings.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or a mapped error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_config_apply(struct HttpTransportContext *ctx,
                         const struct HttpConfig *config);

/**
 * @brief Synchronous single request dispatcher for LiteSpeed QUIC.
 * Matches `http_send_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the newly allocated response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or a mapped error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_send(struct HttpTransportContext *ctx,
                 const struct HttpRequest *req, struct HttpResponse **res);

/**
 * @brief Asynchronous multiplexed multi-request dispatcher for LiteSpeed QUIC.
 * Matches `http_send_multi_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] loop Optional event loop context to notify on completion.
 * @param[in] multi Batch of HTTP requests to multiplex over QUIC streams.
 * @param[out] futures Array of futures populated with responses/errors.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or a mapped error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
http_lsquic_send_multi(struct HttpTransportContext *ctx,
                       struct ModalityEventLoop *loop,
                       const struct HttpMultiRequest *multi,
                       struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_HTTP_LSQUIC_H */
