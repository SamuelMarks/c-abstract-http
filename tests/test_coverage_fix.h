/**
 * @file test_coverage_fix.h
 * @brief Declarations for test coverage verification helper.
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TESTS_TEST_COVERAGE_FIX_H
#define C_ABSTRACT_HTTP_TESTS_TEST_COVERAGE_FIX_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Performs coverage verification operation.
 *
 * @param[out] out_val Pointer to receive verification output.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, C_ABSTRACT_HTTP_ERR_INVAL if
 * out_val is NULL.
 */
extern enum c_abstract_http_error test_coverage_fix_verify(int *out_val);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !C_ABSTRACT_HTTP_TESTS_TEST_COVERAGE_FIX_H */
