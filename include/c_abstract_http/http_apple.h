/**
 * @file http_apple.h
 * @brief Apple CFNetwork implementation of the Abstract Network Interface.
 *
 * Implements the HTTP transport using Apple's CFNetwork framework.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_APPLE_H
#define C_ABSTRACT_HTTP_HTTP_APPLE_H

/* clang-format off */
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Initialize global Apple networking environment.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_apple_global_init(void);

/**
 * @brief Cleanup global Apple networking environment.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_apple_global_cleanup(void);

/**
 * @brief Create a new Apple-backed transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_apple_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free the transport context.
 *
 * @param[in] ctx The context to free. Satisfies NULL-safety.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_apple_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to the Apple session.
 *
 * @param[in] ctx The transport context.
 * @param[in] config The configuration to apply.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_apple_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config);

/**
 * @brief Select between the modern NSURLSession C-bridge and legacy CFStream.
 *
 * When compiled with `MACOSX_DEPLOYMENT_TARGET >= 10.11`, deprecated
 * `CFReadStreamCreateForHTTPRequest` APIs are omitted at compile time and
 * attempting to disable `NSURLSession` returns `C_ABSTRACT_HTTP_ERR_NOTSUP`.
 *
 * @param[in] ctx The transport context.
 * @param[in] enabled Non-zero to dispatch via `NSURLSession`, 0 for `CFStream`.
 * @return `C_ABSTRACT_HTTP_SUCCESS` on success, or an error enum.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_apple_set_use_nsurlsession(struct HttpTransportContext *ctx, int enabled);

/**
 * @brief The send implementation for Apple.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the allocated response object.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_apple_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **res);

/**
 * @brief The send_multi implementation for Apple.
 * Matches `http_send_multi_fn` signature.
 *
 * @param[in] ctx The transport context.
 * @param[in] loop Optional event loop for asynchronous notification.
 * @param[in] multi Container holding multiple requests.
 * @param[out] futures Array of future handles tracking per-request completion.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_apple_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_APPLE_H */
