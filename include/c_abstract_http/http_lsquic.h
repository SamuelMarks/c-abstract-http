/**
 * @file http_lsquic.h
 * @brief LiteSpeed QUIC (lsquic) implementation of the Abstract Network
 * Interface.
 *
 * Provides functions to instantiate a transport context backed by lsquic
 * for high-performance HTTP/3 communication.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_LSQUIC_H
#define C_CDD_HTTP_LSQUIC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>

/**
 * @brief Opaque context definition for lsquic backend.
 */
struct HttpTransportContext;

/**
 * @brief Initialize the global lsquic API state.
 * Reference-counted; safe to call multiple times.
 * @return 0 on success, ENOMEM or native error code on failure.
 */
extern int http_lsquic_global_init(void);

/**
 * @brief Clean up the global lsquic API state.
 * Automatically cleans up when the reference count drops to 0.
 */
extern void http_lsquic_global_cleanup(void);

/**
 * @brief Initialize a new lsquic transport context.
 *
 * @param[out] ctx Double pointer to receive the newly allocated context.
 * @return 0 on success, ENOMEM on failure.
 */
extern int http_lsquic_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free a lsquic transport context and its associated resources.
 *
 * @param[in] ctx The context to free.
 */
extern void http_lsquic_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to an lsquic transport context.
 *
 * Maps abstract settings to lsquic engine settings.
 *
 * @param[in,out] ctx The target context.
 * @param[in] config The configuration settings.
 * @return 0 on success, or a mapped error code on failure.
 */
extern int http_lsquic_config_apply(struct HttpTransportContext *ctx,
                                    const struct HttpConfig *config);

/**
 * @brief Synchronous single request dispatcher for lsquic.
 * Matches `http_send_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the newly allocated response.
 * @return 0 on success, or a mapped error code on failure.
 */
extern int http_lsquic_send(struct HttpTransportContext *ctx,
                            const struct HttpRequest *req,
                            struct HttpResponse **res);

/**
 * @brief Asynchronous multi-send implementation for lsquic.
 * Matches `http_send_multi_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] loop The event loop context.
 * @param[in] multi The multi request structure.
 * @param[out] futures Array of futures.
 * @return 0 on success.
 */
extern int http_lsquic_send_multi(struct HttpTransportContext *ctx,
                                  struct ModalityEventLoop *loop,
                                  const struct HttpMultiRequest *multi,
                                  struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_CDD_HTTP_LSQUIC_H */
