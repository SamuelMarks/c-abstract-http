/**
 * @file fuzz_sse.h
 * @brief Declarations for SSE fuzz harness test runner.
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TESTS_FUZZ_SSE_H
#define C_ABSTRACT_HTTP_TESTS_FUZZ_SSE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/http_types.h>
#include <stddef.h>
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int8 uint8_t;
#else
#include <stdint.h>
#endif
/* clang-format on */

/**
 * @brief Executes the SSE fuzz test harness with provided input.
 *
 * @param[in] data Pointer to input buffer.
 * @param[in] size Number of bytes in input buffer.
 * @param[out] out_rc Pointer to receive execution result code.
 * @return C_ABSTRACT_HTTP_SUCCESS on completion, C_ABSTRACT_HTTP_ERR_INVAL if
 * arguments invalid.
 */
extern enum c_abstract_http_error test_fuzz_sse_run(const uint8_t *data,
                                                    size_t size, int *out_rc);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !C_ABSTRACT_HTTP_TESTS_FUZZ_SSE_H */
