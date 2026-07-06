/**
 * @file transport.h
 * @brief Transport global operations.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TRANSPORT_H
#define C_ABSTRACT_HTTP_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Initialize the global transport layer.
 * @return 0 on success, error code on failure.
 */
enum c_abstract_http_error transport_global_init(void);

/**
 * @brief Clean up the global transport layer.
 */
enum c_abstract_http_error transport_global_cleanup(void);

/**
 * @brief Initialize a new HTTP client transport.
 * @param[in,out] client The HTTP client to initialize.
 * @return 0 on success, error code on failure.
 */
enum c_abstract_http_error
transport_factory_init_client(struct HttpClient *client);

/**
 * @brief Clean up an HTTP client transport.
 * @param[in] client The HTTP client to clean up.
 */
enum c_abstract_http_error
transport_factory_cleanup_client(struct HttpClient *client);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_TRANSPORT_H */
