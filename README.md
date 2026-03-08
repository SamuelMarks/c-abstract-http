c-abstract-http
===============

A cross-platform abstract HTTP network library for C. It unifies Windows (WinHTTP, WinINet), macOS/iOS (Foundation), Android, and POSIX (cURL) beneath a single abstracted API.

## Installation

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

## Header-only Usage

If you prefer to include `c-abstract-http` as a header-only library with zero compile-time overhead (e.g., to generate a single fat executable), you can define `C_ABSTRACT_HTTP_HEADER_ONLY=ON` when configuring CMake, and then `#define C_ABSTRACT_HTTP_IMPLEMENTATION` in exactly one `.c` file before including `c_abstract_http.h`:

```c
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>

int main() {
    // ...
}
```

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
