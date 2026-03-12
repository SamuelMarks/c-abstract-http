#ifndef C_ABSTRACT_HTTP_H
#define C_ABSTRACT_HTTP_H

/* clang-format off */
#include "http_types.h"

/* clang-format boundary */
#if defined(C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION) || !defined(C_ABSTRACT_HTTP_NO_MULTIPLATFORM_INTEGRATION)
#include "cmp_integration.h"
#endif

/* clang-format boundary */
#include "event_loop.h"

/* clang-format boundary */
#include "thread_pool.h"

/* clang-format boundary */
#include "cdd_tls.h"

/* clang-format boundary */
#include "coroutine.h"

/* clang-format boundary */
#include "actor.h"

/* clang-format boundary */
#include "http_libsoup3.h"

/* clang-format boundary */
#include "http_curl.h"

/* clang-format boundary */
#include "http_winhttp.h"

/* clang-format boundary */
#include "http_wininet.h"

/* clang-format boundary */
#include "http_apple.h"

/* clang-format boundary */
#include "http_android.h"

/* clang-format boundary */
#include "http_wasm.h"

/* clang-format boundary */
#include "http_libuv.h"

/* clang-format boundary */
#include "http_libevent.h"

#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION
/* Single translation unit inclusion of the source */
#include "../../src/fs.c"
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
#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)
#include "../../src/http_libsoup3.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBUV)
#include "../../src/http_libuv.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBEVENT)
#include "../../src/http_libevent.c"
#elif defined(C_ABSTRACT_HTTP_USE_LIBFETCH)
#include "../../src/http_fetch.c"
#else
#include "../../src/http_curl.c"
#endif

#endif /* C_ABSTRACT_HTTP_IMPLEMENTATION */

#endif /* C_ABSTRACT_HTTP_H */

/* clang-format on */
