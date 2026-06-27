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

#include "cdd_tls.h"

#include "coroutine.h"

#include "actor.h"

#include "http_libsoup3.h"

#include "http_curl.h"

#include "http_winhttp.h"

#include "http_wininet.h"

#include "http_apple.h"

#include "http_android.h"

#include "http_aria2.h"

#include "http_wasm.h"

#include "http_libuv.h"

#include "http_libevent.h"

#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION
/* Single translation unit inclusion of the source */
#include "../../src/http_types.c"
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "../../src/cmp_integration.c"
#endif
#include "../../src/event_loop.c"
#include "../../src/thread_pool.c"
#include "../../src/cdd_tls.c"
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
#include "../../src/http_aria2.c"
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
