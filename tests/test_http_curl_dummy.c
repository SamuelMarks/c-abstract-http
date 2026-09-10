/**
 * @file test_http_curl_dummy.c
 * @brief Dummy test routines for curl backend.
 * @author Samuel Marks
 */

/* clang-format off */
#include "test_http_curl_dummy.h"
#include <stdio.h>
/* clang-format on */

/**
 * @brief Performs curl dummy test verification operation.
 *
 * @param[out] out_val Pointer to receive verification output.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, C_ABSTRACT_HTTP_ERR_INVAL if
 * out_val is NULL.
 */
enum c_abstract_http_error test_http_curl_dummy_verify(int *out_val) {
  if (out_val == NULL) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_val = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}
