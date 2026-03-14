# Architecture

The `c-abstract-http` library is designed to offer a clean, unified API over disparate native networking implementations. By standardizing across diverse backends, it allows C applications to interact seamlessly with web services while minimizing payload bloat and dependency footprint.

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

## Execution Modalities

To accommodate varied concurrency models, `c-abstract-http` supports multiple execution modalities natively through the dispatcher:

- **Sync** (`MODALITY_SYNC`): Blocking, sequential requests natively executed on the calling thread.
- **Async** (`MODALITY_ASYNC`): Non-blocking execution mapped to the underlying backend's native event loop (e.g., libuv, libevent, IOCP).
- **Thread Pool** (`MODALITY_THREAD_POOL`): Background worker thread execution, abstracting synchronization.
- **Multiprocess** (`MODALITY_MULTIPROCESS`): Isolates networking logic in separate processes, communicating via IPC.
- **Greenthread** (`MODALITY_GREENTHREAD`): Cooperative user-space threads (e.g., `ucontext`/`setjmp` abstractions).
- **Message Passing** (`MODALITY_MESSAGE_PASSING`): Actor-model abstraction utilizing message pipes.

## Abstraction Layers

### 1. Target Platforms

`c-abstract-http` explicitly supports:
- **Windows**: MSVC (2005, 2022, 2026), MinGW, Cygwin
- **macOS / iOS**: Clang / Apple Frameworks
- **Linux / POSIX**: GCC / Clang
- **FreeBSD**: Clang
- **Android**: NDK
- **WebAssembly**: Emscripten
- **DOS**: OpenWatcom (with fallback support for Watt-32 / mTCP)

### 2. Target Network Libraries

The dispatcher layer evaluates the compiled platform and wires up the backend's specific `send` and `send_multi` implementations. Native default backends are preferred to reduce binary size and leverage system-provided certificates:

- **WinHTTP**: The modern standard for Windows networking. Uses `WinHttpSendRequest`.
- **WinINet**: The legacy standard for older Windows environments or specialized proxy/caching setups.    
- **CFNetwork**: Uses `CFNetwork` and `Foundation` types for native execution on macOS and iOS.
- **libcurl**: The robust fallback for POSIX systems (Linux, BSD) leveraging `libcurl`.
- **msh3**: Microsoft's lightweight HTTP/3 client built natively on the fast MsQuic stack.
- **lsquic**: LiteSpeed's high-performance HTTP/3 and QUIC stack.
- **picoquic**: Easy-to-embed, standalone QUIC & HTTP/3 stack via h3zero.
- **nghttp3**: Lightweight HTTP/3 framing state machine backend.
- **aria2**: Highly concurrent downloading utility backend.
- **libuv / libevent**: Modern POSIX backend leveraging `libuv` or `libevent`, ideal for asynchronous Node.js-style environments.
- **libsoup3**: Modern POSIX backend leveraging `libsoup3`, ideal for GTK4 environments.
- **libfetch**: FreeBSD and POSIX backend leveraging `libfetch`, tailored for BSD environments.
- **HttpURLConnection**: Specialized networking integrations for Android JNI environments.
- **Emscripten Fetch API**: The WebAssembly backend leveraging Emscripten's Fetch API to bridge network calls transparently to the browser's native `fetch`.
- **Raw Sockets (`http_raw.c`)**: A fallback manual HTTP protocol implementation utilizing `select`, `read`, and `write` over raw TCP sockets. Ideal for DOS systems or deeply embedded environments that do not provide native HTTP clients but supply fundamental BSD-style sockets (e.g. Watt-32, mTCP).
### 3. Cryptography Integration

To preserve the "Native By Default" philosophy without locking consumers into a heavy OpenSSL dependency, the library dynamically injects cryptography routines natively into the underlying HTTP abstraction via a unified CMake macro (`c_abstract_http_apply_crypto`).
If a backend like `libcurl` or `libevent` is used, its configuration is mapped downwards (e.g., bridging `C_ABSTRACT_HTTP_USE_WOLFSSL` to `CURL_USE_WOLFSSL`). This allows developers to seamlessly pivot between the following crypto libraries without ever modifying their application's code:

- **MbedTLS**
- **OpenSSL**
- **LibreSSL**
- **BoringSSL**
- **wolfSSL**
- **s2n-tls**
- **BearSSL**
- **Schannel** (Native Windows)
- **GnuTLS**
- **Botan**
- **CommonCrypto** (Native macOS/iOS)
- **wincrypt**

### 4. CMake Configuration

CMake configuration drives the abstraction mapping and code generation:

**Core Build Flags**
- `C_ABSTRACT_HTTP_HEADER_ONLY`: Package as a single header (STB-style library).
- `C_ABSTRACT_HTTP_STATIC_CRT`: Use Static CRT (`/MT`, `/MTd`).
- `C_ABSTRACT_HTTP_SHARED_CRT`: Use Shared CRT (`/MD`, `/MDd`).
- `C_ABSTRACT_HTTP_UNICODE`: Use UNICODE `wchar_t` APIs.
- `C_ABSTRACT_HTTP_ANSI`: Use ANSI (Multi-Byte) string APIs.
- `C_ABSTRACT_HTTP_MULTI_THREADED`: Enable multi-threaded safety and locking.
- `C_ABSTRACT_HTTP_SINGLE_THREADED`: Single-threaded build (no locks).
- `C_ABSTRACT_HTTP_LTO`: Enable Link Time Optimization (`-flto` / `/GL`).

**Backend Overrides**
- `C_ABSTRACT_HTTP_USE_LIBSOUP3`: Force libsoup3 over libcurl.
- `C_ABSTRACT_HTTP_USE_LIBUV`: Force libuv over libcurl.
- `C_ABSTRACT_HTTP_USE_LIBEVENT`: Force libevent over libcurl.
- `C_ABSTRACT_HTTP_USE_LIBFETCH`: Force libfetch over libcurl.

**Cryptography Backends**
- `C_ABSTRACT_HTTP_USE_MBEDTLS`
- `C_ABSTRACT_HTTP_USE_OPENSSL`
- `C_ABSTRACT_HTTP_USE_LIBRESSL`
- `C_ABSTRACT_HTTP_USE_BORINGSSL`
- `C_ABSTRACT_HTTP_USE_WOLFSSL`
- `C_ABSTRACT_HTTP_USE_S2N`
- `C_ABSTRACT_HTTP_USE_BEARSSL`
- `C_ABSTRACT_HTTP_USE_SCHANNEL`
- `C_ABSTRACT_HTTP_USE_GNUTLS`
- `C_ABSTRACT_HTTP_USE_BOTAN`
- `C_ABSTRACT_HTTP_USE_COMMONCRYPTO`
- `C_ABSTRACT_HTTP_USE_WINCRYPT`