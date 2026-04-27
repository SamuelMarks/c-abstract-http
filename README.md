c-abstract-http
===============

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Continuous Integration](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml)
[![Test Coverage](https://img.shields.io/badge/coverage-81%25-brightgreen.svg)](#)
[![Doc Coverage](https://img.shields.io/badge/docs-100%25-brightgreen.svg)](#)

A highly robust, cross-platform abstract HTTP network library for C. It unifies various platform-specific network and crypto libraries under a single strict C89 API. Designed with strict C89 compliance, memory safety, and high portability in mind, this library is suitable for deeply embedded environments, legacy systems, and modern high-performance backends. It now supports massive scale HTTP/3 integration spanning 4 native cross-platform engines.

## Key Capabilities

- **Strict C89 Compliance & Legacy MSVC Support**: 100% C89/C90 compliant with custom POSIX header shims (<stdint.h>, <stdbool.h>, <unistd.h>) enabling flawless compilation on MSVC 2005 (_MSC_VER < 1600).
- **C++ Interop**: Universal extern "C" support on all 93 public headers.
- **Clang-Format Safety**: Robust /* clang-format off/on */ integration safeguarding #include block ordering.
- **Advanced OAuth2 Support**: Extensive token grant workflows (Password, Refresh, Authorization Code, Device, Client Credentials, JWT Bearer, Token Revocation, and Introspection).
- **Multipart Form-Data & Muxing**: Robust, random-boundary multipart/form-data serialization.
- **WebSockets & SSE**: Zero-dependency, C89 state-machine based streaming modalities.
- **Modern CMake Integration**: Powered exclusively by FetchContent_MakeAvailable for clean, warning-free dependency ingestion.

## Target Platforms

- **Windows**: MSVC (2005, 2022, 2026), MinGW, Cygwin
- **macOS / iOS**: Clang / Apple Frameworks
- **Linux / POSIX**: GCC / Clang
- **FreeBSD**: Clang
- **Android**: NDK
- **WebAssembly**: Emscripten
- **DOS**: OpenWatcom (with fallback support for Watt-32 / mTCP)

## Target Network Libraries

The library automatically selects the most optimal, native network backend for your target operating system by default. This minimizes dependencies and maximizes performance.

### Default OS/Platform Targets
- **Windows**: Defaults to **WinHTTP** / **WinINet** (Native Windows APIs).
- **macOS / iOS**: Defaults to **CFNetwork / Foundation** (Native Apple Frameworks).
- **Android**: Defaults to **HttpURLConnection** (Native Java bridge via JNI).
- **WebAssembly (Wasm)**: Defaults to **Emscripten Fetch API** (Native browser `fetch`).
- **DOS**: Defaults to **Raw Sockets** (Manual TCP fallback).
- **Linux / POSIX**: Defaults to **libcurl** (Industry standard for POSIX).

### Overriding Defaults via CMake (`-D` flags)
You can override the default native backend on any platform by passing specific `-D` flags to CMake during configuration:

- **`-DC_ABSTRACT_HTTP_USE_MSH3=ON`**: Microsoft's lightweight HTTP/3 client built natively on MsQuic.
- **`-DC_ABSTRACT_HTTP_USE_LSQUIC=ON`**: LiteSpeed's high-performance HTTP/3 and QUIC stack.
- **`-DC_ABSTRACT_HTTP_USE_PICOQUIC=ON`**: Easy-to-embed, standalone QUIC & HTTP/3 stack via h3zero.
- **`-DC_ABSTRACT_HTTP_USE_NGHTTP3=ON`**: Lightweight HTTP/3 framing state machine backend.
- **`-DC_ABSTRACT_HTTP_USE_ARIA2=ON`**: Highly concurrent downloading utility backend.
- **`-DC_ABSTRACT_HTTP_USE_LIBSOUP3=ON`**: Modern GTK4-friendly POSIX backend.
- **`-DC_ABSTRACT_HTTP_USE_LIBUV=ON`**: Node.js-style asynchronous I/O backend.
- **`-DC_ABSTRACT_HTTP_USE_LIBEVENT=ON`**: Fast event notification backend.
- **`-DC_ABSTRACT_HTTP_USE_LIBFETCH=ON`**: Default for FreeBSD/BSD systems.

## Target Crypto Libraries

To maintain a zero-weight philosophy while remaining adaptable, `c-abstract-http` supports dynamically linking and configuring a massive variety of TLS/Cryptography backends via CMake:

- **MbedTLS** (`C_ABSTRACT_HTTP_USE_MBEDTLS`)
- **OpenSSL** (`C_ABSTRACT_HTTP_USE_OPENSSL`)
- **LibreSSL** (`C_ABSTRACT_HTTP_USE_LIBRESSL`)
- **BoringSSL** (`C_ABSTRACT_HTTP_USE_BORINGSSL`)
- **wolfSSL** (`C_ABSTRACT_HTTP_USE_WOLFSSL`)
- **s2n-tls** (`C_ABSTRACT_HTTP_USE_S2N`)
- **BearSSL** (`C_ABSTRACT_HTTP_USE_BEARSSL`)
- **Schannel** (`C_ABSTRACT_HTTP_USE_SCHANNEL` - Windows)
- **GnuTLS** (`C_ABSTRACT_HTTP_USE_GNUTLS`)
- **Botan** (`C_ABSTRACT_HTTP_USE_BOTAN`)
- **CommonCrypto** (`C_ABSTRACT_HTTP_USE_COMMONCRYPTO` - Apple)
- **wincrypt** (`C_ABSTRACT_HTTP_USE_WINCRYPT` - Windows)

## Streaming Modalities
- **WebSockets (RFC 6455)**: 100% Native C89 framing engine featuring 16/64-bit bounds checking, chunk buffering, and zero-allocation masking via `C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS`. Includes native Base64 & SHA1 support.
- **Server-Sent Events (SSE)**: Native C89 parser featuring robust `Last-Event-ID` tracking, `retry` synchronization, and highly flexible line-buffering constraints via `C_ABSTRACT_HTTP_ENABLE_SSE`.

## OAuth 2.0 Support

The library features extensive built-in support for generating and managing OAuth 2.0 requests:

- **Password Grant**
- **Refresh Token Grant**
- **Authorization Code Grant**
- **Client Credentials Grant**
- **JWT Bearer Grant** (RFC 7523)
- **Device Authorization Request** (RFC 8628)
- **Device Access Token Request** (RFC 8628)
- **Token Revocation** (RFC 7009)
- **Token Introspection** (RFC 7662)
- **Localhost Intercept Server**: For handling local loopback flows synchronously.

## HTTP/3 Cross-Platform Support (C Implementations)

The library guarantees 100% C code compliance routing HTTP/3 frames. The following table showcases pure C network engines capable of driving QUIC/H3 streams which have been integrated as native abstraction backends:

```text
+-----------+--------------------+-----------------------------------------------+----------------+
| Name      | QUIC Backend       | Description                                   | Implemented    |
+-----------+--------------------+-----------------------------------------------+----------------+
| libcurl   | ngtcp2/quiche/msh3 | The industry standard multi-protocol library. | Yes            |
| msh3      | MsQuic             | Minimalist HTTP/3 built on MsQuic.            | Yes            |
| lsquic    | Built-in           | High-performance stack by LiteSpeed.          | Yes            |
| picoquic  | Built-in (h3zero)  | Fast and easy-to-embed QUIC & HTTP/3 stack.   | Yes            |
| nghttp3   | Agnostic (ngtcp2)  | Lightweight framing state machine.            | Yes            |
| xquic     | Built-in           | Alibaba's stack optimized for mobile/weak net.| No             |
+-----------+--------------------+-----------------------------------------------+----------------+
```

## CMake Configuration Options

Configure your build precisely by passing these flags to CMake:

| Option | Default | Description |
| :--- | :---: | :--- |
| `C_ABSTRACT_HTTP_HEADER_ONLY` | OFF | Use header-only STB-style library |
| `C_ABSTRACT_HTTP_STATIC_CRT` | OFF | Use Static CRT (/MT, /MTd) |
| `C_ABSTRACT_HTTP_SHARED_CRT` | ON | Use Shared CRT (/MD, /MDd) |
| `C_ABSTRACT_HTTP_UNICODE` | OFF | Use UNICODE on Windows |
| `C_ABSTRACT_HTTP_ANSI` | OFF | Use ANSI (Multi-Byte) on Windows |
| `C_ABSTRACT_HTTP_MULTI_THREADED` | ON | Enable multi-threading support |
| `C_ABSTRACT_HTTP_SINGLE_THREADED` | OFF | Single-threaded mode |
| `C_ABSTRACT_HTTP_LTO` | OFF | Enable Link Time Optimization |
| `C_ABSTRACT_HTTP_RTC1` | OFF | Enable MSVC /RTC1 runtime checks |
| `C_ABSTRACT_HTTP_RTCs` | OFF | Enable MSVC /RTCs runtime checks |
| `C_ABSTRACT_HTTP_RTCu` | OFF | Enable MSVC /RTCu runtime checks |
| `C_ABSTRACT_HTTP_USE_MSH3` | OFF | Use msh3 (HTTP/3) instead of libcurl |
| `C_ABSTRACT_HTTP_USE_LSQUIC` | OFF | Use lsquic (HTTP/3) instead of libcurl |
| `C_ABSTRACT_HTTP_USE_PICOQUIC` | OFF | Use picoquic (HTTP/3) instead of libcurl |
| `C_ABSTRACT_HTTP_USE_NGHTTP3` | OFF | Use nghttp3 (HTTP/3) instead of libcurl |
| `C_ABSTRACT_HTTP_USE_LIBSOUP3` | OFF | Use libsoup3 instead of libcurl |
| `C_ABSTRACT_HTTP_USE_LIBUV` | OFF | Use libuv instead of libcurl |
| `C_ABSTRACT_HTTP_USE_LIBEVENT` | OFF | Use libevent instead of libcurl |
| `C_ABSTRACT_HTTP_USE_LIBFETCH` | OFF | Use libfetch instead of libcurl |
| `C_ABSTRACT_HTTP_MULTIPLATFORM_INTEGRATION` | ON | Enable c-multiplatform integration |
| `BUILD_SHARED_LIBS` | OFF | Build Shared Library |
| `C_ABSTRACT_HTTP_USE_ASAN` | OFF | Enable AddressSanitizer |
| `C_ABSTRACT_HTTP_USE_TSAN` | OFF | Enable ThreadSanitizer |
| `TRY_SYS_LIB` | ON | Fallback to system packages if FetchContent/vcpkg is not available |

## Further Documentation

- **[USAGE.md](USAGE.md):** Practical examples, snippets, and guides.
- **[ARCHITECTURE.md](ARCHITECTURE.md):** Deep dive into the library's design philosophy and architecture.
- **[skills.md](skills.md):** Specific development guidelines, formatting rules, and procedures for contributors.

---

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
