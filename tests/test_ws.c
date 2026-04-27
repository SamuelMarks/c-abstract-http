/* clang-format off */
#include "../include/c_abstract_http/http_ws.h"
#include "../src/ws_internal.h"
#include "greatest.h"
#include <string.h>
/* clang-format on */

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
  struct HttpRequest req;
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
  (void)ctx;
  (void)req;
  res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  memset(res, 0, sizeof(*res));
  res->body = (unsigned char *)strdup("\x81\x05Hello"); /* Text frame "Hello" */
  res->body_len = 7;
  *res_out = res;
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
  uint64_t l = ws_htonll(0x1234567890ABCDEFULL);
  uint64_t r_l = ws_ntohll(l);

  ASSERT_EQ(0x1234, r_s);
  ASSERT(r_l == 0x1234567890ABCDEFULL);
  PASS();
}

TEST test_ws_generate_mask(void) {
  unsigned char mask[4];
  ws_generate_mask_key(mask);
  PASS();
}

TEST test_ws_parser_ext_len(void) {
  struct ws_parser_ctx ctx;
  unsigned char frame16[4 + 126];
  unsigned char frame64[10 + 65536];
  int rc;

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

SUITE(ws_suite) {
  RUN_TEST(test_ws_parser_ext_len);
  RUN_TEST(test_ws_parser_errors);
  RUN_TEST(test_ws_endian);
  RUN_TEST(test_ws_generate_mask);
  RUN_TEST(test_ws_generate_key_length);
  RUN_TEST(test_ws_sign_key_rfc_example);
  RUN_TEST(test_ws_verify_accept_success);
  RUN_TEST(test_ws_verify_accept_failure);
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
}
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(ws_suite);
  GREATEST_MAIN_END();
}
