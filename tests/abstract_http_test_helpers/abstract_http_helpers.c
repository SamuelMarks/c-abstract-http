/**
 * @file abstract_http_helpers.c
 * @brief Implementation of test helpers.
 * @author Samuel Marks
 */

/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abstract_http_helpers.h"
#include "../mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_ENABLE_COVERAGE) ||                                \
    defined(C_ABSTRACT_HTTP_MOCK_ALLOC)
#define fwrite c_abstract_http_mock_fwrite
#define fclose c_abstract_http_mock_fclose
#endif

/**
 * @brief Prints precondition failure message to stderr.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on completion.
 */
enum c_abstract_http_error abstract_http_precondition_failed(void) {
  fputs("abstract_http_precondition_failed\n", stderr);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Writes string contents to a specified file.
 *
 * @param[in] filename The path to destination file.
 * @param[in] contents The string content to write.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error enum on failure.
 */
enum c_abstract_http_error write_to_file(const char *const filename,
                                         const char *const contents) {
  FILE *fh;
  size_t len;
  size_t written;
  enum c_abstract_http_error rc;

  rc = C_ABSTRACT_HTTP_SUCCESS;

  if (filename == NULL || contents == NULL) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  if (fopen_s(&fh, filename, "w") != 0) {
    fh = NULL;
  }
#else
  fh = fopen(filename, "w");
#endif
  if (fh == NULL) {
    fprintf(stderr, "Failed to open for writing %s\n", filename);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  len = strlen(contents);
  written = fwrite(contents, 1, len, fh);
  if (written != len) {
    fprintf(stderr, "Failure to write to %s\n", filename);
    rc = C_ABSTRACT_HTTP_ERR_IO;
  }

  if (fclose(fh) != 0) {
    rc = C_ABSTRACT_HTTP_ERR_IO;
  }

  return rc;
}
