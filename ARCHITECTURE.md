# Architecture

The `c-abstract-http` library is designed to offer a clean, unified API over disparate native networking implementations. 

## Core Philosophy

1. **Native By Default**: The library avoids shipping thick protocol implementations (like libcurl) when the underlying OS already provides a robust HTTP client stack (like WinHTTP/WinINet on Windows or CFNetwork on Apple).
2. **C89 Purity**: All code is written in strict C89 to maximize portability. C++ features and syntax are strictly avoided.
3. **Robust Error Propagation (The `int` Return Paradigm)**: 
   - All functions that perform an operation, allocate memory, or query state **must return an `int`** representing an exit code (e.g., `0` for success, `EINVAL`, `ENOMEM`, etc.).
   - If a function needs to return a value (e.g., retrieving a header), it does so by accepting an `out` pointer (e.g., `const char** out_val`).
   - This allows the caller to perfectly trace failure paths without ambiguous `NULL` returns.
4. **Zero-Bloat Windows Headers**:
   - `c-abstract-http` enforces a strict ban on `#include <windows.h>`.
   - Instead, the codebase utilizes leaner headers like `<windef.h>`, `<winnt.h>`, `<winbase.h>`, `<winnls.h>`, and `<winerror.h>`.
   - This prevents macro collisions, drastically reduces compile time, and keeps the namespace clean for consuming applications.
5. **MSVC Safe CRT Features**:
   - On MSVC environments (`_MSC_VER`), dangerous CRT functions (`sprintf`, `strcpy`, `fopen`) are avoided.
   - The library employs `#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)` compiler guards to selectively utilize `_s` extensions (`sprintf_s`, `strcpy_s`, `fopen_s`) to prevent buffer overruns without sacrificing compatibility with GCC, Clang, or MinGW.

## Abstraction Layers

### 1. `http_types.h` (The Frontend API)
This file dictates the contract the developer interacts with.
- Defines standard structs: `HttpRequest`, `HttpResponse`, `HttpHeaders`, `HttpCookieJar`, `HttpConfig`.  
- Provides lifecycle functions for initializing and freeing these objects.
- Normalizes configurations (e.g., retry policies, proxies, timeouts, caching).

### 2. Modality Execution Runtimes
To handle concurrent HTTP requests without locking consumers into a single paradigm, the library operates through a series of pluggable "Modalities" controlled via `enum ExecutionModality`.
- **`MODALITY_SYNC`:** Default sequential blocking execution.
- **`MODALITY_ASYNC`:** Non-blocking `ModalityEventLoop` executing `uv_run`-style socket polling.
- **`MODALITY_THREAD_POOL`:** Cross-platform multi-threaded pool utilizing Mutexes and Condition Variables (e.g. `pthread` on Unix, `CRITICAL_SECTION` on Windows).
- **`MODALITY_MULTIPROCESS`:** Uses `fork`/`CreateProcess` natively bundled with C89 binary IPC serialization to securely isolate requests in totally detached process states.
- **`MODALITY_GREENTHREAD`:** High-performance user-space thread switching (`ucontext_t` and Windows `Fiber` APIs) allowing synchronous code to pause during I/O without native thread overhead.
- **`MODALITY_MESSAGE_PASSING`:** A built-in actor model and pub-sub bus (`CddMessageBus`) enabling decoupled internal routing of HTTP requests.

### 3. `transport.c` (The Dispatcher)
This acts as the bridge. When you call `http_client_init`, the transport layer evaluates the compiled platform (e.g., `#ifdef _WIN32`) and wires up the backend's specific `send` and `send_multi` implementations and context lifecycle hooks.

### 4. Backend Implementations (`http_winhttp.c`, `http_curl.c`, etc.)
These files implement the raw platform APIs.
Each backend translates the abstract `HttpRequest` into its native equivalent.
- `http_winhttp.c`: The modern standard for Windows networking. Uses `WinHttpSendRequest`.
- `http_wininet.c`: The legacy standard for older Windows environments or specialized proxy/caching setups.
- `http_apple.c`: Uses `CFNetwork` and `Foundation` types for native execution on macOS and iOS.
- `http_curl.c`: The fallback for POSIX systems (Linux, BSD) leveraging `libcurl`.
- `http_libuv / libevent.c`: Modern POSIX backend leveraging `libuv / libevent`, ideal for Node.js environments.
- `http_libsoup3.c`: Modern POSIX backend leveraging `libsoup3`, ideal for GTK4 environments.
- `http_fetch.c`: FreeBSD and POSIX backend leveraging `libfetch`, ideal for BSD environments.
- `http_android.c`: Specialized networking integrations for Android NDK environments.
- `http_wasm.c`: The WebAssembly backend leveraging Emscripten's Fetch API to bridge network calls to the browser's `fetch`.

### 5. Cryptography Integration
To preserve the "Native By Default" philosophy without locking consumers into a heavy OpenSSL dependency, the library dynamically injects cryptography routines natively into the underlying HTTP abstraction via a unified CMake macro (`c_abstract_http_apply_crypto`). 
If a backend like `libcurl` or `libevent` is used, its configuration is mapped downwards (e.g. bridging `C_ABSTRACT_HTTP_USE_WOLFSSL` -> `CURL_USE_WOLFSSL`). This allows developers to seamlessly pivot between OpenSSL, mbedTLS, BearSSL, Botan, s2n, Schannel, and GnuTLS without ever modifying their application's code.

## Memory Management

`c-abstract-http` manages memory explicitly via `malloc`, `calloc`, `realloc`, and `free`.
To prevent memory leaks, every `_init` function (e.g., `http_request_init`) has a corresponding `_free` function (e.g., `http_request_free`). 

- **Headers**: Key-value pairs are `strdup`'d into dynamic arrays and cleaned up automatically when `http_headers_free` is invoked.
- **Cookies**: Handled via `HttpCookieJar`, safely tracking and matching cookies to domains and paths.
- **Multipart Data**: Form-data is natively flattened or natively passed depending on backend support, taking care not to leak boundaries or part payloads.

## Formatting Strings

To maintain C89 compatibility across varying architectures (32-bit vs 64-bit) and legacy MSVC compilers, `c-abstract-http` employs a dedicated `NUM_FORMAT` macro instead of standard `%zu` format specifiers.

```c
#ifndef NUM_FORMAT
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define NUM_FORMAT "%Iu" /* Legacy MSVC size_t format */
#else
#define NUM_FORMAT "%zu" /* C99 standard size_t format */
#endif
#endif
```

## Testing & Coverage

Testing is done via CMake's `enable_testing()` and CTest. Tests are built across all supported MSVC, MinGW, and POSIX toolchains. Test suites assert memory leaks, error code propagation, header formatting, and multipart boundary generation.
