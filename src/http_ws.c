/* clang-format off */
#include "c_abstract_http/http_ws.h"
#include "c_abstract_http/log.h"
#include "ws_internal.h"
#include "crypto_utils.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>





int ws_generate_key(char out_key[25]) {
  unsigned char random_bytes[16];
  char *base64_str = NULL;
  size_t base64_len = 0;
  int i;
  int res;
  static int rand_initialized = 0;

  /* Generate 16 random bytes using rand() fallback */
  if (!rand_initialized) {
    srand((unsigned int)time(NULL));
    rand_initialized = 1;
  }
  for (i = 0; i < 16; ++i) {
    random_bytes[i] = (unsigned char)(rand() % 256);
  }

  res = base64_encode(random_bytes, 16, &base64_str, &base64_len);
  if (res != 0)
    return res;



  C_ABSTRACT_HTTP_STRCPY_S(out_key, 25, base64_str);
  free(base64_str);

  return 0;
}

int ws_sign_key(const char *client_key, char out_accept[29]) {
  const char *magic_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char concatenated[100];
  struct sha1_ctx ctx;
  unsigned char hash[20];
  char *base64_str = NULL;
  size_t base64_len = 0;
  int res;
  size_t len1 = strlen(client_key);
  size_t len2 = strlen(magic_guid);

  if (len1 + len2 >= sizeof(concatenated))
    return -1;

  C_ABSTRACT_HTTP_STRCPY_S(concatenated, sizeof(concatenated), client_key);
  C_ABSTRACT_HTTP_STRCPY_S(concatenated + len1, sizeof(concatenated) - len1,
                           magic_guid);

  sha1_init(&ctx);
  sha1_update(&ctx, (const unsigned char *)concatenated, len1 + len2);
  sha1_final(&ctx, hash);

  res = base64_encode(hash, 20, &base64_str, &base64_len);
  if (res != 0)
    return res;



  C_ABSTRACT_HTTP_STRCPY_S(out_accept, 29, base64_str);
  free(base64_str);

  return 0;
}

int ws_verify_accept(const char *client_key, const char *server_accept) {
  char expected_accept[29];
  int res = ws_sign_key(client_key, expected_accept);
  if (res != 0)
    return res;

  if (const_time_streq(expected_accept, server_accept)) {
    return 0;
  }
  return -1002; /* C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE */
}

int c_abstract_http_ws_init(struct HttpRequest *req,
                            const struct c_abstract_http_ws_config *config) {
  char key[25] = {0};
  int res;

  if (!req)
    return EINVAL;

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

  return 0;
}

uint16_t ws_htons(uint16_t hostshort) {
  uint16_t out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)(hostshort >> 8);
  p[1] = (unsigned char)(hostshort & 0xFF);
  return out;
}

uint64_t ws_htonll(uint64_t hostqword) {
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

uint16_t ws_ntohs(uint16_t netshort) {
  const unsigned char *p = (const unsigned char *)&netshort;
  return (uint16_t)((p[0] << 8) | p[1]);
}

uint64_t ws_ntohll(uint64_t netqword) {
  unsigned char p[8]; memcpy(p, &netqword, 8);
  return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
         ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
         ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
         ((uint64_t)p[6] << 8) | (uint64_t)p[7];
}

int ws_generate_mask_key(unsigned char out_key[4]) {
  int i;
  static int rand_initialized = 0;
  if (!rand_initialized) {
    srand((unsigned int)time(NULL));
    rand_initialized = 1;
  }
  for (i = 0; i < 4; ++i) {
    out_key[i] = (unsigned char)(rand() % 256);
  }
  return 0;
}

int ws_apply_mask(unsigned char *payload, size_t len,
                  const unsigned char mask_key[4]) {
  size_t i;
  if (!payload || len == 0)
    return 0;
  for (i = 0; i < len; i++) {
    payload[i] ^= mask_key[i % 4];
  }
  return 0;
}

int ws_pack_header_small(unsigned char *buf, int fin, int opcode, int mask,
                         size_t len) {
  if (!buf || len > 125)
    return -1;
  buf[0] = (unsigned char)((fin ? 0x80 : 0x00) | (opcode & 0x0F));
  buf[1] = (unsigned char)((mask ? 0x80 : 0x00) | (len & 0x7F));
  return 2;
}

int ws_pack_header_medium(unsigned char *buf, int fin, int opcode, int mask,
                          size_t len) {
  uint16_t net_len;
  if (!buf || len <= 125 || len > 65535)
    return -1;
  buf[0] = (unsigned char)((fin ? 0x80 : 0x00) | (opcode & 0x0F));
  buf[1] = (unsigned char)((mask ? 0x80 : 0x00) | 126);
  net_len = ws_htons((uint16_t)len);
  memcpy(buf + 2, &net_len, 2);
  return 4;
}

int ws_pack_header_large(unsigned char *buf, int fin, int opcode, int mask,
                         size_t len) {
  uint64_t net_len;
  if (!buf || len <= 65535)
    return -1;
  buf[0] = (unsigned char)((fin ? 0x80 : 0x00) | (opcode & 0x0F));
  buf[1] = (unsigned char)((mask ? 0x80 : 0x00) | 127);
  net_len = ws_htonll((uint64_t)len);
  memcpy(buf + 2, &net_len, 8);
  return 10;
}

#include "ws_config.h"
/* clang-format on */

int ws_parser_init(struct ws_parser_ctx *ctx,
                   c_abstract_http_ws_on_message on_msg,
                   c_abstract_http_ws_on_error on_err,
                   c_abstract_http_ws_on_close on_cls, void *user_data) {
  if (!ctx)
    return EINVAL;
  memset(ctx, 0, sizeof(*ctx));

  ctx->payload_capacity = 4096;
  ctx->payload_buffer = (unsigned char *)malloc(ctx->payload_capacity);
  if (!ctx->payload_buffer)
    return ENOMEM; /* ENOMEM fallback */

  ctx->on_message = on_msg;
  ctx->on_error = on_err;
  ctx->on_close = on_cls;
  ctx->user_data = user_data;

  ctx->state = WS_PARSER_READ_OPCODE;
  return 0;
}

void ws_parser_destroy(struct ws_parser_ctx *ctx) {
  if (!ctx)
    return;
  free(ctx->payload_buffer);
  free(ctx->reassembly_buffer);
  memset(ctx, 0, sizeof(*ctx));
}

int ws_parser_feed(struct ws_parser_ctx *ctx, const unsigned char *chunk,
                   size_t len) {
  size_t i = 0;
  if (!ctx || (!chunk && len > 0)) {
    LOG_DEBUG("ws_parser_feed: Error EINVAL");
    return EINVAL;
  }
  while (i < len) {
    switch (ctx->state) {
    case WS_PARSER_READ_OPCODE: {
      unsigned char b = chunk[i++];
      ctx->current_frame.fin = (b & 0x80) != 0;
      ctx->current_frame.opcode = b & 0x0F;

      /* Fail if any RSV bits are set */
      if (b & 0x70) {
        if (ctx->on_error)
          ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
        return C_ABSTRACT_HTTP_ERR_WS_FRAMING; /* C_ABSTRACT_HTTP_ERR_WS_FRAMING
                                                */
      }

      /* Fail if control frame is fragmented */
      if (!ctx->current_frame.fin && ctx->current_frame.opcode >= 0x08) {
        if (ctx->on_error)
          ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
        return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
      }

      ctx->state = WS_PARSER_READ_LEN;
      break;
    }
    case WS_PARSER_READ_LEN: {
      unsigned char b = chunk[i++];
      unsigned char base_len = b & 0x7F;
      ctx->current_frame.mask = (b & 0x80) != 0;

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
        ctx->current_frame.payload_len = ws_ntohs(net_len);
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
        ctx->current_frame.payload_len = ws_ntohll(net_len);
        ctx->state = ctx->current_frame.mask ? WS_PARSER_READ_MASK
                                             : WS_PARSER_READ_PAYLOAD;
        ctx->payload_offset = 0;

        /* EMSGSIZE logic */
        if (ctx->current_frame.payload_len >
            C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE) {
          if (ctx->on_error)
            ctx->on_error(90, ctx->user_data); /* EMSGSIZE */
          return 90;
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
          if (ctx->on_error)
            ctx->on_error(ENOMEM, ctx->user_data); /* ENOMEM */
          return ENOMEM;
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
          int status = 1005; /* Default */
          if (ctx->current_frame.payload_len >= 2) {
            uint16_t net_status;
            memcpy(&net_status, ctx->payload_buffer, 2);
            status = ws_ntohs(net_status);
          }
          if (ctx->on_close)
            ctx->on_close(status, ctx->user_data);
        } else if (ctx->current_frame.opcode ==
                   C_ABSTRACT_HTTP_WS_OPCODE_PING) {
          /* Auto-pong handled by higher level, we just trigger callback if
           * desired */
          /* (For this plan, auto-pong is requested, but typically we'd queue a
             write here. We'll trigger message for now and let the caller
             respond, or implement auto-pong in send later) */
          struct c_abstract_http_ws_event ev;
          ev.opcode = ctx->current_frame.opcode;
          ev.payload = ctx->payload_buffer;
          ev.payload_len = (size_t)ctx->current_frame.payload_len;
          ev.is_fin = ctx->current_frame.fin;
          if (ctx->on_message)
            ctx->on_message(&ev, ctx->user_data);
        } else if (ctx->current_frame.opcode ==
                   C_ABSTRACT_HTTP_WS_OPCODE_PONG) {
          struct c_abstract_http_ws_event ev;
          ev.opcode = ctx->current_frame.opcode;
          ev.payload = ctx->payload_buffer;
          ev.payload_len = (size_t)ctx->current_frame.payload_len;
          ev.is_fin = ctx->current_frame.fin;
          if (ctx->on_message)
            ctx->on_message(&ev, ctx->user_data);
        } else {
          /* Data Frame (Text or Binary or Continuation) */
          if (!ctx->current_frame.fin) {
            /* Fragmented message */
            if (ctx->current_frame.opcode !=
                    C_ABSTRACT_HTTP_WS_OPCODE_CONTINUATION &&
                ctx->reassembly_offset > 0) {
              /* EPROTO: new data frame before old continuation finished */
              if (ctx->on_error)
                ctx->on_error(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx->user_data);
              return C_ABSTRACT_HTTP_ERR_WS_FRAMING;
            }

            /* Expand reassembly buffer */
            if (ctx->reassembly_offset + ctx->current_frame.payload_len >
                ctx->reassembly_capacity) {
              size_t new_cap = ctx->reassembly_capacity == 0
                                   ? 4096
                                   : ctx->reassembly_capacity * 2;
              while (new_cap <
                     ctx->reassembly_offset + ctx->current_frame.payload_len)
                new_cap *= 2;

              /* Memory limit bounds check */
              if (new_cap > C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE) {
                if (ctx->on_error)
                  ctx->on_error(90, ctx->user_data); /* EMSGSIZE */
                return 90;
              }

              {
                unsigned char *new_buf =
                    (unsigned char *)realloc(ctx->reassembly_buffer, new_cap);
                if (!new_buf) {
                  if (ctx->on_error)
                    ctx->on_error(ENOMEM, ctx->user_data);
                  return ENOMEM;
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
            ev.opcode = ctx->current_frame.opcode;
            ev.is_fin = 1;

            if (ctx->reassembly_offset > 0) {
              /* Expand to fit final chunk */
              if (ctx->reassembly_offset + ctx->current_frame.payload_len >
                  ctx->reassembly_capacity) {
                size_t new_cap = ctx->reassembly_offset +
                                 (size_t)ctx->current_frame.payload_len;
                unsigned char *new_buf =
                    (unsigned char *)realloc(ctx->reassembly_buffer, new_cap);
                if (!new_buf) {
                  if (ctx->on_error)
                    ctx->on_error(ENOMEM, ctx->user_data);
                  return ENOMEM;
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
              if (ctx->on_message)
                ctx->on_message(&ev, ctx->user_data);

              ctx->reassembly_offset = 0; /* Reset for next message */
            } else {
              /* Single frame message */
              ev.payload = ctx->payload_buffer;
              ev.payload_len = (size_t)ctx->current_frame.payload_len;
              if (ctx->on_message)
                ctx->on_message(&ev, ctx->user_data);
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
    }
  }
  return 0;
}

int c_abstract_http_ws_sync_read_loop(struct HttpClient *client,
                                      struct HttpRequest *req,
                                      c_abstract_http_ws_on_message on_msg,
                                      c_abstract_http_ws_on_error on_err,
                                      c_abstract_http_ws_on_close on_close,
                                      void *user_data,
                                      volatile int *exit_flag) {
  int rc;
  struct HttpResponse *res = NULL;
  struct ws_parser_ctx parser;
  cah_cppcheck_mut_ptr((void *)exit_flag);

  if (!client || !req) {
    LOG_DEBUG("c_abstract_http_ws_sync_read_loop: Error EINVAL");
    return EINVAL;
  }

  if (exit_flag && *exit_flag)
    return 0;

  rc = c_abstract_http_ws_init(req, NULL);
  if (rc != 0) {
    if (on_err)
      on_err(rc, user_data);
    return rc;
  }

  rc = ws_parser_init(&parser, on_msg, on_err, on_close, user_data);
  if (rc != 0) {
    if (on_err)
      on_err(rc, user_data);
    return rc;
  }

  rc = client->send(client->transport, req, &res);
  if (rc != 0 || !res) {
    ws_parser_destroy(&parser);
    if (on_err)
      on_err(rc, user_data);
    return rc;
  }

  if (res->body && res->body_len > 0) {
    (void)ws_parser_feed(&parser, res->body, res->body_len);
    if (rc != 0 && on_err) {
      on_err(rc, user_data);
    }
  }

  ws_parser_destroy(&parser);
  http_response_free(res);
  free(res);

  if (on_close)
    on_close(200, user_data);

  return 0;
}
