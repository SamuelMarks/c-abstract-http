/* clang-format off */
#include <stdio.h>
#include <stdlib.h>

#ifdef C_ABSTRACT_HTTP_HEADER_ONLY
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>
#endif

#include "greatest.h"

/* Include test suites */
#include "test_http_types.h"

#if defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "test_http_libsoup3.h"

#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "test_http_libuv.h"

#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "test_http_fetch.h"

#elif defined(_WIN32) && !defined(MINGW_TEST_CURL)
#include "test_http_winhttp.h"
#include "test_http_wininet.h"
#elif defined(__APPLE__)
#include "test_http_apple.h"
#elif defined(__ANDROID__)
#include "test_http_android.h"

#elif defined(__EMSCRIPTEN__)
#include "test_http_wasm.h"

#else
#include "test_http_curl.h"
#endif

/* clang-format on */
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();

  RUN_SUITE(http_types_suite);

#if defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
  RUN_SUITE(http_libsoup3_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
  RUN_SUITE(http_libuv_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
  RUN_SUITE(http_fetch_suite);
#elif defined(_WIN32) && !defined(MINGW_TEST_CURL)
  RUN_SUITE(http_winhttp_suite);
  RUN_SUITE(http_wininet_suite);
#elif defined(__APPLE__)
  RUN_SUITE(http_apple_suite);
#elif defined(__ANDROID__)
  RUN_SUITE(http_android_suite);
#elif defined(__EMSCRIPTEN__)
  RUN_SUITE(http_wasm_suite);
#else
  RUN_SUITE(http_curl_suite);
#endif

  GREATEST_MAIN_END();
}
