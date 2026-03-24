#include "../src/ws_internal.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static int dummy_on_msg(const struct c_abstract_http_ws_event *ev,
                        void *user_data) {
  (void)ev;
  (void)user_data;
  return 0;
}

static int dummy_on_err(int error_code, void *user_data) {
  (void)error_code;
  (void)user_data;
  return 0;
}

static int dummy_on_close(int status_code, void *user_data) {
  (void)status_code;
  (void)user_data;
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  struct ws_parser_ctx ctx;

  if (ws_parser_init(&ctx, dummy_on_msg, dummy_on_err, dummy_on_close, NULL) !=
      0) {
    return 0;
  }

  ws_parser_feed(&ctx, data, size);

  ws_parser_destroy(&ctx);
  return 0;
}
