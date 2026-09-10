/**
 * @file fuzz_sse.c
 * @brief Fuzzing harness for SSE parser.
 * @author Samuel Marks
 */

/* clang-format off */
#include "fuzz_sse.h"
#if defined(C_ABSTRACT_HTTP_ENABLE_SSE)
#include "../src/sse_internal.h"
#endif
#include <stddef.h>
#include <stdlib.h>
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_ENABLE_SSE)
/**
 * @brief Dummy event callback.
 *
 * @param[in] ev Pointer to SSE event.
 * @param[in] user_data Opaque pointer to user data.
 * @return 0 on success.
 */
static int dummy_on_event(const struct c_abstract_http_sse_event *ev,
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
static int dummy_on_error(int error_code, void *user_data) {
  (void)error_code;
  (void)user_data;
  return 0;
}

/**
 * @brief Dummy close callback.
 *
 * @param[in] user_data Opaque pointer to user data.
 * @return 0 on success.
 */
static int dummy_on_close(void *user_data) {
  (void)user_data;
  return 0;
}

/**
 * @brief Executes SSE parser fuzz test with input buffer.
 *
 * @param[in] data Pointer to input bytes.
 * @param[in] size Length of input bytes.
 * @param[out] out_rc Optional pointer to receive parser exit code.
 * @return C_ABSTRACT_HTTP_SUCCESS on completion, C_ABSTRACT_HTTP_ERR_INVAL if
 * invalid.
 */
enum c_abstract_http_error test_fuzz_sse_run(const uint8_t *data, size_t size,
                                             int *out_rc) {
  struct sse_parser_ctx ctx;
  enum c_abstract_http_error rc;

  if (out_rc != NULL) {
    *out_rc = 0;
  }

  if (data == NULL && size > 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = sse_parser_init(&ctx, NULL, dummy_on_event, dummy_on_error,
                       dummy_on_close, NULL);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (out_rc != NULL) {
      *out_rc = (int)rc;
    }
    return rc;
  }

  if (data != NULL && size > 0) {
    (void)sse_parser_feed(&ctx, (const char *)data, size);
  } else {
    (void)ctx.on_close(ctx.user_data);
  }

  sse_parser_destroy(&ctx);
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
  (void)test_fuzz_sse_run(data, size, &rc);
  return 0;
}
#endif

#else /* !C_ABSTRACT_HTTP_ENABLE_SSE */

/**
 * @brief Fallback when SSE is disabled.
 *
 * @param[in] data Pointer to input bytes.
 * @param[in] size Length of input bytes.
 * @param[out] out_rc Optional pointer to receive parser exit code.
 * @return C_ABSTRACT_HTTP_SUCCESS on completion.
 */
enum c_abstract_http_error test_fuzz_sse_run(const uint8_t *data, size_t size,
                                             int *out_rc) {
  (void)data;
  (void)size;
  if (out_rc != NULL) {
    *out_rc = 0;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* C_ABSTRACT_HTTP_ENABLE_SSE */
