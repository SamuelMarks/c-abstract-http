c-abstract-http
===============

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Continuous Integration](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml)
[![Test Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen.svg)](#)
[![Doc Coverage](https://img.shields.io/badge/docs-100%25-brightgreen.svg)](#)

A zero-GUI, cross-platform, strict ISO C89 (ANSI C / C90) abstract networking and HTTP client library. `c-abstract-http` provides a unified Abstract Network Interface (ANI) over native OS networking stacks, asynchronous event loops, and modern HTTP/3 QUIC engines, with **6** implemented HTTP/3-capable C transports (`libcurl`, `lsquic`, `msh3`, `nghttp3`, `picoquic`, `xquic`) alongside native OS (`WinHTTP`, `WinINet`, Apple `NSURLSession`/`CFNetwork`), evented (`libevent`, `libsoup3`, `libuv`, `libfetch`), WebAssembly (`Emscripten Fetch`), and raw-socket backends.

## Key Features

- **Strict C89 Portability**: Compiles cleanly on MSVC 2005 through 2026, GCC, Clang, MinGW, Cygwin, Emscripten, and OpenWatcom (MS-DOS) using C89 `int` booleans (zero `<stdbool.h>`, zero `<windows.h>`), guarded POSIX/C99 header shims (`<stdint.h>`, `<unistd.h>`), MSVC Safe CRT (`sprintf_s`, `strcpy_s`), and portable format macros (`SIZE_T_FMT`, `C_ABSTRACT_HTTP_NUM_FORMAT`).
- **Seamless C++ Interop**: Universal `extern "C"` guards across all **30** public headers in `include/c_abstract_http/`.
- **Strict Enum Error Percolation**: Every non-math, non-void public and internal function is annotated with `NO_DISCARD` (`warn_unused_result` / `_Check_return_`) and returns `c_abstract_http_error_t`, passing output values via pointer parameters.
- **Native Execution Modalities**: Supports synchronous blocking, asynchronous event-loop (`select` / `epoll` / `kqueue` / IOCP fallback), thread-pool, cooperative greenthread/coroutine (`ucontext` / Win32 Fibers), publish/subscribe actor message-bus, and child-process IPC modalities, plus optional `c-multiplatform` (`cmp`) integration.
- **Built-in WebSockets & SSE**: C89 state-machine streaming for RFC 6455 WebSockets (`http_ws.h`, with standalone C89 SHA-1/Base64 handshake signing in `crypto_utils.c`) and W3C Server-Sent Events (`http_sse.h`), supporting both synchronous blocking read loops and asynchronous non-blocking chunk dispatch.
- **STB-Style Single-Header Mode**: Optional `INTERFACE` target (`-DC_ABSTRACT_HTTP_HEADER_ONLY=ON`) supporting `#define C_ABSTRACT_HTTP_IMPLEMENTATION` before `#include <c_abstract_http/c_abstract_http.h>`.
- **First-Party C89 Dependencies**: Automatically resolves [`c89stringutils`](https://github.com/offscale/c89stringutils) and [`c-fs`](https://github.com/SamuelMarks/c-fs) (`master`) via CMake `FetchContent_MakeAvailable` when not pre-installed.

## Supported Platforms & Transports

### Target Platforms
- **Windows**: Windows XP SP2+ through Windows 11 / Server 2025 (x86, x64, ARM64; MSVC 2005–2026, MinGW, Cygwin)
- **macOS & iOS**: macOS 10.10+ and iOS (Intel & Apple Silicon)
- **Linux & BSD**: glibc, musl (Alpine), FreeBSD, NetBSD, OpenBSD
- **WebAssembly**: Emscripten (`wasm32`)
- **Retro / Embedded**: MS-DOS (OpenWatcom) and minimal POSIX environments

### Default OS / Platform Transports
When no explicit `-DC_ABSTRACT_HTTP_USE_*` backend override is set, CMake selects the native platform transport:
- **Windows (`WIN32`)**: `WinHTTP` (`http_winhttp.c`) by default; `WinINet` (`http_wininet.c`) on legacy MSVC (`_MSC_VER < 1600`) or when `-DC_ABSTRACT_HTTP_USE_WININET=ON`.
- **macOS & iOS (`APPLE` with Clang)**: Apple `CFNetwork` / modern `NSURLSession` pure-C Objective-C runtime bridge (`http_apple.c`; `NSURLSession` auto-enabled on `MACOSX_DEPLOYMENT_TARGET >= 10.11` or via `-DC_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION=ON`).
- **WebAssembly (`EMSCRIPTEN`)**: Emscripten Fetch API (`http_wasm.c`).
- **MS-DOS (`CMAKE_SYSTEM_NAME=DOS`)**: Raw TCP HTTP/1.1 client (`http_raw.c`, with optional Watt-32 TCP/IP via `-DC_ABSTRACT_HTTP_DOS_WATT32=ON`).
- **Linux, BSD, POSIX (and Android NDK)**: `libcurl` (`http_curl.c`).

### Explicit Transport Overrides
Any platform default can be overridden at CMake configure time:
- `-DC_ABSTRACT_HTTP_USE_MSH3=ON` — Microsoft `msh3` (`msquic` HTTP/3) (`http_msh3.c`)
- `-DC_ABSTRACT_HTTP_USE_PICOQUIC=ON` — Private Octopus `picoquic` HTTP/3 (`http_picoquic.c`)
- `-DC_ABSTRACT_HTTP_USE_XQUIC=ON` — Alibaba `xquic` HTTP/3 (`http_xquic.c`)
- `-DC_ABSTRACT_HTTP_USE_LSQUIC=ON` — LiteSpeed `lsquic` HTTP/3 (`http_lsquic.c`)
- `-DC_ABSTRACT_HTTP_USE_NGHTTP3=ON` — `nghttp3` + `ngtcp2` HTTP/3 (`http_nghttp3.c`)
- `-DC_ABSTRACT_HTTP_USE_LIBUV=ON` — `libuv` async TCP/TLS HTTP client (`http_libuv.c`)
- `-DC_ABSTRACT_HTTP_USE_LIBEVENT=ON` — `libevent` (`evhttp`) client (`http_libevent.c`)
- `-DC_ABSTRACT_HTTP_USE_LIBSOUP3=ON` — GNOME `libsoup-3.0` HTTP/2 & HTTP/3 client (`http_libsoup3.c`)
- `-DC_ABSTRACT_HTTP_USE_LIBFETCH=ON` — FreeBSD `libfetch` client (`http_fetch.c`)
- `-DC_ABSTRACT_HTTP_USE_RAW_SOCKETS=ON` — Raw BSD/Winsock HTTP/1.1 backend with pluggable & built-in TLS hooks (`http_raw.c`)
- `-DC_ABSTRACT_HTTP_USE_WININET=ON` — Windows `WinINet` transport (`http_wininet.c`)
- `-DC_ABSTRACT_HTTP_DOS_WATT32=ON` — Enable Watt-32 (`<tcp.h>`) BSD sockets on MS-DOS / FreeDOS (`http_raw.c`)

### Cryptography & TLS Linkage Options
Optional `-DC_ABSTRACT_HTTP_USE_<CRYPTO>=ON` flags resolve and link a specific TLS/cryptography stack (mapping `OPENSSL`/`BORINGSSL`/`LIBRESSL`, `MBEDTLS`, `WOLFSSL`, `BEARSSL`, `GNUTLS`, `SCHANNEL`, and `COMMONCRYPTO` downward into `libcurl` when building `curl` from source, and providing built-in raw-socket TLS hooks for `OPENSSL`/`BORINGSSL`/`LIBRESSL`/`WOLFSSL`, `MBEDTLS`, and `GNUTLS`):
- **OpenSSL / Forks**: `OPENSSL`, `BORINGSSL`, `LIBRESSL`
- **Lightweight / Embedded**: `MBEDTLS`, `WOLFSSL`, `BEARSSL`
- **Cloud / Modern**: `S2N`, `GNUTLS`, `BOTAN`
- **OS Native**: `SCHANNEL`, `WINCRYPT` (Windows), `COMMONCRYPTO` (Apple)

## Core Capabilities

- **Streaming Modalities**: WebSockets (`http_ws.h`) and Server-Sent Events (`http_sse.h`) with configurable frame and line limits (`C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE`, `C_ABSTRACT_HTTP_SSE_MAX_LINE_SIZE`) and LibFuzzer harnesses (`fuzz_ws`, `fuzz_sse`).
- **Multipart Form-Data**: RFC 7578 `multipart/form-data` builder (`http_request_add_part`, `http_request_add_part_header_last`, `http_request_flatten_parts`).
- **Authentication & OAuth2**: Helpers for Bearer tokens, Basic auth, OAuth2 Client Credentials, Password, Refresh Token, and Authorization Code grants (`http_types.h`).
- **Resilience & Security**: Configurable connect/read timeouts, retry policies, cookie jars (`HttpCookieJar`), HTTP/SOCKS proxy configuration, TLS version pinning (`HttpTlsVersion`), custom CA bundles, and certificate pinning.

## Building & Testing

### CMake Quickstart

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### Selecting an Explicit Backend (e.g., `msh3` HTTP/3)

```bash
cmake -S . -B build -DC_ABSTRACT_HTTP_USE_MSH3=ON
cmake --build build
```

### HTTP/3 Backend Matrix

| Engine | Sponsor / Author | Language | License | Implemented | CMake Flag |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`libcurl`** | Daniel Stenberg | C (C89) | curl (MIT-like) | Yes | *(Default on Linux/POSIX)* |
| **`msh3`** | Microsoft (`nibanks`) | C | MIT | Yes | `-DC_ABSTRACT_HTTP_USE_MSH3=ON` |
| **`picoquic`** | Christian Huitema | C | BSD-3-Clause | Yes | `-DC_ABSTRACT_HTTP_USE_PICOQUIC=ON` |
| **`xquic`** | Alibaba | C | Apache-2.0 | Yes | `-DC_ABSTRACT_HTTP_USE_XQUIC=ON` |
| **`lsquic`** | LiteSpeed Tech | C | MIT | Yes | `-DC_ABSTRACT_HTTP_USE_LSQUIC=ON` |
| **`nghttp3`** | Tatsuhiro Tsujikawa | C | MIT | Yes | `-DC_ABSTRACT_HTTP_USE_NGHTTP3=ON` |

### CMake Configuration Options

| Option | Default | Description |
| :--- | :--- | :--- |
| **Transport Backends** | | |
| `C_ABSTRACT_HTTP_USE_MSH3` | `OFF` | Use Microsoft `msh3` (`msquic`) HTTP/3 backend |
| `C_ABSTRACT_HTTP_USE_PICOQUIC` | `OFF` | Use `picoquic` HTTP/3 backend |
| `C_ABSTRACT_HTTP_USE_XQUIC` | `OFF` | Use Alibaba `xquic` HTTP/3 backend |
| `C_ABSTRACT_HTTP_USE_LSQUIC` | `OFF` | Use LiteSpeed `lsquic` HTTP/3 backend |
| `C_ABSTRACT_HTTP_USE_NGHTTP3` | `OFF` | Use `nghttp3` + `ngtcp2` HTTP/3 backend |
| `C_ABSTRACT_HTTP_USE_LIBSOUP3` | `OFF` | Use GNOME `libsoup-3.0` backend |
| `C_ABSTRACT_HTTP_USE_LIBUV` | `OFF` | Use `libuv` backend |
| `C_ABSTRACT_HTTP_USE_LIBEVENT` | `OFF` | Use `libevent` backend |
| `C_ABSTRACT_HTTP_USE_LIBFETCH` | `OFF` | Use BSD `libfetch` backend |
| `C_ABSTRACT_HTTP_USE_RAW_SOCKETS` | `OFF` | Use raw BSD/Winsock HTTP/1.1 TCP/TLS backend |
| `C_ABSTRACT_HTTP_USE_WININET` | `OFF` | Use Windows `WinINet` backend (auto-enabled on MSVC < 2010) |
| `C_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION` | `OFF` | Force Apple `NSURLSession` C-bridge (`http_apple.c`) |
| `C_ABSTRACT_HTTP_DOS_WATT32` | `OFF` | Enable Watt-32 TCP/IP stack for MS-DOS builds |
| **Streaming & Integration** | | |
| `C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS` | `ON` | Build RFC 6455 WebSocket layer (`http_ws.c`, `crypto_utils.c`) |
| `C_ABSTRACT_HTTP_ENABLE_SSE` | `ON` | Build Server-Sent Events layer (`http_sse.c`) |
| `C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE` | `1048576` | Maximum WebSocket frame payload size in bytes |
| `C_ABSTRACT_HTTP_SSE_MAX_LINE_SIZE` | `32768` | Maximum SSE line buffer length in bytes |
| `C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION` | `ON` | Build `c-multiplatform` modality adapter (`cmp_integration.c`) |
| **Toolchain, Runtime & Packaging** | | |
| `C_ABSTRACT_HTTP_HEADER_ONLY` | `OFF` | Expose `c-abstract-http` as an `INTERFACE` single-header target |
| `BUILD_SHARED_LIBS` | `OFF` | Build `c-abstract-http` as a shared library (`.dll` / `.so` / `.dylib`) |
| `C_ABSTRACT_HTTP_CRT_LINKAGE` | `Shared` | MSVC CRT linkage (`Shared` `/MD` or `Static` `/MT`) |
| `C_ABSTRACT_HTTP_CHARSET` | `Unicode` | Windows character set (`Unicode` or `ANSI`) |
| `C_ABSTRACT_HTTP_THREADING` | `Multi-threaded` | Concurrency mode (`Multi-threaded` or `Single-threaded`) |
| `C_ABSTRACT_HTTP_LTO` | `OFF` | Enable Link-Time / Interprocedural Optimization |
| `C_ABSTRACT_HTTP_MSVC_RTC` | `""` | MSVC runtime checks (`""`, `RTC1`, `RTCs`, `RTCu`) |
| `TRY_SYS_LIB` | `ON` | Try `find_package` before falling back to `FetchContent` |
| **Cryptography Linkage** | | |
| `C_ABSTRACT_HTTP_USE_OPENSSL` | `OFF` | Link OpenSSL (enables `CURL_USE_OPENSSL` and raw/libevent/libuv TLS) |
| `C_ABSTRACT_HTTP_USE_MBEDTLS` | `OFF` | Link MbedTLS (enables `CURL_USE_MBEDTLS` and raw/libuv TLS) |
| `C_ABSTRACT_HTTP_USE_WOLFSSL` | `OFF` | Link wolfSSL (enables `CURL_USE_WOLFSSL` and raw TLS) |
| `C_ABSTRACT_HTTP_USE_BORINGSSL` | `OFF` | Link BoringSSL (enables `CURL_USE_OPENSSL` and raw/libevent/libuv TLS) |
| `C_ABSTRACT_HTTP_USE_LIBRESSL` | `OFF` | Link LibreSSL (enables `CURL_USE_OPENSSL` and raw/libevent/libuv TLS) |
| `C_ABSTRACT_HTTP_USE_S2N` | `OFF` | Link AWS `s2n-tls` |
| `C_ABSTRACT_HTTP_USE_BEARSSL` | `OFF` | Link BearSSL (and enable `CURL_USE_BEARSSL` when building curl) |
| `C_ABSTRACT_HTTP_USE_GNUTLS` | `OFF` | Link GnuTLS (enables `CURL_USE_GNUTLS` and raw TLS) |
| `C_ABSTRACT_HTTP_USE_BOTAN` | `OFF` | Link Botan |
| `C_ABSTRACT_HTTP_USE_SCHANNEL` | `OFF` | Use Windows Schannel (`CURL_USE_SCHANNEL`) |
| `C_ABSTRACT_HTTP_USE_COMMONCRYPTO` | `OFF` | Link Apple Security / SecureTransport (`CURL_USE_SECTRANSP`) |
| `C_ABSTRACT_HTTP_USE_WINCRYPT` | `OFF` | Link Windows `crypt32` |
| **Testing, Coverage & Sanitizers** | | |
| `BUILD_TESTING_c-abstract-http` | `ON` *(top-level)* | Build `greatest.h` unit, integration, and OOM test suites |
| `C_ABSTRACT_HTTP_ENABLE_COVERAGE` | `OFF` | Enable `--coverage` instrumentation (`gcov` / `gcovr`) |
| `C_ABSTRACT_HTTP_ENABLE_FUZZING` | `OFF` | Build LibFuzzer targets (`fuzz_ws`, `fuzz_sse`) |
| `C_ABSTRACT_HTTP_USE_ASAN` | `OFF` | Enable AddressSanitizer (`-fsanitize=address`) on GCC/Clang |
| `C_ABSTRACT_HTTP_USE_TSAN` | `OFF` | Enable ThreadSanitizer (`-fsanitize=thread`) on GCC/Clang |

## Documentation

- [**ARCHITECTURE.md**](./ARCHITECTURE.md) — Abstract Network Interface (ANI), error model, execution modalities, and CMake target architecture.
- [**USAGE.md**](./USAGE.md) — Compilable C89 examples covering lifecycle, OAuth2, streaming, WebSockets, SSE, multipart uploads, and concurrency.
- [**skills.md**](./skills.md) — Contributor & autonomous agent SOPs for strict C89 compliance, error percolation, OOM fault injection, and adding transports.

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
