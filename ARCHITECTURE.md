# Architecture

`c-abstract-http` provides a unified, strict ISO C89 (ANSI C / C90) client networking interface across native OS HTTP stacks, evented C libraries, WebAssembly, HTTP/3 QUIC engines, and raw sockets.

## 1. Core Principles

- **Strict Enum Error Percolation**: Every non-void, non-math function across all **30** public headers in `include/c_abstract_http/` and all internal translation units in `src/` returns `c_abstract_http_error_t` and is annotated with `NO_DISCARD` (`__attribute__((warn_unused_result))` on GCC/Clang, `_Check_return_` on MSVC >= 2012, `[[nodiscard]]` on C23/C++17). Out-parameters are passed by pointer (`C_ABSTRACT_HTTP_SUCCESS = 0`, `C_ABSTRACT_HTTP_ERR_INVAL`, `C_ABSTRACT_HTTP_ERR_NOMEM`, `C_ABSTRACT_HTTP_ERR_IO`, `C_ABSTRACT_HTTP_ERR_NOTSUP`, `C_ABSTRACT_HTTP_ERR_TIMEOUT`, `C_ABSTRACT_HTTP_ERR_SYSCALL`, `C_ABSTRACT_HTTP_ERR_WS_FRAMING`, `C_ABSTRACT_HTTP_ERR_WS_HANDSHAKE`, `C_ABSTRACT_HTTP_ERR_SSE_PARSE`, `C_ABSTRACT_HTTP_ERR_SSE_OOM`, `C_ABSTRACT_HTTP_ERR_WS_CLOSED`, `C_ABSTRACT_HTTP_ERR_ACCES`, `C_ABSTRACT_HTTP_ERR_EXIST`, `C_ABSTRACT_HTTP_ERR_LOOP`, `C_ABSTRACT_HTTP_ERR_MSGSIZE`, `C_ABSTRACT_HTTP_ERR_CONNREFUSED`, `C_ABSTRACT_HTTP_ERR_HOSTUNREACH`, `C_ABSTRACT_HTTP_ERR_CANCELED`).
- **Opaque Contexts & Explicit Vtables**: `struct HttpTransportContext` is opaque to callers. `struct HttpClient` exposes explicit function pointers (`http_send_fn send`, `http_send_multi_fn send_multi`) bound by `transport_factory_init_client()`.
- **Zero Global Application State**: Aside from one-time library/Winsock bootstrap (`transport_global_init()` / `transport_global_cleanup()`), state lives inside `struct HttpClient`, `struct ModalityEventLoop`, `struct AbstractHttpThreadPool`, `struct HttpRequest`, and `struct HttpResponse`.
- **ISO C89 & Legacy Toolchain Discipline**:
  - Zero `//` comments; all declarations appear at the top of blocks.
  - Zero `#include <stdbool.h>` (uses `int` with `_Bool=int` guard where needed) and zero `#include <windows.h>` (uses targeted `<winsock2.h>`, `<ws2tcpip.h>`, or explicit Win32 API prototypes plus `win_compat_sym.h` architecture guards).
  - Guarded `<stdint.h>` and `<unistd.h>` includes with `typedef` fallbacks for MSVC 2005 (`_MSC_VER < 1600`).
  - MSVC Safe CRT paths (`sprintf_s`, `strcpy_s`, `strncpy_s`, `_vsnprintf_s`) guarded by `#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)` alongside portable `c89stringutils` and `c-fs` (`cfs`) helpers.
  - Portable format specifiers: `"%" SIZE_T_FMT` (`win_compat_sym.h`) for `size_t` and `C_ABSTRACT_HTTP_NUM_FORMAT` (`http_types.h`) for 64-bit signed integers (`abstract_http_int64_t`).
  - Single `extern "C"` guard per header and single `/* clang-format off */` / `/* clang-format on */` block protecting `#include` ordering per file.

## 2. Layered Component Architecture

### Layer 1: Abstract Network Interface (`http_types.h`, `transport.h`)
Defines transport-agnostic value and handle types:
- `struct HttpClient`, `struct HttpConfig`, `struct HttpRequest`, `struct HttpResponse`, `struct HttpHeaders`, `struct HttpCookieJar`, `struct HttpPart` / `struct HttpParts`, `struct HttpMultiRequest`, and `struct HttpFuture`.
- Unified client request pipeline (`http_client_send` and `http_client_send_multi`) applying `base_url` resolution, RFC 7578 `multipart/form-data` flattening (`http_request_add_part`, `http_request_flatten_parts`), RFC 6265 `HttpCookieJar` injection/parsing, and configurable retry/backoff policies (`Retry-After`), alongside URL encoding and OAuth2 grant request builders (`src/http_types.c`).
- Global and per-client backend lifecycle dispatch (`src/transport.c`).

### Layer 2: Execution & Concurrency Modalities
- **Asynchronous Event Loop (`event_loop.h` / `src/event_loop.c`)**: Cross-platform `ModalityEventLoop` providing scalable socket readiness polling (`epoll` on Linux, `kqueue` on macOS/BSD, `WSAPoll`/IOCP on Windows, `select` fallback), a min-heap timer queue, thread-safe wakeup signaling, and `struct HttpLoopHooks` (`http_loop_init_external`) to proxy into external host loops.
- **Thread Pool & Synchronization (`thread_pool.h` / `src/thread_pool.c`)**: `AbstractHttpThreadPool`, `AbstractHttpMutex`, and `AbstractHttpCond` backed by pthreads on POSIX and Win32 threads / `CRITICAL_SECTION` / Semaphore+Event condition variables (Windows XP / MSVC 2005 compatible) on Windows, with no-op fallbacks when `C_ABSTRACT_HTTP_SINGLE_THREADED` is active.
- **Thread-Local Storage (`c_abstract_http_tls.h` / `src/c_abstract_http_tls.c`)**: Cross-platform thread-local key management (`pthread_key_t` / `TlsAlloc`) — note this module provides **Thread-Local Storage**, not network transport encryption.
- **Coroutines (`coroutine.h` / `src/coroutine.c`)**: Cooperative stackful coroutines using `ucontext` on POSIX (with thread or `setjmp`/`longjmp` fallback where unavailable) and Win32 Fibers (`CreateFiber` / `SwitchToFiber`) on Windows.
- **Actor Bus (`actor.h` / `src/actor.c`)**: Lightweight publish/subscribe message bus and actor mailbox dispatch.
- **Multiprocessing IPC (`process.h` / `src/process.c`)**: Child process spawning (`fork`/`execv` on POSIX, `CreateProcess` on Windows) and framed pipe serialization for out-of-process HTTP execution (`MODALITY_MULTIPROCESS`).
- **`c-multiplatform` Adapter (`cmp_integration.h` / `src/cmp_integration.c`)**: Maps `cmp_modality_t` constants and progress callbacks onto `c-abstract-http` execution primitives when `C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION=ON`.

### Layer 3: Streaming Protocols (`http_ws.h`, `http_sse.h`)
- **WebSockets (`src/http_ws.c`, `src/ws_internal.h`, `src/crypto_utils.c`)**: Incremental RFC 6455 frame parser/serializer, CSPRNG client masking (`c_abstract_http_random_bytes`), control-frame handling (Ping/Pong/Close), persistent upgraded socket dispatch, synchronous read loop, and asynchronous `on_chunk` registration. Uses standalone C89 SHA-1, SHA-256, Base64, SPKI pinning, and constant-time comparison routines in `src/crypto_utils.c`.
- **Server-Sent Events (`src/http_sse.c`, `src/sse_internal.h`)**: Incremental WHATWG SSE line/event state machine supporting `id:`, `event:`, multi-line `data:`, `retry:`, BOM stripping, embedded-NUL `id` filtering, and automatic `Last-Event-ID` reconnection.

### Layer 4: Transport Backends
Explicit `-DC_ABSTRACT_HTTP_USE_*` overrides take precedence over OS defaults in `CMakeLists.txt`, `src/transport.c`, and `include/c_abstract_http/c_abstract_http.h`:

1. **Explicit Overrides**:
   - `C_ABSTRACT_HTTP_USE_PICOQUIC` -> `src/http_picoquic.c` (`picoquic` + `picotls`/MbedTLS HTTP/3)
   - `C_ABSTRACT_HTTP_USE_MSH3` -> `src/http_msh3.c` (Microsoft `msh3` / `msquic` HTTP/3)
   - `C_ABSTRACT_HTTP_USE_XQUIC` -> `src/http_xquic.c` (Alibaba `xquic` HTTP/3)
   - `C_ABSTRACT_HTTP_USE_LSQUIC` -> `src/http_lsquic.c` (LiteSpeed `lsquic` HTTP/3)
   - `C_ABSTRACT_HTTP_USE_NGHTTP3` -> `src/http_nghttp3.c` (`nghttp3` + `ngtcp2` HTTP/3)
   - `C_ABSTRACT_HTTP_USE_LIBSOUP3` -> `src/http_libsoup3.c` (GNOME `libsoup-3.0`)
   - `C_ABSTRACT_HTTP_USE_LIBUV` -> `src/http_libuv.c` (`libuv` async TCP/TLS HTTP)
   - `C_ABSTRACT_HTTP_USE_LIBEVENT` -> `src/http_libevent.c` (`libevent` `evhttp`; links `event_openssl` when `OPENSSL`/`LIBRESSL`/`BORINGSSL` is enabled, otherwise builds with `EVENT__DISABLE_OPENSSL=ON`)
   - `C_ABSTRACT_HTTP_USE_LIBFETCH` -> `src/http_fetch.c` (BSD `libfetch`)
   - `C_ABSTRACT_HTTP_USE_RAW_SOCKETS` -> `src/http_raw.c` (raw BSD/Winsock HTTP/1.1 with `struct HttpRawTlsHooks` and built-in `OpenSSL`/`wolfSSL`/`LibreSSL`/`BoringSSL`, `mbedTLS`, and `GnuTLS` TLS socket hooks; always compiled into the core library so `http_raw_*` APIs remain directly callable as a fallback)
   - `C_ABSTRACT_HTTP_USE_WININET` -> `src/http_wininet.c` (Windows `WinINet`)
2. **Default OS Fallbacks**:
   - **Emscripten (`__EMSCRIPTEN__`)**: `src/http_wasm.c` (`-s FETCH=1`)
   - **Windows (`WIN32`)**: `src/http_wininet.c` on `_MSC_VER < 1600`, otherwise `src/http_winhttp.c` (`WinHTTP`)
   - **Apple (`APPLE` with Clang/AppleClang)**: `src/http_apple.c` (`NSURLSession` pure-C `objc_msgSend` runtime bridge on macOS 10.9+ or `C_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION=ON`, with `CFNetwork` `CFHTTPMessage`/`CFReadStream` fallback)
   - **MS-DOS (`CMAKE_SYSTEM_NAME STREQUAL "DOS"`)**: `src/http_raw.c` (with optional Watt-32 `<tcp.h>` TCP/IP support via `C_ABSTRACT_HTTP_DOS_WATT32=ON`)
   - **POSIX / Linux / BSD Default**: `src/http_curl.c` (`libcurl` easy + multi interfaces)

## 3. Build, Amalgamation & Test Instrumentation Architecture

- **Dependency Resolution (`c_abstract_http_resolve_dependency`)**: Checks `find_package(..., QUIET)` when `TRY_SYS_LIB=ON` and falls back to CMake `FetchContent_MakeAvailable` (using `master` for `SamuelMarks/*` repositories such as `c-fs` and `greatest`).
- **Cryptography Flag Mapping**: `c_abstract_http_apply_crypto()` resolves and links the requested TLS/crypto library (`OPENSSL`, `MBEDTLS`, `WOLFSSL`, `BORINGSSL`, `LIBRESSL`, `S2N`, `BEARSSL`, `GNUTLS`, `BOTAN`, `SCHANNEL`, `COMMONCRYPTO`, `WINCRYPT`), while `c_abstract_http_resolve_curl()` maps `OPENSSL`/`BORINGSSL`/`LIBRESSL`, `MBEDTLS`, `WOLFSSL`, `BEARSSL`, `GNUTLS`, `SCHANNEL`, and `COMMONCRYPTO` onto `CURL_USE_*` cache variables when `libcurl` is built from source.
- **Single-Header Mode (`C_ABSTRACT_HTTP_HEADER_ONLY=ON`)**: Configures `c-abstract-http` as a CMake `INTERFACE` library. Defining `C_ABSTRACT_HTTP_IMPLEMENTATION` in one C translation unit before `#include <c_abstract_http/c_abstract_http.h>` compiles the core modules, optional WS/SSE layers, and the selected transport backend together.
- **Isolated OOM & Syscall Fault Injection (`c-abstract-http-instrumented`)**:
  - Production `c-abstract-http` (`STATIC`, `SHARED`, or `INTERFACE`) never leaks test allocator or socket macro redefinitions to downstream consumers.
  - When `BUILD_TESTING=ON` (non-header-only, non-Win32-DLL), `CMakeLists.txt` builds a separate `c-abstract-http-instrumented` static target with `PRIVATE` `C_ABSTRACT_HTTP_TEST_OOM=1` and `c_abstract_http_mock_*` symbol overrides (`tests/mock_alloc.c`) plus header-only backend mock stubs (`tests/mock_stubs/*.h`) to achieve 100% line coverage across `src/`.
