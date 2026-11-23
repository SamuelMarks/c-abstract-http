
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
#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include "test_http_raw.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_XQUIC) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_xquic.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_LSQUIC) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_lsquic.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_NGHTTP3) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_nghttp3.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_PICOQUIC) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_picoquic.h"
#endif
#if defined(__EMSCRIPTEN__) || defined(C_ABSTRACT_HTTP_USE_WASM) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_wasm.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBFETCH) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_fetch.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBEVENT) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_libevent.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBSOUP3) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_libsoup3.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBUV) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_libuv.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_MSH3) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
#include "test_http_msh3.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_WINHTTP) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY) && !defined(_WIN32))
#include "test_http_winhttp.h"
#endif
#if defined(C_ABSTRACT_HTTP_USE_WININET) || (defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HEADER_ONLY) && !defined(_WIN32))
#include "test_http_wininet.h"
#endif
#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include "test_mock_coverage.h"
#endif
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "test_cmp_integration.h"
#endif

#if defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
/* Included above */
#elif (defined(_WIN32) || defined(C_ABSTRACT_HTTP_USE_WINHTTP) || defined(C_ABSTRACT_HTTP_USE_WININET)) && !defined(MINGW_TEST_CURL)
/* Windows HTTP backends included above */
#elif defined(C_ABSTRACT_HTTP_USE_APPLE)
#include "test_http_apple.h"
#if defined(C_ABSTRACT_HTTP_HAVE_CURL)
#include "test_http_curl.h"
#endif
#elif defined(__EMSCRIPTEN__)
/* No HTTP curl on Emscripten */

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No HTTP backend tests on DOS currently */

#elif defined(C_ABSTRACT_HTTP_HAVE_CURL)
#include "test_http_curl.h"
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <signal.h>
#endif
/* clang-format on */

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
#if defined(__linux__) || defined(__APPLE__)
  signal(SIGPIPE, SIG_IGN);

#endif
  if (argc > 1 && strcmp(argv[1], "--test-worker") == 0) {
    return 1;
  }

#if defined(_WIN32)
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);
#endif
#if defined(_MSC_VER)
  {
    int is_wine_dll_crt = 0;
#if defined(_DLL)
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll && GetProcAddress(ntdll, "wine_get_version") != NULL) {
      is_wine_dll_crt = 1;
    }
#endif
    if (!is_wine_dll_crt) {
#if defined(_DEBUG)
      _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
      _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
      _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
      _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
      _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
      _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
      _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    }
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

#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
  RUN_SUITE(http_raw_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_XQUIC) ||                                      \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_xquic_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_LSQUIC) ||                                     \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_lsquic_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_NGHTTP3) ||                                    \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_nghttp3_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_PICOQUIC) ||                                   \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_picoquic_suite);
#endif
#if defined(__EMSCRIPTEN__) || defined(C_ABSTRACT_HTTP_USE_WASM) ||            \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_wasm_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBFETCH) ||                                   \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_fetch_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBEVENT) ||                                   \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_libevent_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBSOUP3) ||                                   \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_libsoup3_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_LIBUV) ||                                      \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_libuv_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_MSH3) ||                                       \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY))
  RUN_SUITE(http_msh3_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_WINHTTP) ||                                    \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY) && !defined(_WIN32))
  RUN_SUITE(http_winhttp_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_WININET) ||                                    \
    (defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
     !defined(C_ABSTRACT_HTTP_HEADER_ONLY) && !defined(_WIN32))
  RUN_SUITE(http_wininet_suite);
#endif
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) ||                      \
    !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
  RUN_SUITE(cmp_integration_suite);
#endif

#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
#if defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
  /* http_fetch_suite run unconditionally above */
#elif defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
  /* http_raw_suite run unconditionally above */
#elif (defined(C_ABSTRACT_HTTP_USE_WINHTTP) ||                                 \
       defined(C_ABSTRACT_HTTP_USE_WININET)) &&                                \
    !defined(MINGW_TEST_CURL)
  /* Windows suites run unconditionally above */
#elif defined(C_ABSTRACT_HTTP_USE_APPLE)
  RUN_SUITE(http_apple_suite);
#if defined(C_ABSTRACT_HTTP_HAVE_CURL)
  RUN_SUITE(http_curl_suite);
#endif
#elif defined(__EMSCRIPTEN__)
  /* No HTTP curl suite for Emscripten */

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
  /* No HTTP backend suite for DOS currently */

#elif defined(C_ABSTRACT_HTTP_HAVE_CURL)
  RUN_SUITE(http_curl_suite);
#endif
#endif

#if !defined(C_ABSTRACT_HTTP_SINGLE_THREADED) && !defined(__MSDOS__) &&        \
    !defined(__DOS__) && !defined(DOS)
  RUN_SUITE(mock_coverage_suite);
#endif

  GREATEST_MAIN_END();
}
