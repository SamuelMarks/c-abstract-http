/**
 * @file fuzz_ws.c
 * @brief Fuzzing harness for WebSocket parser.
 * @author Samuel Marks
 */

/* clang-format off */
#include "fuzz_ws.h"
#if defined(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
#include "../src/ws_internal.h"
#endif
#include <stddef.h>
#include <stdlib.h>
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
/**
 * @brief Dummy message callback.
 *
 * @param[in] ev Pointer to WebSocket event.
 * @param[in] user_data Opaque pointer to user data.
 * @return 0 on success.
 */
static int dummy_on_msg(const struct c_abstract_http_ws_event *ev,
                        void *user_data) {
  (void)ev;
  (void)user_data;
  return 0;
}

/**
 * @brief Dummy error callback.
 *
 * @param[in] error_code The error code.
 * @param[in] user_data Opaque pointer to user data.
 * @return 0 on success.
 */
static int dummy_on_err(int error_code, void *user_data) {
  (void)error_code;
  (void)user_data;
  return 0;
}

/**
 * @brief Dummy close callback.
 *
 * @param[in] status_code The closure status code.
 * @param[in] user_data Opaque pointer to user data.
 * @return 0 on success.
 */
static int dummy_on_close(int status_code, void *user_data) {
  (void)status_code;
  (void)user_data;
  return 0;
}

/**
 * @brief Executes WebSocket parser fuzz test with input buffer.
 *
 * @param[in] data Pointer to input bytes.
 * @param[in] size Length of input bytes.
 * @param[out] out_rc Optional pointer to receive parser exit code.
 * @return C_ABSTRACT_HTTP_SUCCESS on completion, C_ABSTRACT_HTTP_ERR_INVAL if
 * invalid.
 */
enum c_abstract_http_error test_fuzz_ws_run(const uint8_t *data, size_t size,
                                            int *out_rc) {
  struct ws_parser_ctx ctx;
  enum c_abstract_http_error rc;

  if (out_rc != NULL) {
    *out_rc = 0;
  }

  if (data == NULL && size > 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = ws_parser_init(&ctx, dummy_on_msg, dummy_on_err, dummy_on_close, NULL);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (out_rc != NULL) {
      *out_rc = (int)rc;
    }
    return rc;
  }

  if (data != NULL && size > 0) {
    (void)ws_parser_feed(&ctx, data, size);
  }

  ws_parser_destroy(&ctx);
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_ENABLE_FUZZING)
/**
 * @brief LLVM Fuzzer entrypoint.
 *
 * @param[in] data Pointer to fuzzer input.
 * @param[in] size Size of fuzzer input.
 * @return 0 on completion.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  int rc;
  (void)test_fuzz_ws_run(data, size, &rc);
  return 0;
}
#endif

#else /* !C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS */

/**
 * @brief Fallback when WebSockets is disabled.
 *
 * @param[in] data Pointer to input bytes.
 * @param[in] size Length of input bytes.
 * @param[out] out_rc Optional pointer to receive parser exit code.
 * @return C_ABSTRACT_HTTP_SUCCESS on completion.
 */
enum c_abstract_http_error test_fuzz_ws_run(const uint8_t *data, size_t size,
                                            int *out_rc) {
  (void)data;
  (void)size;
  if (out_rc != NULL) {
    *out_rc = 0;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS */
