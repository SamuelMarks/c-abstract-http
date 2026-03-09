#include <stdio.h>
#include <stdlib.h>

/* clang-format boundary */
#include "greatest.h"

/* Include test suites */
/* clang-format boundary */
#include "test_http_types.h"

#if defined(_WIN32) && !defined(MINGW_TEST_CURL)
#include "test_http_winhttp.h"
#include "test_http_wininet.h"
#elif defined(__APPLE__)
#include "test_http_apple.h"
#elif defined(__ANDROID__)
#include "test_http_android.h"
#else
#include "test_http_curl.h"
#endif

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();

  RUN_SUITE(http_types_suite);

#if defined(_WIN32) && !defined(MINGW_TEST_CURL)
  RUN_SUITE(http_winhttp_suite);
  RUN_SUITE(http_wininet_suite);
#elif defined(__APPLE__)
  RUN_SUITE(http_apple_suite);
#elif defined(__ANDROID__)
  RUN_SUITE(http_android_suite);
#else
  RUN_SUITE(http_curl_suite);
#endif

  GREATEST_MAIN_END();
}
