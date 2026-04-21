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

6. **C++ Interoperability**:
   - Universal extern "C" encapsulation is strictly enforced across all 93 public headers.
   - Allows seamless drop-in usage from C++ codebases without name mangling issues.
7. **Robust Legacy Support (MSVC 2005)**:
   - C89 purity extends to handling modern POSIX headers (<stdint.h>, <stdbool.h>, <unistd.h>).
   - The library employs custom #if defined(_MSC_VER) && _MSC_VER < 1600 guards with safe 	ypedef fallbacks, guaranteeing successful compilation on toolchains as old as MSVC 2005.
8. **Clang-Format Header Safeguards**:
   - Every #include hierarchy is shielded by /* clang-format off */ and /* clang-format on */.
   - Ensures automated formatting never silently breaks delicate header dependency orders.

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

### 2. Target Network Libraries & OS Defaults

The dispatcher layer evaluates the compiled platform and wires up the backend's specific `send` and `send_multi` implementations. **Native default backends are strictly preferred** to reduce binary size, avoid heavy third-party dependencies, and leverage system-provided root certificate stores.

#### Default Backend Selection
When no override flags are provided, CMake automatically configures the following targets based on your OS:
- **Windows (`WIN32`)**: Maps to `http_winhttp.c` / `http_wininet.c` (WinHTTP / WinINet).
- **macOS/iOS (`APPLE`)**: Maps to `http_apple.c` (CFNetwork/Foundation).
- **Android (`ANDROID`)**: Maps to `http_android.c` (JNI HttpURLConnection).
- **WebAssembly (`EMSCRIPTEN`)**: Maps to `http_wasm.c` (Emscripten Fetch API).
- **DOS (`DOS`)**: Maps to `http_raw.c` (Raw BSD sockets via `select`/`read`/`write`).
- **Linux/POSIX (Fallback)**: Maps to `http_curl.c` (libcurl).

#### Build-Time Overrides (`-D` flags)
To support specialized environments (like async event loops or HTTP/3), developers can bypass the OS default by passing `-D` flags to CMake. This triggers `#elif` branches in `transport.c` and changes the source files compiled in `CMakeLists.txt`:
- `-DC_ABSTRACT_HTTP_USE_LIBUV=ON` -> Compiles `http_libuv.c`
- `-DC_ABSTRACT_HTTP_USE_LIBEVENT=ON` -> Compiles `http_libevent.c`
- `-DC_ABSTRACT_HTTP_USE_LSQUIC=ON` -> Compiles `http_lsquic.c`
- `-DC_ABSTRACT_HTTP_USE_PICOQUIC=ON` -> Compiles `http_picoquic.c`
- `-DC_ABSTRACT_HTTP_USE_NGHTTP3=ON` -> Compiles `http_nghttp3.c`
- `-DC_ABSTRACT_HTTP_USE_MSH3=ON` -> Compiles `http_msh3.c`
- `-DC_ABSTRACT_HTTP_USE_ARIA2=ON` -> Compiles `http_aria2.c`
- `-DC_ABSTRACT_HTTP_USE_LIBSOUP3=ON` -> Compiles `http_libsoup3.c`
- `-DC_ABSTRACT_HTTP_USE_LIBFETCH=ON` -> Compiles `http_fetch.c`

### 3. Streaming Modalities (WebSockets & SSE)
The library provides native, zero-dependency implementations for **WebSockets (RFC 6455)** and **Server-Sent Events (SSE)**.
- **WebSocket Framing Engine**: A fully portable C89 state machine parses WebSocket frames chunk-by-chunk. It handles 16-bit and 64-bit payload unmasking dynamically, reconstructs fragmented `FIN=0` messages gracefully, and enforces bounds checking based on `C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE` to prevent DoS attacks.
- **SSE Chunk Buffer**: Uses a dynamic line-reader state machine to capture streaming `text/event-stream` payloads without exploding memory constraints. Supports `Last-Event-ID` tracking for reconnect loops.

### 4. Advanced Protocol Implementations
The library avoids pushing complex HTTP specifications onto the consumer, integrating them securely into the core C89 types layer:
- **OAuth 2.0 Workflows**: First-class memory-safe generation of OAuth 2.0 token requests (Password, Refresh, Authorization Code, Device Authorization, Client Credentials, JWT Bearer) and Token Management (Revocation, Introspection).
- **Multipart Form-Data & Muxing**: Robust, random-boundary multipart/form-data serialization logic using safe CRT string formatting natively within the HttpRequest lifecycle.

### 5. Cryptography Integration

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

### 6. CMake Configuration

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