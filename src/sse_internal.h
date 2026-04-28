#ifndef C_ABSTRACT_HTTP_SSE_INTERNAL_H
#define C_ABSTRACT_HTTP_SSE_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @file sse_internal.h
 * @brief Internal Server-Sent Events definitions and parsing logic.
 */

/* clang-format off */
#include "c_abstract_http/http_sse.h"
#include <stddef.h>
/* clang-format on */

struct c_abstract_http_sse_async_ctx {
  struct HttpClient *client;
  struct HttpRequest *req;
  c_abstract_http_sse_on_event on_evt;
  c_abstract_http_sse_on_error on_err;
  c_abstract_http_sse_on_close on_close;
  void *user_data;
};

void c_abstract_http_sse_async_task(void *arg);


/**
 * @brief Context structure for parsing incoming Server-Sent Events.
 */
struct sse_parser_ctx {
  char *line_buffer;    /**< Dynamic buffer for the current line */
  size_t line_capacity; /**< Capacity of the line buffer */
  size_t line_offset;   /**< Current write offset in the line buffer */

  char *current_event;  /**< Current event type (e.g., "message") */
  char *current_data;   /**< Dynamic buffer for the aggregated event data */
  size_t data_capacity; /**< Capacity of the data buffer */
  size_t data_offset;   /**< Current write offset in the data buffer */

  char *last_event_id; /**< Last received event ID */
  int retry_ms;        /**< Current retry timeout in milliseconds */

  c_abstract_http_sse_on_event
      on_event; /**< User callback for completed events */
  c_abstract_http_sse_on_error
      on_error; /**< User callback for parsing errors */
  c_abstract_http_sse_on_close
      on_close;    /**< User callback for connection closures */
  void *user_data; /**< Opaque user data */
};

/**
 * @brief Initialize an SSE parser context.
 * @param ctx The parser context to initialize.
 * @param config Optional configuration for the SSE stream.
 * @param on_evt The callback for completed events.
 * @param on_err The callback for parsing errors.
 * @param on_cls The callback for connection closures.
 * @param user_data Opaque pointer to user data.
 * @return 0 on success, negative error code on failure.
 */
int sse_parser_init(struct sse_parser_ctx *ctx,
                    const struct c_abstract_http_sse_config *config,
                    c_abstract_http_sse_on_event on_evt,
                    c_abstract_http_sse_on_error on_err,
                    c_abstract_http_sse_on_close on_cls, void *user_data);

/**
 * @brief Destroy an SSE parser context and free its buffers.
 * @param ctx The parser context to destroy.
 */
void sse_parser_destroy(struct sse_parser_ctx *ctx);

/**
 * @brief Feed raw network bytes into the SSE parser.
 * @param ctx The parser context.
 * @param chunk The raw byte chunk from the network.
 * @param len The length of the chunk.
 * @return 0 on success, negative error code on protocol violation.
 */
int sse_parser_feed(struct sse_parser_ctx *ctx, const char *chunk, size_t len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_SSE_INTERNAL_H */
