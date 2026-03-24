/* clang-format off */
#include "greatest.h"

#include "../include/c_abstract_http/http_sse.h"
#include "../src/sse_internal.h"
#include "greatest.h"
#include <string.h>
/* clang-format on */

struct test_sse_ctx {
  int error_code;
  int close_called;
  int event_count;
  char last_id[256];
  char last_event[256];
  char last_data[4096];
};

static int test_sse_on_event(const struct c_abstract_http_sse_event *ev,
                             void *user_data) {
  struct test_sse_ctx *ctx = (struct test_sse_ctx *)user_data;
  ctx->event_count++;
  if (ev->id) {
    strncpy(ctx->last_id, ev->id, sizeof(ctx->last_id) - 1);
  } else {
    ctx->last_id[0] = '\0';
  }
  if (ev->event) {
    strncpy(ctx->last_event, ev->event, sizeof(ctx->last_event) - 1);
  } else {
    ctx->last_event[0] = '\0';
  }
  if (ev->data) {
    memcpy(ctx->last_data, ev->data, ev->data_len);
    ctx->last_data[ev->data_len] = '\0';
  } else {
    ctx->last_data[0] = '\0';
  }
  return 0;
}

static int test_sse_on_error(int error_code, void *user_data) {
  struct test_sse_ctx *ctx = (struct test_sse_ctx *)user_data;
  ctx->error_code = error_code;
  return 0;
}

static int test_sse_on_close(void *user_data) {
  struct test_sse_ctx *ctx = (struct test_sse_ctx *)user_data;
  ctx->close_called = 1;
  return 0;
}

TEST test_sse_parse_basic_data(void) {
  struct sse_parser_ctx parser;
  struct test_sse_ctx ctx = {0};
  const char *chunk = "data: hello\n\n";

  ASSERT_EQ(0, sse_parser_init(&parser, NULL, test_sse_on_event,
                               test_sse_on_error, test_sse_on_close, &ctx));
  ASSERT_EQ(0, sse_parser_feed(&parser, chunk, strlen(chunk)));

  ASSERT_EQ(1, ctx.event_count);
  ASSERT_STR_EQ("message", ctx.last_event);
  ASSERT_STR_EQ("hello", ctx.last_data);

  sse_parser_destroy(&parser);
  PASS();
}

TEST test_sse_parse_multi_line_data(void) {
  struct sse_parser_ctx parser;
  struct test_sse_ctx ctx = {0};
  const char *chunk = "data: first\ndata: second\n\n";

  ASSERT_EQ(0, sse_parser_init(&parser, NULL, test_sse_on_event,
                               test_sse_on_error, test_sse_on_close, &ctx));
  ASSERT_EQ(0, sse_parser_feed(&parser, chunk, strlen(chunk)));

  ASSERT_EQ(1, ctx.event_count);
  ASSERT_STR_EQ("first\nsecond", ctx.last_data);

  sse_parser_destroy(&parser);
  PASS();
}

TEST test_sse_parse_event_type(void) {
  struct sse_parser_ctx parser;
  struct test_sse_ctx ctx = {0};
  const char *chunk = "event: custom_event\ndata: payload\n\n";

  ASSERT_EQ(0, sse_parser_init(&parser, NULL, test_sse_on_event,
                               test_sse_on_error, test_sse_on_close, &ctx));
  ASSERT_EQ(0, sse_parser_feed(&parser, chunk, strlen(chunk)));

  ASSERT_EQ(1, ctx.event_count);
  ASSERT_STR_EQ("custom_event", ctx.last_event);
  ASSERT_STR_EQ("payload", ctx.last_data);

  sse_parser_destroy(&parser);
  PASS();
}

TEST test_sse_parse_ignore_comments(void) {
  struct sse_parser_ctx parser;
  struct test_sse_ctx ctx = {0};
  const char *chunk = ": this is a comment\ndata: ping\n\n";

  ASSERT_EQ(0, sse_parser_init(&parser, NULL, test_sse_on_event,
                               test_sse_on_error, test_sse_on_close, &ctx));
  ASSERT_EQ(0, sse_parser_feed(&parser, chunk, strlen(chunk)));

  ASSERT_EQ(1, ctx.event_count);
  ASSERT_STR_EQ("ping", ctx.last_data);

  sse_parser_destroy(&parser);
  PASS();
}

TEST test_sse_parse_id_and_retry(void) {
  struct sse_parser_ctx parser;
  struct test_sse_ctx ctx = {0};
  const char *chunk = "id: 99\nretry: 5000\ndata: test\n\n";

  ASSERT_EQ(0, sse_parser_init(&parser, NULL, test_sse_on_event,
                               test_sse_on_error, test_sse_on_close, &ctx));
  ASSERT_EQ(0, sse_parser_feed(&parser, chunk, strlen(chunk)));

  ASSERT_EQ(1, ctx.event_count);
  ASSERT_STR_EQ("99", ctx.last_id);
  ASSERT_EQ(5000, parser.retry_ms);
  ASSERT_STR_EQ("test", ctx.last_data);

  sse_parser_destroy(&parser);
  PASS();
}

TEST test_sse_chunked_delivery(void) {
  struct sse_parser_ctx parser;
  struct test_sse_ctx ctx = {0};
  const char *p1 = "data: he";
  const char *p2 = "llo\r";
  const char *p3 = "\n\r";
  const char *p4 = "\n";

  ASSERT_EQ(0, sse_parser_init(&parser, NULL, test_sse_on_event,
                               test_sse_on_error, test_sse_on_close, &ctx));

  ASSERT_EQ(0, sse_parser_feed(&parser, p1, strlen(p1)));
  ASSERT_EQ(0, ctx.event_count);

  ASSERT_EQ(0, sse_parser_feed(&parser, p2, strlen(p2)));
  ASSERT_EQ(0, ctx.event_count);

  ASSERT_EQ(0, sse_parser_feed(&parser, p3, strlen(p3)));
  ASSERT_EQ(0, ctx.event_count);

  ASSERT_EQ(0, sse_parser_feed(&parser, p4, strlen(p4)));
  ASSERT_EQ(1, ctx.event_count);
  ASSERT_STR_EQ("hello", ctx.last_data);

  sse_parser_destroy(&parser);
  PASS();
}

TEST test_sse_init_headers(void) {
  struct HttpRequest req;
  struct c_abstract_http_sse_config config = {0};
  const char *val = NULL;

  ASSERT_EQ(0, http_request_init(&req));

  config.last_event_id = "12345";
  config.retry_timeout_ms = 2000;

  ASSERT_EQ(0, c_abstract_http_sse_init(&req, &config));

  ASSERT_EQ(0, http_headers_get(&req.headers, "Accept", &val));
  ASSERT_STR_EQ("text/event-stream", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Connection", &val));
  ASSERT_STR_EQ("keep-alive", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Cache-Control", &val));
  ASSERT_STR_EQ("no-cache", val);

  ASSERT_EQ(0, http_headers_get(&req.headers, "Last-Event-ID", &val));
  ASSERT_STR_EQ("12345", val);

  http_request_free(&req);
  PASS();
}

TEST test_sse_sync_loop_exit_flag(void) {
  volatile int exit_flag = 1;
  ASSERT_EQ(0, c_abstract_http_sse_sync_read_loop(NULL, NULL, NULL, NULL, NULL,
                                                  NULL, &exit_flag));
  PASS();
}

TEST test_sse_async_register_stub(void) {
  ASSERT_EQ(-1, c_abstract_http_sse_async_register(NULL, NULL, NULL, NULL, NULL,
                                                   NULL));
  PASS();
}

TEST test_sse_init_headers_null(void) {
  ASSERT_EQ(-1, c_abstract_http_sse_init(NULL, NULL));
  PASS();
}

TEST test_sse_parser_init_null(void) {
  ASSERT_EQ(-1, sse_parser_init(NULL, NULL, NULL, NULL, NULL, NULL));
  PASS();
}

SUITE(sse_suite) {
  RUN_TEST(test_sse_parse_basic_data);
  RUN_TEST(test_sse_parse_multi_line_data);
  RUN_TEST(test_sse_parse_event_type);
  RUN_TEST(test_sse_parse_ignore_comments);
  RUN_TEST(test_sse_parse_id_and_retry);
  RUN_TEST(test_sse_chunked_delivery);
  RUN_TEST(test_sse_init_headers);
  RUN_TEST(test_sse_init_headers_null);
  RUN_TEST(test_sse_parser_init_null);
  RUN_TEST(test_sse_sync_loop_exit_flag);
  RUN_TEST(test_sse_async_register_stub);
}
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(sse_suite);
  GREATEST_MAIN_END();
  return 0;
}
