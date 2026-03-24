#include "../src/sse_internal.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static int dummy_on_event(const struct c_abstract_http_sse_event *ev,
                          void *user_data) {
  (void)ev;
  (void)user_data;
  return 0;
}

static int dummy_on_error(int error_code, void *user_data) {
  (void)error_code;
  (void)user_data;
  return 0;
}

static int dummy_on_close(void *user_data) {
  (void)user_data;
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  struct sse_parser_ctx ctx;

  if (sse_parser_init(&ctx, NULL, dummy_on_event, dummy_on_error,
                      dummy_on_close, NULL) != 0) {
    return 0;
  }

  sse_parser_feed(&ctx, (const char *)data, size);

  sse_parser_destroy(&ctx);
  return 0;
}
