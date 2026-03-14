/**
 * @file http_msh3.h
 * @brief MsH3 implementation of the Abstract Network Interface (ANI).
 *
 * Provides functions to instantiate a transport context backed by MsH3
 * (Microsoft HTTP/3).
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_MSH3_H
#define C_CDD_HTTP_MSH3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>

/**
 * @brief Opaque context definition.
 */
struct HttpTransportContext;

/**
 * @brief Initialize the global MsH3 API state.
 * Reference-counted; safe to call multiple times.
 * @return 0 on success, ENOMEM or native error code on failure.
 */
extern int http_msh3_global_init(void);

/**
 * @brief Clean up the global MsH3 API state.
 * Automatically cleans up when the reference count drops to 0.
 */
extern void http_msh3_global_cleanup(void);

/**
 * @brief Initialize a new MsH3 transport context.
 *
 * @param[out] ctx Double pointer to receive the newly allocated context.
 * @return 0 on success, ENOMEM on failure.
 */
extern int http_msh3_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free a MsH3 transport context and its associated resources.
 *
 * @param[in] ctx The context to free.
 */
extern void http_msh3_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to an MsH3 transport context.
 *
 * Maps abstract settings (like timeouts and SSL verification) to their
 * MsH3 equivalents.
 *
 * @param[in,out] ctx The target context.
 * @param[in] config The configuration settings.
 * @return 0 on success, or a mapped error code on failure.
 */
extern int http_msh3_config_apply(struct HttpTransportContext *ctx,
                                  const struct HttpConfig *config);

/**
 * @brief Synchronous single request dispatcher for MsH3.
 * Matches `http_send_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the newly allocated response.
 * @return 0 on success, or a mapped error code on failure.
 */
extern int http_msh3_send(struct HttpTransportContext *ctx,
                          const struct HttpRequest *req,
                          struct HttpResponse **res);

/**
 * @brief Asynchronous multi-send implementation for MsH3.
 * Matches `http_send_multi_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] loop The event loop context.
 * @param[in] multi The multi request structure.
 * @param[out] futures Array of futures.
 * @return 0 on success.
 */
extern int http_msh3_send_multi(struct HttpTransportContext *ctx,
                                struct ModalityEventLoop *loop,
                                const struct HttpMultiRequest *multi,
                                struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_CDD_HTTP_MSH3_H */
