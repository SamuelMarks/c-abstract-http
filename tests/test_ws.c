
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

/* clang-format off */
#ifdef C_ABSTRACT_HTTP_HEADER_ONLY
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>
#endif
#include "greatest.h"
#include "../include/c_abstract_http/http_ws.h"
#include "../include/c_abstract_http/thread_pool.h"
#include "../src/ws_internal.h"
#include "ws_config.h"
#include <string.h>
#include "mock_alloc.h"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <signal.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_ws_send_mode(int mode);
#endif

static int test_ws_mock_on_error_called = 0;
NO_DISCARD static c_abstract_http_error_t
test_ws_mock_on_error(int error_code, void *user_data) {
  (void)user_data;
  test_ws_mock_on_error_called = error_code;
  return 0;
}

static int test_ws_mock_on_close_called = 0;
NO_DISCARD static c_abstract_http_error_t
test_ws_mock_on_close(int status_code, void *user_data) {
  (void)user_data;
  test_ws_mock_on_close_called = status_code;
  return 0;
}

TEST test_ws_generate_key_length(void) {
  char key[25] = {0};
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_generate_key(key));
  ASSERT_EQ(24, strlen(key));
  PASS();
}

TEST test_ws_sign_key_rfc_example(void) {
  /* RFC 6455 Section 1.2: */
  /* Client key: dGhlIHNhbXBsZSBub25jZQ== */
  /* Server response: s3pPLMBiTxaQ9kYGzzhZRbK+xOo= */
  const char *client_key = "dGhlIHNhbXBsZSBub25jZQ==";
  char accept_key[29] = {0};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_sign_key(client_key, accept_key));
  ASSERT_STR_EQ("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", accept_key);
  PASS();
}

TEST test_ws_verify_accept_success(void) {
  const char *client_key = "dGhlIHNhbXBsZSBub25jZQ==";
  const char *server_accept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_verify_accept(client_key, server_accept));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  config.subprotocols = "chat, superchat";
  config.custom_headers = custom_headers;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, &config));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&req.headers, "Upgrade", &val));
  ASSERT_STR_EQ("websocket", val);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&req.headers, "Connection", &val));
  ASSERT_STR_EQ("Upgrade", val);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&req.headers, "Sec-WebSocket-Version", &val));
  ASSERT_STR_EQ("13", val);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&req.headers, "Sec-WebSocket-Protocol", &val));
  ASSERT_STR_EQ("chat, superchat", val);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&req.headers, "X-Custom", &val));
  ASSERT_STR_EQ("value1", val);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_get(&req.headers, "Sec-WebSocket-Key", &val));
  ASSERT_EQ(24, strlen(val));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_pack_header_small(void) {
  unsigned char buf[10];
  size_t len;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_pack_header_small(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 10,
                                 &len));
  ASSERT_EQ(2, len);
  ASSERT_EQ(0x81, buf[0]); /* FIN=1, Opcode=1 */
  ASSERT_EQ(0x8A, buf[1]); /* Mask=1, Len=10 */
  PASS();
}

TEST test_ws_pack_header_medium(void) {
  unsigned char buf[10];
  size_t len;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_pack_header_medium(buf, 0, C_ABSTRACT_HTTP_WS_OPCODE_BINARY, 0,
                                  1024, &len));
  ASSERT_EQ(4, len);
  ASSERT_EQ(0x02, buf[0]); /* FIN=0, Opcode=2 */
  ASSERT_EQ(126, buf[1]);  /* Mask=0, Len=126 */
  ASSERT_EQ(0x04, buf[2]); /* 1024 = 0x0400 */
  ASSERT_EQ(0x00, buf[3]);
  PASS();
}

TEST test_ws_pack_header_large(void) {
  unsigned char buf[10];
  size_t len;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_pack_header_large(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_BINARY, 1,
                                 65536, &len));
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_apply_mask(payload, 12, mask_key));
  ASSERT_FALSE(memcmp(payload, original, 12) == 0);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_apply_mask(payload, 12, mask_key));
  ASSERT_MEM_EQ(original, payload, 12);

  PASS();
}

/** @brief Documented */
struct test_ws_ctx {
  /** @brief Documented */
  int error_code;
  /** @brief Documented */
  int close_status;
  /** @brief Documented */
  int message_count;
  /** @brief Documented */
  struct c_abstract_http_ws_event last_message;
  /** @brief Documented */
  unsigned char last_payload[4096];
};

NO_DISCARD static c_abstract_http_error_t
test_ws_on_message(const struct c_abstract_http_ws_event *ev, void *user_data) {
  struct test_ws_ctx *ctx = (struct test_ws_ctx *)user_data;
  size_t copy_len = ev->payload_len < sizeof(ctx->last_payload) - 1
                        ? ev->payload_len
                        : sizeof(ctx->last_payload) - 1;
  ctx->message_count++;
  ctx->last_message = *ev;
  memcpy(ctx->last_payload, ev->payload, copy_len);
  ctx->last_payload[copy_len] = '\0';
  return 0;
}

NO_DISCARD static c_abstract_http_error_t test_ws_on_error(int error_code,
                                                           void *user_data) {
  struct test_ws_ctx *ctx = (struct test_ws_ctx *)user_data;
  ctx->error_code = error_code;
  return 0;
}

NO_DISCARD static c_abstract_http_error_t test_ws_on_close(int status_code,
                                                           void *user_data) {
  struct test_ws_ctx *ctx = (struct test_ws_ctx *)user_data;
  ctx->close_status = status_code;
  return 0;
}

TEST test_ws_parser_single_frame(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frame, sizeof(frame)));

  ASSERT_EQ(1, ctx.message_count);
  ASSERT_EQ(C_ABSTRACT_HTTP_WS_OPCODE_TEXT, ctx.last_message.opcode);
  ASSERT_EQ(1, ctx.last_message.is_fin);
  ASSERT_EQ(5, ctx.last_message.payload_len);
  ASSERT_STR_EQ("Hello", (const char *)ctx.last_payload);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_chunked_delivery(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x81, 0x05, 'W', 'o', 'r', 'l', 'd'};
  size_t i;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));

  for (i = 0; i < sizeof(frame); i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_feed(&parser, &frame[i], 1));
  }

  ASSERT_EQ(1, ctx.message_count);
  ASSERT_STR_EQ("World", (const char *)ctx.last_payload);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_fragmented_message(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame1[] = {0x01, 0x03, 'A', 'B', 'C'}; /* Text, FIN=0 */
  unsigned char frame2[] = {0x00, 0x03, 'D', 'E', 'F'}; /* Cont, FIN=0 */
  unsigned char frame3[] = {0x80, 0x03, 'G', 'H', 'I'}; /* Cont, FIN=1 */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frame1, sizeof(frame1)));
  ASSERT_EQ(0, ctx.message_count);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frame2, sizeof(frame2)));
  ASSERT_EQ(0, ctx.message_count);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frame3, sizeof(frame3)));
  ASSERT_EQ(1, ctx.message_count);
  ASSERT_STR_EQ("ABCDEFGHI", (const char *)ctx.last_payload);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_reject_fragmented_control(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x08, 0x00}; /* CLOSE, FIN=0 */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, frame, sizeof(frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ctx.error_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_auto_pong(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char frame[] = {0x89, 0x04, 'P', 'I', 'N', 'G'}; /* PING */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frame, sizeof(frame)));

  ASSERT_EQ(1, ctx.message_count);
  ASSERT_EQ(C_ABSTRACT_HTTP_WS_OPCODE_PING, ctx.last_message.opcode);
  ASSERT_STR_EQ("PING", (const char *)ctx.last_payload);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_sync_loop_exit_flag(void) {
  volatile int exit_flag = 1;
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, &exit_flag));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_sync_loop_null(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_sync_read_loop(NULL, NULL, NULL, NULL, NULL,
                                              NULL, NULL));
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
mock_send_success_ws(struct HttpTransportContext *ctx,
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

NO_DISCARD static c_abstract_http_error_t
mock_send_fail_ws(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req,
                  struct HttpResponse **res_out) {
  (void)ctx;
  (void)req;
  (void)res_out;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_ws_sync_loop_success(void) {
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_success_ws;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_sync_read_loop(&client, &req, test_ws_on_message,
                                              test_ws_on_error,
                                              test_ws_on_close, &ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_sync_loop_fail(void) {
  struct HttpClient client = {0};
  struct HttpRequest req = {0};
  struct test_ws_ctx ctx = {0};
  client.send = mock_send_fail_ws;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, test_ws_on_message,
                                              test_ws_on_error,
                                              test_ws_on_close, &ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_init_headers_null(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_ws_init(NULL, NULL));
  PASS();
}

TEST test_ws_pack_header_invalid(void) {
  unsigned char buf[10];

  /* Small buffer invalid payload length */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_small(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 126,
                                 NULL));

  /* Medium buffer invalid payload length */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_medium(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                  125, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_medium(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                  65536, NULL));

  /* Large buffer invalid payload length */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_large(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                 65535, NULL));

  /* Null buffer */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_small(NULL, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 10,
                                 NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_medium(NULL, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                  126, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_pack_header_large(NULL, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                 65536, NULL));

  PASS();
}

TEST test_ws_apply_mask_invalid(void) {
  unsigned char mask_key[4] = {0x12, 0x34, 0x56, 0x78};
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_apply_mask(NULL, 12, mask_key));
  PASS();
}

TEST test_ws_endian(void) {
  uint16_t s = math_ws_htons(0x1234);
  uint16_t r_s = math_ws_ntohs(s);
  uint64_t l = math_ws_htonll((((uint64_t)0x12345678) << 32) | 0x90ABCDEF);
  uint64_t r_l = math_ws_ntohll(l);

  ASSERT_EQ(0x1234, r_s);
  ASSERT(r_l == ((((uint64_t)0x12345678) << 32) | 0x90ABCDEF));
  PASS();
}

TEST test_ws_generate_mask(void) {
  unsigned char mask[4];
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_generate_mask_key(mask));
  PASS();
}

TEST test_ws_parser_ext_len(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&ctx, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &my_ctx));
  rc = ws_parser_feed(&ctx, frame16, sizeof(frame16));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&ctx));

  /* 64-bit length frame */
  memset(frame64, 0, sizeof(frame64));
  frame64[0] = 0x81;
  frame64[1] = 127;
  /* Length 65536 is 0x00000000 00010000 */
  frame64[7] = 0x01;
  frame64[8] = 0x00;
  frame64[9] = 0x00;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&ctx, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &my_ctx));
  rc = ws_parser_feed(&ctx, frame64, sizeof(frame64));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&ctx));

  PASS();
}

TEST test_ws_parser_errors(void) {
  struct ws_parser_ctx ctx;
  unsigned char frame[2];

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            ws_parser_init(NULL, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&ctx, NULL, NULL, NULL, NULL));

  frame[0] = 0x0F; /* Reserved opcode */
  frame[1] = 0x00;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, ws_parser_feed(&ctx, frame, 2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&ctx));

  /* NULL tests */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_parser_feed(NULL, frame, 2));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_parser_feed(&ctx, NULL, 2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(NULL));

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_oom_branches(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpRequest req = {0};
  struct c_abstract_http_ws_config config = {0};
  const char *headers[] = {"X-Test", "123", NULL};
  int i;

  config.subprotocols = "chat, superchat";
  config.custom_headers = headers;

  for (i = 0; i < 25; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = i;
    rc = c_abstract_http_ws_init(&req, &config);
    g_mock_alloc_fail = 0;
    if (rc == 0) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
      break;
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
    memset(&req, 0, sizeof(req));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_init_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
  PASS();
}
#endif

TEST test_ws_rsv_bit_set(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char chunk[] = {0x81,       0x85,       0x01,       0x02,
                           0x03,       0x04,       'h' ^ 0x01, 'e' ^ 0x02,
                           'l' ^ 0x03, 'l' ^ 0x04, 'o' ^ 0x01};
  memset(&parser, 0, sizeof(parser));
  parser.on_message = test_ws_on_message;
  parser.user_data = &ctx;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(1, ctx.message_count);
  ASSERT_STR_EQ("hello", ctx.last_payload);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_realloc_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");

  /* Repeat without on_error callback */
  memset(&parser, 0, sizeof(parser));
  parser.on_message = test_ws_on_message;
  parser.on_error = NULL;
  parser.user_data = &ctx;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
  PASS();
}
#endif

TEST test_ws_parser_close_frame(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  unsigned char chunk[] = {0x88, 0x02, 0x03, 0xE8};
  memset(&parser, 0, sizeof(parser));
  parser.on_close = test_ws_mock_on_close;
  test_ws_mock_on_close_called = 0;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(1000, test_ws_mock_on_close_called);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_pong_frame(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx = {0};
  unsigned char chunk[] = {0x8A, 0x00, 0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_message = test_ws_on_message;
  parser.user_data = &ctx;
  rc = ws_parser_feed(&parser, chunk, sizeof(chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_invalid_fragmentation(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  unsigned char chunk[] = {0x01, 0x01, 'a', 0x01, 0x00, 0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;
  test_ws_mock_on_error_called = 0;
  rc = ws_parser_feed(&parser, chunk, 3);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = ws_parser_feed(&parser, chunk + 3, 3);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, test_ws_mock_on_error_called);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

TEST test_ws_parser_reassembly_too_large(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  const unsigned char chunk2[] = {0x00};
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_mock_on_error;
  test_ws_mock_on_error_called = 0;
  rc = ws_parser_feed(&parser, chunk1, 3);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  if (parser.payload_buffer)
    free(parser.payload_buffer);
  parser.payload_buffer = (unsigned char *)malloc(1);
  ASSERT(parser.payload_buffer != NULL);
  parser.payload_capacity = 1;
  parser.state = WS_PARSER_READ_PAYLOAD;
  parser.current_frame.fin = 0;
  parser.current_frame.opcode = 0;
  parser.current_frame.payload_len = 1;
  parser.payload_offset = 0;
  parser.reassembly_offset = (size_t)C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE;

  rc = ws_parser_feed(&parser, chunk2, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_MSGSIZE, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_MSGSIZE, test_ws_mock_on_error_called);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);

  /* Repeat with parser.on_error == NULL */
  memset(&parser, 0, sizeof(parser));
  rc = ws_parser_feed(&parser, chunk1, 3);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  parser.payload_buffer = (unsigned char *)malloc(1);
  ASSERT(parser.payload_buffer != NULL);
  parser.payload_capacity = 1;
  parser.state = WS_PARSER_READ_PAYLOAD;
  parser.current_frame.fin = 0;
  parser.current_frame.opcode = 0;
  parser.current_frame.payload_len = 1;
  parser.payload_offset = 0;
  parser.reassembly_offset = (size_t)C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE;
  rc = ws_parser_feed(&parser, chunk2, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_MSGSIZE, rc);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  PASS();
}

TEST test_ws_parser_reassembly_expand_twice(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  struct ws_parser_ctx parser;
  const unsigned char chunk1[] = {0x01, 0x01, 'a'};
  unsigned char chunk2[4096 + 4];
  memset(&parser, 0, sizeof(parser));
  rc = ws_parser_feed(&parser, chunk1, 3);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  chunk2[0] = 0x00;
  chunk2[1] = 126;
  chunk2[2] = (4096 >> 8) & 0xFF;
  chunk2[3] = 4096 & 0xFF;
  memset(chunk2 + 4, 'b', 4096);
  rc = ws_parser_feed(&parser, chunk2, 4 + 4096);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  ASSERT_EQ(8192, parser.reassembly_capacity);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_reassembly_fin_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");

  /* Repeat without on_error callback */
  memset(&parser, 0, sizeof(parser));
  rc = ws_parser_feed(&parser, chunk1, 3);
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = ws_parser_feed(&parser, chunk2, 4 + 4096);
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_reassembly_fin_expand_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_sign_key_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  char out_accept[29];
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", out_accept);
  g_mock_alloc_fail = 0;
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
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
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  if (parser.payload_buffer)
    free(parser.payload_buffer);
  if (parser.reassembly_buffer)
    free(parser.reassembly_buffer);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_parser_reassembly_frag_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");

  /* Repeat without on_error callback */
  memset(&parser, 0, sizeof(parser));
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
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc, "%d");
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
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  memset(&req, 0, sizeof(req));

  /* Repeat without on_error callback */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = c_abstract_http_ws_sync_read_loop(
        &client, &req, test_ws_on_message, NULL, test_ws_on_close, &ctx, NULL);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_sync_loop_parser_oom(void) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
      memset(&req, 0, sizeof(req));

      g_mock_alloc_fail = 1;
      g_mock_alloc_count = i;
      rc =
          c_abstract_http_ws_sync_read_loop(&client, &req, test_ws_on_message,
                                            NULL, test_ws_on_close, &ctx, NULL);
      g_mock_alloc_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
      memset(&req, 0, sizeof(req));
      if (rc == C_ABSTRACT_HTTP_ERR_NOMEM) {
        /* we just want to cover the branches, so doing this in a loop is fine
         */
      }
    }
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}
#endif

NO_DISCARD static c_abstract_http_error_t
mock_send_bad_payload(struct HttpTransportContext *tctx,
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
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            c_abstract_http_ws_sync_read_loop(&client, &req, test_ws_on_message,
                                              test_ws_mock_on_error,
                                              test_ws_on_close, &ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, test_ws_mock_on_error_called);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  memset(&req, 0, sizeof(req));

  /* Feed framing error without on_err callback */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            c_abstract_http_ws_sync_read_loop(&client, &req, test_ws_on_message,
                                              NULL, test_ws_on_close, &ctx,
                                              NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_verify_accept_sign_error(void) {
  char long_key[100];
  memset(long_key, 'a', 99);
  long_key[99] = '\0';
  ASSERT_EQ(-1, ws_verify_accept(long_key, "anything"));
  PASS();
}

TEST test_ws_stubs(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_ws_async_register(
                                           NULL, NULL, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_send_async(NULL, 0, NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_send(NULL, 0, NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_ws_close(NULL, 0));
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
mock_client_send(struct HttpTransportContext *ctx,
                 const struct HttpRequest *req, struct HttpResponse **res) {
  (void)ctx;
  (void)req;
  (void)res;
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
}

NO_DISCARD static c_abstract_http_error_t mock_on_err(int err_code,
                                                      void *user_data) {
  int *called = (int *)user_data;
  *called = (err_code != 0) ? 1 : 1;
  return 0;
}

static abstract_http_thread_task_cb g_ws_queued_cb = NULL;
static void *g_ws_queued_arg = NULL;

NO_DISCARD static c_abstract_http_error_t
mock_push(void *ctx, abstract_http_thread_task_cb cb, void *arg) {
  (void)ctx;
  g_ws_queued_cb = cb;
  g_ws_queued_arg = arg;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_push_fail(void *ctx, abstract_http_thread_task_cb cb, void *arg) {
  (void)ctx;
  (void)cb;
  (void)arg;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_ws_async_register_success(void) {
  struct HttpClient client;
  struct HttpRequest req;
  struct AbstractHttpThreadPool *pool = NULL;
  struct AbstractHttpThreadPoolHooks hooks;
  int err_called = 0;

  memset(&client, 0, sizeof(client));
  memset(&req, 0, sizeof(req));
  memset(&hooks, 0, sizeof(hooks));

  hooks.push = mock_push;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_init_external(&pool, &hooks));
  client.thread_pool = pool;
  client.send = mock_client_send;

  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_ws_async_register(&client, NULL, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_ws_async_register(
                                           NULL, &req, NULL, NULL, NULL, NULL));

  /* Test push failure */
  hooks.push = mock_push_fail;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_init_external(&pool, &hooks));
  client.thread_pool = pool;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_register(
                                        &client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));

  /* Restore successful push */
  hooks.push = mock_push;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_init_external(&pool, &hooks));
  client.thread_pool = pool;

  /* Since c_abstract_http_ws_init needs URL, etc., it will fail inside
   * sync_read_loop. */
  /* We just test that it doesn't crash and gets scheduled. */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_async_register(&client, &req, NULL, mock_on_err,
                                              NULL, &err_called));
  ASSERT(g_ws_queued_cb != NULL);
  ASSERT_NEQ(C_ABSTRACT_HTTP_SUCCESS, g_ws_queued_cb(g_ws_queued_arg));
  g_ws_queued_cb = NULL;
  g_ws_queued_arg = NULL;
  ASSERT_EQ(1, err_called);

  /* Register with NULL on_err to hit free(ctx) at bottom of async task */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_ws_async_register(&client, &req, NULL, NULL, NULL, NULL));
  ASSERT(g_ws_queued_cb != NULL);
  ASSERT_NEQ(C_ABSTRACT_HTTP_SUCCESS, g_ws_queued_cb(g_ws_queued_arg));
  g_ws_queued_cb = NULL;
  g_ws_queued_arg = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    (const unsigned char *)"Hello", 5));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_send_async(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                          (const unsigned char *)"Hello", 5));

  /* read_chunk should have data */
  if (req.read_chunk) {
    char buf[1024];
    size_t read_bytes = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf),
                             &read_bytes));
    ASSERT(read_bytes > 0);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_close(&req, 1000));

  /* Simulate exit of sync_read_loop which frees the ws_ctx */
  {
    int exit_flag = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
              c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                                NULL, &exit_flag));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_async_coverage(void) {
  struct HttpClient client;
  struct HttpRequest req;
  c_abstract_http_error_t rc;
  int err_called = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));

  rc = c_abstract_http_ws_async_register(&client, &req, NULL, mock_on_err, NULL,
                                         &err_called);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP, rc);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  rc = abstract_http_thread_pool_init(&client.thread_pool, 1);
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    rc = c_abstract_http_ws_async_register(&client, &req, NULL, mock_on_err,
                                           NULL, &err_called);
    g_mock_alloc_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_free(client.thread_pool));
  }
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  PASS();
}

TEST test_ws_edge_cases(void) {
  struct HttpRequest req3;
  size_t out_read;
  char buf[128];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req3));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req3, NULL));

  if (req3.read_chunk) {
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              req3.read_chunk(NULL, buf, sizeof(buf), &out_read));
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req3));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req3));
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
mock_send_empty_body(struct HttpTransportContext *ctx,
                     const struct HttpRequest *req, struct HttpResponse **res) {
  char *b;
  (void)ctx;
  (void)req;
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  b = (char *)calloc(1, 1);
  (*res)->body = b;
  (*res)->body_len = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_send_null_res(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  (void)ctx;
  (void)req;
  *res = NULL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_send_null_body(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req, struct HttpResponse **res) {
  (void)ctx;
  (void)req;
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  (*res)->body = NULL;
  (*res)->body_len = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_send_null_ws_ctx(struct HttpTransportContext *ctx,
                      const struct HttpRequest *req,
                      struct HttpResponse **res) {
  (void)ctx;
  if (req->ws_ctx) {
    if (C_ABSTRACT_HTTP_SUCCESS !=
        c_abstract_http_ws_free((struct HttpRequest *)req))
      return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int test_ws_sync_close_called = 0;
NO_DISCARD static c_abstract_http_error_t mock_sync_on_close(int status,
                                                             void *user_data) {
  (void)user_data;
  test_ws_sync_close_called = status;
  return 0;
}

TEST test_ws_sync_loop_branches(void) {
  struct HttpClient client;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* NULL req with non-NULL client */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_sync_read_loop(&client, NULL, NULL, NULL, NULL,
                                              NULL, NULL));

  /* send returns success but res has body_len == 0 */
  client.send = mock_send_empty_body;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));

  /* send returns success but body is NULL */
  client.send = mock_send_null_body;
  test_ws_sync_close_called = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL,
                                              mock_sync_on_close, NULL, NULL));
  ASSERT_EQ(200, test_ws_sync_close_called);

  /* send returns success and clears req->ws_ctx */
  client.send = mock_send_null_ws_ctx;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));

  /* send returns success but *res == NULL */
  client.send = mock_send_null_res;
  ASSERT_EQ(0, c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL,
                                                 NULL, NULL, NULL));

  /* send fails with on_err == NULL */
  client.send = mock_client_send;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  PASS();
}

TEST test_ws_init_config_branches(void) {
  struct HttpRequest req;
  struct c_abstract_http_ws_config cfg;
  const char *headers_odd[2];
  const char *headers_even[3];

  headers_odd[0] = "Header1";
  headers_odd[1] = NULL;

  headers_even[0] = "Header1";
  headers_even[1] = "Val1";
  headers_even[2] = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  /* config with subprotocols == NULL, custom_headers == NULL */
  memset(&cfg, 0, sizeof(cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));

  /* config with custom_headers odd */
  memset(&cfg, 0, sizeof(cfg));
  cfg.custom_headers = headers_odd;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));

  /* config with custom_headers even */
  memset(&cfg, 0, sizeof(cfg));
  cfg.custom_headers = headers_even;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, &cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_read_chunk_more(void) {
  struct HttpRequest req;
  size_t out_read;
  char buf[4];
  struct ws_stream_ctx *sctx;

  out_read = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
  sctx = (struct ws_stream_ctx *)req.ws_ctx;

  /* Send some data so queue has content */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_send(
                &req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                (const unsigned char *)"01234567890123456789", 20));

  /* Read with smaller buffer: to_copy > buf_len */
  ASSERT_EQ(
      0, req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  ASSERT_EQ(sizeof(buf), out_read);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Mutex lock failure in ws_read_chunk_cb */
  g_mock_mutex_fail = 1;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO,
      req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  g_mock_mutex_fail = 0;

  /* Mutex unlock failure in ws_read_chunk_cb */
  g_mock_mutex_fail = 2;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO,
      req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  g_mock_mutex_fail = 0;
#endif

  /* Close the request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_close(&req, 1000));

  /* Drain the queue */
  while (sctx->queue_len > 0) {
    char big_buf[1024];
    ASSERT_EQ(0, req.read_chunk(req.read_chunk_user_data, big_buf,
                                sizeof(big_buf), &out_read));
  }

  /* Now queue_len == 0 && close_requested == 1: returns EOF (*out_read = 0) */
  out_read = 99;
  ASSERT_EQ(
      0, req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  ASSERT_EQ(0, out_read);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Cond wait failure in ws_read_chunk_cb when queue is empty and not closed */
  sctx->close_requested = 0;
  g_mock_cond_fail = 1;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO,
      req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  g_mock_cond_fail = 0;
  sctx->close_requested = 1;

  /* Cond wait failure with mutex unlock failure in ws_read_chunk_cb */
  sctx->close_requested = 0;
  g_mock_cond_fail = 1;
  g_mock_mutex_fail = 2;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO,
      req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  g_mock_cond_fail = 0;
  g_mock_mutex_fail = 0;
  sctx->close_requested = 1;
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_send_branches(void) {
  struct HttpRequest req;
  struct ws_stream_ctx *sctx;
  char *medium_buf;
  char *large_buf;
  unsigned char small_buf[5];

  memcpy(small_buf, "test", 5);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
  sctx = (struct ws_stream_ctx *)req.ws_ctx;

  /* Send with len == 0 */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, NULL, 0));

  /* Send medium payload (len <= 65535, > 125) */
  medium_buf = (char *)malloc(200);
  ASSERT(medium_buf != NULL);
  memset(medium_buf, 'A', 200);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    (const unsigned char *)medium_buf, 200));
  free(medium_buf);

  /* Send large payload (len > 65535) */
  large_buf = (char *)malloc(70000);
  ASSERT(large_buf != NULL);
  memset(large_buf, 'B', 70000);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_BINARY,
                                    (const unsigned char *)large_buf, 70000));
  free(large_buf);

  /* Queue expansion when queue_cap == 0 */
  sctx->queue_cap = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Malloc failure for masked_payload */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_alloc_fail = 0;

  /* Realloc failure when expanding queue */
  sctx->queue_cap = 10;
  sctx->queue_len = 10;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_alloc_fail = 0;

  /* Realloc failure when expanding queue with mutex unlock failure */
  sctx->queue_cap = 10;
  sctx->queue_len = 10;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  g_mock_mutex_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_alloc_fail = 0;
  g_mock_mutex_fail = 0;

  /* ws_generate_mask_key failure */
  g_mock_mask_key_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_mask_key_fail = 0;

  /* ws_apply_mask failure */
  g_mock_mask_key_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_mask_key_fail = 0;

  /* ws_pack_header_small failure */
  g_mock_pack_header_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_pack_header_fail = 0;

  /* ws_pack_header_medium failure */
  g_mock_pack_header_fail = 2;
  medium_buf = (char *)malloc(200);
  ASSERT(medium_buf != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    (const unsigned char *)medium_buf, 200));
  g_mock_pack_header_fail = 0;
  free(medium_buf);

  /* ws_pack_header_large failure */
  g_mock_pack_header_fail = 3;
  large_buf = (char *)malloc(70000);
  ASSERT(large_buf != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_BINARY,
                                    (const unsigned char *)large_buf, 70000));
  g_mock_pack_header_fail = 0;
  free(large_buf);

  /* Mutex lock failure */
  g_mock_mutex_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_mutex_fail = 0;

  /* Cond signal failure */
  g_mock_cond_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_cond_fail = 0;

  /* Cond signal failure with mutex unlock failure */
  g_mock_cond_fail = 2;
  g_mock_mutex_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_cond_fail = 0;
  g_mock_mutex_fail = 0;

  /* Mutex unlock failure */
  g_mock_mutex_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_mutex_fail = 0;
#endif

  /* Close request */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_close(&req, 1000));

  /* Send after close */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Send after close with mutex unlock failure */
  g_mock_mutex_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                    small_buf, 4));
  g_mock_mutex_fail = 0;
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_close_branches(void) {
  struct HttpRequest req;
  struct ws_stream_ctx *sctx;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
  sctx = (struct ws_stream_ctx *)req.ws_ctx;
  (void)sctx;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Mutex lock failure in close */
  g_mock_mutex_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_close(&req, 1000));
  g_mock_mutex_fail = 0;
  sctx->close_requested = 0;

  /* Cond signal failure in close */
  g_mock_cond_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_close(&req, 1000));
  g_mock_cond_fail = 0;
  sctx->close_requested = 0;

  /* Cond signal failure with mutex unlock failure in close */
  g_mock_cond_fail = 4;
  g_mock_mutex_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_close(&req, 1000));
  g_mock_cond_fail = 0;
  g_mock_mutex_fail = 0;
  sctx->close_requested = 0;

  /* Mutex unlock failure in close */
  g_mock_mutex_fail = 4;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_close(&req, 1000));
  g_mock_mutex_fail = 0;
  sctx->close_requested = 0;
#endif

  /* Successful close */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_close(&req, 1000));

  /* Second close fails send */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_ws_close(&req, 1000));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}

TEST test_ws_header_and_mask_coverage(void) {
  unsigned char buf[16];
  unsigned char key[4];

  key[0] = 1;
  key[1] = 2;
  key[2] = 3;
  key[3] = 4;

  /* Pack with out_len == NULL */
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      ws_pack_header_small(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1, 5, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_pack_header_medium(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_TEXT, 1,
                                  200, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_pack_header_large(buf, 1, C_ABSTRACT_HTTP_WS_OPCODE_BINARY, 1,
                                 70000, NULL));

  /* Mask key error checks */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_generate_mask_key(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_apply_mask(buf, 5, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_apply_mask(NULL, 0, key));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_apply_mask(buf, 0, key));

  /* send and close with NULL ws_ctx */
  {
    struct HttpRequest null_ctx_req;
    memset(&null_ctx_req, 0, sizeof(null_ctx_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_ws_send(&null_ctx_req,
                                      C_ABSTRACT_HTTP_WS_OPCODE_TEXT, buf, 1));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_ws_close(&null_ctx_req, 1000));
  }

  PASS();
}

TEST test_ws_parser_missing_callbacks(void) {
  struct ws_parser_ctx parser;
  unsigned char rsv_chunk[2];
  unsigned char frag_ctrl[2];
  unsigned char ping_chunk[2];
  unsigned char pong_chunk[2];
  unsigned char close_short[2];
  unsigned char text_chunk[4];
  unsigned char close_payload[4];
  unsigned char frag1[3];
  unsigned char frag2[3];

  rsv_chunk[0] = 0xB1;
  rsv_chunk[1] = 0x00;
  frag_ctrl[0] = 0x08;
  frag_ctrl[1] = 0x00;
  ping_chunk[0] = 0x89;
  ping_chunk[1] = 0x00;
  pong_chunk[0] = 0x8A;
  pong_chunk[1] = 0x00;
  close_short[0] = 0x88;
  close_short[1] = 0x00;
  text_chunk[0] = 0x81;
  text_chunk[1] = 0x02;
  text_chunk[2] = 'H';
  text_chunk[3] = 'i';

  close_payload[0] = 0x88;
  close_payload[1] = 0x02;
  close_payload[2] = 0x03;
  close_payload[3] = 0xE8;

  frag1[0] = 0x01;
  frag1[1] = 0x01;
  frag1[2] = 'A';
  frag2[0] = 0x01;
  frag2[1] = 0x01;
  frag2[2] = 'B';

  /* Init parser with ALL callbacks NULL */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));

  /* Feed errors with NULL on_error */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, rsv_chunk, sizeof(rsv_chunk)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, frag_ctrl, sizeof(frag_ctrl)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Close with payload >= 2 and NULL on_close */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, close_payload, sizeof(close_payload)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Fragmented message continuation error with NULL on_error */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag1, sizeof(frag1)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, frag2, sizeof(frag2)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* PING with NULL on_message */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, ping_chunk, sizeof(ping_chunk)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* PONG with NULL on_message */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, pong_chunk, sizeof(pong_chunk)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Close with payload < 2 and NULL on_close */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, close_short, sizeof(close_short)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Single frame with NULL on_message */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, text_chunk, sizeof(text_chunk)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Feed edge cases */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_parser_feed(NULL, text_chunk, 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_parser_feed(&parser, NULL, 5));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_feed(&parser, text_chunk, 0));

  PASS();
}

TEST test_ws_free_partial(void) {
  struct HttpRequest req;
  struct ws_stream_ctx *sctx;
  memset(&req, 0, sizeof(req));

  sctx = (struct ws_stream_ctx *)calloc(1, sizeof(struct ws_stream_ctx));
  ASSERT(sctx != NULL);

  /* sctx with NULL mutex, cond, queue */
  req.ws_ctx = sctx;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(NULL, req.ws_ctx);

  /* req with NULL ws_ctx */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));

  /* NULL req */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(NULL));
  PASS();
}

#if !defined(C_ABSTRACT_HTTP_SINGLE_THREADED) && !defined(__EMSCRIPTEN__) &&   \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
NO_DISCARD static c_abstract_http_error_t ws_cond_wake_cb(void *arg) {
  struct HttpRequest *req = (struct HttpRequest *)arg;
  if (!req) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(_WIN32)
  Sleep(30);
#else
  usleep(30000);
#endif
  return c_abstract_http_ws_send(req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                 (const unsigned char *)"hi", 2);
}

TEST test_ws_read_chunk_cond_wait_success(void) {
  struct HttpRequest req;
  struct AbstractHttpThreadPool *pool = NULL;
  char buf[32];
  size_t out_read = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, ws_cond_wake_cb(NULL));

  memset(&req, 0, sizeof(req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_cond_wake_cb, &req));

  ASSERT_EQ(
      0, req.read_chunk(req.read_chunk_user_data, buf, sizeof(buf), &out_read));
  ASSERT(out_read > 0);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
abstract_http_test_ws_async_task(void *arg);
TEST test_ws_async_task_null(void) {
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_test_ws_async_task(NULL));
  PASS();
}

TEST test_ws_async_task_branches(void) {
  struct c_abstract_http_ws_async_ctx *ctx1;
  struct c_abstract_http_ws_async_ctx *ctx2;
  struct HttpClient client;
  int rc_test;
  struct HttpRequest req;

  memset(&client, 0, sizeof(client));
  memset(&req, 0, sizeof(req));
  client.send = mock_send_success_ws;

  /* Case 1: err == SUCCESS */
  ctx1 = (struct c_abstract_http_ws_async_ctx *)malloc(sizeof(*ctx1));
  ASSERT(ctx1 != NULL);
  ctx1->client = &client;
  ctx1->req = &req;
  ctx1->on_msg = NULL;
  ctx1->on_err = NULL;
  ctx1->on_close = NULL;
  ctx1->user_data = NULL;
  rc_test = abstract_http_test_ws_async_task(ctx1);
  ASSERT_EQ_FMT(C_ABSTRACT_HTTP_SUCCESS, rc_test, "%d");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* Case 2: err != SUCCESS and on_err == NULL */
  memset(&req, 0, sizeof(req));
  client.send = mock_send_fail_ws;
  ctx2 = (struct c_abstract_http_ws_async_ctx *)malloc(sizeof(*ctx2));
  ASSERT(ctx2 != NULL);
  ctx2->client = &client;
  ctx2->req = &req;
  ctx2->on_msg = NULL;
  ctx2->on_err = NULL;
  ctx2->on_close = NULL;
  ctx2->user_data = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, abstract_http_test_ws_async_task(ctx2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  PASS();
}

TEST test_ws_sha1_failures(void) {
  char accept[29];
  g_mock_sha1_fail = -99;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", accept));
  g_mock_sha1_fail = -1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", accept));
  g_mock_sha1_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", accept));
  g_mock_sha1_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", accept));
  g_mock_sha1_fail = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_sign_key("dGhlIHNhbXBsZSBub25jZQ==", accept));
  g_mock_sha1_fail = 0;
  PASS();
}
#endif

TEST test_ws_parser_branch_coverage(void) {
  struct ws_parser_ctx parser;
  c_abstract_http_error_t rc;
  int exit_flag;
  struct HttpClient client;
  struct HttpRequest req;
  const unsigned char rsv_chunk[2] = {0xF1, 0x00};
  const unsigned char oversized_chunk[10] = {0x81, 0x7F, 0x00, 0x00, 0x00,
                                             0x00, 0x02, 0x00, 0x00, 0x00};
  const unsigned char close_chunk[4] = {0x88, 0x02, 0x03, 0xE8};
  const unsigned char ping_chunk[6] = {0x89, 0x04, 'p', 'i', 'n', 'g'};
  const unsigned char pong_chunk[6] = {0x8A, 0x04, 'p', 'o', 'n', 'g'};
  const unsigned char frag1[6] = {0x01, 0x04, 't', 'e', 's', 't'};
  const unsigned char non_cont[6] = {0x02, 0x04, 'b', 'a', 'd', 'f'};

  exit_flag = 1;
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Feed empty chunk */
  rc = ws_parser_feed(&parser, NULL, 0);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Feed RSV bit without on_error */
  rc = ws_parser_feed(&parser, rsv_chunk, sizeof(rsv_chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Feed oversized chunk without on_error */
  rc = ws_parser_feed(&parser, oversized_chunk, sizeof(oversized_chunk));
  ASSERT_EQ(90, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Feed close chunk without on_close */
  rc = ws_parser_feed(&parser, close_chunk, sizeof(close_chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Feed close chunk with 0 payload bytes followed by another frame */
  {
    const unsigned char close_two_frames[6] = {0x88, 0x00, 0x88,
                                               0x02, 0x03, 0xE8};
    rc = ws_parser_init(&parser, NULL, NULL, test_ws_mock_on_close, NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    rc = ws_parser_feed(&parser, close_two_frames, sizeof(close_two_frames));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  }

  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Feed ping and pong without on_message */
  rc = ws_parser_feed(&parser, ping_chunk, sizeof(ping_chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  rc = ws_parser_feed(&parser, pong_chunk, sizeof(pong_chunk));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* Framing error: fragment 1 then non-continuation opcode */
  rc = ws_parser_feed(&parser, frag1, sizeof(frag1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = ws_parser_feed(&parser, non_cont, sizeof(non_cont));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Parser with invalid state */
  rc = ws_parser_init(&parser, NULL, NULL, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  parser.state = (enum ws_parser_state)999;
  rc = ws_parser_feed(&parser, ping_chunk, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  rc = ws_parser_init(&parser, NULL, test_ws_mock_on_error, NULL, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  test_ws_mock_on_error_called = 0;
  parser.state = (enum ws_parser_state)999;
  rc = ws_parser_feed(&parser, ping_chunk, 1);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, test_ws_mock_on_error_called);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* c_abstract_http_ws_sync_read_loop with exit_flag = 1 */
  memset(&client, 0, sizeof(client));
  memset(&req, 0, sizeof(req));
  rc = c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL, NULL,
                                         &exit_flag);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_free_ooms(void) {
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));

  g_mock_cond_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_free(&req));
  g_mock_cond_free_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));

  g_mock_mutex_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_free(&req));
  g_mock_mutex_free_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));

  g_mock_cond_init_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_init(&req, NULL));
  g_mock_cond_init_fail = 0;

  g_mock_cond_init_fail = 1;
  g_mock_mutex_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_init(&req, NULL));
  g_mock_mutex_free_fail = 0;
  g_mock_cond_init_fail = 0;

  g_mock_queue_malloc_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, c_abstract_http_ws_init(&req, NULL));
  g_mock_queue_malloc_fail = 0;

  g_mock_queue_malloc_fail = 1;
  g_mock_cond_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_init(&req, NULL));
  g_mock_cond_free_fail = 0;
  g_mock_queue_malloc_fail = 0;

  g_mock_queue_malloc_fail = 1;
  g_mock_cond_free_fail = 1;
  g_mock_mutex_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_init(&req, NULL));
  g_mock_mutex_free_fail = 0;
  g_mock_cond_free_fail = 0;
  g_mock_queue_malloc_fail = 0;

  g_mock_queue_malloc_fail = 1;
  g_mock_mutex_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_init(&req, NULL));
  g_mock_mutex_free_fail = 0;
  g_mock_queue_malloc_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  PASS();
}
#endif

NO_DISCARD static c_abstract_http_error_t
mock_send_invalid_body(struct HttpTransportContext *ctx,
                       const struct HttpRequest *req,
                       struct HttpResponse **res) {
  unsigned char *b;
  if (ctx) {
  }
  if (req) {
  }
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  b = (unsigned char *)malloc(5);
  if (!b) {
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  b[0] = 0xF1U;
  b[1] = 0x00U;
  (*res)->body = b;
  (*res)->body_len = 2;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_ws_sync_loop_cleanup_ooms(void) {
  struct HttpClient client;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));

  client.send = mock_send_success_ws;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  g_mock_response_free_fail = 0;

  g_mock_cond_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  g_mock_cond_free_fail = 0;

  client.send = mock_send_invalid_body;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  g_mock_response_free_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  PASS();
}

TEST test_ws_parser_destroy_ooms(void) {
  struct HttpClient client;
  struct HttpRequest req;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));

  client.send = mock_send_success_ws;
  g_mock_parser_destroy_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  g_mock_parser_destroy_fail = 0;

  client.send = mock_send_fail_ws;
  g_mock_parser_destroy_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  g_mock_parser_destroy_fail = 0;

  client.send = mock_send_invalid_body;
  g_mock_parser_destroy_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(&client, &req, NULL, NULL, NULL,
                                              NULL, NULL));
  g_mock_parser_destroy_fail = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  PASS();
}
#endif
NO_DISCARD static c_abstract_http_error_t
test_ws_on_msg_abort(const struct c_abstract_http_ws_event *ev,
                     void *user_data) {
  (void)ev;
  (void)user_data;
  return C_ABSTRACT_HTTP_ERR_CANCELED;
}

NO_DISCARD static c_abstract_http_error_t
test_ws_on_err_abort(int error_code, void *user_data) {
  (void)error_code;
  (void)user_data;
  return C_ABSTRACT_HTTP_ERR_CANCELED;
}

NO_DISCARD static c_abstract_http_error_t
test_ws_on_close_abort(int status_code, void *user_data) {
  (void)status_code;
  (void)user_data;
  return C_ABSTRACT_HTTP_ERR_CANCELED;
}

NO_DISCARD static c_abstract_http_error_t
test_ws_mock_send_valid_body(struct HttpTransportContext *ctx,
                             const struct HttpRequest *req,
                             struct HttpResponse **res) {
  unsigned char *b;
  c_abstract_http_error_t rc;
  (void)ctx;
  (void)req;
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    return rc;
  }
  b = (unsigned char *)malloc(3);
  if (!b) {
    c_abstract_http_error_t fr = http_response_free(*res);
    free(*res);
    *res = NULL;
    if (fr != C_ABSTRACT_HTTP_SUCCESS)
      return fr;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  b[0] = 0x81U;
  b[1] = 0x01U;
  b[2] = 'A';
  (*res)->body = b;
  (*res)->body_len = 3;
  return C_ABSTRACT_HTTP_SUCCESS;
}

TEST test_ws_callback_error_percolation(void) {
  struct ws_parser_ctx parser;
  struct HttpClient client;
  struct HttpRequest req;
  unsigned char rsv_frame[2] = {0x91, 0x00};
  unsigned char frag_ctrl[2] = {0x09, 0x00};
  unsigned char huge_frame[10] = {0x82, 127,  0x00, 0x00, 0x00,
                                  0x01, 0x00, 0x00, 0x00, 0x00};
  unsigned char close_frame[4] = {0x88, 0x02, 0x03, 0xE8};
  unsigned char ping_frame[3] = {0x89, 0x01, 'P'};
  unsigned char pong_frame[3] = {0x8A, 0x01, 'P'};
  unsigned char text_frame[3] = {0x81, 0x01, 'A'};
  unsigned char frag1[3] = {0x01, 0x01, 'A'};
  unsigned char frag_bad[3] = {0x01, 0x01, 'B'};
  unsigned char frag_fin[3] = {0x80, 0x01, 'B'};

  /* 1. RSV bit error with on_error abort */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, rsv_frame, sizeof(rsv_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* 2. Fragmented control frame with on_error abort */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, frag_ctrl, sizeof(frag_ctrl)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* 3. Extended 64-bit frame exceeding MAX_FRAME_SIZE with on_error abort */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, huge_frame, sizeof(huge_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* 4. Close, Ping, Pong, Single Text, Fragmented Text, Invalid Frag, Default
   * state */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, NULL, test_ws_on_close_abort, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, close_frame, sizeof(close_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_msg_abort, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, ping_frame, sizeof(ping_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_msg_abort, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, pong_frame, sizeof(pong_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_msg_abort, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, text_frame, sizeof(text_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_msg_abort, test_ws_on_err_abort,
                           NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag1, sizeof(frag1)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, frag_fin, sizeof(frag_fin)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag1, sizeof(frag1)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, frag_bad, sizeof(frag_bad)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  parser.state = (enum ws_parser_state)99;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, text_frame, sizeof(text_frame)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* 5. Fragmented reassembly > MAX_FRAME_SIZE with on_error abort */
  {
    size_t half_sz = 60000;
    unsigned char *big_frag = (unsigned char *)malloc(4 + half_sz);
    size_t k;
    ASSERT(big_frag != NULL);
    big_frag[0] = 0x02;
    big_frag[1] = 126;
    big_frag[2] = (unsigned char)((half_sz >> 8) & 0xFF);
    big_frag[3] = (unsigned char)(half_sz & 0xFF);
    memset(big_frag + 4, 'z', half_sz);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
    for (k = 0; k < 200; ++k) {
      c_abstract_http_error_t fr =
          ws_parser_feed(&parser, big_frag, 4 + half_sz);
      big_frag[0] = 0x00;
      if (fr != C_ABSTRACT_HTTP_SUCCESS) {
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, fr);
        break;
      }
    }
    free(big_frag);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* 6. Payload realloc OOM, frag realloc OOM, and final frag realloc OOM with
   * on_error abort */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  {
    unsigned char med_hdr[6] = {0x81, 126, 0x20, 0x00, 'A', 'B'};
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
              ws_parser_feed(&parser, med_hdr, sizeof(med_hdr)));
    g_mock_alloc_fail = 0;
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, frag1, sizeof(frag1)));
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, NULL, test_ws_on_err_abort, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag1, sizeof(frag1)));
  parser.reassembly_capacity = 1;
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            ws_parser_feed(&parser, frag_fin, sizeof(frag_fin)));
  g_mock_alloc_fail = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
#endif

  /* 7. sync_read_loop and async_task callback error percolation */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));

  client.send = mock_send_fail_ws;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            c_abstract_http_ws_sync_read_loop(
                &client, &req, NULL, test_ws_on_err_abort, NULL, NULL, NULL));

  client.send = mock_send_invalid_body;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            c_abstract_http_ws_sync_read_loop(
                &client, &req, NULL, test_ws_on_err_abort, NULL, NULL, NULL));

  client.send = test_ws_mock_send_valid_body;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            c_abstract_http_ws_sync_read_loop(
                &client, &req, NULL, NULL, test_ws_on_close_abort, NULL, NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  client.send = mock_send_invalid_body;
  g_mock_parser_destroy_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(
                &client, &req, NULL, test_ws_on_err_abort, NULL, NULL, NULL));
  g_mock_parser_destroy_fail = 0;

  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_ws_sync_read_loop(
                &client, &req, NULL, test_ws_on_err_abort, NULL, NULL, NULL));
  g_mock_response_free_fail = 0;

  {
    int ac;
    for (ac = 0; ac < 25; ++ac) {
      struct HttpRequest req_fresh;
      c_abstract_http_error_t lrc;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req_fresh));
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = ac;
      lrc = c_abstract_http_ws_sync_read_loop(
          &client, &req_fresh, NULL, test_ws_on_err_abort, NULL, NULL, NULL);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (lrc != C_ABSTRACT_HTTP_SUCCESS) {
        ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, lrc);
      }
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req_fresh));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req_fresh));
    }
  }

  /* async_task on_err returning non-zero */
  client.send = mock_send_fail_ws;
  {
    struct AbstractHttpThreadPool *mock_pool = NULL;
    struct AbstractHttpThreadPoolHooks hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.push = mock_push;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_init_external(&mock_pool, &hooks));
    client.thread_pool = mock_pool;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_register(
                  &client, &req, NULL, test_ws_on_err_abort, NULL, NULL));
    ASSERT(g_ws_queued_cb != NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED, g_ws_queued_cb(g_ws_queued_arg));
    g_ws_queued_cb = NULL;
    g_ws_queued_arg = NULL;
    client.thread_pool = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_free(mock_pool));
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  PASS();
}

#if !defined(C_ABSTRACT_HTTP_SINGLE_THREADED) && !defined(__EMSCRIPTEN__) &&   \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
struct ws_test_server_cfg {
  int srv_fd;
  int mode;
  char last_rx[256];
  size_t last_rx_len;
};

NO_DISCARD static c_abstract_http_error_t ws_test_srv_task(void *arg) {
  struct ws_test_server_cfg *cfg = (struct ws_test_server_cfg *)arg;
#if defined(_WIN32)
  int cli_fd = (int)accept((SOCKET)cfg->srv_fd, NULL, NULL);
#else
  int cli_fd = (int)accept(cfg->srv_fd, NULL, NULL);
#endif
  char req_buf[2048];
  int n;
  if (cli_fd < 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#if defined(SO_NOSIGPIPE)
  {
    int nosig = 1;
    setsockopt(cli_fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&nosig,
               sizeof(nosig));
  }
#endif
#if defined(_WIN32)
  n = (int)recv((SOCKET)cli_fd, req_buf, (int)sizeof(req_buf) - 1, 0);
#else
  n = (int)recv(cli_fd, req_buf, sizeof(req_buf) - 1, 0);
#endif
  if (n <= 0) {
#if defined(_WIN32)
    closesocket((SOCKET)cli_fd);
#else
    close(cli_fd);
#endif
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  req_buf[n] = '\0';

  if (cfg->mode == 1) {
    const char *bad = "HTTP/1.1 400 Bad Request\r\n\r\n";
#if defined(_WIN32)
    send((SOCKET)cli_fd, bad, (int)strlen(bad), 0);
#elif defined(MSG_NOSIGNAL)
    send(cli_fd, bad, strlen(bad), MSG_NOSIGNAL);
#else
    send(cli_fd, bad, strlen(bad), 0);
#endif
  } else if (cfg->mode == 9) {
    /* Immediate close before sending any response bytes */
  } else if (cfg->mode == 2) {
    const char *no_acc =
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n";
#if defined(_WIN32)
    send((SOCKET)cli_fd, no_acc, (int)strlen(no_acc), 0);
#elif defined(MSG_NOSIGNAL)
    send(cli_fd, no_acc, strlen(no_acc), MSG_NOSIGNAL);
#else
    send(cli_fd, no_acc, strlen(no_acc), 0);
#endif
  } else if (cfg->mode == 3) {
    const char *bad_acc = "HTTP/1.1 101 Switching Protocols\r\n"
                          "Sec-WebSocket-Accept: invalidaccept==\r\n\r\n";
#if defined(_WIN32)
    send((SOCKET)cli_fd, bad_acc, (int)strlen(bad_acc), 0);
#elif defined(MSG_NOSIGNAL)
    send(cli_fd, bad_acc, strlen(bad_acc), MSG_NOSIGNAL);
#else
    send(cli_fd, bad_acc, strlen(bad_acc), 0);
#endif
  } else {
    char key_val[64];
    char accept_val[29];
    char resp[512];
    const char *kp = strstr(req_buf, "Sec-WebSocket-Key:");
    int roff;
    memset(key_val, 0, sizeof(key_val));
    if (kp) {
      const char *ks = kp + 18;
      size_t ki = 0;
      while (*ks == ' ')
        ks++;
      while (*ks && *ks != '\r' && *ks != '\n' && ki + 1 < sizeof(key_val)) {
        key_val[ki++] = *ks++;
      }
      key_val[ki] = '\0';
    }
    if (ws_sign_key(key_val, accept_val) != C_ABSTRACT_HTTP_SUCCESS) {
#if defined(_WIN32)
      closesocket((SOCKET)cli_fd);
#else
      close(cli_fd);
#endif
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    roff = sprintf_s(resp, sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept_val);
#else
    roff = c89stringutils_snprintf(resp, sizeof(resp),
                                   "HTTP/1.1 101 Switching Protocols\r\n"
                                   "Upgrade: websocket\r\n"
                                   "Connection: Upgrade\r\n"
                                   "Sec-WebSocket-Accept: %s\r\n\r\n",
                                   accept_val);
#endif
    if (cfg->mode == 4) {
      ((unsigned char *)resp)[roff++] = 0x81U;
      ((unsigned char *)resp)[roff++] = 0x02U;
      resp[roff++] = 'h';
      resp[roff++] = 'i';
    } else if (cfg->mode == 5) {
      ((unsigned char *)resp)[roff++] = 0x88U;
      ((unsigned char *)resp)[roff++] = 0x02U;
      ((unsigned char *)resp)[roff++] = 0x03U;
      ((unsigned char *)resp)[roff++] = 0xE8U;
    } else if (cfg->mode == 6) {
      ((unsigned char *)resp)[roff++] = 0x08U;
      ((unsigned char *)resp)[roff++] = 0x00U;
    }
#if defined(_WIN32)
    send((SOCKET)cli_fd, resp, roff, 0);
#elif defined(MSG_NOSIGNAL)
    send(cli_fd, resp, (size_t)roff, MSG_NOSIGNAL);
#else
    send(cli_fd, resp, (size_t)roff, 0);
#endif

    if (cfg->mode == 4) {
      unsigned char ping_f[3] = {0x89, 0x01, 'p'};
      unsigned char close_f[4] = {0x88, 0x02, 0x03, 0xE8};
      unsigned char rx_f[128];
      int rn;
#if defined(_WIN32)
      send((SOCKET)cli_fd, (const char *)ping_f, 3, 0);
      rn = (int)recv((SOCKET)cli_fd, (char *)rx_f, (int)sizeof(rx_f), 0);
#elif defined(MSG_NOSIGNAL)
      send(cli_fd, (const char *)ping_f, 3, MSG_NOSIGNAL);
      rn = (int)recv(cli_fd, (char *)rx_f, sizeof(rx_f), 0);
#else
      send(cli_fd, (const char *)ping_f, 3, 0);
      rn = (int)recv(cli_fd, (char *)rx_f, sizeof(rx_f), 0);
#endif
      if (rn > 0) {
        cfg->last_rx_len = (size_t)rn;
        memcpy(cfg->last_rx, rx_f, (size_t)rn);
      }
#if defined(_WIN32)
      send((SOCKET)cli_fd, (const char *)close_f, 4, 0);
      shutdown((SOCKET)cli_fd, SD_SEND);
      while (recv((SOCKET)cli_fd, (char *)rx_f, (int)sizeof(rx_f), 0) > 0) {
      }
#else
#if defined(MSG_NOSIGNAL)
      send(cli_fd, (const char *)close_f, 4, MSG_NOSIGNAL);
#else
      send(cli_fd, (const char *)close_f, 4, 0);
#endif
      shutdown(cli_fd, SHUT_WR);
      while (recv(cli_fd, (char *)rx_f, sizeof(rx_f), 0) > 0) {
      }
#endif
    } else if (cfg->mode == 8) {
      unsigned char rx_f[64];
      unsigned char bad_f[2] = {0x08, 0x00};
#if defined(_WIN32)
      recv((SOCKET)cli_fd, (char *)rx_f, (int)sizeof(rx_f), 0);
      send((SOCKET)cli_fd, (const char *)bad_f, 2, 0);
#else
      recv(cli_fd, (char *)rx_f, sizeof(rx_f), 0);
#if defined(MSG_NOSIGNAL)
      send(cli_fd, (const char *)bad_f, 2, MSG_NOSIGNAL);
#else
      send(cli_fd, (const char *)bad_f, 2, 0);
#endif
#endif
    } else if (cfg->mode == 10) {
      unsigned char rx_f[64];
#if defined(_WIN32)
      while (recv((SOCKET)cli_fd, (char *)rx_f, (int)sizeof(rx_f), 0) > 0) {
      }
#else
      while (recv(cli_fd, (char *)rx_f, sizeof(rx_f), 0) > 0) {
      }
#endif
    }
  }

#if defined(_WIN32)
  closesocket((SOCKET)cli_fd);
#else
  close(cli_fd);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

struct ws_upgraded_cb_trace {
  int msg_count;
  int ping_count;
  int err_code;
  int close_code;
  char text[32];
};

NO_DISCARD static c_abstract_http_error_t
ws_trace_on_msg(const struct c_abstract_http_ws_event *ev, void *user_data) {
  struct ws_upgraded_cb_trace *tr = (struct ws_upgraded_cb_trace *)user_data;
  if (ev->opcode == C_ABSTRACT_HTTP_WS_OPCODE_PING) {
    tr->ping_count++;
  } else if (ev->opcode == C_ABSTRACT_HTTP_WS_OPCODE_TEXT) {
    size_t cp = ev->payload_len < sizeof(tr->text) - 1 ? ev->payload_len
                                                       : sizeof(tr->text) - 1;
    memcpy(tr->text, ev->payload, cp);
    tr->text[cp] = '\0';
    tr->msg_count++;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t ws_trace_on_err(int error_code,
                                                          void *user_data) {
  struct ws_upgraded_cb_trace *tr = (struct ws_upgraded_cb_trace *)user_data;
  tr->err_code = error_code;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t ws_trace_on_close(int status_code,
                                                            void *user_data) {
  struct ws_upgraded_cb_trace *tr = (struct ws_upgraded_cb_trace *)user_data;
  tr->close_code = status_code;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
ws_bind_ephemeral_listener(int *out_sock, int *out_port) {
  struct sockaddr_in addr;
  int s;
  int opt = 1;
#if defined(_WIN32)
  int alen = (int)sizeof(addr);
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#else
  socklen_t alen = (socklen_t)sizeof(addr);
#endif
  s = (int)socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
#if defined(_WIN32)
  setsockopt((SOCKET)s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
             sizeof(opt));
  bind((SOCKET)s, (struct sockaddr *)&addr, sizeof(addr));
  listen((SOCKET)s, 64);
  getsockname((SOCKET)s, (struct sockaddr *)&addr, &alen);
#else
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
  bind(s, (struct sockaddr *)&addr, sizeof(addr));
  listen(s, 64);
  getsockname(s, (struct sockaddr *)&addr, &alen);
#endif
  *out_sock = s;
  *out_port = (int)math_ws_ntohs(addr.sin_port);
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD static c_abstract_http_error_t
ws_ext_add_fail(void *ctx, int fd, int ev, http_loop_cb cb, void *ud) {
  (void)ctx;
  (void)fd;
  (void)ev;
  (void)cb;
  (void)ud;
  return C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD static c_abstract_http_error_t
ws_ext_add_ok(void *ctx, int fd, int ev, http_loop_cb cb, void *ud) {
  http_loop_cb *slot = (http_loop_cb *)ctx;
  (void)fd;
  (void)ev;
  (void)ud;
  if (slot)
    *slot = cb;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t ws_ext_mod_ok(void *ctx, int fd,
                                                        int ev) {
  (void)ctx;
  (void)fd;
  (void)ev;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t ws_ext_rm_fail(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD static c_abstract_http_error_t ws_ext_wakeup_ok(void *ctx) {
  (void)ctx;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

TEST test_ws_upgraded_socket_event_loop(void) {
  struct HttpClient client;
  struct HttpRequest req;
  struct AbstractHttpThreadPool *pool = NULL;
  struct ws_test_server_cfg srv;
  struct ws_upgraded_cb_trace tr;
  char url[128];
  int port = 0;
  int m;

  memset(&client, 0, sizeof(client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&client.loop));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 1));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_ws_async_connect(
                                           NULL, NULL, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.url = "wss://127.0.0.1:1/ws";
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
    (defined(__APPLE__) || defined(_WIN32))
  {
    c_abstract_http_error_t wrc =
        c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL);
    ASSERT(wrc != C_ABSTRACT_HTTP_ERR_NOTSUP);
    if (wrc == C_ABSTRACT_HTTP_SUCCESS) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_close(&req, 1000));
    }
  }
#else
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOTSUP,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
#endif
  req.url = "ftp://127.0.0.1/ws";
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  req.url = "ws://:0/ws";
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  req.url = "ws://:80/ws";
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  req.url = "ws:///ws";
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  req.url = "http://127.0.0.1:1";
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_CONNREFUSED,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Parser OOM (0), host OOM (0 after parser init), path OOM (1), DNS, socket
   */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
  req.url = "ws://invalid.host.test";
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  g_mock_alloc_fail = 0;
  g_mock_raw_gethostbyname_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_connect(
                                        &client, &req, NULL, NULL, NULL, NULL));
  g_mock_raw_gethostbyname_fail = 0;
  req.url = "ws://127.0.0.1:80/ws";
  g_mock_socket_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_connect(
                                        &client, &req, NULL, NULL, NULL, NULL));
  g_mock_socket_fail = 0;
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  req.url = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_bind_ephemeral_listener(&srv.srv_fd, &port));
  for (m = 1; m <= 8; ++m) {
    char *dup_url = NULL;
    memset(&tr, 0, sizeof(tr));
    srv.mode = m;
    srv.last_rx_len = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url, sizeof(url),
              (m % 2 == 0) ? "ws://127.0.0.1:%d/chat"
                           : "http://localhost:%d/chat",
              port);
#else
    c89stringutils_snprintf(url, sizeof(url),
                            (m % 2 == 0) ? "ws://127.0.0.1:%d/chat"
                                         : "http://localhost:%d/chat",
                            port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;

    if (m <= 3) {
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE,
                c_abstract_http_ws_async_register(
                    &client, &req, ws_trace_on_msg, ws_trace_on_err,
                    ws_trace_on_close, &tr));
    } else if (m == 5) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_async_connect(&client, &req, NULL, NULL,
                                                 NULL, &tr));
    } else if (m == 6) {
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
                c_abstract_http_ws_async_connect(&client, &req, NULL, NULL,
                                                 NULL, &tr));
    } else if (m == 4) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_async_register(
                    &client, &req, ws_trace_on_msg, ws_trace_on_err,
                    ws_trace_on_close, &tr));
      ASSERT_EQ(1, tr.msg_count);
      ASSERT_STR_EQ("hi", tr.text);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      {
        /* EWOULDBLOCK queues frame + schedules HTTP_LOOP_WRITE + wakeup */
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_ws_send_mode(1));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                          (const unsigned char *)"ok", 2));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_tick(client.loop));
        ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                  c_abstract_http_test_set_ws_send_mode(0));
      }
#else
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                        (const unsigned char *)"ok", 2));
#endif
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(client.loop));
      ASSERT_EQ(1, tr.ping_count);
      ASSERT_EQ(1000, tr.close_code);
    } else if (m == 7) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_async_connect(&client, &req, NULL, NULL,
                                                 ws_trace_on_close, &tr));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(client.loop));
      ASSERT_EQ(1006, tr.close_code);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_async_connect(&client, &req, NULL, NULL,
                                                 NULL, NULL));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(client.loop));
    } else if (m == 8) {
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_async_connect(&client, &req, ws_trace_on_msg,
                                                 ws_trace_on_err,
                                                 ws_trace_on_close, &tr));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                        (const unsigned char *)"go", 2));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, http_loop_run(client.loop));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING, tr.err_code);
    }

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Exercise handshake send fail, missing key, external loop add_fd/remove_fd
   * failures, send I/O error, and parser_destroy error */
  {
    struct HttpLoopHooks lhooks;
    struct HttpClient ext_cli;
    http_loop_cb captured_cb = NULL;
    char *dup_url = NULL;

    /* Handshake send failure */
    srv.mode = 7;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url, sizeof(url), "ws://127.0.0.1:%d/chat", port);
#else
    c89stringutils_snprintf(url, sizeof(url), "ws://127.0.0.1:%d/chat", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_ws_send_mode(2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_ws_send_mode(0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_ws_send_mode(1));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_ws_send_mode(0));
    /* Missing Sec-WebSocket-Key header when ws_ctx already allocated */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_free(&req.headers));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_headers_init(&req.headers));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* External loop add_fd failure + pre-queued frame before connect */
    memset(&lhooks, 0, sizeof(lhooks));
    memset(&ext_cli, 0, sizeof(ext_cli));
    lhooks.add_fd = ws_ext_add_fail;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_init_external(&ext_cli.loop, &lhooks));
    srv.mode = 7;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_init(&req, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"q", 1));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    g_mock_parser_destroy_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_free(&req));
    g_mock_parser_destroy_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_cli.loop));

    /* ws_init OOM inside async_connect */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    req.url = "ws://127.0.0.1:80/ws";
    g_mock_queue_malloc_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL,
                                               NULL));
    g_mock_queue_malloc_fail = 0;
    req.url = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* Mode 9: empty handshake EOF */
    srv.mode = 9;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(url, sizeof(url), "ws://127.0.0.1:%d/chat", port);
#else
    c89stringutils_snprintf(url, sizeof(url), "ws://127.0.0.1:%d/chat", port);
#endif
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE,
              c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* External loop remove_fd failure + send error + WRITE close_requested */
    memset(&lhooks, 0, sizeof(lhooks));
    lhooks.external_context = &captured_cb;
    lhooks.add_fd = ws_ext_add_ok;
    lhooks.mod_fd = ws_ext_mod_ok;
    lhooks.remove_fd = ws_ext_rm_fail;
    lhooks.wakeup = ws_ext_wakeup_ok;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_init_external(&ext_cli.loop, &lhooks));
    srv.mode = 10;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT(captured_cb != NULL);

    /* Test flush when loop_registered == 0 */
    ((struct ws_stream_ctx *)req.ws_ctx)->loop_registered = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"z", 1));
    ((struct ws_stream_ctx *)req.ws_ctx)->loop_registered = 1;

    /* Test PING with NULL user_on_msg + PING auto-pong send failure */
    {
      unsigned char ping_f[3] = {0x89, 0x01, 'p'};
      ASSERT_EQ(
          C_ABSTRACT_HTTP_SUCCESS,
          ws_parser_feed(&((struct ws_stream_ctx *)req.ws_ctx)->async_parser,
                         ping_f, 3));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_ws_send_mode(2));
      ASSERT_EQ(
          C_ABSTRACT_HTTP_ERR_IO,
          ws_parser_feed(&((struct ws_stream_ctx *)req.ws_ctx)->async_parser,
                         ping_f, 3));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_test_set_ws_send_mode(0));
      ((struct ws_stream_ctx *)req.ws_ctx)->queue_len = 0;
    }

    /* Test send mode 2 + mutex unlock failures on ws_send & loop WRITE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_ws_send_mode(2));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"x", 1));
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"x", 1));
    g_mock_mutex_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              captured_cb(ext_cli.loop,
                          ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                          HTTP_LOOP_WRITE, &req));
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              captured_cb(ext_cli.loop,
                          ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                          HTTP_LOOP_WRITE, &req));
    g_mock_mutex_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              captured_cb(ext_cli.loop,
                          ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                          HTTP_LOOP_WRITE, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_ws_send_mode(0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* Re-establish live upgraded socket after fatal HTTP_LOOP_WRITE teardown */
    srv.mode = 10;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));

    /* Test ws_send normal mutex unlock fail + loop WRITE mutex unlock fail */
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"x", 1));
    g_mock_mutex_fail = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              captured_cb(ext_cli.loop,
                          ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                          HTTP_LOOP_WRITE, &req));
    g_mock_mutex_fail = 0;

    /* Test close_requested flush via HTTP_LOOP_WRITE + remove_fd fail */
    ((struct ws_stream_ctx *)req.ws_ctx)->close_requested = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              captured_cb(ext_cli.loop,
                          ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                          HTTP_LOOP_WRITE, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* Also test ws_fail_upgraded_socket remove_fd fail + ws_free remove_fd fail
     * + ws_close immediate detach */
    srv.mode = 10;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_close(&req, 1000));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    srv.mode = 8;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              captured_cb(ext_cli.loop,
                          ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                          HTTP_LOOP_READ, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"go", 2));
    {
      fd_set rset;
      struct timeval tv;
      int ufd = ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd;
      FD_ZERO(&rset);
#if defined(_WIN32)
      FD_SET((SOCKET)ufd, &rset);
#else
      FD_SET((size_t)ufd, &rset);
#endif
      tv.tv_sec = 2;
      tv.tv_usec = 0;
      select(ufd + 1, &rset, NULL, NULL, &tv);
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                captured_cb(ext_cli.loop, ufd, HTTP_LOOP_READ, &req));
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    srv.mode = 7;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    {
      fd_set rset;
      struct timeval tv;
      int ufd = ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd;
      /* Wait for peer EOF, then exercise READ EOF -> remove_fd failure */
      FD_ZERO(&rset);
#if defined(_WIN32)
      FD_SET((SOCKET)ufd, &rset);
#else
      FD_SET((size_t)ufd, &rset);
#endif
      tv.tv_sec = 2;
      tv.tv_usec = 0;
      select(ufd + 1, &rset, NULL, NULL, &tv);
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                captured_cb(ext_cli.loop, ufd, HTTP_LOOP_READ, &req));
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    srv.mode = 7;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_cli.loop));
  }
#endif

#if defined(_WIN32)
  closesocket((SOCKET)srv.srv_fd);
#else
  close(srv.srv_fd);
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(client.loop));
  PASS();
}
#endif

NO_DISCARD static c_abstract_http_error_t
test_ws_on_error_abort(int error_code, void *user_data) {
  (void)error_code;
  (void)user_data;
  return C_ABSTRACT_HTTP_ERR_IO;
}

TEST test_ws_rfc6455_framing_edge_cases(void) {
  struct ws_parser_ctx parser;
  struct test_ws_ctx ctx;
  const unsigned char big_ping[4] = {0x89, 126, 0x00, 0x80};
  const unsigned char orphan_cont[3] = {0x80, 0x01, 'a'};
  const unsigned char frag_start[3] = {0x01, 0x01, 'H'};
  const unsigned char bad_fin_data[3] = {0x81, 0x01, 'i'};
  const unsigned char good_fin_cont[3] = {0x80, 0x01, 'i'};

  memset(&ctx, 0, sizeof(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, big_ping, sizeof(big_ping)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  memset(&ctx, 0, sizeof(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, orphan_cont, sizeof(orphan_cont)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  memset(&ctx, 0, sizeof(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag_start, sizeof(frag_start)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, bad_fin_data, sizeof(bad_fin_data)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  memset(&ctx, 0, sizeof(ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_init(&parser, test_ws_on_message, test_ws_on_error,
                           test_ws_on_close, &ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag_start, sizeof(frag_start)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, good_fin_cont, sizeof(good_fin_cont)));
  ASSERT_EQ(C_ABSTRACT_HTTP_WS_OPCODE_TEXT, ctx.last_message.opcode);
  ASSERT_EQ(2, ctx.last_message.payload_len);
  ASSERT_MEM_EQ("Hi", ctx.last_payload, 2);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  /* Error callback percolation and NULL on_error branches */
  memset(&parser, 0, sizeof(parser));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, big_ping, sizeof(big_ping)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  memset(&parser, 0, sizeof(parser));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, orphan_cont, sizeof(orphan_cont)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  memset(&parser, 0, sizeof(parser));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag_start, sizeof(frag_start)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
            ws_parser_feed(&parser, bad_fin_data, sizeof(bad_fin_data)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_on_error_abort;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_parser_feed(&parser, big_ping, sizeof(big_ping)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_on_error_abort;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_parser_feed(&parser, orphan_cont, sizeof(orphan_cont)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));
  memset(&parser, 0, sizeof(parser));
  parser.on_error = test_ws_on_error_abort;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_parser_feed(&parser, frag_start, sizeof(frag_start)));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            ws_parser_feed(&parser, bad_fin_data, sizeof(bad_fin_data)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, ws_parser_destroy(&parser));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    char key_buf[25];
    g_mock_sha1_fail = -502;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, ws_generate_key(key_buf));
    g_mock_sha1_fail = 0;
  }
#endif

  PASS();
}

#if !defined(C_ABSTRACT_HTTP_SINGLE_THREADED) && !defined(__EMSCRIPTEN__) &&   \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
struct ws_mock_tls_ctl {
  int fail_open;
  int hs_steps;
  int fail_hs;
  int write_mode; /* 0=sock, 1=err, 2=eof(0,0), 3=want_w(0,2), 4=want_r(0,1) */
  int read_mode;  /* 0=sock, 1=err, 2=eof(0,0), 3=want_w(0,2), 4=bad_frame,
                     5=close_frame */
  int fail_close;
};

struct ws_mock_tls_sess {
  int fd;
  struct ws_mock_tls_ctl *ctl;
};

NO_DISCARD static c_abstract_http_error_t
ws_mock_tls_open(void *provider_data, int sock_fd, const char *host,
                 const struct HttpConfig *config, void **out_session) {
  struct ws_mock_tls_ctl *ctl = (struct ws_mock_tls_ctl *)provider_data;
  struct ws_mock_tls_sess *s;
  (void)host;
  (void)config;
  if (ctl && ctl->fail_open) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  s = (struct ws_mock_tls_sess *)malloc(sizeof(*s));
  if (!s) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  s->fd = sock_fd;
  s->ctl = ctl;
  *out_session = s;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t ws_mock_tls_handshake(void *session,
                                                                int *out_want) {
  struct ws_mock_tls_sess *s = (struct ws_mock_tls_sess *)session;
  if (s->ctl && s->ctl->hs_steps > 0) {
    s->ctl->hs_steps--;
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->ctl && s->ctl->fail_hs) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  *out_want = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
ws_mock_tls_write(void *session, const void *buf, size_t len, size_t *out_n,
                  int *out_want) {
  struct ws_mock_tls_sess *s = (struct ws_mock_tls_sess *)session;
  int n;
  *out_n = 0;
  *out_want = 0;
  if (s->ctl) {
    if (s->ctl->write_mode == 1) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (s->ctl->write_mode == 2) {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (s->ctl->write_mode == 3) {
      *out_want = 2;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (s->ctl->write_mode == 4) {
      *out_want = 1;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
#if defined(_WIN32)
  n = send((SOCKET)s->fd, (const char *)buf, (int)len, 0);
#elif defined(MSG_NOSIGNAL)
  n = (int)send(s->fd, buf, len, MSG_NOSIGNAL);
#else
  n = (int)send(s->fd, buf, len, 0);
#endif
  if (n > 0) {
    *out_n = (size_t)n;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD static c_abstract_http_error_t
ws_mock_tls_read(void *session, void *buf, size_t cap, size_t *out_n,
                 int *out_want) {
  struct ws_mock_tls_sess *s = (struct ws_mock_tls_sess *)session;
  int n;
  *out_n = 0;
  *out_want = 0;
  if (s->ctl) {
    if (s->ctl->read_mode == 1) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (s->ctl->read_mode == 2) {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (s->ctl->read_mode == 3) {
      *out_want = 2;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (s->ctl->read_mode == 4) {
      unsigned char *b = (unsigned char *)buf;
      if (cap >= 2) {
        b[0] = 0x08U;
        b[1] = 0x00U;
        *out_n = 2;
      }
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (s->ctl->read_mode == 5) {
      unsigned char *b = (unsigned char *)buf;
      if (cap >= 4) {
        b[0] = 0x88U;
        b[1] = 0x02U;
        b[2] = 0x03U;
        b[3] = 0xE8U;
        *out_n = 4;
      }
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
#if defined(_WIN32)
  n = recv((SOCKET)s->fd, (char *)buf, (int)cap, 0);
#else
  n = (int)recv(s->fd, buf, cap, 0);
#endif
  if (n > 0) {
    *out_n = (size_t)n;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (n == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(_WIN32)
  if (WSAGetLastError() == WSAEWOULDBLOCK) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#else
  if (errno == EAGAIN
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
      || errno == EWOULDBLOCK
#endif
  ) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif
  return C_ABSTRACT_HTTP_ERR_IO;
}

NO_DISCARD static c_abstract_http_error_t ws_mock_tls_close(void *session) {
  struct ws_mock_tls_sess *s = (struct ws_mock_tls_sess *)session;
  int fail = (s && s->ctl) ? s->ctl->fail_close : 0;
  free(s);
  return fail ? C_ABSTRACT_HTTP_ERR_IO : C_ABSTRACT_HTTP_SUCCESS;
}

struct ws_sync_stop_ctx {
  volatile int *flag;
  int msg_cnt;
  int arm_oom_on_msg;
};

NO_DISCARD static c_abstract_http_error_t
ws_sync_stop_on_msg(const struct c_abstract_http_ws_event *ev,
                    void *user_data) {
  struct ws_sync_stop_ctx *sc = (struct ws_sync_stop_ctx *)user_data;
  if (ev->opcode == C_ABSTRACT_HTTP_WS_OPCODE_TEXT) {
    sc->msg_cnt++;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (sc->arm_oom_on_msg) {
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = 0;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
#endif
    if (sc->flag) {
      *(sc->flag) = 1;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

struct ws_delayed_exit_arg {
  volatile int *ef;
};

NO_DISCARD static c_abstract_http_error_t ws_delayed_exit_task(void *arg) {
  struct ws_delayed_exit_arg *da = (struct ws_delayed_exit_arg *)arg;
  abstract_http_int64_t t0 = 0;
  abstract_http_int64_t t1 = 0;
  c_abstract_http_error_t rc = real_c_abstract_http_get_time_ms(&t0);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  for (;;) {
#if !defined(_WIN32)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 2000;
    select(0, NULL, NULL, NULL, &tv);
#endif
    rc = real_c_abstract_http_get_time_ms(&t1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    if (t1 - t0 >= 28) {
      break;
    }
  }
  *(da->ef) = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD static c_abstract_http_error_t ws_ext_rm_ok(void *ctx, int fd) {
  (void)ctx;
  (void)fd;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

TEST test_ws_wss_tls_and_sync_upgraded_loop(void) {
  struct HttpClient client;
  struct HttpRequest req;
  struct HttpRawTlsHooks thooks;
  struct HttpRawTlsHooks bad_hooks;
  struct ws_mock_tls_ctl ctl;
  struct AbstractHttpThreadPool *pool = NULL;
  struct ws_test_server_cfg srv;
  struct ws_upgraded_cb_trace tr;
  char wss_url[128];
  char https_url[128];
  char wss_def_url[64];
  char https_def_url[64];
  int port = 0;
  int listen_fd;
  char *dup_url = NULL;

  /* 1. c_abstract_http_ws_set_tls_hooks validation, OOM, set, and clear */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_set_tls_hooks(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  memset(&bad_hooks, 0, sizeof(bad_hooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_set_tls_hooks(&req, &bad_hooks));
  bad_hooks.open = ws_mock_tls_open;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_set_tls_hooks(&req, &bad_hooks));
  bad_hooks.handshake = ws_mock_tls_handshake;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_set_tls_hooks(&req, &bad_hooks));
  bad_hooks.read = ws_mock_tls_read;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_set_tls_hooks(&req, &bad_hooks));
  bad_hooks.write = ws_mock_tls_write;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_ws_set_tls_hooks(&req, &bad_hooks));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_ws_set_tls_hooks(&req, NULL));
  g_mock_alloc_fail = 0;
#endif

  memset(&ctl, 0, sizeof(ctl));
  memset(&thooks, 0, sizeof(thooks));
  thooks.provider_data = &ctl;
  thooks.open = ws_mock_tls_open;
  thooks.handshake = ws_mock_tls_handshake;
  thooks.read = ws_mock_tls_read;
  thooks.write = ws_mock_tls_write;
  thooks.close = ws_mock_tls_close;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            ws_bind_ephemeral_listener(&listen_fd, &port));
  ASSERT(listen_fd >= 0);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(wss_url, sizeof(wss_url), "wss://127.0.0.1:%d/chat", port);
  sprintf_s(https_url, sizeof(https_url), "https://127.0.0.1:%d/chat", port);
  strcpy_s(wss_def_url, sizeof(wss_def_url), "wss://127.0.0.1/chat");
  strcpy_s(https_def_url, sizeof(https_def_url), "https://127.0.0.1/chat");
#else
  c89stringutils_snprintf(wss_url, sizeof(wss_url), "wss://127.0.0.1:%d/chat",
                          port);
  c89stringutils_snprintf(https_url, sizeof(https_url),
                          "https://127.0.0.1:%d/chat", port);
  strcpy(wss_def_url, "wss://127.0.0.1/chat");
  strcpy(https_def_url, "https://127.0.0.1/chat");
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_init(&pool, 1));
  memset(&client, 0, sizeof(client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_init(&client.loop));

  /* 2. wss:// / https:// default TLS resolution & error modes */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(wss_def_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_ws_send_mode(3));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_ws_send_mode(4));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOTSUP,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_test_set_ws_send_mode(0));
  /* Also exercise default TLS hooks lookup without custom hooks */
  (void)0;
  {
    c_abstract_http_error_t drc =
        c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL);
    ASSERT(drc == C_ABSTRACT_HTTP_ERR_NOTSUP ||
           drc == C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  }
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

  /* 3. wss:// handshake failure branches (open fail, hs fail + close fail,
   * write err, write 0/0, read err, read 0/0) */
  memset(&srv, 0, sizeof(srv));
  srv.srv_fd = listen_fd;

  /* 3a. open fail */
  srv.mode = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ctl.fail_open = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(wss_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 3b. multi-step hs + hs fail + close fail (covers ws_close_tls_and_fd trc !=
   * 0) */
  srv.mode = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ctl.hs_steps = 1;
  ctl.fail_hs = 1;
  ctl.fail_close = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(https_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_connect(
                                        &client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 3b2. hs fail with clean close (covers ws_close_tls_and_fd trc == 0) */
  srv.mode = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ctl.fail_hs = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup(https_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 3c. handshake write error & write (0,0) EOF */
  srv.mode = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ctl.write_mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(wss_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_connect(
                                        &client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  srv.mode = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ctl.write_mode = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(wss_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_connect(
                                        &client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 3d. handshake read error & read (0,0) EOF */
  srv.mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ctl.read_mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(wss_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_async_connect(
                                        &client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  srv.mode = 9;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  memset(&ctl, 0, sizeof(ctl));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(wss_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE,
      c_abstract_http_ws_async_connect(&client, &req, NULL, NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

  /* 4. Full wss:// async connect + PING/PONG + TEXT + CLOSE over TLS */
  srv.mode = 4;
  memset(&tr, 0, sizeof(tr));
  memset(&ctl, 0, sizeof(ctl));
  ctl.hs_steps = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_strdup(wss_url, &dup_url));
  req.url = dup_url;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_set_tls_hooks(&req, &thooks));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_ws_async_connect(&client, &req, ws_trace_on_msg,
                                             ws_trace_on_err, ws_trace_on_close,
                                             &tr));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_run(client.loop));
  ASSERT_EQ(1, tr.msg_count);
  ASSERT_STR_EQ("hi", tr.text);
  ASSERT_EQ(1000, tr.close_code);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* 5. TLS queue flush & ws_upgraded_loop_cb TLS read/write/close branches */
  {
    struct HttpLoopHooks lhooks;
    struct HttpClient ext_cli;
    http_loop_cb cap_cb = NULL;

    memset(&lhooks, 0, sizeof(lhooks));
    lhooks.external_context = &cap_cb;
    lhooks.add_fd = ws_ext_add_ok;
    lhooks.mod_fd = ws_ext_mod_ok;
    lhooks.remove_fd = ws_ext_rm_ok;
    lhooks.wakeup = ws_ext_wakeup_ok;

    /* Helper remove_fd that succeeds vs fails */
    memset(&ext_cli, 0, sizeof(ext_cli));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              http_loop_init_external(&ext_cli.loop, &lhooks));

    /* Connect live TLS upgraded socket */
    srv.mode = 10;
    memset(&ctl, 0, sizeof(ctl));
    memset(&tr, 0, sizeof(tr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, ws_trace_on_msg,
                                               ws_trace_on_err,
                                               ws_trace_on_close, &tr));
    ASSERT(cap_cb != NULL);

    /* 5a. TLS write want_write (twant=2) -> queues frame; then want_read
     * (twant=1) + HTTP_LOOP_READ flushes queue! */
    ctl.write_mode = 3;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"z", 1));
    ctl.write_mode = 4;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              cap_cb(ext_cli.loop,
                     ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                     HTTP_LOOP_WRITE, &req));
    /* Now sctx->tls_want == 1 and queue_len > 0; HTTP_LOOP_READ flushes +
     * read_mode=3 (twant=2) mods fd */
    ctl.write_mode = 0;
    ctl.read_mode = 3;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              cap_cb(ext_cli.loop,
                     ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                     HTTP_LOOP_READ, &req));

    /* 5b. TLS write (0,0) -> ERR_IO on send */
    ctl.write_mode = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"z", 1));

    /* 5c. TLS write error (trc != 0) on send */
    ctl.write_mode = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_send(&req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                      (const unsigned char *)"z", 1));
    ctl.write_mode = 0;

    /* 5d. TLS read EOF (nr=0, twant=0) -> ws_close_upgraded_socket +
     * user_on_close(1006) */
    ((struct ws_stream_ctx *)req.ws_ctx)->queue_len = 0;
    ctl.read_mode = 2;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              cap_cb(ext_cli.loop,
                     ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                     HTTP_LOOP_READ, &req));
    ASSERT_EQ(1006, tr.close_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 5e. TLS read error (rc != 0) -> ws_fail_upgraded_socket */
    srv.mode = 10;
    memset(&ctl, 0, sizeof(ctl));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ctl.read_mode = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              cap_cb(ext_cli.loop,
                     ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                     HTTP_LOOP_READ, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 5f. TLS read bad frame -> ws_parser_feed error -> ws_fail_upgraded_socket
     */
    srv.mode = 10;
    memset(&ctl, 0, sizeof(ctl));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ctl.read_mode = 4;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
              cap_cb(ext_cli.loop,
                     ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                     HTTP_LOOP_READ, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 5g. TLS read EOF when tls_hooks.close fails -> returns close error */
    srv.mode = 10;
    memset(&ctl, 0, sizeof(ctl));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_async_connect(&ext_cli, &req, NULL, NULL, NULL,
                                               NULL));
    ctl.read_mode = 2;
    ctl.fail_close = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              cap_cb(ext_cli.loop,
                     ((struct ws_stream_ctx *)req.ws_ctx)->upgraded_fd,
                     HTTP_LOOP_READ, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(ext_cli.loop));
  }
#endif

  /* 6. ws_sync_upgraded_socket_loop (client->send == NULL) */
  {
    struct HttpClient raw_cli;
    struct ws_sync_stop_ctx sstop;
    volatile int eflag = 0;

    memset(&raw_cli, 0, sizeof(raw_cli));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&raw_cli));
    raw_cli.send = NULL;

    /* 6a. NULL req->url -> ERR_INVAL */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                NULL, NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6b. Connect failure (with & without on_err override) */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("bad://url", &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_ws_sync_read_loop(
                  &raw_cli, &req, NULL, ws_trace_on_err, NULL, &tr, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL,
                                                test_ws_on_error_abort, NULL,
                                                NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    /* 6c. http_loop_init OOM (with & without on_err override) */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              c_abstract_http_ws_sync_read_loop(
                  &raw_cli, &req, NULL, ws_trace_on_err, NULL, &tr, NULL));
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL,
                                                test_ws_on_error_abort, NULL,
                                                NULL, NULL));
    g_mock_alloc_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

    /* 6d. Full wss:// sync upgraded socket loop (until peer CLOSE) */
    srv.mode = 4;
    memset(&ctl, 0, sizeof(ctl));
    memset(&tr, 0, sizeof(tr));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, ws_trace_on_msg,
                                                ws_trace_on_err,
                                                ws_trace_on_close, &tr, NULL));
    ASSERT_EQ(1, tr.msg_count);
    ASSERT_EQ(1000, tr.close_code);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6e. Sync upgraded socket loop with exit_flag timer + cooperative stop */
    srv.mode = 4;
    memset(&ctl, 0, sizeof(ctl));
    memset(&sstop, 0, sizeof(sstop));
    eflag = 0;
    sstop.flag = &eflag;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_sync_read_loop(
                                           &raw_cli, &req, ws_sync_stop_on_msg,
                                           NULL, NULL, &sstop, &eflag));
    ASSERT_EQ(1, sstop.msg_cnt);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6f. Sync upgraded socket loop framing error */
    srv.mode = 8;
    memset(&ctl, 0, sizeof(ctl));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_send_async(
                                           &req, C_ABSTRACT_HTTP_WS_OPCODE_TEXT,
                                           (const unsigned char *)"q", 1));
    eflag = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_WS_FRAMING,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                NULL, NULL, &eflag));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6g. Sync loop where exit_flag stops while TLS session active + fail_close
     */
    srv.mode = 4;
    memset(&ctl, 0, sizeof(ctl));
    memset(&sstop, 0, sizeof(sstop));
    ctl.fail_close = 1;
    eflag = 0;
    sstop.flag = &eflag;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_sync_read_loop(
                                          &raw_cli, &req, ws_sync_stop_on_msg,
                                          NULL, NULL, &sstop, &eflag));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6h. Idle upgraded socket woken by 15ms ws_sync_exit_poll_timer_cb */
    {
      struct AbstractHttpThreadPool *wpool = NULL;
      struct ws_delayed_exit_arg darg;
      darg.ef = &eflag;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_thread_pool_init(&wpool, 1));
      srv.mode = 10;
      memset(&ctl, 0, sizeof(ctl));
      eflag = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
      ASSERT_EQ(
          C_ABSTRACT_HTTP_SUCCESS,
          abstract_http_thread_pool_push(wpool, ws_delayed_exit_task, &darg));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_strdup(wss_url, &dup_url));
      req.url = dup_url;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_set_tls_hooks(&req, &thooks));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                  NULL, NULL, &eflag));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(wpool));
    }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    /* 6i. Connect fail + http_loop_free fail (op=1) */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup("bad://url", &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(1, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                NULL, NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(0, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6j. http_loop_add_timer fail (op=4) inside ws_sync_upgraded_socket_loop
     */
    srv.mode = 10;
    memset(&ctl, 0, sizeof(ctl));
    eflag = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(4, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                NULL, NULL, &eflag));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(0, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* 6k. finish_sync_ws: cl_rc != 0 + lp_rc != 0, cl_rc != 0 + rc != 0, and
     * cl_rc == 0 + lp_rc != 0 */
    srv.mode = 4;
    memset(&ctl, 0, sizeof(ctl));
    memset(&sstop, 0, sizeof(sstop));
    ctl.fail_close = 1;
    eflag = 0;
    sstop.flag = &eflag;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(1, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_ws_sync_read_loop(
                                          &raw_cli, &req, ws_sync_stop_on_msg,
                                          NULL, NULL, &sstop, &eflag));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(0, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* cl_rc != 0 + rc != 0 (add_timer op=4 fail + fail_close = 1) */
    srv.mode = 10;
    memset(&ctl, 0, sizeof(ctl));
    ctl.fail_close = 1;
    eflag = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(4, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                NULL, NULL, &eflag));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(0, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));

    /* cl_rc == 0 + lp_rc != 0 */
    srv.mode = 4;
    memset(&ctl, 0, sizeof(ctl));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_thread_pool_push(pool, ws_test_srv_task, &srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(wss_url, &dup_url));
    req.url = dup_url;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_ws_set_tls_hooks(&req, &thooks));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(1, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              c_abstract_http_ws_sync_read_loop(&raw_cli, &req, NULL, NULL,
                                                NULL, NULL, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_test_set_loop_op_fail(0, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
#endif

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_strdup(https_def_url, &dup_url));
    req.url = dup_url;
    {
      c_abstract_http_error_t hrc = c_abstract_http_ws_sync_read_loop(
          &raw_cli, &req, NULL, NULL, NULL, NULL, NULL);
      ASSERT(hrc != C_ABSTRACT_HTTP_SUCCESS);
    }
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_ws_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&raw_cli));
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_loop_free(client.loop));
  client.loop = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
#if defined(_WIN32)
  closesocket((SOCKET)listen_fd);
#else
  close(listen_fd);
#endif
  PASS();
}
#endif

SUITE(ws_suite) {
  RUN_TEST(test_ws_rfc6455_framing_edge_cases);
  RUN_TEST(test_ws_callback_error_percolation);
  RUN_TEST(test_ws_parser_branch_coverage);
  RUN_TEST(test_ws_async_coverage);
  /* RUN_TEST(test_ws_send_large); */
  RUN_TEST(test_ws_edge_cases);

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
  RUN_TEST(test_ws_stubs);
  RUN_TEST(test_ws_async_register_success);
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
  RUN_TEST(test_ws_read_chunk_more);
  RUN_TEST(test_ws_send_branches);
  RUN_TEST(test_ws_close_branches);
  RUN_TEST(test_ws_header_and_mask_coverage);
  RUN_TEST(test_ws_parser_missing_callbacks);
  RUN_TEST(test_ws_init_config_branches);
  RUN_TEST(test_ws_sync_loop_branches);
  RUN_TEST(test_ws_free_partial);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_sync_loop_cleanup_ooms);
  RUN_TEST(test_ws_parser_destroy_ooms);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_free_ooms);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_ws_async_task_null);
  RUN_TEST(test_ws_async_task_branches);
  RUN_TEST(test_ws_sha1_failures);
#endif
#if !defined(C_ABSTRACT_HTTP_SINGLE_THREADED) && !defined(__EMSCRIPTEN__) &&   \
    !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
  RUN_TEST(test_ws_read_chunk_cond_wait_success);
  RUN_TEST(test_ws_upgraded_socket_event_loop);
  RUN_TEST(test_ws_wss_tls_and_sync_upgraded_loop);
#endif
}
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
#if defined(__linux__) || defined(__APPLE__)
  signal(SIGPIPE, SIG_IGN);
#endif
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(ws_suite);
  GREATEST_MAIN_END();
}
