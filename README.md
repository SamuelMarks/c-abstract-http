# c-abstract-http

A highly robust, cross-platform abstract HTTP network library for C. It unifies Windows (WinHTTP, WinINet), macOS/iOS (CFNetwork/Foundation), Android, and POSIX (cURL) beneath a single abstracted API.

Designed with strict C89 compliance, memory safety, and high portability in mind, this library is suitable for deeply embedded environments, legacy systems, and modern high-performance backends.

## Features

- **Cross-Platform Abstraction:** Write once, compile anywhere. Automatically targets native platform APIs to reduce binary bloat (e.g., uses WinHTTP/WinINet on Windows instead of bundling cURL).
- **Strict C89 Compliance:** Operates entirely within the C89 standard. No C++ or modern C features are relied upon unless wrapped in strict platform/compiler feature macros.
- **Robust Error Handling:** All non-void utility and lifecycle functions return an `int` exit code/status. Real return values are populated via `out` pointers, ensuring that failure states (like `ENOMEM`, `EINVAL`, or `EIO`) are properly cascaded and handled.
- **Safe CRT Integration:** Extensive support for MSVC Safe C11 extensions (`strcpy_s`, `sprintf_s`, `fopen_s`). Unsafe counterparts are completely avoided on MSVC without breaking POSIX/MinGW compliance.
- **Zero Windows.h Bloat:** Explicitly avoids including the massive `<windows.h>` header. Uses lean alternatives (`<windef.h>`, `<winbase.h>`, `<winnt.h>`, `<winerror.h>`, etc.) to drastically reduce compilation times and namespace pollution.
- **Extensive CMake Configuration:** Support for granular control over linkage and compilation:
  - `/MD`, `/MDd`, `/MT`, `/MTd` for MSVC CRT linkage.
  - Runtime Checks (`/RTC1`, `/RTCs`, `/RTCu`).
  - Link-Time Optimization (LTO).
  - Single/Multi-threaded configurations.
  - ANSI / UNICODE character set toggles.
  - Static or Shared CRT and Library building.
- **Header-Only Mode:** Can be consumed as a single-header STB-style library for easy integration.

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
- **Linux / POSIX** (GCC / Clang + libcurl)
- **Android** (NDK)

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or <https://www.apache.org/licenses/LICENSE-2.0>)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or <https://opensource.org/licenses/MIT>)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
