/* clang-format off */
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

/* clang-format boundary */
#elif defined(__EMSCRIPTEN__)
#include "test_http_wasm.h"

/* clang-format boundary */
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "test_http_libsoup3.h"

/* clang-format boundary */
#else
#include "test_http_curl.h"
#endif

/* clang-format on */
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
#elif defined(__EMSCRIPTEN__)
  RUN_SUITE(http_wasm_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
  RUN_SUITE(http_libsoup3_suite);
#else
  RUN_SUITE(http_curl_suite);
#endif

  GREATEST_MAIN_END();
}
