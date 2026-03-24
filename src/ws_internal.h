/* clang-format off */
#ifndef C_ABSTRACT_HTTP_WS_INTERNAL_H
#define C_ABSTRACT_HTTP_WS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Internal WS definitions */

#include "c_abstract_http/http_ws.h"
#include <stdint.h>

struct ws_frame_header {
    int fin;
    int opcode;
    int mask;
    uint64_t payload_len;
    unsigned char masking_key[4];
};

uint16_t ws_htons(uint16_t hostshort);
uint64_t ws_htonll(uint64_t hostqword);
uint16_t ws_ntohs(uint16_t netshort);
uint64_t ws_ntohll(uint64_t netqword);

int ws_generate_mask_key(unsigned char out_key[4]);
int ws_apply_mask(unsigned char* payload, size_t len, const unsigned char mask_key[4]);

int ws_pack_header_small(unsigned char* buf, int fin, int opcode, int mask, size_t len);
int ws_pack_header_medium(unsigned char* buf, int fin, int opcode, int mask, size_t len);
int ws_pack_header_large(unsigned char* buf, int fin, int opcode, int mask, size_t len);

enum ws_parser_state {
    WS_PARSER_READ_OPCODE,
    WS_PARSER_READ_LEN,
    WS_PARSER_READ_EXT_LEN_16,
    WS_PARSER_READ_EXT_LEN_64,
    WS_PARSER_READ_MASK,
    WS_PARSER_READ_PAYLOAD
};

struct ws_parser_ctx {
    enum ws_parser_state state;
    struct ws_frame_header current_frame;
    unsigned char* payload_buffer;
    size_t payload_capacity;
    size_t payload_offset;
    size_t ext_len_offset;
    unsigned char ext_len_buffer[8];
    unsigned char mask_offset;
    
    unsigned char* reassembly_buffer;
    size_t reassembly_capacity;
    size_t reassembly_offset;

    c_abstract_http_ws_on_message on_message;
    c_abstract_http_ws_on_error on_error;
    c_abstract_http_ws_on_close on_close;
    void* user_data;
};

int ws_parser_init(struct ws_parser_ctx* ctx, c_abstract_http_ws_on_message on_msg, c_abstract_http_ws_on_error on_err, c_abstract_http_ws_on_close on_cls, void* user_data);
void ws_parser_destroy(struct ws_parser_ctx* ctx);
int ws_parser_feed(struct ws_parser_ctx* ctx, const unsigned char* chunk, size_t len);

int ws_generate_key(char out_key[25]);
int ws_sign_key(const char* client_key, char out_accept[29]);
int ws_verify_accept(const char* client_key, const char* server_accept);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_WS_INTERNAL_H */
/* clang-format on */
