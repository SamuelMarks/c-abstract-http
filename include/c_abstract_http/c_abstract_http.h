#ifndef C_ABSTRACT_HTTP_H
#define C_ABSTRACT_HTTP_H

/* clang-format off */
#include "http_types.h"

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

#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION
/* Single translation unit inclusion of the source */
#include "../../src/fs.c"
#include "../../src/http_types.c"
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
#else
#include "../../src/http_curl.c"
#endif

#endif /* C_ABSTRACT_HTTP_IMPLEMENTATION */

#endif /* C_ABSTRACT_HTTP_H */

/* clang-format on */