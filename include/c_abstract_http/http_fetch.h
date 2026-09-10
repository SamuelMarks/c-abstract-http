/**
 * @file http_fetch.h
 * @brief Libfetch implementation of the Abstract Network Interface (ANI).
 *
 * Provides functions to instantiate a transport context backed by libfetch.
 * Handles the mapping between the generic `HttpClient`/`HttpRequest` structures
 * and the specific `FETCH` handle options.
 *
 * Includes support for:
 * - Reference-counted global initialization.
 * - Configuration application (Timeouts, SSL, Proxy).
 * - Comprehensive error code mapping.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_FETCH_H
#define C_ABSTRACT_HTTP_HTTP_FETCH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Initialize the global fetch environment safely.
 *
 * Uses an internal reference counter to ensure fetch global initialization
 * occurs safely without race conditions.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_fetch_global_init(void);

/**
 * @brief Decrement the global initialization reference count.
 *
 * If the count reaches zero, global cleanup is invoked.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_fetch_global_cleanup(void);

/**
 * @brief Create a new Fetch-backed transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_fetch_context_init(struct HttpTransportContext **ctx);

/**
 * @brief Free the transport context.
 * Cleans up the internal FETCH handle and any cached configuration.
 *
 * @param[in] ctx The context to free. Safe to pass NULL.
 */
void http_fetch_context_free(struct HttpTransportContext *ctx);

/**
 * @brief Apply configuration settings to the FETCH context.
 *
 * Maps abstract `HttpConfig` settings (timeout, verify peer, proxy, user-agent)
 * to transport context options.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config The configuration structure to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_fetch_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config);

/**
 * @brief The send implementation for libfetch.
 * Matches `http_send_fn` signature.
 *
 * Performs the HTTP request using the stored FETCH handle.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The request to send.
 * @param[out] res Double pointer to receive the newly allocated response
 * object.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
http_fetch_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **res);

/**
 * @brief Dispatch multiple HTTP requests using libfetch asynchronous modes.
 *
 * Provides multiplexed execution over a single transport context.
 *
 * @param[in] ctx Pointer to an initialized HttpTransportContext.
 * @param[in,out] loop Event loop to drive the execution.
 * @param[in] multi Multi-request specification.
 * @param[out] futures Array of returned HttpFuture handles, allocated upon
 * success.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_fetch_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_FETCH_H */
