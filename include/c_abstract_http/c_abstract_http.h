#ifndef C_ABSTRACT_HTTP_H
#define C_ABSTRACT_HTTP_H

#include "http_types.h"
#include "http_curl.h"
#include "http_winhttp.h"
#include "http_wininet.h"
#include "http_apple.h"
#include "http_android.h"

#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION
/* Single translation unit inclusion of the source */
#include "../src/str.c"
#include "../src/fs.c"
#include "../src/http_types.c"
#include "../src/transport.c"

#if defined(_WIN32)
#include "../src/http_winhttp.c"
#include "../src/http_wininet.c"
#elif defined(__APPLE__)
#include "../src/http_apple.c"
#elif defined(__ANDROID__)
#include "../src/http_android.c"
#else
#include "../src/http_curl.c"
#endif

#endif /* C_ABSTRACT_HTTP_IMPLEMENTATION */

#endif /* C_ABSTRACT_HTTP_H */
