/**
 * @file c_abstract_http.h
 * @brief Main inclusion header for the C Abstract HTTP library.
 *
 * This header conditionally includes the entire API and all implemented
 * transport backends, allowing single-header-style amalgamation when
 * C_ABSTRACT_HTTP_AMALGAMATION is defined.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_H
#define C_ABSTRACT_HTTP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include "http_types.h"

#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "cmp_integration.h"
#endif

#include "event_loop.h"

#include "thread_pool.h"

#include "c_abstract_http_tls.h"

#include "coroutine.h"

#include "actor.h"

#if defined(C_ABSTRACT_HTTP_USE_ARIA2)
#elif defined(C_ABSTRACT_HTTP_USE_LSQUIC)
#include "http_lsquic.h"
#elif defined(C_ABSTRACT_HTTP_USE_PICOQUIC)
#include "http_picoquic.h"
#elif defined(C_ABSTRACT_HTTP_USE_NGHTTP3)
#include "http_nghttp3.h"
#elif defined(C_ABSTRACT_HTTP_USE_MSH3)
#include "http_msh3.h"
#elif defined(C_ABSTRACT_HTTP_USE_WININET)
#include "http_wininet.h"
#elif defined(C_ABSTRACT_HTTP_USE_WINHTTP) || defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#include "http_winhttp.h"
#elif defined(__APPLE__)
#include "http_apple.h"
#elif defined(__ANDROID__)
#include "http_android.h"
#elif defined(C_ABSTRACT_HTTP_USE_XQUIC)
#include "http_xquic.h"
#elif defined(__EMSCRIPTEN__) || defined(C_ABSTRACT_HTTP_USE_WASM)
#include "http_wasm.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "http_libsoup3.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "http_libuv.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
#include "http_libevent.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "http_fetch.h"
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS) || defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
#include "http_raw.h"
#else
#include "http_curl.h"
#endif

#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION
/* Single translation unit inclusion of the source */
#include "../../src/http_types.c"
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "../../src/cmp_integration.c"
#endif
#include "../../src/event_loop.c"
#include "../../src/thread_pool.c"
#include "../../src/c_abstract_http_tls.c"
#include "../../src/coroutine.c"
#include "../../src/actor.c"
#include "../../src/str.c"
#include "../../src/transport.c"

#if defined(_WIN32)
#include "../../src/http_winhttp.c"
#include "../../src/http_wininet.c"
#elif defined(__APPLE__)
#include "../../src/http_apple.c"
#elif defined(__ANDROID__)
#include "../../src/http_android.c"
#elif defined(__EMSCRIPTEN__)
#include "../../src/http_wasm.c"
#elif defined(C_ABSTRACT_HTTP_USE_ARIA2)
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "../../src/http_libsoup3.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "../../src/http_libuv.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
#include "../../src/http_libevent.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "../../src/http_fetch.c"
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS) || defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
#include "../../src/http_raw.c"
#else
#include "../../src/http_curl.c"
#endif
/* clang-format on */

#endif /* C_ABSTRACT_HTTP_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_H */
