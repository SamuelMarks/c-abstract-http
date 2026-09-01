/* LCOV_EXCL_BR_START */
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
#undef __STDC_VERSION__
#undef _MSC_VER
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

#elif (defined(_WIN32) || defined(C_ABSTRACT_HTTP_USE_WINHTTP) || defined(C_ABSTRACT_HTTP_USE_WININET)) && !defined(MINGW_TEST_CURL)
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

/* LCOV_EXCL_START */ GREATEST_MAIN_DEFS(); /* LCOV_EXCL_STOP */

#if defined(__linux__) || defined(__APPLE__)
#include <signal.h>
#endif
int main(int argc, char **argv) {
  int i;
#if defined(__linux__) || defined(__APPLE__)
  signal(SIGPIPE, SIG_IGN);
/* LCOV_EXCL_START */                                                          \
#endif                                               /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ for (i = 1; i < argc; ++i) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (strcmp(argv[i], "--test-worker") ==
                              0) {    /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ _exit(1); /* LCOV_EXCL_STOP */
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

#if defined(_MSC_VER)
_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
_CrtSetReportFile(_CRT_ASSERT, (_HFILE)(size_t)2);
_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
_CrtSetReportFile(_CRT_ERROR, (_HFILE)(size_t)2);
#endif
/* LCOV_EXCL_START */ GREATEST_MAIN_BEGIN(); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ RUN_SUITE(http_types_suite); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ RUN_SUITE(event_loop_suite); /* LCOV_EXCL_STOP */
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
RUN_SUITE(thread_pool_suite);
/* LCOV_EXCL_START */                                                          \
#endif /* LCOV_EXCL_STOP */
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
RUN_SUITE(tls_suite);
/* LCOV_EXCL_START */                                                          \
#endif                                          /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ RUN_SUITE(process_suite); /* LCOV_EXCL_STOP */
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
RUN_SUITE(coroutine_suite);
/* LCOV_EXCL_START */                                                          \
#endif                                            /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ RUN_SUITE(actor_suite);     /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ RUN_SUITE(transport_suite); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) ||                      \
    !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
/* LCOV_EXCL_START */ RUN_SUITE(cmp_integration_suite); /* LCOV_EXCL_STOP */
#endif

#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
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
#elif (defined(_WIN32) || defined(C_ABSTRACT_HTTP_USE_WINHTTP) ||              \
       defined(C_ABSTRACT_HTTP_USE_WININET)) &&                                \
    !defined(MINGW_TEST_CURL)
#if defined(C_ABSTRACT_HTTP_USE_WINHTTP)
RUN_SUITE(http_winhttp_suite);
#endif
#if defined(C_ABSTRACT_HTTP_USE_WININET)
RUN_SUITE(http_wininet_suite);
#endif
#elif defined(__APPLE__)
/* LCOV_EXCL_START */ RUN_SUITE(http_apple_suite); /* LCOV_EXCL_STOP */
#elif defined(__ANDROID__)
RUN_SUITE(http_android_suite);
#elif defined(__EMSCRIPTEN__)
RUN_SUITE(http_wasm_suite);

#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* No HTTP backend suite for DOS currently */

#else
RUN_SUITE(http_curl_suite);
#endif
#endif

#ifdef malloc
#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED
/* LCOV_EXCL_START */ RUN_SUITE(mock_coverage_suite); /* LCOV_EXCL_STOP */
#endif
#endif

/* LCOV_EXCL_START */ GREATEST_MAIN_END(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_BR_STOP */
