/**
 * @file http_raw.h
 * @brief Raw POSIX socket fallback implementation for HTTP client.
 */

#ifndef C_CDD_HTTP_RAW_H
#define C_CDD_HTTP_RAW_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Initialize the global raw socket environment.
 * @return 0 on success.
 */
extern int http_raw_global_init(void);

/**
 * @brief Cleanup the global raw socket environment.
 */
extern void http_raw_global_cleanup(void);

/**
 * @brief Initialize a new raw socket transport context.
 * @param ctx Pointer to the newly allocated context.
 * @return 0 on success.
 */
extern int http_raw_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free a raw socket transport context.
 * @param ctx Context to free.
 */
extern void http_raw_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Send an HTTP request synchronously using raw sockets.
 * @param ctx Transport context.
 * @param req Request to send.
 * @param res Response to return.
 * @return 0 on success.
 */
extern int http_raw_send(struct HttpTransportContext *ctx,
                         const struct HttpRequest *req,
                         struct HttpResponse **res);

/**
 * @brief Send multiple HTTP requests asynchronously using raw sockets.
 * @param ctx Transport context.
 * @param loop Event loop.
 * @param reqs Multiple requests to send.
 * @param future Futures to return.
 * @return 0 on success.
 */
extern int http_raw_send_multi(struct HttpTransportContext *ctx,
                               struct ModalityEventLoop *loop,
                               const struct HttpMultiRequest *reqs,
                               struct HttpFuture **future);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_CDD_HTTP_RAW_H */