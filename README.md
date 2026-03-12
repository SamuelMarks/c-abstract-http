c-abstract-http
===============

[![License](https://img.shields.io/badge/license-Apache--2.0%20OR%20MIT-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Continuous Integration](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml/badge.svg)](https://github.com/SamuelMarks/c-abstract-http/actions/workflows/ci.yml)

A highly robust, cross-platform abstract HTTP network library for C. It unifies various platform-specific network and crypto libraries under a single strict C89 API. Designed with strict C89 compliance, memory safety, and high portability in mind, this library is suitable for deeply embedded environments, legacy systems, and modern high-performance backends.

## Target Platforms

- **Windows**: MSVC (2005, 2022, 2026), MinGW, Cygwin
- **macOS / iOS**: Clang / Apple Frameworks
- **Linux / POSIX**: GCC / Clang
- **FreeBSD**: Clang
- **Android**: NDK
- **WebAssembly**: Emscripten
- **DOS**: OpenWatcom (with fallback support for Watt-32 / mTCP)

## Target Network Libraries

The library defaults to the best native network library for the target platform, but can be configured to use others:

- **WinHTTP**: Default for Windows system services/background.
- **WinINet**: Alternative for Windows UI/IE compatibility.
- **CFNetwork / Foundation**: Default for Apple macOS/iOS.
- **libcurl**: Default for Linux/POSIX. Supported on Windows/macOS as fallback.
- **libsoup3**: Modern GTK4-friendly POSIX backend.
- **libuv**: Node.js-style asynchronous I/O backend.
- **libevent**: Fast event notification backend.
- **libfetch**: Default for FreeBSD/BSD systems.
- **HttpURLConnection**: Java-bridged backend for Android.
- **Emscripten Fetch API**: Natively bridges to JS `fetch` for WebAssembly.
- **Raw Sockets (`C_ABSTRACT_HTTP_USE_RAW_SOCKETS`)**: Manual fallback implementation using `select`/`read`/`write` for deeply embedded systems or DOS network stacks.

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
