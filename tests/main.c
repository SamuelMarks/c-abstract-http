
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(__WATCOMC__)
#define strncat_s(dest, destsz, src, count) strncat(dest, src, count)
#endif

#ifdef C_ABSTRACT_HTTP_HEADER_ONLY
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
/* __STDC_VERSION__ and _MSC_VER undefs removed to fix macro redefinition errors */
#endif
#include "greatest.h"
#include "mock_alloc.h"
#include "../src/str.h"

/* Include test suites */

#include "test_http_types.h"
#include "test_event_loop.h"
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
#include "test_thread_pool.h"
#endif
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
#include "test_tls.h"
#endif
#include "test_process.h"
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
#include "test_coroutine.h"
#endif
#include "test_actor.h"
#include "test_transport.h"
#include "test_http_raw.h"
#include "test_http_aria2.h"
#include "test_http_xquic.h"
#include "test_http_picoquic.h"
#include "test_http_wasm.h"
#include "test_http_fetch.h"
#include "test_http_nghttp3.h"
#include "test_http_libevent.h"
#include "test_http_libuv.h"
#include "test_http_lsquic.h"
#include "test_http_libsoup3.h"
#include "test_http_android.h"
#include "test_http_msh3.h"
#include "test_http_winhttp.h"
#include "test_http_wininet.h"
#include "test_mock_coverage.h"
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "test_cmp_integration.h"
#endif

#if defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "test_http_fetch.h"

#elif (defined(_WIN32) || defined(C_ABSTRACT_HTTP_USE_WINHTTP) || defined(C_ABSTRACT_HTTP_USE_WININET)) && !defined(MINGW_TEST_CURL)
/* Windows HTTP backends included above */
#elif defined(__APPLE__)
#include "test_http_apple.h"
#if defined(C_ABSTRACT_HTTP_HAVE_CURL)
#include "test_http_curl.h"
#endif
#elif defined(__ANDROID__)
#include "test_http_android.h"

#elif defined(__EMSCRIPTEN__)
/* No HTTP curl on Emscripten */

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No HTTP backend tests on DOS currently */

#else
#include "test_http_curl.h"
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <signal.h>
#endif
/* clang-format on */

GREATEST_MAIN_DEFS();

#if defined(_MSC_VER)
#include <libloaderapi.h>
#endif

int main(int argc, char **argv) {
  int i;
#if defined(__linux__) || defined(__APPLE__)
  signal(SIGPIPE, SIG_IGN);

#endif
  for (i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--test-worker") == 0) {
      exit(1);
    }
  }

#if defined(_MSC_VER)
  if (!GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version")) {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, (_HFILE)(size_t)2);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, (_HFILE)(size_t)2);
  }
#endif
  GREATEST_MAIN_BEGIN();

  RUN_SUITE(http_types_suite);
  RUN_SUITE(event_loop_suite);
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
  RUN_SUITE(thread_pool_suite);

#endif
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
  RUN_SUITE(tls_suite);

#endif
  RUN_SUITE(process_suite);
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
  RUN_SUITE(coroutine_suite);

#endif
  RUN_SUITE(actor_suite);
  RUN_SUITE(transport_suite);
  RUN_SUITE(http_raw_suite);
  RUN_SUITE(http_aria2_suite);
  RUN_SUITE(http_xquic_suite);
  RUN_SUITE(http_picoquic_suite);
  RUN_SUITE(http_wasm_suite);
  RUN_SUITE(http_fetch_suite);
  RUN_SUITE(http_nghttp3_suite);
  RUN_SUITE(http_libevent_suite);
  RUN_SUITE(http_libuv_suite);
  RUN_SUITE(http_lsquic_suite);
  RUN_SUITE(http_libsoup3_suite);
  RUN_SUITE(http_android_suite);
  RUN_SUITE(http_msh3_suite);
#if !defined(_WIN32)
  RUN_SUITE(http_winhttp_suite);
  RUN_SUITE(http_wininet_suite);
#else
#if defined(C_ABSTRACT_HTTP_USE_WINHTTP)
  RUN_SUITE(http_winhttp_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_WININET)
  RUN_SUITE(http_wininet_suite);
#endif
#endif
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) ||                      \
    !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
  RUN_SUITE(cmp_integration_suite);
#endif

#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
#if defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
  RUN_SUITE(http_fetch_suite);
#elif (defined(_WIN32) || defined(C_ABSTRACT_HTTP_USE_WINHTTP) ||              \
       defined(C_ABSTRACT_HTTP_USE_WININET)) &&                                \
    !defined(MINGW_TEST_CURL)
  /* Windows suites run unconditionally above */
#elif defined(__APPLE__)
  RUN_SUITE(http_apple_suite);
#if defined(C_ABSTRACT_HTTP_HAVE_CURL)
  RUN_SUITE(http_curl_suite);
#endif
#elif defined(__ANDROID__)
  /* Android suite run unconditionally above */

#elif defined(__EMSCRIPTEN__)
  /* No HTTP curl suite for Emscripten */

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  /* No HTTP backend suite for DOS currently */

#else
  RUN_SUITE(http_curl_suite);
#endif
#endif

#ifdef malloc
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
  RUN_SUITE(mock_coverage_suite);
#endif
#endif

  GREATEST_MAIN_END();
}
