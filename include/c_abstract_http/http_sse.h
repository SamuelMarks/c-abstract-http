/* clang-format off */
#ifndef C_ABSTRACT_HTTP_SSE_H
#define C_ABSTRACT_HTTP_SSE_H

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

/* SSE public API definitions will go here */

/**
 * @brief SSE connection state.
 */
enum c_abstract_http_sse_state {
    C_ABSTRACT_HTTP_SSE_STATE_CONNECTING = 0,
    C_ABSTRACT_HTTP_SSE_STATE_CONNECTED,
    C_ABSTRACT_HTTP_SSE_STATE_RECONNECTING,
    C_ABSTRACT_HTTP_SSE_STATE_CLOSED
};

/**
 * @brief Represents a parsed Server-Sent Event.
 */
struct c_abstract_http_sse_event {
    const char* id;      /**< Event ID, or NULL if none */
    const char* event;   /**< Event type (e.g., "message") */
    const char* data;    /**< Event data payload */
    size_t data_len;     /**< Length of event data payload */
};

/**
 * @brief Configuration for an SSE connection.
 */
struct c_abstract_http_sse_config {
    const char* last_event_id; /**< Optional Last-Event-ID for resuming streams */
    int retry_timeout_ms;      /**< Initial reconnection timeout in milliseconds */
};

/**
 * @brief Callback invoked when a complete SSE is received.
 *
 * @param ev Pointer to the SSE event data.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, or a negative error code.
 */
typedef int (*c_abstract_http_sse_on_event)(const struct c_abstract_http_sse_event* ev, void* user_data);

/**
 * @brief Callback invoked when an SSE error occurs.
 *
 * @param error_code The specific error code.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, or a negative error code.
 */
typedef int (*c_abstract_http_sse_on_error)(int error_code, void* user_data);

/**
 * @brief Callback invoked when an SSE stream is closed permanently.
 *
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, or a negative error code.
 */
typedef int (*c_abstract_http_sse_on_close)(void* user_data);

/**
 * @brief Initialize an SSE stream request.
 *
 * @param req The HTTP request to initialize.
 * @param config Optional SSE configuration.
 * @return 0 on success, or a negative error code.
 */
int c_abstract_http_sse_init(struct HttpRequest* req, const struct c_abstract_http_sse_config* config);

/**
 * @brief Run a blocking loop to read SSE events for the given connection.
 * @param client The HTTP client.
 * @param req The connected request.
 * @param on_evt The callback to trigger on events.
 * @param on_err The callback to trigger on errors.
 * @param on_close The callback to trigger when the stream closes permanently.
 * @param user_data Opaque pointer to pass to the callbacks.
 * @param exit_flag Optional volatile int pointer. Loop exits cleanly when *exit_flag != 0.
 * @return 0 on clean exit, negative error code on failure.
 */
int c_abstract_http_sse_sync_read_loop(struct HttpClient* client, struct HttpRequest* req, 
                                       c_abstract_http_sse_on_event on_evt, 
                                       c_abstract_http_sse_on_error on_err, 
                                       c_abstract_http_sse_on_close on_close, 
                                       void* user_data, volatile int* exit_flag);

/**
 * @brief Register an SSE connection with the underlying transport's asynchronous event loop.
 * @param client The HTTP client.
 * @param req The connected request.
 * @param on_evt The callback to trigger on events.
 * @param on_err The callback to trigger on errors.
 * @param on_close The callback to trigger when the stream closes permanently.
 * @param user_data Opaque pointer to pass to the callbacks.
 * @return 0 on success, negative error code on failure.
 */
int c_abstract_http_sse_async_register(struct HttpClient* client, struct HttpRequest* req,
                                       c_abstract_http_sse_on_event on_evt,
                                       c_abstract_http_sse_on_error on_err,
                                       c_abstract_http_sse_on_close on_close,
                                       void* user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_SSE_H */
/* clang-format on */
