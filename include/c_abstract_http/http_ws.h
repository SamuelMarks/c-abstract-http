/* clang-format off */
#ifndef C_ABSTRACT_HTTP_WS_H
#define C_ABSTRACT_HTTP_WS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "c_abstract_http/http_types.h"
#include "c_abstract_http/win_compat_sym.h"

#ifdef _WIN32
#include <windef.h>
#endif

#ifdef _MSC_VER
#define C_ABSTRACT_HTTP_STRCPY_S(dest, sz, src) strcpy_s(dest, sz, src)
#else
#define C_ABSTRACT_HTTP_STRCPY_S(dest, sz, src) strcpy(dest, src)
#endif

/* WebSocket public API definitions will go here */

/**
 * @brief WebSocket opcodes per RFC 6455.
 */
enum c_abstract_http_ws_opcode {
    C_ABSTRACT_HTTP_WS_OPCODE_CONTINUATION = 0x0,
    C_ABSTRACT_HTTP_WS_OPCODE_TEXT = 0x1,
    C_ABSTRACT_HTTP_WS_OPCODE_BINARY = 0x2,
    C_ABSTRACT_HTTP_WS_OPCODE_CLOSE = 0x8,
    C_ABSTRACT_HTTP_WS_OPCODE_PING = 0x9,
    C_ABSTRACT_HTTP_WS_OPCODE_PONG = 0xA
};

/**
 * @brief WebSocket connection state.
 */
enum c_abstract_http_ws_state {
    C_ABSTRACT_HTTP_WS_STATE_CONNECTING = 0,
    C_ABSTRACT_HTTP_WS_STATE_OPEN,
    C_ABSTRACT_HTTP_WS_STATE_CLOSING,
    C_ABSTRACT_HTTP_WS_STATE_CLOSED
};

/**
 * @brief Represents a parsed WebSocket event/message.
 */
struct c_abstract_http_ws_event {
    enum c_abstract_http_ws_opcode opcode; /**< Frame opcode */
    const unsigned char* payload;          /**< Pointer to payload data */
    size_t payload_len;                    /**< Length of payload data */
    int is_fin;                            /**< Non-zero if FIN bit is set */
};

/**
 * @brief Configuration for a WebSocket connection.
 */
struct c_abstract_http_ws_config {
    const char** custom_headers; /**< Null-terminated array of custom headers */
    const char* subprotocols;    /**< Comma-separated list of subprotocols */
    int masking_preference;      /**< 1 to enable client masking, 0 otherwise */
};

/**
 * @brief Callback invoked when a complete WebSocket message is received.
 *
 * @param ev Pointer to the WebSocket event containing the message data.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, or a negative error code.
 */
typedef int (*c_abstract_http_ws_on_message)(const struct c_abstract_http_ws_event* ev, void* user_data);

/**
 * @brief Callback invoked when a WebSocket error occurs.
 *
 * @param error_code The specific error code.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, or a negative error code.
 */
typedef int (*c_abstract_http_ws_on_error)(int error_code, void* user_data);

/**
 * @brief Callback invoked when a WebSocket connection is closed.
 *
 * @param status_code The closure status code.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, or a negative error code.
 */
typedef int (*c_abstract_http_ws_on_close)(int status_code, void* user_data);

/**
 * @brief Initialize a WebSocket request.
 *
 * @param req The HTTP request to initialize.
 * @param config Optional WebSocket configuration.
 * @return 0 on success, or a negative error code.
 */
int c_abstract_http_ws_init(struct HttpRequest* req, const struct c_abstract_http_ws_config* config);

/**
 * @brief Run a blocking loop to read WebSocket frames for the given connection.
 * @param client The HTTP client.
 * @param req The connected request.
 * @param on_msg The callback to trigger on messages.
 * @param on_err The callback to trigger on errors.
 * @param on_close The callback to trigger when the stream closes.
 * @param user_data Opaque pointer to pass to the callbacks.
 * @param exit_flag Optional volatile int pointer. Loop exits cleanly when *exit_flag != 0.
 * @return 0 on clean exit, negative error code on failure.
 */
int c_abstract_http_ws_sync_read_loop(struct HttpClient* client, struct HttpRequest* req, 
                                      c_abstract_http_ws_on_message on_msg, 
                                      c_abstract_http_ws_on_error on_err, 
                                      c_abstract_http_ws_on_close on_close, 
                                      void* user_data, volatile int* exit_flag);

/**
 * @brief Register a WebSocket connection with the underlying transport's asynchronous event loop.
 * @param client The HTTP client.
 * @param req The connected request.
 * @param on_msg The callback to trigger on messages.
 * @param on_err The callback to trigger on errors.
 * @param on_close The callback to trigger when the stream closes.
 * @param user_data Opaque pointer to pass to the callbacks.
 * @return 0 on success, negative error code on failure.
 */
int c_abstract_http_ws_async_register(struct HttpClient* client, struct HttpRequest* req,
                                      c_abstract_http_ws_on_message on_msg,
                                      c_abstract_http_ws_on_error on_err,
                                      c_abstract_http_ws_on_close on_close,
                                      void* user_data);

/**
 * @brief Queue a WebSocket frame to be sent asynchronously or from a thread pool.
 *
 * @param req The HTTP request (acting as the connection handle).
 * @param opcode The WebSocket opcode to send.
 * @param payload The payload data to send.
 * @param len The length of the payload data.
 * @return 0 on success, or a negative error code.
 */
int c_abstract_http_ws_async_send(struct HttpRequest* req, enum c_abstract_http_ws_opcode opcode, const unsigned char* payload, size_t len);

/**
 * @brief Send a WebSocket frame.
 *
 * @param req The HTTP request (acting as the connection handle).
 * @param opcode The WebSocket opcode to send.
 * @param payload The payload data to send.
 * @param len The length of the payload data.
 * @return 0 on success, or a negative error code.
 */
int c_abstract_http_ws_send(struct HttpRequest* req, enum c_abstract_http_ws_opcode opcode, const unsigned char* payload, size_t len);

/**
 * @brief Close a WebSocket connection.
 *
 * @param req The HTTP request (acting as the connection handle).
 * @param status_code The closure status code (e.g., 1000 for normal closure).
 * @return 0 on success, or a negative error code.
 */
int c_abstract_http_ws_close(struct HttpRequest* req, int status_code);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_WS_H */
/* clang-format on */
