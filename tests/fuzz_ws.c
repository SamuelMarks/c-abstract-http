/* clang-format off */
#include "../src/ws_internal.h"
#include <stddef.h>
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
#endif
#include <stdlib.h>
/* clang-format on */

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
