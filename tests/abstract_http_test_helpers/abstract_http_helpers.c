/* LCOV_EXCL_BR_START */
/**
 * @file abstract_http_helpers.c
 * @brief Implementation of test helpers.
 * @author Samuel Marks
 */

/* clang-format off */
#include <stdio.h>
#include <stdlib.h>

#include "abstract_http_helpers.h"
/* clang-format on */

void abstract_http_precondition_failed(void) {
  fputs("abstract_http_precondition_failed\n", stderr);
}

int write_to_file(const char *const filename, const char *const contents) {
  FILE *fh;
  int rc = EXIT_SUCCESS;

  if (filename == NULL || contents == NULL)
    return EXIT_FAILURE;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER) ||                         \
    defined(__STDC_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__
  {
    errno_t err;
    err = fopen_s(&fh, filename, "w");
    if (err != 0 || fh == NULL) {
      fprintf(stderr, "Failed to open for writing %s\n", filename);
      return EXIT_FAILURE;
    }
  }
#else
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  fopen_s(&fh, filename, "w");
#else
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  fopen_s(&fh, filename, "w");
#else
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  fopen_s(&fh, filename, "w");
#else
#if defined(_MSC_VER)
  if (fopen_s(&fh, filename, "w") != 0)
    fh = NULL;
#else
#if defined(_MSC_VER)
  if (fopen_s(&fh, filename, "w") != 0)
    fh = NULL;
#else
  fh = fopen(filename, "w");
#endif
#endif
#endif
#endif
#endif
  if (fh == NULL) {
    fprintf(stderr, "Failed to open for writing %s\n", filename);
    return EXIT_FAILURE;
  }
#endif

  if (fputs(contents, fh) < 0) {
    fprintf(stderr, "Failure to write to %s\n", filename);
    rc = EXIT_FAILURE;
  }

  if (fclose(fh) != 0) {
    rc = EXIT_FAILURE;
  }

  return rc;
}

/* LCOV_EXCL_BR_STOP */
