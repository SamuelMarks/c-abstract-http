c-abstract-http
===============

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Continuous Integration](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml)

A highly robust, cross-platform abstract HTTP network library for C. It unifies Windows (WinHTTP, WinINet), macOS/iOS (CFNetwork/Foundation), Android, FreeBSD (libfetch), and POSIX (cURL) beneath a single abstracted API.

Designed with strict C89 compliance, memory safety, and high portability in mind, this library is suitable for deeply embedded environments, legacy systems, and modern high-performance backends.

## Features

Below is a detailed table summarizing the core capabilities that are currently implemented, alongside our roadmap for future enhancements.

| Feature | Description | Status |
| :--- | :--- | :---: |
| **Cross-Platform API** | Write once, compile anywhere. Automatically targets native platform APIs to reduce binary bloat. | Implemented ✅ |
| **Strict C89 Compliance** | Operates entirely within the C89 standard. No C++ or modern C features are relied upon. | Implemented ✅ |
| **Robust Error Handling** | All non-void functions return `int` statuses. Real values are populated via `out` pointers. | Implemented ✅ |
| **HTTP Verbs** | Full support for standard verbs: `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD`, `OPTIONS`. | Implemented ✅ |
| **Multipart Form-Data** | Build complex payloads with rich boundary generation, file attachments, and headers. | Implemented ✅ |
| **Authentication** | Built-in Base64 encoding for Basic Auth, and direct Bearer Token support. | Implemented ✅ |
| **Streaming Responses** | Optional `http_on_chunk_fn` callbacks for processing response bodies as they stream. | Implemented ✅ |
| **Streaming Uploads** | Optional `http_read_chunk_fn` callbacks for sending large request bodies as a stream. | Implemented ✅ |
| **Cookie Persistence** | Shared `HttpCookieJar` structures for persistent sessions across multiple requests. | Implemented ✅ |
| **Retry & Backoff** | Configurable `HttpRetryPolicy` to automatically handle transient network failures. | Implemented ✅ |
| **Safe CRT Integration** | Extensive support for MSVC Safe C11 extensions (`strcpy_s`, `sprintf_s`, `fopen_s`). | Implemented ✅ |
| **Zero Windows.h Bloat** | Avoids massive `<windows.h>` includes to drastically reduce compile times on Windows. | Implemented ✅ |
| **Extensive CMake Config** | Granular control over linkage (`/MD`, `/MT`), Runtime Checks, LTO, and UNICODE toggles. | Implemented ✅ |
| **Header-Only Mode** | Can be consumed as a single-header STB-style library for easy, localized integration. | Implemented ✅ |
| **Asynchronous API** | Non-blocking event loop integration mimicking Node.js (`uv_run`) across natively asynchronous backends. | Implemented ✅ |
| **Multi-Threading** | Cross-platform thread pool architecture with robust Mutex and CondVar abstraction. | Implemented ✅ |
| **Multi-Processing** | Subprocess dispatching with built-in C89 binary IPC serialization over anonymous pipes. | Implemented ✅ |
| **Greenthreads** | High-performance user-space coroutine contexts via Fibers (Windows) and `ucontext_t` (POSIX). | Implemented ✅ |
| **Message Passing** | Decentralized actor model and pub-sub bus for scalable request routing. | Implemented ✅ |
| **Concurrent Dispatch** | Native `http_client_send_multi` executing concurrent batch downloads via Modality engines. | Implemented ✅ |
| **Framework Integration** | Deep 1:1 execution bridging with `c-multiplatform` event loops, actors, and UI. | Implemented ✅ |
| **WebSockets** | Built-in support for upgrading connections to WebSockets. | Planned 🚧 |
| **HTTP/3 (QUIC)** | First-class abstraction for UDP-based HTTP/3 traffic. | Planned 🚧 |

## Cryptography & TLS Support

To maintain a zero-weight philosophy while remaining adaptable, `c-abstract-http` supports dynamically linking and configuring a massive variety of TLS/Cryptography backends. When enabled via CMake (e.g., `-DC_ABSTRACT_HTTP_USE_MBEDTLS=ON`), the build system will automatically fetch, resolve, and wire the cryptography dependency down into your underlying HTTP implementation (like configuring `libcurl` to compile against it natively).

| Backend | Platform Focus | CMake Option | Description |
| :--- | :--- | :--- | :--- |
| **OpenSSL** | Universal / POSIX | `C_ABSTRACT_HTTP_USE_OPENSSL` | The industry standard TLS backend. |
| **BoringSSL** | Universal / Android | `C_ABSTRACT_HTTP_USE_BORINGSSL` | Google's optimized fork of OpenSSL. |
| **LibreSSL** | Universal / OpenBSD | `C_ABSTRACT_HTTP_USE_LIBRESSL` | OpenBSD's security-focused OpenSSL fork. |
| **mbedTLS** | Embedded / IoT | `C_ABSTRACT_HTTP_USE_MBEDTLS` | Extremely lightweight, highly portable TLS. |
| **wolfSSL** | Embedded / RTOS | `C_ABSTRACT_HTTP_USE_WOLFSSL` | Highly optimized for footprint and speed. |
| **BearSSL** | Microcontrollers | `C_ABSTRACT_HTTP_USE_BEARSSL` | Minimalist footprint with a simple API. |
| **s2n-tls** | AWS Environments | `C_ABSTRACT_HTTP_USE_S2N` | AWS's minimal and secure TLS implementation. |
| **GnuTLS** | Linux / GNU | `C_ABSTRACT_HTTP_USE_GNUTLS` | Primary standard TLS backend in Debian/Ubuntu. |
| **Botan** | Universal / C++ | `C_ABSTRACT_HTTP_USE_BOTAN` | Modern cryptography library with C FFI. |
| **Schannel** | Windows | `C_ABSTRACT_HTTP_USE_SCHANNEL` | OS-native Windows TLS. |
| **CommonCrypto**| macOS / iOS | `C_ABSTRACT_HTTP_USE_COMMONCRYPTO`| OS-native Apple TLS (SecureTransport). |
| **wincrypt** | Windows | `C_ABSTRACT_HTTP_USE_WINCRYPT` | Windows native cryptographic API. |

## OS and Network Library Support

The library is designed to compile out-of-the-box using the optimal native network backend for each platform. This minimizes binary bloat and leverages the host's native TLS/SSL certificate stores.

| Operating System | Network Library | Default on OS | Native TLS/SSL Support | Notes |
| :--- | :--- | :---: | :---: | :--- |
| **Windows** | WinHTTP | **Yes** | Schannel | Primary API for background/system services. |
| **Windows** | WinINet | No | Schannel | Available alternative (often used for UI/IE compatibility). |
| **Windows** | libcurl | No | OpenSSL / Schannel | Supported but requires manual compilation and linking. |
| **macOS / iOS** | CFNetwork / Foundation | **Yes** | SecureTransport | Leverages Apple's native network framework. |
| **macOS / iOS** | libcurl | No | OpenSSL / SecureTransport | Available fallback. |
| **Linux / POSIX** | libsoup3 | No | GnuTLS | Modern fallback often used in GTK4 environments. |
| **Linux / POSIX** | libuv / libevent | No | OpenSSL | Modern asynchronous fallback often used in Node.js environments. |
| **FreeBSD / POSIX** | libfetch | **Yes** | OpenSSL | Native fallback for FreeBSD/BSD systems. |
| **Linux / POSIX** | libcurl | **Yes** | OpenSSL / GnuTLS | Standard backend for most POSIX platforms. |
| **Android** | libcurl (NDK) | **Yes** | BoringSSL / OpenSSL | Default fallback when compiled with Android NDK. |
| **Android** | `HttpURLConnection` | No | Java Provider | Experimental JNI backend natively integrating with Android's Java network stack. |
| **WebAssembly** | Emscripten Fetch API | **Yes** | Browser / Host | Natively bridges to JS `fetch` for WASM. |

## Documentation Overview

To help you get the most out of `c-abstract-http`, we provide the following detailed guides:

- **[USAGE.md](USAGE.md):** Practical examples, snippets, and guides on how to make GET/POST requests, handle headers, and manage cookies.
- **[ARCHITECTURE.md](ARCHITECTURE.md):** Deep dive into the library's design philosophy, transport abstraction, and error handling patterns.
- **[skills.md](skills.md):** Specific development guidelines, formatting rules, and procedures for contributors (and AI agents) working on the codebase.
- **[llm.txt](llm.txt):** A prompt-friendly context file containing strict rules for AI assistants operating on this repository.

## Installation & Building

This project is fully compatible with CMake `FetchContent`.

```cmake
include(FetchContent)
FetchContent_Declare(
    c-abstract-http
    GIT_REPOSITORY https://github.com/SamuelMarks/c-abstract-http.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(c-abstract-http)

# Link to your target
target_link_libraries(my_app PRIVATE c-abstract-http)
```

### Advanced CMake Options

Configure your build precisely by passing these flags to CMake:

```bash
cmake -B build -S . \
  -DC_ABSTRACT_HTTP_HEADER_ONLY=OFF \
  -DC_ABSTRACT_HTTP_STATIC_CRT=ON \
  -DC_ABSTRACT_HTTP_MULTI_THREADED=ON \
  -DC_ABSTRACT_HTTP_UNICODE=ON \
  -DC_ABSTRACT_HTTP_LTO=ON
```

## Header-only Usage

If you prefer to include `c-abstract-http` as a header-only library with zero compile-time overhead (e.g., to generate a single fat executable), you can define `C_ABSTRACT_HTTP_HEADER_ONLY=ON` when configuring CMake, and then `#define C_ABSTRACT_HTTP_IMPLEMENTATION` in exactly one `.c` file before including `c_abstract_http.h`:

```c
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>

int main() {
    /* Your code here */
    return 0;
}
```

## Platform Support

Verified to work flawlessly across:
- **MSVC 2005**, **MSVC 2022**, **MSVC 2026**
- **MinGW** & **Cygwin**
- **macOS / iOS** (Clang / Apple Frameworks)
- **Linux / POSIX** (GCC / Clang + libcurl / libsoup3 / libuv / libevent / libfetch)
- **FreeBSD** (Clang + libfetch)
- **Android** (NDK)
- **WebAssembly** (Emscripten)

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
