/* clang-format off */
#ifndef C_ABSTRACT_HTTP_SSE_INTERNAL_H
#define C_ABSTRACT_HTTP_SSE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Internal SSE definitions */

#include "c_abstract_http/http_sse.h"
#include <stddef.h>

struct sse_parser_ctx {
    char* line_buffer;
    size_t line_capacity;
    size_t line_offset;
    
    char* current_event;
    char* current_data;
    size_t data_capacity;
    size_t data_offset;
    
    char* last_event_id;
    int retry_ms;

    c_abstract_http_sse_on_event on_event;
    c_abstract_http_sse_on_error on_error;
    c_abstract_http_sse_on_close on_close;
    void* user_data;
};

int sse_parser_init(struct sse_parser_ctx* ctx, const struct c_abstract_http_sse_config* config, c_abstract_http_sse_on_event on_evt, c_abstract_http_sse_on_error on_err, c_abstract_http_sse_on_close on_cls, void* user_data);
void sse_parser_destroy(struct sse_parser_ctx* ctx);
int sse_parser_feed(struct sse_parser_ctx* ctx, const char* chunk, size_t len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_SSE_INTERNAL_H */
/* clang-format on */
