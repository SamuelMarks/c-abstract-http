/**
 * @file http_raw.h
 * @brief Raw POSIX socket fallback implementation for HTTP client.
 */

#ifndef C_CDD_HTTP_RAW_H
#define C_CDD_HTTP_RAW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>

extern int http_raw_global_init(void);
extern void http_raw_global_cleanup(void);
extern int http_raw_context_init(struct HttpTransportContext **ctx);
extern void http_raw_context_free(struct HttpTransportContext *ctx);
extern int http_raw_send(struct HttpTransportContext *ctx,
                         const struct HttpRequest *req,
                         struct HttpResponse **res);
extern int http_raw_send_multi(struct HttpTransportContext *ctx,
                               struct ModalityEventLoop *loop,
                               const struct HttpMultiRequest *reqs,
                               struct HttpFuture **future);

#ifdef __cplusplus
}
#endif

#endif
