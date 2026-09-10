/**
 * @file test_coverage_fix.c
 * @brief Coverage fix verification routines.
 * @author Samuel Marks
 */

/* clang-format off */
#include "test_coverage_fix.h"
#include <stdio.h>
#include <stdlib.h>
/* clang-format on */

/**
 * @brief Performs coverage verification operation.
 *
 * @param[out] out_val Pointer to receive verification output.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, C_ABSTRACT_HTTP_ERR_INVAL if
 * out_val is NULL.
 */
enum c_abstract_http_error test_coverage_fix_verify(int *out_val) {
  if (out_val == NULL) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_val = 42;
  return C_ABSTRACT_HTTP_SUCCESS;
}
