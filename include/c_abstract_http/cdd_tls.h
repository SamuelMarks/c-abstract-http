#ifndef C_CDD_HTTP_TLS_H
#define C_CDD_HTTP_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#include <stddef.h>
/* clang-format on */

struct CddTlsKey;

extern int cdd_tls_key_create(struct CddTlsKey **key,
                              void (*destructor)(void *));
extern int cdd_tls_set(struct CddTlsKey *key, void *value);
extern int cdd_tls_get(struct CddTlsKey *key, void **out_value);
extern void cdd_tls_key_delete(struct CddTlsKey *key);

#ifdef __cplusplus
}
#endif

#endif
