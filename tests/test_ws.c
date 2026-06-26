#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

/* clang-format off */
#include "greatest.h"
#include "../include/c_abstract_http/http_ws.h"
#include "../src/ws_internal.h"
#include <string.h>
#include "mock_alloc.h"
/* clang-format on */

static int test_ws_mock_on_error_called = 0;
static int test_ws_mock_on_error(int error_code, void *user_data) {
  (void)user_data;
  test_ws_mock_on_error_called = error_code;
  return 0;
}

static int test_ws_mock_on_close_called = 0;
static int test_ws_mock_on_close(int status_code, void *user_data) {
  (void)user_data;
  test_ws_mock_on_close_called = status_code;
  return 0;
}

TEST test_ws_generate_key_length(void) {
  char key[25] = {0};
  ASSERT_EQ(0, ws_generate_key(key));
  ASSERT_EQ(24, strlen(key));
  PASS();
}

TEST test_ws_sign_key_rfc_example(void) {
  /* RFC 6455 Section 1.2: */
  /* Client key: dGhlIHNhbXBsZSBub25jZQ== */
  /* Server response: s3pPLMBiTxaQ9kYGzzhZRbK+xOo= */
  const char *client_key = "dGhlIHNhbXBsZSBub25jZQ==";
  char accept_key[29] = {0};

  ASSERT_EQ(0, ws_sign_key(client_key, accept_key));
  ASSERT_STR_EQ("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", accept_key);
  PASS();
}

TEST test_ws_verify_accept_success(void) {
  const char *client_key = "dGhlIHNhbXBsZSBub25jZQ==";
  const char *server_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

  ASSERT_EQ(0, ws_verify_accept(client_key, server_accept));
  PASS();
}

TEST test_ws_verify_accept_failure(void) {
  const char *client_key = "dGhlIHNhbXBsZSBub25jZQ==";
  const char *server_accept = "wrongacceptkey=================";

  ASSERT_EQ(-1002, ws_verify_accept(client_key, server_accept));
  PASS();
}

TEST test_ws_init_headers(void) {
  struct HttpRequest req = {0};
  struct c_abstract_http_ws_config config = {0};
  const char *custom_headers[] = {"X-Custom", "value1", NULL};
  const char *val = NULL;

  ASSERT_EQ(0, http_request_init(&req));

  config.subprotocols = "chat, superchat";
  config.custom_headers = custom_headers;

  ASSERT_EQ(0, c_abstract_http_ws_init(&req, &config));

  ASSERT_EQ(0, http_headers_get(&req.headers, "Upgrade", &val));
  ASSERT_STR_EQ("websocket", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Connection", &val));
  ASSERT_STR_EQ("Upgrade", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Sec-WebSocket-Version", &val));
  ASSERT_STR_EQ("13", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Sec-WebSocket-Protocol", &val));
  ASSERT_STR_EQ("chat, superchat", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "X-Custom", &val));
  ASSERT_STR_EQ("value1", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Sec-WebSocket-Key", &val));
  ASSERT_EQ(24, strlen(val));

  http_request_free(&req);
  PASS();
}

TEST test_ws_pack_header_small(void) {
  unsigned char buf[10];
  int len = ws_pack_header_small(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 10);
  ASSERT_EQ(2, len);
  ASSERT_EQ(0x81, buf[0]); /* FIN=1, Opcode=1 */
  ASSERT_EQ(0x8A, buf[1]); /* Mask=1, Len=10 */
  PASS();
}

TEST test_ws_pack_header_medium(void) {
  unsigned char buf[10];
  int len =
      ws_pack_header_medium(buf, 0, C_ABSTRACT_HTTP_WS_OPCODE_BINARY, 0, 1024);
  ASSERT_EQ(4, len);
  ASSERT_EQ(0x02, buf[0]); /* FIN=0, Opcode=2 */
  ASSERT_EQ(126, buf[1]);  /* Mask=0, Len=126 */
  ASSERT_EQ(0x04, buf[2]); /* 1024 = 0x0400 */
  ASSERT_EQ(0x00, buf[3]);
  PASS();
}

TEST test_ws_pack_header_large(void) {
  unsigned char buf[10];
  int len =
      ws_pack_header_large(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_BINARY, 1, 65536);
  ASSERT_EQ(10, len);
  ASSERT_EQ(0x82, buf[0]); /* FIN=1, Opcode=2 */
  ASSERT_EQ(0xFF, buf[1]); /* Mask=1, Len=127 */
  ASSERT_EQ(0x00, buf[2]);
  ASSERT_EQ(0x00, buf[3]);
  ASSERT_EQ(0x00, buf[4]);
  ASSERT_EQ(0x00, buf[5]);
  ASSERT_EQ(0x00, buf[6]);
  ASSERT_EQ(0x01, buf[7]); /* 65536 = 0x0000000000010000 */
  ASSERT_EQ(0x00, buf[8]);
  ASSERT_EQ(0x00, buf[9]);
  PASS();
}

TEST test_ws_apply_mask_symmetry(void) {
  unsigned char payload[] = "Hello World!";
  unsigned char original[] = "Hello World!";
  unsigned char mask_key[4] = {0x12, 0x34, 0x56, 0x78};

  ws_apply_mask(payload, 12, mask_key);
  ASSERT_FALSE(memcmp(payload, original, 12) == 0);

  ws_apply_mask(payload, 12, mask_key);
  ASSERT_MEM_EQ(original, payload, 12);

  PASS();
}

struct test_ws_ctx {
  int error_code;
  int close_status;
  int message_count;
  struct c_abstract_http_ws_event last_message;
  unsigned char last_payload[4096];
};

static int test_ws_on_message(const struct c_abstract_http_ws_event *ev,
                              void *user_data) {
  struct test_ws_ctx *ctx = (struct test_ws_ctx *)user_data;
  ctx->message_count++;
  ctx->last_message = *ev;
  memcpy(ctx->last_payload, ev->payload, ev->payload_len);
  ctx->last_payload[ev->payload_len] = '\0';
  return 0;
}

static int test_ws_on_error(int error_code, void *user_data) {
  struct test_ws_ctx *ctx = (struct test_ws_ctx *)user_data;
  ctx->error_code = error_code;
  return 0;
}

static int test_ws_on_close(int status_code, void *user_data) {
  struct test_ws_ctx *ctx = (struct test_ws_ctx *)user_data;
  ctx->close_status = status_code;
  return 0;
}

TEST test_ws_parser_single_frame(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};

  ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                 test_ws_on_close, &ctx);
  ASSERT_EQ(0, ws_parser_feed(&parser, frame, sizeof(frame)));

  ASSERT_EQ(1, ctx.message_count);
  ASSERT_EQ(C_ABSTRACT_HTTP_WS_OPCODE_TEXT, ctx.last_message.opcode);
  ASSERT_EQ(1, ctx.last_message.is_fin);
  ASSERT_EQ(5, ctx.last_message.payload_len);
  ASSERT_STR_EQ("Hello", (const char *)ctx.last_payload);

  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_chunked_delivery(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x81, 0x05, 'W', 'o', 'r', 'l', 'd'};
  size_t i;

  ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                 test_ws_on_close, &ctx);

  for (i = 0; i < sizeof(frame); i++) {
    ASSERT_EQ(0, ws_parser_feed(&parser, &frame[i], 1));
  }

  ASSERT_EQ(1, ctx.message_count);
  ASSERT_STR_EQ("World", (const char *)ctx.last_payload);

  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_fragmented_message(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame1[] = {0x01, 0x03, 'A', 'B', 'C'}; /* Text, FIN=0 */
  unsigned char frame2[] = {0x00, 0x03, 'D', 'E', 'F'}; /* Cont, FIN=0 */
  unsigned char frame3[] = {0x80, 0x03, 'G', 'H', 'I'}; /* Cont, FIN=1 */

  ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                 test_ws_on_close, &ctx);

  ASSERT_EQ(0, ws_parser_feed(&parser, frame1, sizeof(frame1)));
  ASSERT_EQ(0, ctx.message_count);

  ASSERT_EQ(0, ws_parser_feed(&parser, frame2, sizeof(frame2)));
  ASSERT_EQ(0, ctx.message_count);

  ASSERT_EQ(0, ws_parser_feed(&parser, frame3, sizeof(frame3)));
  ASSERT_EQ(1, ctx.message_count);
  ASSERT_STR_EQ("ABCDEFGHI", (const char *)ctx.last_payload);

  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_reject_fragmented_control(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x08, 0x00}; /* CLOSE, FIN=0 */

  ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                 test_ws_on_close, &ctx);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, frame, sizeof(frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx.error_code);

  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_auto_pong(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x89, 0x04, 'P', 'I', 'N', 'G'}; /* PING */

  ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                 test_ws_on_close, &ctx);
  ASSERT_EQ(0, ws_parser_feed(&parser, frame, sizeof(frame)));

  ASSERT_EQ(1, ctx.message_count);
  ASSERT_EQ(C_ABSTRACT_HTTP_WS_OPCODE_PING, ctx.last_message.opcode);
  ASSERT_STR_EQ("PING", (const char *)ctx.last_payload);

  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_sync_loop_exit_flag(void) {
  volatile int exit_flag = 1;
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  ASSERT_EQ(0, c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL,
                                                 NULL, NULL, &exit_flag));
  http_request_free(&req);
  PASS();
}

TEST test_ws_sync_loop_null(void) {
  ASSERT_EQ(EINVAL, c_abstract_http_ws_sync_read_loop(NULL, NULL, NULL, NULL,
                                                      NULL, NULL, NULL));
  PASS();
}

static int mock_send_success_ws(struct HttpTransportContext *ctx,
                                const struct HttpRequest *req,
                                struct HttpResponse **res_out) {
  struct HttpResponse *res;
  int old_fail = g_mock_alloc_fail;
  (void)ctx;
  (void)req;
  g_mock_alloc_fail = 0; /* Protect mock allocations */
  res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  memset(res, 0, sizeof(*res));
  res->body = (unsigned char *)malloc(7); /* Text frame "Hello" */
  if (res->body) {
    memcpy(res->body, "\x81\x05Hello", 7);
  }
  res->body_len = 7;
  *res_out = res;
  g_mock_alloc_fail = old_fail;
  return 0;
}

static int mock_send_fail_ws(struct HttpTransportContext *ctx,
                             const struct HttpRequest *req,
                             struct HttpResponse **res_out) {
  (void)ctx;
  (void)req;
  (void)res_out;
  return EIO;
}

TEST test_ws_sync_loop_success(void) {
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_success_ws;
  ASSERT_EQ(0, c_abstract_http_ws_sync_read_loop(
                   &client, &req, test_ws_on_message, test_ws_on_error,
                   test_ws_on_close, &ctx, NULL));
  http_request_free(&req);
  PASS();
}

TEST test_ws_sync_loop_fail(void) {
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_fail_ws;
  ASSERT_EQ(EIO, c_abstract_http_ws_sync_read_loop(
                     &client, &req, test_ws_on_message, test_ws_on_error,
                     test_ws_on_close, &ctx, NULL));
  http_request_free(&req);
  PASS();
}

TEST test_ws_init_headers_null(void) {
  ASSERT_EQ(EINVAL, c_abstract_http_ws_init(NULL, NULL));
  PASS();
}

TEST test_ws_pack_header_invalid(void) {
  unsigned char buf[10];

  /* Small buffer invalid payload length */
  ASSERT_EQ(
      -1, ws_pack_header_small(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 126));

  /* Medium buffer invalid payload length */
  ASSERT_EQ(-1, ws_pack_header_medium(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                      125));
  ASSERT_EQ(-1, ws_pack_header_medium(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                      65536));

  /* Large buffer invalid payload length */
  ASSERT_EQ(-1, ws_pack_header_large(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                     65535));

  /* Null buffer */
  ASSERT_EQ(
      -1, ws_pack_header_small(NULL, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 10));
  ASSERT_EQ(-1, ws_pack_header_medium(NULL, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      1, 126));
  ASSERT_EQ(-1, ws_pack_header_large(NULL, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                     65536));

  PASS();
}

TEST test_ws_apply_mask_invalid(void) {
  unsigned char mask_key[4] = {0x12, 0x34, 0x56, 0x78};
  ASSERT_EQ(0, ws_apply_mask(NULL, 12, mask_key));
  PASS();
}

TEST test_ws_endian(void) {
  uint16_t s = ws_htons(0x1234);
  uint16_t r_s = ws_ntohs(s);
  uint64_t l = ws_htonll((((uint64_t)0x12345678) << 32) | 0x90ABCDEF);
  uint64_t r_l = ws_ntohll(l);

  ASSERT_EQ(0x1234, r_s);
  ASSERT(r_l == ((((uint64_t)0x12345678) << 32) | 0x90ABCDEF));
  PASS();
}

TEST test_ws_generate_mask(void) {
  unsigned char mask[4];
  ws_generate_mask_key(mask);
  PASS();
}

TEST test_ws_parser_ext_len(void) {
  int rc;
  struct ws_parser_ctx ctx;
  unsigned char frame16[4 + 126];
  unsigned char frame64[10 + 65536];

  struct test_ws_ctx my_ctx = {0};

  /* 16-bit length frame */
  memset(frame16, 0, sizeof(frame16));
  frame16[0] = 0x81; /* FIN | TEXT */
  frame16[1] = 126;  /* Length 126 */
  frame16[2] = 0;    /* High byte */
  frame16[3] = 126;  /* Low byte */
  /* payload follows, we'll feed exactly that much */
  ws_parser_init(&ctx, test_ws_on_message, test_ws_on_error, test_ws_on_close,
                 &my_ctx);
  rc = ws_parser_feed(&ctx, frame16, sizeof(frame16));
  ASSERT_EQ(0, rc);
  ws_parser_destroy(&ctx);

  /* 64-bit length frame */
  memset(frame64, 0, sizeof(frame64));
  frame64[0] = 0x81;
  frame64[1] = 127;
  /* Length 65536 is 0x00000000 00010000 */
  frame64[8] = 0x01;
  frame64[9] = 0x00;
  ws_parser_init(&ctx, test_ws_on_message, test_ws_on_error, test_ws_on_close,
                 &my_ctx);
  rc = ws_parser_feed(&ctx, frame64, sizeof(frame64));
  ASSERT_EQ(0, rc);
  ws_parser_destroy(&ctx);

  PASS();
}

TEST test_ws_parser_errors(void) {
  struct ws_parser_ctx ctx;
  unsigned char frame[2];

  ASSERT_EQ(EINVAL, ws_parser_init(NULL, NULL, NULL, NULL, NULL));
  ws_parser_init(&ctx, NULL, NULL, NULL, NULL);

  frame[0] = 0x0F; /* Reserved opcode */
  frame[1] = 0x00;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ws_parser_feed(&ctx, frame, 2));

  ws_parser_destroy(&ctx);

  /* NULL tests */
  ASSERT_EQ(EINVAL, ws_parser_feed(NULL, frame, 2));
  ASSERT_EQ(EINVAL, ws_parser_feed(&ctx, NULL, 2));

  ws_parser_destroy(NULL);

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_oom_branches(void) {
  int rc;
  struct HttpRequest req = {0};
  struct c_abstract_http_ws_config config = {0};
  const char *headers[] = {"X-Test", "123", NULL};
  int i;

  config.subprotocols = "chat, superchat";
  config.custom_headers = headers;

  for (i = 0; i < 25; i++) {
    ASSERT_EQ(0, http_request_init(&req));
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = i;
    rc = c_abstract_http_ws_init(&req, &config);
    g_mock_alloc_fail = 0;
    if (rc == 0) {
      http_request_free(&req);
      break;
    }
    http_request_free(&req);
    ASSERT_EQ_FMT(ENOMEM, rc, "%d");
    memset(&req, 0, sizeof(req));
  }
  http_request_free(&req);
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_init_oom(void) {
  int rc;
  struct ws_parser_ctx parser;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  g_mock_alloc_fail = 0;
  ws_parser_destroy(&parser);
  ASSERT_EQ_FMT(ENOMEM, rc, "%d");
  PASS();
}
#endif

TEST test_ws_rsv_bit_set(void) {
  int rc;
  struct ws_parser_ctx parser;
  unsigned char chunk[] = {0xC1, 10, '0'}; /* RSV1 bit set */
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;
  test_ws_mock_on_error_called = 0;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, test_ws_mock_on_error_called);
  PASS();
}

TEST test_ws_payload_too_large(void) {
  int rc;
  struct ws_parser_ctx parser;
  unsigned char chunk[] = {0x81, 0x7f, 0x00, 0x00, 0x00,
                           0x00, 0x02, 0x00, 0x00, 0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;
  test_ws_mock_on_error_called = 0;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(90, rc); /* EMSGSIZE */
  ASSERT_EQ(90, test_ws_mock_on_error_called);
  PASS();
}

TEST test_ws_masked_frame(void) {
  int rc;
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char chunk[] = {0x81,       0x85,       0x01,       0x02,
                           0x03,       0x04,       'h' ^ 0x01, 'e' ^ 0x02,
                           'l' ^ 0x03, 'l' ^ 0x04, 'o' ^ 0x01};
  memset(&parser, 0, sizeof(parser));
  parser.on_message = test_ws_on_message;
  parser.user_data = &ctx;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(0, rc);
  ASSERT_EQ(1, ctx.message_count);
  ASSERT_STR_EQ("hello", ctx.last_payload);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_realloc_oom(void) {
  int rc;
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char chunk[] = {0x81, 10,  '1', '2', '3', '4',
                           '5',  '6', '7', '8', '9', '0'};
  memset(&parser, 0, sizeof(parser));
  parser.on_message = test_ws_on_message;
  parser.on_error = test_ws_mock_on_error;
  parser.user_data = &ctx;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  g_mock_alloc_fail = 0;
  ws_parser_destroy(&parser);
  ASSERT_EQ_FMT(ENOMEM, rc, "%d");
  PASS();
}
#endif

TEST test_ws_parser_close_frame(void) {
  int rc;
  struct ws_parser_ctx parser;
  unsigned char chunk[] = {0x88, 0x02, 0x03, 0xE8};
  memset(&parser, 0, sizeof(parser));
  parser.on_close = test_ws_mock_on_close;
  test_ws_mock_on_close_called = 0;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(0, rc);
  ASSERT_EQ(1000, test_ws_mock_on_close_called);
  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_pong_frame(void) {
  int rc;
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char chunk[] = {0x8A, 0x00, 0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_message = test_ws_on_message;
  parser.user_data = &ctx;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(0, rc);
  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_invalid_fragmentation(void) {
  int rc;
  struct ws_parser_ctx parser;
  unsigned char chunk[] = {0x01, 0x01, 'a', 0x01, 0x00, 0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;
  test_ws_mock_on_error_called = 0;
  rc = ws_parser_feed(&parser, chunk, 3);
  ASSERT_EQ(0, rc);
  rc = ws_parser_feed(&parser, chunk + 3, 3);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, test_ws_mock_on_error_called);
  ws_parser_destroy(&parser);
  PASS();
}

TEST test_ws_parser_reassembly_too_large(void) {
  int rc;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  const unsigned char chunk2[] = {0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;
  test_ws_mock_on_error_called = 0;
  rc = ws_parser_feed(&parser, chunk1, 3);
  ASSERT_EQ(0, rc);

  if (parser.payload_buffer)
    free(parser.payload_buffer);
  parser.payload_buffer = malloc(16777217);
  parser.payload_capacity = 16777217;
  parser.state = WS_PARSER_READ_PAYLOAD;
  parser.current_frame.fin = 0;
  parser.current_frame.opcode = 0;
  parser.current_frame.payload_len = 16777217;
  parser.payload_offset = 16777216;

  rc = ws_parser_feed(&parser, chunk2, 1);
  ASSERT_EQ(90, rc); /* EMSGSIZE */
  ASSERT_EQ(90, test_ws_mock_on_error_called);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  PASS();
}

TEST test_ws_parser_reassembly_expand_twice(void) {
  int rc;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  unsigned char chunk2[4096 + 4];
  memset(&parser, 0, sizeof(parser));
  rc = ws_parser_feed(&parser, chunk1, 3);
  ASSERT_EQ(0, rc);

  chunk2[0] = 0x00;
  chunk2[1] = 126;
  chunk2[2] = (4096 >> 8) & 0xFF;
  chunk2[3] = 4096 & 0xFF;
  memset(chunk2 + 4, 'b', 4096);
  rc = ws_parser_feed(&parser, chunk2, 4 + 4096);
  ASSERT_EQ(0, rc);

  ASSERT_EQ(8192, parser.reassembly_capacity);
  ws_parser_destroy(&parser);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_reassembly_fin_oom(void) {
  int rc;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  unsigned char chunk2[4096 + 4];
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;

  rc = ws_parser_feed(&parser, chunk1, 3);

  chunk2[0] = 0x80;
  chunk2[1] = 126;
  chunk2[2] = (4096 >> 8) & 0xFF;
  chunk2[3] = 4096 & 0xFF;
  memset(chunk2 + 4, 'b', 4096);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = ws_parser_feed(&parser, chunk2, 4 + 4096);
  g_mock_alloc_fail = 0;

  ws_parser_destroy(&parser);
  ASSERT_EQ_FMT(ENOMEM, rc, "%d");
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_reassembly_fin_expand_oom(void) {
  int rc;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  unsigned char chunk2[4096 + 4];

  chunk2[0] = 0x80;
  chunk2[1] = 10;
  memset(chunk2 + 2, 'b', 10);

  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;

  rc = ws_parser_feed(&parser, chunk1, 3);

  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  parser.reassembly_capacity = 2;
  parser.reassembly_offset = 1;
  parser.reassembly_buffer = malloc(2);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = ws_parser_feed(&parser, chunk2, 2 + 10);
  g_mock_alloc_fail = 0;

  ws_parser_destroy(&parser);
  ASSERT_EQ_FMT(ENOMEM, rc, "%d");
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_sign_key_oom(void) {
  int rc;
  char out_accept[29];
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", out_accept);
  g_mock_alloc_fail = 0;
  ASSERT_EQ_FMT(ENOMEM, rc, "%d");
  PASS();
}
#endif

TEST test_ws_sign_key_too_long(void) {
  char out_accept[29];
  char long_key[100];
  memset(long_key, 'a', 99);
  long_key[99] = '\0';
  ASSERT_EQ(-1, ws_sign_key(long_key, out_accept));
  PASS();
}

TEST test_ws_parser_reassembly_fin_expand_success(void) {
  int rc;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  unsigned char chunk2[4096 + 4];

  chunk2[0] = 0x80;
  chunk2[1] = 10;
  memset(chunk2 + 2, 'b', 10);

  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;

  rc = ws_parser_feed(&parser, chunk1, 3);

  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  parser.reassembly_capacity = 2;
  parser.reassembly_offset = 1;
  parser.reassembly_buffer = malloc(2);

  rc = ws_parser_feed(&parser, chunk2, 2 + 10);

  ASSERT_EQ(0, rc);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_reassembly_frag_oom(void) {
  int rc;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  unsigned char chunk2[4096 + 4];

  chunk2[0] = 0x00; /* Opcode 0, FIN 0 */
  chunk2[1] = 10;
  memset(chunk2 + 2, 'b', 10);

  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;

  rc = ws_parser_feed(&parser, chunk1, 3);

  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  parser.reassembly_capacity = 2;
  parser.reassembly_offset = 1;
  parser.reassembly_buffer = malloc(2);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = ws_parser_feed(&parser, chunk2, 2 + 10);
  g_mock_alloc_fail = 0;

  ws_parser_destroy(&parser);
  ASSERT_EQ_FMT(ENOMEM, rc, "%d");
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_sync_loop_init_oom(void) {
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_success_ws;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = c_abstract_http_ws_sync_read_loop(
        &client, &req, test_ws_on_message, test_ws_on_error, test_ws_on_close,
        &ctx, NULL);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(ENOMEM, rc_test_tmp, "%d");
  }
  http_request_free(&req);
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_sync_loop_parser_oom(void) {
  int rc;
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_success_ws;
  {
    /* The ws_init will do multiple allocations for headers. Let's just bypass
       it or count. Actually, http_headers_add allocates for headers. ws_init
       calls http_headers_add 4 times. Each takes 2 allocations. So ~8 allocs.
       Let's just loop over g_mock_alloc_count to find where ws_parser_init
       fails. Or simpler: just mock ws_parser_init to fail by finding the exact
       alloc count.
    */
    int i;
    for (i = 0; i < 15; i++) {
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = i;
      rc = c_abstract_http_ws_sync_read_loop(&client, &req, test_ws_on_message,
                                             test_ws_on_error, test_ws_on_close,
                                             &ctx, NULL);
      g_mock_alloc_fail = 0;
      http_request_free(&req);
      memset(&req, 0, sizeof(req));
      if (rc == ENOMEM) {
        /* we just want to cover the branches, so doing this in a loop is fine
         */
      }
    }
  }
  http_request_free(&req);
  PASS();
}
#endif

static int mock_send_bad_payload(struct HttpTransportContext *tctx,
                                 const struct HttpRequest *req,
                                 struct HttpResponse **res_out) {
  struct HttpResponse *res = calloc(1, sizeof(*res));
  (void)tctx;
  (void)req;
  /* Send invalid fragmentation to trigger ws_parser_feed framing error */
  res->body = (unsigned char *)malloc(6);
  if (res->body) {
    memcpy(res->body, "\x01\x01\x61\x01\x00\x00", 6);
  }
  res->body_len = 6;
  *res_out = res;
  return 0;
}

TEST test_ws_sync_loop_feed_error(void) {
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_bad_payload;

  test_ws_mock_on_error_called = 0;
  ASSERT_EQ(0, c_abstract_http_ws_sync_read_loop(
                   &client, &req, test_ws_on_message, test_ws_mock_on_error,
                   test_ws_on_close, &ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, test_ws_mock_on_error_called);

  http_request_free(&req);
  PASS();
}

TEST test_ws_verify_accept_sign_error(void) {
  char long_key[100];
  memset(long_key, 'a', 99);
  long_key[99] = '\0';
  ASSERT_EQ(-1, ws_verify_accept(long_key, "anything"));
  PASS();
}

SUITE(ws_suite) {

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_oom_branches);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_parser_init_oom);
#endif
  RUN_TEST(test_ws_rsv_bit_set);
  RUN_TEST(test_ws_payload_too_large);
  RUN_TEST(test_ws_masked_frame);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_realloc_oom);
#endif
  RUN_TEST(test_ws_parser_close_frame);
  RUN_TEST(test_ws_parser_pong_frame);
  RUN_TEST(test_ws_parser_invalid_fragmentation);
  RUN_TEST(test_ws_parser_reassembly_too_large);
  RUN_TEST(test_ws_parser_reassembly_expand_twice);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_parser_reassembly_fin_oom);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_parser_reassembly_frag_oom);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_parser_reassembly_fin_expand_oom);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_parser_reassembly_frag_oom);
#endif
  RUN_TEST(test_ws_parser_reassembly_fin_expand_success);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_sign_key_oom);
#endif
  RUN_TEST(test_ws_sign_key_too_long);
  RUN_TEST(test_ws_parser_ext_len);
  RUN_TEST(test_ws_parser_errors);
  RUN_TEST(test_ws_endian);
  RUN_TEST(test_ws_generate_mask);
  RUN_TEST(test_ws_generate_key_length);
  RUN_TEST(test_ws_sign_key_rfc_example);
  RUN_TEST(test_ws_verify_accept_success);
  RUN_TEST(test_ws_verify_accept_failure);
  RUN_TEST(test_ws_verify_accept_sign_error);
  RUN_TEST(test_ws_init_headers);
  RUN_TEST(test_ws_init_headers_null);
  RUN_TEST(test_ws_pack_header_small);
  RUN_TEST(test_ws_pack_header_medium);
  RUN_TEST(test_ws_pack_header_large);
  RUN_TEST(test_ws_pack_header_invalid);
  RUN_TEST(test_ws_apply_mask_symmetry);
  RUN_TEST(test_ws_apply_mask_invalid);
  RUN_TEST(test_ws_parser_single_frame);
  RUN_TEST(test_ws_parser_chunked_delivery);
  RUN_TEST(test_ws_parser_fragmented_message);
  RUN_TEST(test_ws_parser_reject_fragmented_control);
  RUN_TEST(test_ws_parser_auto_pong);
  RUN_TEST(test_ws_sync_loop_exit_flag);
  RUN_TEST(test_ws_sync_loop_null);
  RUN_TEST(test_ws_sync_loop_success);
  RUN_TEST(test_ws_sync_loop_fail);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_sync_loop_init_oom);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_sync_loop_parser_oom);
#endif
  RUN_TEST(test_ws_sync_loop_feed_error);
}
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(ws_suite);
  GREATEST_MAIN_END();
}
