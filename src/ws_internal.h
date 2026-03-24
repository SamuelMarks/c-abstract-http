/* clang-format off */
#ifndef C_ABSTRACT_HTTP_WS_INTERNAL_H
#define C_ABSTRACT_HTTP_WS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file ws_internal.h
 * @brief Internal WebSocket definitions and parsing logic.
 */

#include "c_abstract_http/http_ws.h"
#include <stdint.h>

/**
 * @brief Internal structure representing a parsed WebSocket frame header.
 */
struct ws_frame_header {
    int fin;                       /**< FIN bit flag */
    int opcode;                    /**< Frame opcode */
    int mask;                      /**< Mask bit flag */
    uint64_t payload_len;          /**< Parsed payload length */
    unsigned char masking_key[4];  /**< Masking key if present */
};

/**
 * @brief Portable host-to-network short (16-bit).
 * @param hostshort The host 16-bit integer.
 * @return The network 16-bit integer.
 */
uint16_t ws_htons(uint16_t hostshort);

/**
 * @brief Portable host-to-network long long (64-bit).
 * @param hostqword The host 64-bit integer.
 * @return The network 64-bit integer.
 */
uint64_t ws_htonll(uint64_t hostqword);

/**
 * @brief Portable network-to-host short (16-bit).
 * @param netshort The network 16-bit integer.
 * @return The host 16-bit integer.
 */
uint16_t ws_ntohs(uint16_t netshort);

/**
 * @brief Portable network-to-host long long (64-bit).
 * @param netqword The network 64-bit integer.
 * @return The host 64-bit integer.
 */
uint64_t ws_ntohll(uint64_t netqword);

/**
 * @brief Generate a random 4-byte masking key.
 * @param out_key The array to store the masking key.
 * @return 0 on success.
 */
int ws_generate_mask_key(unsigned char out_key[4]);

/**
 * @brief Apply a WebSocket mask to a payload in place.
 * @param payload The payload to mask/unmask.
 * @param len The length of the payload.
 * @param mask_key The 4-byte masking key.
 * @return 0 on success.
 */
int ws_apply_mask(unsigned char* payload, size_t len, const unsigned char mask_key[4]);

/**
 * @brief Pack a small WebSocket frame header (payload <= 125).
 * @param buf The buffer to write into (must be at least 2 bytes).
 * @param fin FIN flag.
 * @param opcode Frame opcode.
 * @param mask Mask flag.
 * @param len Payload length.
 * @return Number of bytes written.
 */
int ws_pack_header_small(unsigned char* buf, int fin, int opcode, int mask, size_t len);

/**
 * @brief Pack a medium WebSocket frame header (126 <= payload <= 65535).
 * @param buf The buffer to write into (must be at least 4 bytes).
 * @param fin FIN flag.
 * @param opcode Frame opcode.
 * @param mask Mask flag.
 * @param len Payload length.
 * @return Number of bytes written.
 */
int ws_pack_header_medium(unsigned char* buf, int fin, int opcode, int mask, size_t len);

/**
 * @brief Pack a large WebSocket frame header (payload > 65535).
 * @param buf The buffer to write into (must be at least 10 bytes).
 * @param fin FIN flag.
 * @param opcode Frame opcode.
 * @param mask Mask flag.
 * @param len Payload length.
 * @return Number of bytes written.
 */
int ws_pack_header_large(unsigned char* buf, int fin, int opcode, int mask, size_t len);

/**
 * @brief State machine enum for the WebSocket parser.
 */
enum ws_parser_state {
    WS_PARSER_READ_OPCODE,
    WS_PARSER_READ_LEN,
    WS_PARSER_READ_EXT_LEN_16,
    WS_PARSER_READ_EXT_LEN_64,
    WS_PARSER_READ_MASK,
    WS_PARSER_READ_PAYLOAD
};

/**
 * @brief Context structure for parsing incoming WebSocket frames.
 */
struct ws_parser_ctx {
    enum ws_parser_state state;            /**< Current parser state */
    struct ws_frame_header current_frame;  /**< In-progress frame header */
    unsigned char* payload_buffer;         /**< Dynamic buffer for the payload */
    size_t payload_capacity;               /**< Capacity of the payload buffer */
    size_t payload_offset;                 /**< Current write offset in payload buffer */
    size_t ext_len_offset;                 /**< Offset for reading extended lengths */
    unsigned char ext_len_buffer[8];       /**< Buffer for reading extended lengths */
    unsigned char mask_offset;             /**< Offset for reading the masking key */
    
    unsigned char* reassembly_buffer;      /**< Buffer for fragmented messages */
    size_t reassembly_capacity;            /**< Capacity of the reassembly buffer */
    size_t reassembly_offset;              /**< Current offset in reassembly buffer */

    c_abstract_http_ws_on_message on_message; /**< User callback for messages */
    c_abstract_http_ws_on_error on_error;     /**< User callback for errors */
    c_abstract_http_ws_on_close on_close;     /**< User callback for closures */
    void* user_data;                          /**< Opaque user data */
};

/**
 * @brief Initialize a WebSocket parser context.
 * @param ctx The parser context to initialize.
 * @param on_msg The callback for completed messages.
 * @param on_err The callback for parsing errors.
 * @param on_cls The callback for connection closures.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, negative error code on failure.
 */
int ws_parser_init(struct ws_parser_ctx* ctx, c_abstract_http_ws_on_message on_msg, c_abstract_http_ws_on_error on_err, c_abstract_http_ws_on_close on_cls, void* user_data);

/**
 * @brief Destroy a WebSocket parser context and free its buffers.
 * @param ctx The parser context to destroy.
 */
void ws_parser_destroy(struct ws_parser_ctx* ctx);

/**
 * @brief Feed raw network bytes into the WebSocket parser.
 * @param ctx The parser context.
 * @param chunk The raw byte chunk from the network.
 * @param len The length of the chunk.
 * @return 0 on success, negative error code on protocol violation.
 */
int ws_parser_feed(struct ws_parser_ctx* ctx, const unsigned char* chunk, size_t len);

/**
 * @brief Generate a random 24-character Base64 encoded client key.
 * @param out_key The output buffer (must hold 25 characters including null-terminator).
 * @return 0 on success, negative error code on failure.
 */
int ws_generate_key(char out_key[25]);

/**
 * @brief Sign a client key with the RFC 6455 magic GUID and Base64 encode the SHA-1 hash.
 * @param client_key The raw 24-character client key.
 * @param out_accept The output buffer (must hold 29 characters including null-terminator).
 * @return 0 on success, negative error code on failure.
 */
int ws_sign_key(const char* client_key, char out_accept[29]);

/**
 * @brief Verify a server's Sec-WebSocket-Accept response against a given client key.
 * @param client_key The original client key.
 * @param server_accept The server's response.
 * @return 0 on success, negative error code on failure.
 */
int ws_verify_accept(const char* client_key, const char* server_accept);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_WS_INTERNAL_H */
/* clang-format on */
