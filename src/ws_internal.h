#ifndef C_ABSTRACT_HTTP_WS_INTERNAL_H
#define C_ABSTRACT_HTTP_WS_INTERNAL_H

/**
 * @file ws_internal.h
 * @brief Internal WebSocket definitions and parsing logic.
 */

/* clang-format off */
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/thread_pool.h>
#include "c_abstract_http/http_ws.h"
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Internal structure representing a parsed WebSocket frame header.
 */
struct ws_frame_header {
  int fin;                               /**< FIN bit flag */
  enum c_abstract_http_ws_opcode opcode; /**< Frame opcode */
  int mask;                              /**< Mask bit flag */
  uint64_t payload_len;                  /**< Parsed payload length */
  unsigned char masking_key[4];          /**< Masking key if present */
};

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
  enum ws_parser_state state;           /**< Current parser state */
  struct ws_frame_header current_frame; /**< In-progress frame header */
  unsigned char *payload_buffer;        /**< Dynamic buffer for the payload */
  size_t payload_capacity;              /**< Capacity of the payload buffer */
  size_t payload_offset; /**< Current write offset in payload buffer */
  size_t ext_len_offset; /**< Offset for reading extended lengths */
  unsigned char ext_len_buffer[8]; /**< Buffer for reading extended lengths */
  unsigned char mask_offset;       /**< Offset for reading the masking key */

  unsigned char *reassembly_buffer; /**< Buffer for fragmented messages */
  size_t reassembly_capacity;       /**< Capacity of the reassembly buffer */
  size_t reassembly_offset;         /**< Current offset in reassembly buffer */
  int in_fragment; /**< Non-zero while reassembling a fragmented message */
  enum c_abstract_http_ws_opcode
      fragment_opcode; /**< Initial opcode of active fragmented message */

  c_abstract_http_ws_on_message on_message; /**< User callback for messages */
  c_abstract_http_ws_on_error on_error;     /**< User callback for errors */
  c_abstract_http_ws_on_close on_close;     /**< User callback for closures */
  void *user_data;                          /**< Opaque user data */
};

/**
 * @brief Internal WebSocket stream queue, upgraded socket, and event-loop
 * state.
 */
struct ws_stream_ctx {
  struct AbstractHttpMutex *mutex; /**< Queue mutex */
  struct AbstractHttpCond *cond;   /**< Queue condition variable */
  unsigned char *queue;            /**< Outbound frame queue buffer */
  size_t queue_len;                /**< Current queued byte length */
  size_t queue_cap;                /**< Allocated queue capacity */
  int close_requested;             /**< Flag indicating close requested */
  int upgraded_fd;                 /**< Persistent upgraded socket descriptor */
  int has_upgraded_fd;             /**< Non-zero when `upgraded_fd` is owned */
  int loop_registered;  /**< Non-zero while `upgraded_fd` is on `loop` */
  int has_async_parser; /**< Non-zero when `async_parser` is active */
  int peer_closed;      /**< Non-zero once CLOSE frame received from peer */
  struct ModalityEventLoop *loop;            /**< Attached async event loop */
  struct HttpRequest *owner_req;             /**< Owning HTTP request handle */
  struct ws_parser_ctx async_parser;         /**< Stateful RFC 6455 parser */
  c_abstract_http_ws_on_message user_on_msg; /**< User message callback */
  c_abstract_http_ws_on_error user_on_err;   /**< User error callback */
  c_abstract_http_ws_on_close user_on_close; /**< User close callback */
  void *user_cb_data;                        /**< User callback context */
};

/**
 * @brief Portable host-to-network short (16-bit).
 * @param hostshort The host 16-bit integer.
 * @return The network 16-bit integer.
 */
C_ABSTRACT_HTTP_API uint16_t math_ws_htons(uint16_t hostshort);

/**
 * @brief Portable host-to-network long long (64-bit).
 * @param hostqword The host 64-bit integer.
 * @return The network 64-bit integer.
 */
C_ABSTRACT_HTTP_API uint64_t math_ws_htonll(uint64_t hostqword);

/**
 * @brief Portable network-to-host short (16-bit).
 * @param netshort The network 16-bit integer.
 * @return The host 16-bit integer.
 */
C_ABSTRACT_HTTP_API uint16_t math_ws_ntohs(uint16_t netshort);

/**
 * @brief Portable network-to-host long long (64-bit).
 * @param netqword The network 64-bit integer.
 * @return The host 64-bit integer.
 */
C_ABSTRACT_HTTP_API uint64_t math_ws_ntohll(uint64_t netqword);

/**
 * @brief Generate a random 4-byte masking key.
 * @param out_key The array to store the masking key.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
ws_generate_mask_key(unsigned char out_key[4]);

/**
 * @brief Apply a WebSocket mask to a payload in place.
 * @param payload The payload to mask/unmask.
 * @param len The length of the payload.
 * @param mask_key The 4-byte masking key.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t ws_apply_mask(
    unsigned char *payload, size_t len, const unsigned char mask_key[4]);

/**
 * @brief Pack a small WebSocket frame header (payload <= 125).
 * @param buf The buffer to write into (must be at least 2 bytes).
 * @param fin FIN flag.
 * @param opcode Frame opcode.
 * @param mask Mask flag.
 * @param len Payload length.
 * @param out_len Pointer to store the number of bytes written.
 * @return 0 on success, or an error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t ws_pack_header_small(
    unsigned char *buf, int fin, enum c_abstract_http_ws_opcode opcode,
    int mask, size_t len, size_t *out_len);

/**
 * @brief Pack a medium WebSocket frame header (126 <= payload <= 65535).
 * @param buf The buffer to write into (must be at least 4 bytes).
 * @param fin FIN flag.
 * @param opcode Frame opcode.
 * @param mask Mask flag.
 * @param len Payload length.
 * @param out_len Pointer to store the number of bytes written.
 * @return 0 on success, or an error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t ws_pack_header_medium(
    unsigned char *buf, int fin, enum c_abstract_http_ws_opcode opcode,
    int mask, size_t len, size_t *out_len);

/**
 * @brief Pack a large WebSocket frame header (payload > 65535).
 * @param buf The buffer to write into (must be at least 10 bytes).
 * @param fin FIN flag.
 * @param opcode Frame opcode.
 * @param mask Mask flag.
 * @param len Payload length.
 * @param out_len Pointer to store the number of bytes written.
 * @return 0 on success, or an error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t ws_pack_header_large(
    unsigned char *buf, int fin, enum c_abstract_http_ws_opcode opcode,
    int mask, size_t len, size_t *out_len);

/**
 * @brief Internal async task context for WebSocket reading.
 */
struct c_abstract_http_ws_async_ctx {
  /** @brief HTTP client handle */
  struct HttpClient *client;
  /** @brief HTTP request */
  struct HttpRequest *req;
  /** @brief User callback for messages */
  c_abstract_http_ws_on_message on_msg;
  /** @brief User callback for errors */
  c_abstract_http_ws_on_error on_err;
  /** @brief User callback for closures */
  c_abstract_http_ws_on_close on_close;
  /** @brief Opaque user data */
  void *user_data;
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
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
ws_parser_init(struct ws_parser_ctx *ctx, c_abstract_http_ws_on_message on_msg,
               c_abstract_http_ws_on_error on_err,
               c_abstract_http_ws_on_close on_cls, void *user_data);

/**
 * @brief Destroy a WebSocket parser context and free its buffers.
 * @param ctx The parser context to destroy.
 * @return 0 on success, negative error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
ws_parser_destroy(struct ws_parser_ctx *ctx);

/**
 * @brief Feed raw network bytes into the WebSocket parser.
 * @param ctx The parser context.
 * @param chunk The raw byte chunk from the network.
 * @param len The length of the chunk.
 * @return 0 on success, negative error code on protocol violation.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t ws_parser_feed(
    struct ws_parser_ctx *ctx, const unsigned char *chunk, size_t len);

/**
 * @brief Generate a random 24-character Base64 encoded client key.
 * @param out_key The output buffer (must hold 25 characters including
 * null-terminator).
 * @return 0 on success, negative error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
ws_generate_key(char out_key[25]);

/**
 * @brief Sign a client key with the RFC 6455 magic GUID and Base64 encode the
 * SHA-1 hash.
 * @param client_key The raw 24-character client key.
 * @param out_accept The output buffer (must hold 29 characters including
 * null-terminator).
 * @return 0 on success, negative error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
ws_sign_key(const char *client_key, char out_accept[29]);

/**
 * @brief Verify a server's Sec-WebSocket-Accept response against a given client
 * key.
 * @param client_key The original client key.
 * @param server_accept The server's response.
 * @return 0 on success, negative error code on failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
ws_verify_accept(const char *client_key, const char *server_accept);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_WS_INTERNAL_H */
