/**
 * @file abstract_http_helpers.h
 * @brief Helper functions for unit testing.
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_TESTS_ABSTRACT_HTTP_HELPERS_H
#define C_ABSTRACT_HTTP_TESTS_ABSTRACT_HTTP_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Callback for assertion failures in dependencies.
 * Usually mocked out or prints to stderr.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on completion.
 */
extern enum c_abstract_http_error abstract_http_precondition_failed(void);

/**
 * @brief Write string content to a file.
 * Helper for setting up test conditions.
 *
 * @param[in] filename Path to the file to write.
 * @param[in] contents The data to write.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or an error enum on failure.
 */
extern enum c_abstract_http_error write_to_file(const char *filename,
                                                const char *contents);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !C_ABSTRACT_HTTP_TESTS_ABSTRACT_HTTP_HELPERS_H */
