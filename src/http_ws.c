/**
 * @file http_ws.c
 * @brief RFC 6455 WebSocket handshake, framing, masking, and client session
 * implementation.
 */

/* clang-format off */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef _WIN32_WINNT
/** @brief Target Windows Vista+ API level */
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include "c_abstract_http/http_ws.h"
#include "c_abstract_http/thread_pool.h"
#include "c_abstract_http/log.h"
#include "ws_internal.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection flag for condition variable init failure */
extern int g_mock_cond_init_fail;
/** @brief Fault-injection flag for mutex destruction failure */
extern int g_mock_mutex_free_fail;
/** @brief Fault-injection flag for condition variable destruction failure */
extern int g_mock_cond_free_fail;
/** @brief Fault-injection flag for WebSocket send-queue allocation failure */
extern int g_mock_queue_malloc_fail;
/** @brief Fault-injection flag for WebSocket parser destruction failure */
extern int g_mock_parser_destroy_fail;
#endif
#include "ws_config.h"
#include "crypto_utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <c89stringutils_string_extras.h>
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection mode for WebSocket mask key helpers */
extern int g_mock_mask_key_fail;
/** @brief Fault-injection mode for WebSocket frame header packers */
extern int g_mock_pack_header_fail;
#endif

/** @brief Generate a Base64-encoded 16-byte Sec-WebSocket-Key */
NO_DISCARD c_abstract_http_error_t ws_generate_key(char out_key[25]) {
  unsigned char random_bytes[16];
  char *base64_str = NULL;
  size_t base64_len = 0;
  c_abstract_http_error_t res;

  res = c_abstract_http_random_bytes(random_bytes, 16);
  if (res != C_ABSTRACT_HTTP_SUCCESS)
    return res;

  res = base64_encode(random_bytes, 16, &base64_str, &base64_len);
  if (res != 0)
    return res;

  c89stringutils_strcpy_s(out_key, 25, base64_str);
  free(base64_str);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Sign a client Sec-WebSocket-Key to produce Sec-WebSocket-Accept */
NO_DISCARD c_abstract_http_error_t ws_sign_key(const char *client_key,
                                               char out_accept[29]) {
  c_abstract_http_error_t rc;
  const char *magic_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char concatenated[100];
  struct sha1_ctx ctx;
  unsigned char hash[20];
  char *base64_str = NULL;
  size_t base64_len = 0;
  c_abstract_http_error_t res;
  size_t len1 = strlen(client_key);
  size_t len2 = strlen(magic_guid);

  if (len1 + len2 >= sizeof(concatenated))
    return C_ABSTRACT_HTTP_ERR_INVAL;

  c89stringutils_strcpy_s(concatenated, sizeof(concatenated), client_key);
  c89stringutils_strcpy_s(concatenated + len1, sizeof(concatenated) - len1,
                          magic_guid);

  rc = sha1_init(&ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = sha1_update(&ctx, (const unsigned char *)concatenated, len1 + len2);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = sha1_final(&ctx, hash);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  res = base64_encode(hash, 20, &base64_str, &base64_len);
  if (res != C_ABSTRACT_HTTP_SUCCESS)
    return res;

  c89stringutils_strcpy_s(out_accept, 29, base64_str);
  free(base64_str);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Verify server Sec-WebSocket-Accept header against client key */
NO_DISCARD c_abstract_http_error_t ws_verify_accept(const char *client_key,
                                                    const char *server_accept) {
  char expected_accept[29];
  c_abstract_http_error_t res;
  res = ws_sign_key(client_key, expected_accept);
  if (res != C_ABSTRACT_HTTP_SUCCESS)
    return res;

  if (math_is_const_time_streq(expected_accept, server_accept)) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE;
}

/**
 * @brief Streaming upload callback draining queued outgoing WebSocket frames.
 * @param user_data Pointer to struct ws_stream_ctx.
 * @param buf Destination upload buffer.
 * @param buf_len Maximum bytes to copy into buf.
 * @param out_read Output byte count written (0 signals EOF).
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on synchronization failure.
 */
NO_DISCARD static c_abstract_http_error_t
ws_read_chunk_cb(void *user_data, void *buf, size_t buf_len, size_t *out_read) {
  c_abstract_http_error_t rc;
  struct ws_stream_ctx *sctx = (struct ws_stream_ctx *)user_data;
  if (!sctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  rc = abstract_http_mutex_lock(sctx->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("ws_read_chunk_cb: mutex lock failed with %d", (int)rc);
    return rc;
  }
  while (sctx->queue_len == 0 && !sctx->close_requested) {
    /* Wait for data or close */
    rc = abstract_http_cond_wait(sctx->cond, sctx->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t u_rc = abstract_http_mutex_unlock(sctx->mutex);
      if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("ws_read_chunk_cb: unlock error %d", (int)u_rc);
        return u_rc;
      }
      LOG_DEBUG("ws_read_chunk_cb: cond_wait failed with %d", (int)rc);
      return rc;
    }
  }

  if (sctx->queue_len > 0) {
    size_t to_copy = sctx->queue_len;
    if (to_copy > buf_len)
      to_copy = buf_len;
    memcpy(buf, sctx->queue, to_copy);

    memmove(sctx->queue, sctx->queue + to_copy, sctx->queue_len - to_copy);
    sctx->queue_len -= to_copy;
    *out_read = to_copy;
  } else {
    /* queue_len == 0 and close_requested == 1 */
    *out_read = 0; /* EOF */
  }

  rc = abstract_http_mutex_unlock(sctx->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("ws_read_chunk_cb: mutex unlock failed with %d", (int)rc);
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Close a raw socket descriptor and return `orig_rc`.
 * @param fd Socket descriptor to close.
 * @param orig_rc Status enum to return.
 * @return `orig_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
ws_close_fd_and_return(int fd, c_abstract_http_error_t orig_rc) {
#if defined(_WIN32)
  closesocket((SOCKET)fd);
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
  close(fd);
#else
  (void)fd;
#endif
  return orig_rc;
}

/**
 * @brief Detach from event loop and close the owned upgraded socket if open.
 * @param sctx WebSocket stream context.
 * @return C_ABSTRACT_HTTP_SUCCESS or event-loop removal error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_close_upgraded_socket(struct ws_stream_ctx *sctx) {
  if (sctx->loop_registered && sctx->loop && sctx->has_upgraded_fd) {
    int fd = sctx->upgraded_fd;
    c_abstract_http_error_t rm_rc;
    sctx->loop_registered = 0;
    sctx->has_upgraded_fd = 0;
    sctx->upgraded_fd = -1;
    rm_rc = http_loop_remove_fd(sctx->loop, fd);
    if (rm_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ws_close_fd_and_return(fd, rm_rc);
    }
    return ws_close_fd_and_return(fd, C_ABSTRACT_HTTP_SUCCESS);
  }
  if (sctx->has_upgraded_fd) {
    int fd = sctx->upgraded_fd;
    sctx->has_upgraded_fd = 0;
    sctx->upgraded_fd = -1;
    return ws_close_fd_and_return(fd, C_ABSTRACT_HTTP_SUCCESS);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
/**
 * @brief Close upgraded socket and propagate cleanup or primary error enum.
 * @param sctx WebSocket stream context.
 * @param orig_rc Primary error enum.
 * @return Cleanup error if socket removal fails, otherwise `orig_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
ws_fail_upgraded_socket(struct ws_stream_ctx *sctx,
                        c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t cl_rc;
  cl_rc = ws_close_upgraded_socket(sctx);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return cl_rc;
  }
  return orig_rc;
}
#endif

/**
 * @brief Destroy and free a WebSocket streaming queue context.
 * @param sctx Streaming context pointer (may be NULL).
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on mutex/cond cleanup failure.
 */
NO_DISCARD static c_abstract_http_error_t
ws_stream_ctx_free(struct ws_stream_ctx *sctx) {
  if (!sctx)
    return C_ABSTRACT_HTTP_SUCCESS;
  {
    c_abstract_http_error_t sc_rc;
    sc_rc = ws_close_upgraded_socket(sctx);
    if (sc_rc != C_ABSTRACT_HTTP_SUCCESS)
      return sc_rc;
  }
  if (sctx->has_async_parser) {
    c_abstract_http_error_t pr_rc;
    sctx->has_async_parser = 0;
    pr_rc = ws_parser_destroy(&sctx->async_parser);
    if (pr_rc != C_ABSTRACT_HTTP_SUCCESS)
      return pr_rc;
  }
  if (sctx->mutex) {
    struct AbstractHttpMutex *m = sctx->mutex;
    c_abstract_http_error_t rc;
    sctx->mutex = NULL;
    rc = abstract_http_mutex_free(m);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
  }
  if (sctx->cond) {
    struct AbstractHttpCond *c = sctx->cond;
    c_abstract_http_error_t rc;
    sctx->cond = NULL;
    rc = abstract_http_cond_free(c);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
  }
  if (sctx->queue)
    free(sctx->queue);
  free(sctx);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a WebSocket condition variable with OOM fault-injection
 * support.
 * @param cond Output pointer receiving the initialized condition variable.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code on failure.
 */
NO_DISCARD static c_abstract_http_error_t
ws_cond_init_checked(struct AbstractHttpCond **cond) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_cond_init_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  return abstract_http_cond_init(cond);
}

NO_DISCARD c_abstract_http_error_t c_abstract_http_ws_init(
    struct HttpRequest *req, const struct c_abstract_http_ws_config *config) {
  char key[25] = {0};
  c_abstract_http_error_t res;

  if (!req)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  if (!req->ws_ctx) {
    struct ws_stream_ctx *sctx =
        (struct ws_stream_ctx *)calloc(1, sizeof(struct ws_stream_ctx));
    if (!sctx)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    res = abstract_http_mutex_init(&sctx->mutex);
    if (res != C_ABSTRACT_HTTP_SUCCESS) {
      free(sctx);
      return (c_abstract_http_error_t)res;
    }
    res = ws_cond_init_checked(&sctx->cond);
    if (res != C_ABSTRACT_HTTP_SUCCESS) {
      {
        c_abstract_http_error_t rc_tmp = abstract_http_mutex_free(sctx->mutex);
        if (rc_tmp != C_ABSTRACT_HTTP_SUCCESS) {
          free(sctx);
          return rc_tmp;
        }
      }
      free(sctx);
      return (c_abstract_http_error_t)res;
    }
    sctx->queue_cap = 4096;
    sctx->queue = (unsigned char *)malloc(sctx->queue_cap);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_queue_malloc_fail) {
      free(sctx->queue);
      sctx->queue = NULL;
    }
#endif
    if (!sctx->queue) {
      {
        c_abstract_http_error_t rc_tmp = abstract_http_cond_free(sctx->cond);
        if (rc_tmp != C_ABSTRACT_HTTP_SUCCESS) {
          c_abstract_http_error_t rc_mtx;
          rc_mtx = abstract_http_mutex_free(sctx->mutex);
          if (rc_mtx != C_ABSTRACT_HTTP_SUCCESS) {
            free(sctx);
            return rc_mtx;
          }
          free(sctx);
          return rc_tmp;
        }
      }
      {
        c_abstract_http_error_t rc_tmp = abstract_http_mutex_free(sctx->mutex);
        if (rc_tmp != C_ABSTRACT_HTTP_SUCCESS) {
          free(sctx);
          return rc_tmp;
        }
      }
      free(sctx);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    req->ws_ctx = sctx;
    req->read_chunk = ws_read_chunk_cb;
    req->read_chunk_user_data = sctx;
  }

  res = ws_generate_key(key);
  if (res != 0)
    return res;

  res = http_headers_add(&req->headers, "Upgrade", "websocket");
  if (res != 0)
    return res;

  res = http_headers_add(&req->headers, "Connection", "Upgrade");
  if (res != 0)
    return res;

  res = http_headers_add(&req->headers, "Sec-WebSocket-Key", key);
  if (res != 0)
    return res;

  res = http_headers_add(&req->headers, "Sec-WebSocket-Version", "13");
  if (res != 0)
    return res;

  if (config && config->subprotocols) {
    res = http_headers_add(&req->headers, "Sec-WebSocket-Protocol",
                           config->subprotocols);
    if (res != 0)
      return res;
  }

  if (config && config->custom_headers) {
    int i = 0;
    while (config->custom_headers[i] && config->custom_headers[i + 1]) {
      res = http_headers_add(&req->headers, config->custom_headers[i],
                             config->custom_headers[i + 1]);
      if (res != 0)
        return res;
      i += 2;
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

uint16_t math_ws_htons(uint16_t hostshort) {
  uint16_t out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)(hostshort >> 8);
  p[1] = (unsigned char)(hostshort & 0xFF);
  return out;
}

uint64_t math_ws_htonll(uint64_t hostqword) {
  uint64_t out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)((hostqword >> 56) & 0xFF);
  p[1] = (unsigned char)((hostqword >> 48) & 0xFF);
  p[2] = (unsigned char)((hostqword >> 40) & 0xFF);
  p[3] = (unsigned char)((hostqword >> 32) & 0xFF);
  p[4] = (unsigned char)((hostqword >> 24) & 0xFF);
  p[5] = (unsigned char)((hostqword >> 16) & 0xFF);
  p[6] = (unsigned char)((hostqword >> 8) & 0xFF);
  p[7] = (unsigned char)(hostqword & 0xFF);
  return out;
}

uint16_t math_ws_ntohs(uint16_t netshort) {
  const unsigned char *p = (const unsigned char *)&netshort;
  return (uint16_t)((p[0] << 8) | p[1]);
}

uint64_t math_ws_ntohll(uint64_t netqword) {
  unsigned char p[8];
  memcpy(p, &netqword, 8);
  return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
         ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
         ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
         ((uint64_t)p[6] << 8) | (uint64_t)p[7];
}

/** @brief Generate a 4-byte WebSocket masking key */
NO_DISCARD c_abstract_http_error_t
ws_generate_mask_key(unsigned char out_key[4]) {
  if (!out_key)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mask_key_fail == 1)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  return c_abstract_http_random_bytes(out_key, 4);
}

/** @brief Apply XOR mask to WebSocket frame payload */
NO_DISCARD c_abstract_http_error_t ws_apply_mask(
    unsigned char *payload, size_t len, const unsigned char mask_key[4]) {
  size_t i;
  if (!payload || len == 0)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (!mask_key)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_mask_key_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  for (i = 0; i < len; i++) {
    payload[i] ^= mask_key[i % 4];
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t ws_pack_header_small(
    unsigned char *buf, int fin, enum c_abstract_http_ws_opcode opcode,
    int mask, size_t len, size_t *out_len) {
  if (!buf || len > 125)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_pack_header_fail == 1)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  buf[0] = (unsigned char)((fin ? 0x80 : 0x00) | (opcode & 0x0F));
  buf[1] = (unsigned char)((mask ? 0x80 : 0x00) | (len & 0x7F));
  if (out_len)
    *out_len = 2;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t ws_pack_header_medium(
    unsigned char *buf, int fin, enum c_abstract_http_ws_opcode opcode,
    int mask, size_t len, size_t *out_len) {
  uint16_t net_len;
  if (!buf || len <= 125 || (unsigned long)len > 65535UL)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_pack_header_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  buf[0] = (unsigned char)((fin ? 0x80 : 0x00) | (opcode & 0x0F));
  buf[1] = (unsigned char)((mask ? 0x80 : 0x00) | 126);
  net_len = math_ws_htons((uint16_t)len);
  memcpy(buf + 2, &net_len, 2);
  if (out_len)
    *out_len = 4;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t ws_pack_header_large(
    unsigned char *buf, int fin, enum c_abstract_http_ws_opcode opcode,
    int mask, size_t len, size_t *out_len) {
  uint64_t net_len;
  if (!buf || (unsigned long)len <= 65535UL)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_pack_header_fail == 3)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  buf[0] = (unsigned char)((fin ? 0x80 : 0x00) | (opcode & 0x0F));
  buf[1] = (unsigned char)((mask ? 0x80 : 0x00) | 127);
  net_len = math_ws_htonll((uint64_t)len);
  memcpy(buf + 2, &net_len, 8);
  if (out_len)
    *out_len = 10;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize a WebSocket frame parser context */
NO_DISCARD c_abstract_http_error_t
ws_parser_init(struct ws_parser_ctx *ctx, c_abstract_http_ws_on_message on_msg,
               c_abstract_http_ws_on_error on_err,
               c_abstract_http_ws_on_close on_cls, void *user_data) {
  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  memset(ctx, 0, sizeof(*ctx));

  ctx->payload_capacity = 4096;
  ctx->payload_buffer = (unsigned char *)malloc(ctx->payload_capacity);
  if (!ctx->payload_buffer)
    return C_ABSTRACT_HTTP_ERR_NOMEM; /* ENOMEM fallback */

  ctx->on_message = on_msg;
  ctx->on_error = on_err;
  ctx->on_close = on_cls;
  ctx->user_data = user_data;

  ctx->state = WS_PARSER_READ_OPCODE;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Destroy a WebSocket frame parser context */
NO_DISCARD c_abstract_http_error_t
ws_parser_destroy(struct ws_parser_ctx *ctx) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_parser_destroy_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (!ctx)
    return C_ABSTRACT_HTTP_SUCCESS;
  free(ctx->payload_buffer);
  free(ctx->reassembly_buffer);
  memset(ctx, 0, sizeof(*ctx));
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Feed raw bytes into the WebSocket frame parser */
NO_DISCARD c_abstract_http_error_t ws_parser_feed(struct ws_parser_ctx *ctx,
                                                  const unsigned char *chunk,
                                                  size_t len) {
  size_t i = 0;
  if (!ctx || (!chunk && len > 0)) {
    LOG_DEBUG("ws_parser_feed: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  while (i < len) {
    switch ((int)ctx->state) {
    case WS_PARSER_READ_OPCODE: {
      unsigned char b = chunk[i++];
      ctx->current_frame.fin = (b >> 7) & 1;
      ctx->current_frame.opcode = (enum c_abstract_http_ws_opcode)(b & 0x0F);

      /* Fail if any RSV bits are set */
      if (b & 0x70) {
        if (ctx->on_error) {
          c_abstract_http_error_t cb_rc =
              ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
          if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
            return cb_rc;
        }
        return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
      }

      /* Fail if control frame is fragmented */
      if (!ctx->current_frame.fin && ctx->current_frame.opcode >= 0x08) {
        if (ctx->on_error) {
          c_abstract_http_error_t cb_rc =
              ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
          if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
            return cb_rc;
        }
        return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
      }

      ctx->state = WS_PARSER_READ_LEN;
      break;
    }
    case WS_PARSER_READ_LEN: {
      unsigned char b = chunk[i++];
      unsigned char base_len = b & 0x7F;
      ctx->current_frame.mask = (b >> 7) & 1;

      if (base_len >= 126 && ctx->current_frame.opcode >= 0x08) {
        if (ctx->on_error) {
          c_abstract_http_error_t cb_rc =
              ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
          if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
            return cb_rc;
        }
        return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
      }

      if (base_len == 126) {
        ctx->state = WS_PARSER_READ_EXT_LEN_16;
        ctx->ext_len_offset = 0;
      } else if (base_len == 127) {
        ctx->state = WS_PARSER_READ_EXT_LEN_64;
        ctx->ext_len_offset = 0;
      } else {
        ctx->current_frame.payload_len = base_len;
        ctx->state = ctx->current_frame.mask ? WS_PARSER_READ_MASK
                                             : WS_PARSER_READ_PAYLOAD;
        ctx->payload_offset = 0;
      }
      break;
    }
    case WS_PARSER_READ_EXT_LEN_16: {
      ctx->ext_len_buffer[ctx->ext_len_offset++] = chunk[i++];
      if (ctx->ext_len_offset == 2) {
        uint16_t net_len;
        memcpy(&net_len, ctx->ext_len_buffer, 2);
        ctx->current_frame.payload_len = math_ws_ntohs(net_len);
        ctx->state = ctx->current_frame.mask ? WS_PARSER_READ_MASK
                                             : WS_PARSER_READ_PAYLOAD;
        ctx->payload_offset = 0;
      }
      break;
    }
    case WS_PARSER_READ_EXT_LEN_64: {
      ctx->ext_len_buffer[ctx->ext_len_offset++] = chunk[i++];
      if (ctx->ext_len_offset == 8) {
        uint64_t net_len;
        memcpy(&net_len, ctx->ext_len_buffer, 8);
        ctx->current_frame.payload_len = math_ws_ntohll(net_len);
        ctx->state = ctx->current_frame.mask ? WS_PARSER_READ_MASK
                                             : WS_PARSER_READ_PAYLOAD;
        ctx->payload_offset = 0;

        /* EMSGSIZE logic */
        if (ctx->current_frame.payload_len >
            C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE) {
          if (ctx->on_error) {
            c_abstract_http_error_t cb_rc =
                ctx->on_error(C_ABSTRACT_HTTP_ERR_MSGSIZE, ctx->user_data);
            if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
              return cb_rc;
          }
          return C_ABSTRACT_HTTP_ERR_MSGSIZE;
        }
      }
      break;
    }
    case WS_PARSER_READ_MASK: {
      ctx->current_frame.masking_key[ctx->mask_offset++] = chunk[i++];
      if (ctx->mask_offset == 4) {
        ctx->state = WS_PARSER_READ_PAYLOAD;
        ctx->payload_offset = 0;
      }
      break;
    }
    case WS_PARSER_READ_PAYLOAD: {
      size_t remaining =
          (size_t)ctx->current_frame.payload_len - ctx->payload_offset;
      size_t available = len - i;
      size_t to_copy = available < remaining ? available : remaining;

      /* Ensure payload buffer is large enough */
      if (ctx->current_frame.payload_len > ctx->payload_capacity) {
        unsigned char *new_buf = (unsigned char *)realloc(
            ctx->payload_buffer, (size_t)ctx->current_frame.payload_len);
        if (!new_buf) {
          if (ctx->on_error) {
            c_abstract_http_error_t cb_rc =
                ctx->on_error(C_ABSTRACT_HTTP_ERR_NOMEM, ctx->user_data);
            if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
              return cb_rc;
          }
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        ctx->payload_buffer = new_buf;
        ctx->payload_capacity = (size_t)ctx->current_frame.payload_len;
      }

      memcpy(ctx->payload_buffer + ctx->payload_offset, chunk + i, to_copy);

      /* Unmask on the fly */
      if (ctx->current_frame.mask) {
        size_t j;
        for (j = 0; j < to_copy; ++j) {
          ctx->payload_buffer[ctx->payload_offset + j] ^=
              ctx->current_frame.masking_key[(ctx->payload_offset + j) % 4];
        }
      }

      ctx->payload_offset += to_copy;
      i += to_copy;

      if (ctx->payload_offset == ctx->current_frame.payload_len) {
        /* Frame Complete */

        if (ctx->current_frame.opcode == C_ABSTRACT_HTTP_WS_OPCODE_CLOSE) {
          int status;
          status = 1005; /* Default */
          if (ctx->current_frame.payload_len >= 2) {
            uint16_t net_status;
            memcpy(&net_status, ctx->payload_buffer, 2);
            status = math_ws_ntohs(net_status);
          }
          if (ctx->on_close) {
            c_abstract_http_error_t cb_rc =
                ctx->on_close(status, ctx->user_data);
            if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
              return cb_rc;
          }
        } else if (ctx->current_frame.opcode ==
                   C_ABSTRACT_HTTP_WS_OPCODE_PING) {
          struct c_abstract_http_ws_event ev;
          ev.opcode = ctx->current_frame.opcode;
          ev.payload = ctx->payload_buffer;
          ev.payload_len = (size_t)ctx->current_frame.payload_len;
          ev.is_fin = ctx->current_frame.fin;
          if (ctx->on_message) {
            c_abstract_http_error_t cb_rc =
                ctx->on_message(&ev, ctx->user_data);
            if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
              return cb_rc;
          }
        } else if (ctx->current_frame.opcode ==
                   C_ABSTRACT_HTTP_WS_OPCODE_PONG) {
          struct c_abstract_http_ws_event ev;
          ev.opcode = ctx->current_frame.opcode;
          ev.payload = ctx->payload_buffer;
          ev.payload_len = (size_t)ctx->current_frame.payload_len;
          ev.is_fin = ctx->current_frame.fin;
          if (ctx->on_message) {
            c_abstract_http_error_t cb_rc =
                ctx->on_message(&ev, ctx->user_data);
            if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
              return cb_rc;
          }
        } else {
          /* Data Frame (Text or Binary or Continuation) */
          if (ctx->current_frame.opcode ==
                  C_ABSTRACT_HTTP_WS_OPCODE_CONTINUATION &&
              !ctx->in_fragment && ctx->reassembly_offset == 0) {
            if (ctx->on_error) {
              c_abstract_http_error_t cb_rc =
                  ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
              if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                return cb_rc;
            }
            return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
          }
          if (!ctx->current_frame.fin) {
            /* Fragmented message */
            if (ctx->current_frame.opcode !=
                    C_ABSTRACT_HTTP_WS_OPCODE_CONTINUATION &&
                (ctx->in_fragment || ctx->reassembly_offset > 0)) {
              /* EPROTO: new data frame before old
               * continuation finished */
              if (ctx->on_error) {
                c_abstract_http_error_t cb_rc = ctx->on_error(
                    C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
                if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                  return cb_rc;
              }
              return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
            }
            if (!ctx->in_fragment) {
              ctx->in_fragment = 1;
              ctx->fragment_opcode = ctx->current_frame.opcode;
            }

            /* Expand reassembly buffer */
            if ((uint64_t)ctx->reassembly_offset +
                    ctx->current_frame.payload_len >
                (uint64_t)ctx->reassembly_capacity) {
              uint64_t needed_cap = (uint64_t)ctx->reassembly_offset +
                                    ctx->current_frame.payload_len;
              uint64_t new_cap64 =
                  ctx->reassembly_capacity == 0
                      ? (uint64_t)4096U
                      : (uint64_t)ctx->reassembly_capacity * (uint64_t)2U;
              size_t new_cap;
              while (new_cap64 < needed_cap)
                new_cap64 *= (uint64_t)2U;

              /* Memory limit bounds check */
              if (new_cap64 > (uint64_t)C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE ||
                  new_cap64 > (uint64_t)((size_t)-1)) {
                if (ctx->on_error) {
                  c_abstract_http_error_t cb_rc = ctx->on_error(
                      C_ABSTRACT_HTTP_ERR_MSGSIZE, ctx->user_data);
                  if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                    return cb_rc;
                }
                return C_ABSTRACT_HTTP_ERR_MSGSIZE;
              }
              new_cap = (size_t)new_cap64;

              {
                unsigned char *new_buf =
                    (unsigned char *)realloc(ctx->reassembly_buffer, new_cap);
                if (!new_buf) {
                  if (ctx->on_error) {
                    c_abstract_http_error_t cb_rc = ctx->on_error(
                        C_ABSTRACT_HTTP_ERR_NOMEM, ctx->user_data);
                    if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                      return cb_rc;
                  }
                  return C_ABSTRACT_HTTP_ERR_NOMEM;
                }
                ctx->reassembly_buffer = new_buf;
                ctx->reassembly_capacity = new_cap;
              }
            }

            memcpy(ctx->reassembly_buffer + ctx->reassembly_offset,
                   ctx->payload_buffer, (size_t)ctx->current_frame.payload_len);
            ctx->reassembly_offset += (size_t)ctx->current_frame.payload_len;

          } else {
            /* Final frame */
            struct c_abstract_http_ws_event ev;
            if (ctx->current_frame.opcode !=
                    C_ABSTRACT_HTTP_WS_OPCODE_CONTINUATION &&
                (ctx->in_fragment || ctx->reassembly_offset > 0)) {
              if (ctx->on_error) {
                c_abstract_http_error_t cb_rc = ctx->on_error(
                    C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
                if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                  return cb_rc;
              }
              return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
            }
            ev.opcode = (ctx->in_fragment && ctx->fragment_opcode != 0)
                            ? ctx->fragment_opcode
                            : ctx->current_frame.opcode;
            ev.is_fin = 1;

            if (ctx->in_fragment || ctx->reassembly_offset > 0) {
              /* Expand to fit final chunk */
              if (ctx->reassembly_offset + ctx->current_frame.payload_len >
                  ctx->reassembly_capacity) {
                size_t new_cap = ctx->reassembly_offset +
                                 (size_t)ctx->current_frame.payload_len;
                unsigned char *new_buf =
                    (unsigned char *)realloc(ctx->reassembly_buffer, new_cap);
                if (!new_buf) {
                  if (ctx->on_error) {
                    c_abstract_http_error_t cb_rc = ctx->on_error(
                        C_ABSTRACT_HTTP_ERR_NOMEM, ctx->user_data);
                    if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                      return cb_rc;
                  }
                  return C_ABSTRACT_HTTP_ERR_NOMEM;
                }
                ctx->reassembly_buffer = new_buf;
                ctx->reassembly_capacity = new_cap;
              }
              memcpy(ctx->reassembly_buffer + ctx->reassembly_offset,
                     ctx->payload_buffer,
                     (size_t)ctx->current_frame.payload_len);
              ctx->reassembly_offset += (size_t)ctx->current_frame.payload_len;

              ev.payload = ctx->reassembly_buffer;
              ev.payload_len = ctx->reassembly_offset;
              ctx->reassembly_offset = 0; /* Reset for next message */
              ctx->in_fragment = 0;
              ctx->fragment_opcode = C_ABSTRACT_HTTP_WS_OPCODE_CONTINUATION;
              if (ctx->on_message) {
                c_abstract_http_error_t cb_rc =
                    ctx->on_message(&ev, ctx->user_data);
                if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                  return cb_rc;
              }
            } else {
              /* Single frame message */
              ev.payload = ctx->payload_buffer;
              ev.payload_len = (size_t)ctx->current_frame.payload_len;
              if (ctx->on_message) {
                c_abstract_http_error_t cb_rc =
                    ctx->on_message(&ev, ctx->user_data);
                if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
                  return cb_rc;
              }
            }
          }
        }

        /* Reset for next frame */
        ctx->state = WS_PARSER_READ_OPCODE;
        ctx->mask_offset = 0;
        ctx->ext_len_offset = 0;
        memset(&ctx->current_frame, 0, sizeof(ctx->current_frame));
      }
      break;
    }
    default: {
      if (ctx->on_error) {
        c_abstract_http_error_t cb_rc =
            ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
        if (cb_rc != C_ABSTRACT_HTTP_SUCCESS)
          return cb_rc;
      }
      return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
    }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Execute a synchronous WebSocket read loop */
NO_DISCARD c_abstract_http_error_t c_abstract_http_ws_sync_read_loop(
    struct HttpClient *client, struct HttpRequest *req,
    c_abstract_http_ws_on_message on_msg, c_abstract_http_ws_on_error on_err,
    c_abstract_http_ws_on_close on_close, void *user_data,
    volatile int *exit_flag) {
  c_abstract_http_error_t rc;
  struct HttpResponse *res = NULL;
  struct ws_parser_ctx parser;
  cah_cppcheck_mut_ptr(exit_flag);

  if (!client || !req) {
    LOG_DEBUG("c_abstract_http_ws_sync_read_loop: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (exit_flag && *exit_flag)
    return C_ABSTRACT_HTTP_SUCCESS;

  rc = c_abstract_http_ws_init(req, NULL);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (on_err) {
      c_abstract_http_error_t err_rc = on_err(rc, user_data);
      if (err_rc != C_ABSTRACT_HTTP_SUCCESS)
        return err_rc;
    }
    return rc;
  }

  rc = ws_parser_init(&parser, on_msg, on_err, on_close, user_data);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (on_err) {
      c_abstract_http_error_t err_rc = on_err(rc, user_data);
      if (err_rc != C_ABSTRACT_HTTP_SUCCESS)
        return err_rc;
    }
    return rc;
  }

  rc = client->send(client->transport, req, &res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS || !res) {
    {
      c_abstract_http_error_t rch = ws_parser_destroy(&parser);
      if (rch != C_ABSTRACT_HTTP_SUCCESS)
        return rch;
    }
    if (on_err) {
      c_abstract_http_error_t err_rc = on_err(rc, user_data);
      if (err_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return err_rc;
      }
    }
    return rc;
  }

  if (res->body && res->body_len > 0) {
    rc = ws_parser_feed(&parser, (const unsigned char *)res->body,
                        res->body_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      if (on_err) {
        c_abstract_http_error_t err_rc = on_err(rc, user_data);
        if (err_rc != C_ABSTRACT_HTTP_SUCCESS) {
          c_abstract_http_error_t rch;
          c_abstract_http_error_t rc_tmp;
          rch = ws_parser_destroy(&parser);
          if (rch != C_ABSTRACT_HTTP_SUCCESS) {
            free(res);
            return rch;
          }
          rc_tmp = http_response_free(res);
          if (rc_tmp != C_ABSTRACT_HTTP_SUCCESS) {
            free(res);
            return rc_tmp;
          }
          free(res);
          return err_rc;
        }
      }
      {
        c_abstract_http_error_t rch = ws_parser_destroy(&parser);
        if (rch != C_ABSTRACT_HTTP_SUCCESS) {
          free(res);
          return rch;
        }
      }
      {
        c_abstract_http_error_t rc_tmp = http_response_free(res);
        if (rc_tmp != C_ABSTRACT_HTTP_SUCCESS) {
          free(res);
          return rc_tmp;
        }
      }
      free(res);
      return rc;
    }
  }

  {
    c_abstract_http_error_t rch = ws_parser_destroy(&parser);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(res);
      return rch;
    }
  }
  {
    c_abstract_http_error_t rc_tmp = http_response_free(res);
    if (rc_tmp != C_ABSTRACT_HTTP_SUCCESS) {
      free(res);
      return rc_tmp;
    }
  }
  free(res);

  if (on_close) {
    c_abstract_http_error_t cls_rc = on_close(200, user_data);
    if (cls_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return cls_rc;
    }
  }

  if (req->ws_ctx) {
    {
      c_abstract_http_error_t rch =
          ws_stream_ctx_free((struct ws_stream_ctx *)req->ws_ctx);
      if (rch != C_ABSTRACT_HTTP_SUCCESS)
        return rch;
    }
    req->ws_ctx = NULL;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Background worker task running the WebSocket read loop asynchronously.
 * @param arg Owned pointer to struct c_abstract_http_ws_async_ctx.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on failure.
 */
NO_DISCARD static c_abstract_http_error_t
c_abstract_http_ws_async_task(void *arg) {
  c_abstract_http_error_t err;
  struct c_abstract_http_ws_async_ctx *ctx =
      (struct c_abstract_http_ws_async_ctx *)arg;
  volatile int exit_flag = 0;
  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  err = c_abstract_http_ws_sync_read_loop(ctx->client, ctx->req, ctx->on_msg,
                                          ctx->on_err, ctx->on_close,
                                          ctx->user_data, &exit_flag);
  if (err != C_ABSTRACT_HTTP_SUCCESS) {
    if (ctx->on_err) {
      c_abstract_http_error_t err_rc = ctx->on_err(err, ctx->user_data);
      if (err_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(ctx);
        return err_rc;
      }
    }
    free(ctx);
    return err;
  }
  free(ctx);
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Test helper for WebSocket async task.
 * @param arg Owned pointer to struct c_abstract_http_ws_async_ctx.
 * @return Result from c_abstract_http_ws_async_task.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_ws_async_task(void *arg);
/**
 * @brief Test helper for WebSocket async task.
 * @param arg Owned pointer to struct c_abstract_http_ws_async_ctx.
 * @return Result from c_abstract_http_ws_async_task.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_ws_async_task(void *arg) {
  return c_abstract_http_ws_async_task(arg);
}
#endif

#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection mode for upgraded WebSocket `send()` (1=EAGAIN, 2=IO)
 */
static int g_mock_ws_send_mode = 0;

/**
 * @brief Configure test-only upgraded WebSocket `send()` fault mode.
 * @param mode `0` normal, `1` `EWOULDBLOCK`, `2` fatal error.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_test_set_ws_send_mode(int mode);

NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_test_set_ws_send_mode(int mode) {
  g_mock_ws_send_mode = mode;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Wrapper around socket `send()` supporting OOM/EAGAIN fault injection.
 * @param fd Socket descriptor.
 * @param buf Bytes to transmit.
 * @param len Byte length to transmit.
 * @return Bytes sent, or `-1` on error / `EWOULDBLOCK`.
 */
static int math_ws_sock_send(int fd, const void *buf, size_t len) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_ws_send_mode == 1) {
#if defined(_WIN32)
    WSASetLastError(WSAEWOULDBLOCK);
#else
    errno = EAGAIN;
#endif
    return -1;
  }
  if (g_mock_ws_send_mode == 2) {
#if defined(_WIN32)
    WSASetLastError(WSAECONNRESET);
#else
    errno = EPIPE;
#endif
    return -1;
  }
#endif
#if defined(_WIN32)
  return send((SOCKET)fd, (const char *)buf, (int)len, 0);
#elif defined(MSG_NOSIGNAL)
  return (int)send(fd, buf, len, MSG_NOSIGNAL);
#else
  return (int)send(fd, buf, len, 0);
#endif
}

/**
 * @brief Parse a `ws://` or `http://` URL into host, port, and path.
 * @param url Input URL string.
 * @param out_host Allocated hostname string.
 * @param out_port Parsed TCP port number.
 * @param out_path Allocated request path string.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_INVAL,
 * C_ABSTRACT_HTTP_ERR_NOTSUP, or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
ws_parse_connect_url(const char *url, char **out_host, int *out_port,
                     char **out_path) {
  const char *p;
  const char *slash;
  const char *colon;
  size_t host_len;
  c_abstract_http_error_t rc;

  if (strncmp(url, "ws://", 5) == 0) {
    p = url + 5;
  } else if (strncmp(url, "http://", 7) == 0) {
    p = url + 7;
  } else if (strncmp(url, "wss://", 6) == 0 ||
             strncmp(url, "https://", 8) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  } else {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  slash = strchr(p, '/');
  colon = strchr(p, ':');
  if (colon && (!slash || colon < slash)) {
    long parsed_port = strtol(colon + 1, NULL, 10);
    host_len = (size_t)(colon - p);
    if (parsed_port <= 0L || parsed_port > 65535L) {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    *out_port = (int)parsed_port;
  } else {
    host_len = slash ? (size_t)(slash - p) : strlen(p);
    *out_port = 80;
  }
  if (host_len == 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *out_host = (char *)malloc(host_len + 1);
  if (!*out_host) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(*out_host, p, host_len);
  (*out_host)[host_len] = '\0';

  rc = c_abstract_http_strdup(slash ? slash : "/", out_path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*out_host);
    *out_host = NULL;
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Pure predicate checking whether the last socket call hit EWOULDBLOCK.
 * @return 1 if transient non-blocking retry condition, 0 otherwise.
 */
static int math_ws_sock_would_block(void) {
#if defined(_WIN32)
  return WSAGetLastError() == WSAEWOULDBLOCK ? 1 : 0;
#else
  return (errno == EAGAIN
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
          || errno == EWOULDBLOCK
#endif
          )
             ? 1
             : 0;
#endif
}

/**
 * @brief Flush queued outbound WebSocket bytes onto `sctx->upgraded_fd` while
 * holding `sctx->mutex`, and update `ModalityEventLoop` write interest.
 * @param sctx Active upgraded WebSocket stream context.
 * @return C_ABSTRACT_HTTP_SUCCESS or I/O / loop error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_flush_upgraded_queue_locked(struct ws_stream_ctx *sctx) {
  while (sctx->queue_len > 0 && sctx->has_upgraded_fd) {
    int n = math_ws_sock_send(sctx->upgraded_fd, sctx->queue, sctx->queue_len);
    if (n > 0) {
      size_t un = (size_t)n;
      memmove(sctx->queue, sctx->queue + un, sctx->queue_len - un);
      sctx->queue_len -= un;
    } else if (n < 0 && math_ws_sock_would_block()) {
      break;
    } else {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  if (sctx->loop_registered && sctx->loop && sctx->has_upgraded_fd) {
    int want = HTTP_LOOP_READ | HTTP_LOOP_ERROR;
    if (sctx->queue_len > 0) {
      want |= HTTP_LOOP_WRITE;
    }
    return http_loop_mod_fd(sctx->loop, sctx->upgraded_fd, want);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Internal parser message interceptor handling RFC 6455 PING->PONG
 * auto-reply on upgraded sockets and forwarding to `user_on_msg`.
 * @param ev Parsed WebSocket event.
 * @param user_data Pointer to `struct ws_stream_ctx`.
 * @return C_ABSTRACT_HTTP_SUCCESS or callback/send error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_upgraded_on_msg_cb(const struct c_abstract_http_ws_event *ev,
                      void *user_data) {
  struct ws_stream_ctx *sctx = (struct ws_stream_ctx *)user_data;
  if (ev->opcode == C_ABSTRACT_HTTP_WS_OPCODE_PING && sctx->owner_req) {
    c_abstract_http_error_t prc;
    prc =
        c_abstract_http_ws_send(sctx->owner_req, C_ABSTRACT_HTTP_WS_OPCODE_PONG,
                                ev->payload, ev->payload_len);
    if (prc != C_ABSTRACT_HTTP_SUCCESS) {
      return prc;
    }
  }
  if (sctx->user_on_msg) {
    return sctx->user_on_msg(ev, sctx->user_cb_data);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Internal parser error interceptor forwarding to `user_on_err`.
 * @param error_code Framing error code.
 * @param user_data Pointer to `struct ws_stream_ctx`.
 * @return C_ABSTRACT_HTTP_SUCCESS or user callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_upgraded_on_err_cb(int error_code, void *user_data) {
  struct ws_stream_ctx *sctx = (struct ws_stream_ctx *)user_data;
  if (sctx->user_on_err) {
    return sctx->user_on_err(error_code, sctx->user_cb_data);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Internal parser close interceptor marking peer closure and forwarding
 * to `user_on_close`.
 * @param status_code RFC 6455 close status code.
 * @param user_data Pointer to `struct ws_stream_ctx`.
 * @return C_ABSTRACT_HTTP_SUCCESS or user callback error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_upgraded_on_close_cb(int status_code, void *user_data) {
  struct ws_stream_ctx *sctx = (struct ws_stream_ctx *)user_data;
  sctx->peer_closed = 1;
  if (sctx->user_on_close) {
    return sctx->user_on_close(status_code, sctx->user_cb_data);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Full-duplex `ModalityEventLoop` I/O callback driving an upgraded
 * WebSocket socket.
 * @param loop Active event loop.
 * @param fd Upgraded socket descriptor.
 * @param revents Triggered `HttpLoopEvent` bitmask.
 * @param user_data Pointer to owning `struct HttpRequest`.
 * @return C_ABSTRACT_HTTP_SUCCESS or callback/protocol error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_upgraded_loop_cb(struct ModalityEventLoop *loop, int fd, int revents,
                    void *user_data) {
  struct HttpRequest *req = (struct HttpRequest *)user_data;
  struct ws_stream_ctx *sctx = (struct ws_stream_ctx *)req->ws_ctx;
  c_abstract_http_error_t rc;

  (void)loop;
  if (revents & HTTP_LOOP_WRITE) {
    int should_close = 0;
    rc = abstract_http_mutex_lock(sctx->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = ws_flush_upgraded_queue_locked(sctx);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t u_rc = abstract_http_mutex_unlock(sctx->mutex);
      if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return u_rc;
      }
      return rc;
    }
    should_close = (sctx->close_requested && sctx->queue_len == 0) ? 1 : 0;
    rc = abstract_http_mutex_unlock(sctx->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    if (should_close) {
      return ws_close_upgraded_socket(sctx);
    }
  }

  if ((revents & (HTTP_LOOP_READ | HTTP_LOOP_ERROR)) && sctx->has_upgraded_fd) {
    unsigned char rbuf[4096];
#if defined(_WIN32)
    int n = recv((SOCKET)fd, (char *)rbuf, (int)sizeof(rbuf), 0);
#else
    int n = (int)recv(fd, rbuf, sizeof(rbuf), 0);
#endif
    if (n > 0) {
      rc = ws_parser_feed(&sctx->async_parser, rbuf, (size_t)n);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return ws_fail_upgraded_socket(sctx, rc);
      }
      if (sctx->peer_closed) {
        return ws_close_upgraded_socket(sctx);
      }
    } else if (n == 0 || !math_ws_sock_would_block()) {
      rc = ws_close_upgraded_socket(sctx);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      if (!sctx->peer_closed && sctx->user_on_close) {
        sctx->peer_closed = 1;
        return sctx->user_on_close(1006, sctx->user_cb_data);
      }
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Connect TCP socket and perform RFC 6455 HTTP/1.1 101 handshake,
 * returning the upgraded socket and any pipelined frame bytes.
 * @param req Initialized WebSocket request containing `Sec-WebSocket-Key`.
 * @param host Target hostname/IP.
 * @param port Target TCP port.
 * @param path Request path.
 * @param out_fd Output upgraded socket descriptor.
 * @param rem_buf Buffer receiving trailing bytes after header terminator.
 * @param out_rem_len Number of trailing bytes written into `rem_buf`.
 * @return C_ABSTRACT_HTTP_SUCCESS or handshake/I/O error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ws_perform_101_handshake(const struct HttpRequest *req, const char *host,
                         int port, const char *path, int *out_fd,
                         unsigned char rem_buf[4096], size_t *out_rem_len) {
  struct sockaddr_in addr;
  char req_buf[2048];
  char rx_buf[4096];
  size_t rx_len = 0;
  char *hdr_end = NULL;
  const char *client_key = NULL;
  const char *accept_hdr = NULL;
  size_t i;
  int off = 0;
  int sock_fd = -1;
  c_abstract_http_error_t rc;

#if defined(_WIN32)
  {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
#endif

  rc = http_headers_get(&req->headers, "Sec-WebSocket-Key", &client_key);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = math_ws_htons((uint16_t)port);
  if (strcmp(host, "127.0.0.1") == 0) {
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else {
    struct hostent *he = gethostbyname(host);
    char *first_addr = NULL;
    if (he && he->h_addr_list) {
      memcpy(&first_addr, he->h_addr_list, sizeof(first_addr));
    }
    if (!first_addr) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    memcpy(&addr.sin_addr, first_addr, (size_t)he->h_length);
  }

  sock_fd = (int)socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#if defined(SO_NOSIGPIPE)
  {
    int nosig = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&nosig,
               sizeof(nosig));
  }
#endif
#if defined(_WIN32)
  if (connect((SOCKET)sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#else
  if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#endif
    return ws_close_fd_and_return(sock_fd, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  off = sprintf_s(req_buf, sizeof(req_buf),
                  "GET %s HTTP/1.1\r\nHost: %s:%d\r\n", path, host, port);
#else
  off =
      sprintf(req_buf, "GET %s HTTP/1.1\r\nHost: %s:%d\r\n", path, host, port);
#endif
  for (i = 0; i < req->headers.count; ++i) {
    size_t klen = strlen(req->headers.headers[i].key);
    size_t vlen = strlen(req->headers.headers[i].value);
    if ((size_t)off + klen + vlen + 6U < sizeof(req_buf)) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      off +=
          sprintf_s(req_buf + off, sizeof(req_buf) - (size_t)off, "%s: %s\r\n",
                    req->headers.headers[i].key, req->headers.headers[i].value);
#else
      off += sprintf(req_buf + off, "%s: %s\r\n", req->headers.headers[i].key,
                     req->headers.headers[i].value);
#endif
    }
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcat_s(req_buf, sizeof(req_buf), "\r\n");
#else
  strcat(req_buf, "\r\n");
#endif
  off += 2;

  if (math_ws_sock_send(sock_fd, req_buf, (size_t)off) <= 0) {
    return ws_close_fd_and_return(sock_fd, C_ABSTRACT_HTTP_ERR_IO);
  }

  while (rx_len + 1U < sizeof(rx_buf)) {
#if defined(_WIN32)
    int n = (int)recv((SOCKET)sock_fd, rx_buf + rx_len,
                      (int)(sizeof(rx_buf) - 1U - rx_len), 0);
#else
    int n =
        (int)recv(sock_fd, rx_buf + rx_len, sizeof(rx_buf) - 1U - rx_len, 0);
#endif
    if (n <= 0) {
      break;
    }
    rx_len += (size_t)n;
    rx_buf[rx_len] = '\0';
    hdr_end = strstr(rx_buf, "\r\n\r\n");
    if (hdr_end) {
      break;
    }
  }

  if (!hdr_end || strncmp(rx_buf, "HTTP/1.1 101", 12) != 0) {
    return ws_close_fd_and_return(sock_fd, C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE);
  }

  {
    size_t hdr_bytes = (size_t)((hdr_end + 4) - rx_buf);
    char *line = rx_buf;
    *out_rem_len = rx_len - hdr_bytes;
    if (*out_rem_len > 0) {
      memcpy(rem_buf, rx_buf + hdr_bytes, *out_rem_len);
    }
    *hdr_end = '\0';
    while (line && *line) {
      char *next = strstr(line, "\r\n");
      char *colon;
      if (next) {
        *next = '\0';
      }
      colon = strchr(line, ':');
      if (colon) {
        *colon = '\0';
        if (c89stringutils_strcasecmp(line, "Sec-WebSocket-Accept") == 0) {
          accept_hdr = colon + 1;
          while (*accept_hdr == ' ' || *accept_hdr == '\t') {
            accept_hdr++;
          }
        }
      }
      line = next ? (next + 2) : NULL;
    }
  }

  if (!accept_hdr) {
    return ws_close_fd_and_return(sock_fd, C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE);
  }

  rc = ws_verify_accept(client_key, accept_hdr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ws_close_fd_and_return(sock_fd, rc);
  }

#if defined(_WIN32)
  {
    u_long mode = 1;
    ioctlsocket((SOCKET)sock_fd, (long)FIONBIO, &mode);
  }
#else
  {
    int fl = fcntl(sock_fd, F_GETFL, 0);
    if (fl >= 0) {
      fcntl(sock_fd, F_SETFL, fl | O_NONBLOCK);
    }
  }
#endif

  *out_fd = sock_fd;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/** @brief Connect, upgrade, detach raw socket, and register on event loop */
NO_DISCARD c_abstract_http_error_t c_abstract_http_ws_async_connect(
    struct HttpClient *client, struct HttpRequest *req,
    c_abstract_http_ws_on_message on_msg, c_abstract_http_ws_on_error on_err,
    c_abstract_http_ws_on_close on_close, void *user_data) {
#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  (void)client;
  (void)req;
  (void)on_msg;
  (void)on_err;
  (void)on_close;
  (void)user_data;
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#else
  char *host = NULL;
  char *path = NULL;
  int port = 80;
  int sock_fd = -1;
  unsigned char rem_buf[4096];
  size_t rem_len = 0;
  struct ws_stream_ctx *sctx;
  int init_ev;
  c_abstract_http_error_t rc;

  if (!client || !req || !req->url || !client->loop) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (!req->ws_ctx) {
    rc = c_abstract_http_ws_init(req, NULL);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  sctx = (struct ws_stream_ctx *)req->ws_ctx;
  sctx->owner_req = req;
  sctx->user_on_msg = on_msg;
  sctx->user_on_err = on_err;
  sctx->user_on_close = on_close;
  sctx->user_cb_data = user_data;
  sctx->peer_closed = 0;

  if (!sctx->has_async_parser) {
    rc = ws_parser_init(&sctx->async_parser, ws_upgraded_on_msg_cb,
                        ws_upgraded_on_err_cb, ws_upgraded_on_close_cb, sctx);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    sctx->has_async_parser = 1;
  }

  rc = ws_parse_connect_url(req->url, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = ws_perform_101_handshake(req, host, port, path, &sock_fd, rem_buf,
                                &rem_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(host);
    free(path);
    return rc;
  }
  free(host);
  free(path);

  sctx->loop = client->loop;
  sctx->upgraded_fd = sock_fd;
  sctx->has_upgraded_fd = 1;

  if (rem_len > 0) {
    rc = ws_parser_feed(&sctx->async_parser, rem_buf, rem_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ws_fail_upgraded_socket(sctx, rc);
    }
  }

  if (sctx->peer_closed) {
    return ws_close_upgraded_socket(sctx);
  }

  init_ev = HTTP_LOOP_READ | HTTP_LOOP_ERROR;
  if (sctx->queue_len > 0) {
    init_ev |= HTTP_LOOP_WRITE;
  }
  rc = http_loop_add_fd(client->loop, sock_fd, init_ev, ws_upgraded_loop_cb,
                        req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ws_fail_upgraded_socket(sctx, rc);
  }
  sctx->loop_registered = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/** @brief Register an asynchronous WebSocket reader */
NO_DISCARD c_abstract_http_error_t c_abstract_http_ws_async_register(
    struct HttpClient *client, struct HttpRequest *req,
    c_abstract_http_ws_on_message on_msg, c_abstract_http_ws_on_error on_err,
    c_abstract_http_ws_on_close on_close, void *user_data) {
  c_abstract_http_error_t rc;
  struct c_abstract_http_ws_async_ctx *ctx;

  if (!client || !req) {
    LOG_DEBUG("c_abstract_http_ws_async_register: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (client->loop) {
    return c_abstract_http_ws_async_connect(client, req, on_msg, on_err,
                                            on_close, user_data);
  }

  if (client->thread_pool) {
    ctx = (struct c_abstract_http_ws_async_ctx *)malloc(sizeof(*ctx));
    if (!ctx) {
      LOG_DEBUG("c_abstract_http_ws_async_register: Error ENOMEM");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    ctx->client = client;
    ctx->req = req;
    ctx->on_msg = on_msg;
    ctx->on_err = on_err;
    ctx->on_close = on_close;
    ctx->user_data = user_data;

    rc = abstract_http_thread_pool_push(client->thread_pool,
                                        c_abstract_http_ws_async_task, ctx);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("c_abstract_http_ws_async_register: Error "
                "thread pool push failed");
      free(ctx);
      return rc;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  LOG_DEBUG("c_abstract_http_ws_async_register: Error "
            "ENOTSUP (No thread pool "
            "available)");
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
}

NO_DISCARD c_abstract_http_error_t c_abstract_http_ws_send_async(
    struct HttpRequest *req, enum c_abstract_http_ws_opcode opcode,
    const unsigned char *payload, size_t len) {
  return c_abstract_http_ws_send(req, opcode, payload, len);
}

NO_DISCARD c_abstract_http_error_t c_abstract_http_ws_send(
    struct HttpRequest *req, enum c_abstract_http_ws_opcode opcode,
    const unsigned char *payload, size_t len) {
  struct ws_stream_ctx *sctx;
  unsigned char header[10];
  size_t header_len = 0;
  c_abstract_http_error_t rc;
  unsigned char mask_key[4] = {0, 0, 0, 0};
  unsigned char *masked_payload = NULL;

  if (!req || !req->ws_ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  sctx = (struct ws_stream_ctx *)req->ws_ctx;

  /* Standard requires client-to-server frames to be masked */
  rc = ws_generate_mask_key(mask_key);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (len > 0) {
    masked_payload = (unsigned char *)malloc(len);
    if (!masked_payload)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    memcpy(masked_payload, payload, len);
    rc = ws_apply_mask(masked_payload, len, mask_key);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(masked_payload);
      return rc;
    }
  }

  if (len <= 125) {
    rc = ws_pack_header_small(header, 1, opcode, 1, len, &header_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(masked_payload);
      return rc;
    }
  } else if ((unsigned long)len <= 65535UL) {
    rc = ws_pack_header_medium(header, 1, opcode, 1, len, &header_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(masked_payload);
      return rc;
    }
  } else {
    rc = ws_pack_header_large(header, 1, opcode, 1, len, &header_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(masked_payload);
      return rc;
    }
  }

  rc = abstract_http_mutex_lock(sctx->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(masked_payload);
    return rc;
  }
  if (sctx->close_requested) {
    rc = abstract_http_mutex_unlock(sctx->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(masked_payload);
      return rc;
    }
    free(masked_payload);
    return C_ABSTRACT_HTTP_ERR_INVAL;
    /* Cannot send after close */
  }

  /* Expand queue if necessary */
  if (sctx->queue_len + header_len + 4 + len > sctx->queue_cap) {
    size_t new_cap = sctx->queue_cap == 0 ? 4096 : sctx->queue_cap * 2;
    while (new_cap < sctx->queue_len + header_len + 4 + len)
      new_cap *= 2;
    {
      unsigned char *new_queue = (unsigned char *)realloc(sctx->queue, new_cap);
      if (!new_queue) {
        rc = abstract_http_mutex_unlock(sctx->mutex);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(masked_payload);
          return rc;
        }
        free(masked_payload);

        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      sctx->queue = new_queue;
      sctx->queue_cap = new_cap;
    }
  }

  memcpy(sctx->queue + sctx->queue_len, header, header_len);
  sctx->queue_len += header_len;

  memcpy(sctx->queue + sctx->queue_len, mask_key, 4);
  sctx->queue_len += 4;

  if (len > 0) {
    memcpy(sctx->queue + sctx->queue_len, masked_payload, len);
    sctx->queue_len += len;
  }

#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
  if (sctx->has_upgraded_fd) {
    int need_wakeup = 0;
    rc = ws_flush_upgraded_queue_locked(sctx);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t u_rc = abstract_http_mutex_unlock(sctx->mutex);
      if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(masked_payload);
        return u_rc;
      }
      free(masked_payload);
      return rc;
    }
    need_wakeup = (sctx->queue_len > 0 && sctx->loop != NULL) ? 1 : 0;
    rc = abstract_http_mutex_unlock(sctx->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(masked_payload);
      return rc;
    }
    free(masked_payload);
    if (need_wakeup) {
      return http_loop_wakeup(sctx->loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif

  rc = abstract_http_cond_signal(sctx->cond);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t u_rc = abstract_http_mutex_unlock(sctx->mutex);
    if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("c_abstract_http_ws_send: unlock error %d", (int)u_rc);
      free(masked_payload);
      return u_rc;
    }
    free(masked_payload);
    return rc;
  }
  rc = abstract_http_mutex_unlock(sctx->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(masked_payload);
    return rc;
  }

  free(masked_payload);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Send a WebSocket close frame and close the stream */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_ws_close(struct HttpRequest *req, int status_code) {
  struct ws_stream_ctx *sctx;
  unsigned char payload[2];
  uint16_t net_status = math_ws_htons((uint16_t)status_code);
  c_abstract_http_error_t rc;
  if (!req || !req->ws_ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  sctx = (struct ws_stream_ctx *)req->ws_ctx;

  memcpy(payload, &net_status, 2);
  rc =
      c_abstract_http_ws_send(req, C_ABSTRACT_HTTP_WS_OPCODE_CLOSE, payload, 2);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = abstract_http_mutex_lock(sctx->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  sctx->close_requested = 1;
  rc = abstract_http_cond_signal(sctx->cond);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t u_rc = abstract_http_mutex_unlock(sctx->mutex);
    if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("c_abstract_http_ws_close: unlock error %d", (int)u_rc);
      return u_rc;
    }
    return rc;
  }
  rc = abstract_http_mutex_unlock(sctx->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (sctx->has_upgraded_fd && sctx->queue_len == 0) {
    return ws_close_upgraded_socket(sctx);
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free WebSocket stream resources attached to an HTTP request */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_ws_free(struct HttpRequest *req) {
  if (req) {
    if (req->ws_ctx) {
      {
        c_abstract_http_error_t rch =
            ws_stream_ctx_free((struct ws_stream_ctx *)req->ws_ctx);
        if (rch != C_ABSTRACT_HTTP_SUCCESS)
          return rch;
      }
      req->ws_ctx = NULL;
      return C_ABSTRACT_HTTP_SUCCESS;
    } else {
      return ws_stream_ctx_free(NULL);
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
