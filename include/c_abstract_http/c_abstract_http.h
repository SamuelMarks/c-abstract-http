/**
 * @file c_abstract_http.h
 * @brief Main inclusion header for the C Abstract HTTP library.
 *
 * This header conditionally includes the entire API and all implemented
 * transport backends, allowing single-header-style amalgamation if
 * `C_ABSTRACT_HTTP_IMPLEMENTATION` is defined before inclusion.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_H
#define C_ABSTRACT_HTTP_H

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
#include "process.h"
#include "transport.h"
#include "http_raw.h"

#ifdef C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS
#include "http_ws.h"
#endif
#ifdef C_ABSTRACT_HTTP_ENABLE_SSE
#include "http_sse.h"
#endif

/* Include active backend header */
#if defined(C_ABSTRACT_HTTP_USE_PICOQUIC)
#include "http_picoquic.h"
#elif defined(C_ABSTRACT_HTTP_USE_MSH3)
#include "http_msh3.h"
#elif defined(C_ABSTRACT_HTTP_USE_XQUIC)
#include "http_xquic.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "http_libsoup3.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "http_libuv.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
#include "http_libevent.h"
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "http_fetch.h"
#elif defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
/* http_raw.h already included above */
#elif defined(C_ABSTRACT_HTTP_USE_WININET)
#include "http_wininet.h"
#elif defined(C_ABSTRACT_HTTP_USE_WINHTTP)
#include "http_winhttp.h"
#elif defined(C_ABSTRACT_HTTP_USE_APPLE)
#include "http_apple.h"
#elif defined(C_ABSTRACT_HTTP_USE_WASM) || defined(__EMSCRIPTEN__)
#include "http_wasm.h"
#elif defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#if defined(_MSC_VER) && _MSC_VER < 1600
#include "http_wininet.h"
#else
#include "http_winhttp.h"
#endif
#elif defined(__APPLE__) && defined(__clang__)
#include "http_apple.h"
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* http_raw.h already included above */
#else
#include "http_curl.h"
#endif

#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION

/* Core implementations */
#include "../../src/http_types.c"
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "../../src/cmp_integration.c"
#endif
#include "../../src/event_loop.c"
#include "../../src/thread_pool.c"
#include "../../src/c_abstract_http_tls.c"
#include "../../src/coroutine.c"
#include "../../src/actor.c"
#include "../../src/process.c"
#include "../../src/transport.c"
#include "../../src/http_raw.c"

#ifdef C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS
#include "../../src/crypto_utils.c"
#include "../../src/http_ws.c"
#endif
#ifdef C_ABSTRACT_HTTP_ENABLE_SSE
#include "../../src/http_sse.c"
#endif

/* Active backend implementation */
#if defined(C_ABSTRACT_HTTP_USE_PICOQUIC)
#include "../../src/http_picoquic.c"
#elif defined(C_ABSTRACT_HTTP_USE_MSH3)
#include "../../src/http_msh3.c"
#elif defined(C_ABSTRACT_HTTP_USE_XQUIC)
#include "../../src/http_xquic.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "../../src/http_libsoup3.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "../../src/http_libuv.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
#include "../../src/http_libevent.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "../../src/http_fetch.c"
#elif defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS)
/* http_raw.c already included above */
#elif defined(C_ABSTRACT_HTTP_USE_WININET)
#include "../../src/http_wininet.c"
#elif defined(C_ABSTRACT_HTTP_USE_WINHTTP)
#include "../../src/http_winhttp.c"
#elif defined(C_ABSTRACT_HTTP_USE_APPLE)
#include "../../src/http_apple.c"
#elif defined(C_ABSTRACT_HTTP_USE_WASM) || defined(__EMSCRIPTEN__)
#include "../../src/http_wasm.c"
#elif defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#if defined(_MSC_VER) && _MSC_VER < 1600
#include "../../src/http_wininet.c"
#else
#include "../../src/http_winhttp.c"
#endif
#elif defined(__APPLE__) && defined(__clang__)
#include "../../src/http_apple.c"
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
/* http_raw.c already included above */
#else
#include "../../src/http_curl.c"
#endif
/* clang-format on */

#endif /* C_ABSTRACT_HTTP_IMPLEMENTATION */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_H */
