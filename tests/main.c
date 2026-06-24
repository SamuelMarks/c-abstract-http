/* clang-format off */
#include <stdio.h>
#include <stdlib.h>

#if defined(__WATCOMC__)
#define strncat_s(dest, destsz, src, count) strncat(dest, src, count)
#endif

#ifdef C_ABSTRACT_HTTP_HEADER_ONLY
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>
#endif

#include "greatest.h"
#include "mock_alloc.h"

/* Include test suites */

#include "test_http_types.h"
#include "test_event_loop.h"
#include "test_thread_pool.h"
#include "test_tls.h"
#include "test_process.h"
#include "test_coroutine.h"
#include "test_actor.h"
#include "test_transport.h"
#include "test_mock_coverage.h"
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "test_cmp_integration.h"
#endif

#if defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "test_http_libsoup3.h"

#elif defined(C_ABSTRACT_HTTP_USE_LSQUIC)
#include "test_http_lsquic.h"

#elif defined(C_ABSTRACT_HTTP_USE_PICOQUIC)
#include "test_http_picoquic.h"

#elif defined(C_ABSTRACT_HTTP_USE_NGHTTP3)
#include "test_http_nghttp3.h"

#elif defined(C_ABSTRACT_HTTP_USE_MSH3)
#include "test_http_msh3.h"

#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "test_http_libuv.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
#include "test_http_libevent.h"

#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "test_http_fetch.h"

#elif defined(_WIN32) && !defined(MINGW_TEST_CURL)
#if defined(C_ABSTRACT_HTTP_USE_WINHTTP)
#include "test_http_winhttp.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_WININET)
#include "test_http_wininet.h"
#endif
#elif defined(__APPLE__)
#include "test_http_apple.h"
#elif defined(__ANDROID__)
#include "test_http_android.h"

#elif defined(__EMSCRIPTEN__)
#include "test_http_wasm.h"

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No HTTP backend tests on DOS currently */

#else
#include "test_http_curl.h"
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
/* clang-format on */

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  int i;
  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--cdd-worker") == 0) {
      return 1;
    }
  }

#if defined(_MSC_VER)
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
  GREATEST_MAIN_BEGIN();

  RUN_SUITE(http_types_suite);
  RUN_SUITE(event_loop_suite);
  RUN_SUITE(thread_pool_suite);
  RUN_SUITE(tls_suite);
  RUN_SUITE(process_suite);
  RUN_SUITE(coroutine_suite);
  RUN_SUITE(actor_suite);
  RUN_SUITE(transport_suite);
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) ||                      \
    !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
  RUN_SUITE(cmp_integration_suite);
#endif

#if defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
  RUN_SUITE(http_libsoup3_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LSQUIC)
  RUN_SUITE(http_lsquic_suite);
#elif defined(C_ABSTRACT_HTTP_USE_PICOQUIC)
  RUN_SUITE(http_picoquic_suite);
#elif defined(C_ABSTRACT_HTTP_USE_NGHTTP3)
  RUN_SUITE(http_nghttp3_suite);
#elif defined(C_ABSTRACT_HTTP_USE_MSH3)
  RUN_SUITE(http_msh3_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
  RUN_SUITE(http_libuv_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
  RUN_SUITE(http_libevent_suite);
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
  RUN_SUITE(http_fetch_suite);
#elif defined(_WIN32) && !defined(MINGW_TEST_CURL)
#if defined(C_ABSTRACT_HTTP_USE_WINHTTP)
  RUN_SUITE(http_winhttp_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_WININET)
  RUN_SUITE(http_wininet_suite);
#endif
#elif defined(__APPLE__)
  RUN_SUITE(http_apple_suite);
#elif defined(__ANDROID__)
  RUN_SUITE(http_android_suite);
#elif defined(__EMSCRIPTEN__)
  RUN_SUITE(http_wasm_suite);

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  /* No HTTP backend suite for DOS currently */

#else
  RUN_SUITE(http_curl_suite);
#endif

#ifdef malloc
  RUN_SUITE(mock_coverage_suite);
#endif

  GREATEST_MAIN_END();
}
