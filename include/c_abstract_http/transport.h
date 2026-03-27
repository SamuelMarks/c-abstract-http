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

int transport_global_init(void);
void transport_global_cleanup(void);
int transport_factory_init_client(struct HttpClient *client);
void transport_factory_cleanup_client(struct HttpClient *client);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TRANSPORT_H */