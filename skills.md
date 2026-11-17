# Contributor & Agent Skills

This document defines standard operating procedures (SOPs) and strict engineering invariants for contributors and autonomous agents working on `c-abstract-http`.

## 1. Extending the Library with a New Transport Backend

1. **Public Header**: Create `include/c_abstract_http/http_<backend>.h` wrapped in a single `#ifdef __cplusplus` guard and a single `/* clang-format off */` / `/* clang-format on */` `#include` block. Declare `http_<backend>_global_init`, `http_<backend>_global_cleanup`, `http_<backend>_context_init`, `http_<backend>_context_free`, `http_<backend>_send` (`http_send_fn`), and (if supported) `http_<backend>_send_multi` (`http_send_multi_fn`), all prefixed with `NO_DISCARD C_ABSTRACT_HTTP_API` and returning `enum c_abstract_http_error` (`c_abstract_http_error_t`).
2. **Implementation**: Implement `src/http_<backend>.c` in strict ISO C89, translating every third-party/OS failure into `enum c_abstract_http_error`.
3. **Factory & Amalgamation Wiring**:
   - Wire `C_ABSTRACT_HTTP_USE_<BACKEND>` into `src/transport.c` (`transport_global_init`, `transport_global_cleanup`, `transport_factory_init_client`, `transport_factory_cleanup_client`) **above** implicit OS fallbacks (`_WIN32`, `__APPLE__`, `DOS`).
   - Mirror the header and `#ifdef C_ABSTRACT_HTTP_IMPLEMENTATION` `.c` include in `include/c_abstract_http/c_abstract_http.h`.
   - Add `option(C_ABSTRACT_HTTP_USE_<BACKEND> ... OFF)` and wire both the `if(C_ABSTRACT_HTTP_HEADER_ONLY)` (`INTERFACE`) and compiled library branches in `CMakeLists.txt`.
4. **Testing & OOM Stubs**: Add deterministic mock stubs under `tests/mock_stubs/mock_<backend>.h` (if the external C library is optional), unit/OOM tests in `tests/test_http_<backend>.h`, and register the suite in `tests/main.c`.

## 2. Strict ISO C89 (ANSI C / C90) Compliance

Every `.c` and `.h` file is compiled with `-std=c89 -pedantic -Wall -Wextra -Werror -Wdeclaration-after-statement` on GCC/Clang and `/W4 /WX` on MSVC:
- **Comments**: Use `/* ... */` exclusively. Never use `//` comments.
- **Declarations**: Declare all local variables at the very top of a `{ ... }` block before any executable statement.
- **Forbidden Headers**: Never `#include <stdbool.h>` (use `int` with `0`/`1`) and never `#include <windows.h>` (include `<winsock2.h>` / `<ws2tcpip.h>` or forward-declare minimal Win32 signatures using `include/c_abstract_http/win_compat_sym.h`). Guard `<stdint.h>` and `<unistd.h>` for `_MSC_VER < 1600`.

## 3. Mandatory Enum Error Return & Percolation Policy

Every non-void, non-math function MUST return `enum c_abstract_http_error` (`c_abstract_http_error_t`), pass outputs via pointer parameters, and percolate errors without `(void)` return discards.

### Bad (Forbidden)
```c
const char *http_headers_get(const struct HttpHeaders *h, const char *key);
```

### Good (Required)
```c
/**
 * @brief Retrieve a header value by case-insensitive key.
 * @param headers The headers container.
 * @param key Header name to look up.
 * @param out Output pointer receiving the borrowed header value.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t http_headers_get(
    const struct HttpHeaders *headers, const char *key, const char **out);
```

At every call site, capture and immediately check the returned enum without mutating the error variable before returning it:
```c
enum c_abstract_http_error rc;
rc = http_headers_add(&req.headers, "Accept", "application/json");
if (rc != C_ABSTRACT_HTTP_SUCCESS) {
  LOG_DEBUG("http_headers_add failed: %d", (int)rc);
  return rc;
}
```
Validate with `python3 scripts/validate_api_signatures.py`, `check_no_discard_void_clang.py`, and `check_error_percolation_clang.py`.

## 4. MSVC Safe CRT & Portable String Handling

Guard MSVC Safe CRT calls (`sprintf_s`, `strcpy_s`, `strncpy_s`, `_vsnprintf_s`) with `#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)` and provide standard C89 / `c89stringutils` fallbacks for GCC, Clang, MinGW, and OpenWatcom:
```c
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
strcpy_s(dest, dest_cap, src);
#else
strcpy(dest, src);
#endif
```

## 5. Portable `size_t` & 64-Bit Integer Formatting

Never use `%zu` or `%lld` directly in format strings (unsupported on MSVC 2005 / legacy MSVCRT):
- For `size_t`, include `<c_abstract_http/win_compat_sym.h>` and use `"%" SIZE_T_FMT` (which expands to `"Iu"`, `"zu"`, or `"lu"`):
  ```c
  printf("Payload bytes: %" SIZE_T_FMT "\n", body_len);
  ```
- For signed 64-bit integers (`abstract_http_int64_t`), include `<c_abstract_http/http_types.h>` and use `C_ABSTRACT_HTTP_NUM_FORMAT` (which includes `%` and expands to `"%I64d"`, `"%ld"`, or `"%lld"`):
  ```c
  printf("Timestamp: " C_ABSTRACT_HTTP_NUM_FORMAT "\n", ts_ms);
  ```

## 6. Dependency & CMake `FetchContent` Policy

- Always use `FetchContent_MakeAvailable` (never `FetchContent_Populate`).
- Always track the `master` branch (never pin commit hashes) for `SamuelMarks/*` repositories (`c-fs`, `c-orm`, `cdd-c`, `c-ci`, `greatest`).
- Ensure any CMake flag or definition works identically for `STATIC`, `SHARED` (`BUILD_SHARED_LIBS=ON`), and `INTERFACE` (`C_ABSTRACT_HTTP_HEADER_ONLY=ON`) builds.

## 7. Header Formatting, `extern "C"`, and Clang-Format Safety

- Wrap every public header in `#ifdef __cplusplus` / `extern "C" {` ... `#endif /* __cplusplus */` at most **once** per file.
- Wrap every `#include` block in `/* clang-format off */` and `/* clang-format on */` at most **once** per file to preserve include ordering on Windows/Winsock headers.
- Format all C/C++ headers and sources before finishing:
  ```bash
  fd -eh -ec -x clang-format -i --style=LLVM
  ```

## 8. 100% Documentation & 100% Line/Branch Coverage

- **Doxygen**: Every public and internal file, struct, field, enum, macro, typedef, and function must have complete Doxygen comments (`0` warnings with `WARN_NO_PARAMDOC=YES`).
- **Coverage & OOM Testing**: Maintain `100.0%` line, function, and branch coverage in `src/` (`gcovr --fail-under-line 100 --fail-under-branch 100`) using `greatest.h` suites and `tests/mock_alloc.h` fault injection (`g_mock_alloc_fail`, `g_mock_strdup_fail`, etc.). Do not introduce unreachable defensive branches that cannot be exercised by tests.

## 9. Parser Safety & Fuzzing (WebSockets & SSE)

- Respect `C_ABSTRACT_HTTP_WS_MAX_FRAME_SIZE` and `C_ABSTRACT_HTTP_SSE_MAX_LINE_SIZE` bounds before buffer growth, returning `C_ABSTRACT_HTTP_ERR_MSGSIZE` / `C_ABSTRACT_HTTP_ERR_SSE_OOM` on violations.
- Keep `tests/fuzz_ws.c` and `tests/fuzz_sse.c` deterministic and leak-free (`-DC_ABSTRACT_HTTP_ENABLE_FUZZING=ON`).

## 10. Concurrency & External Event Loop Integration

- Guard multi-threaded code and tests with `#ifndef C_ABSTRACT_HTTP_SINGLE_THREADED`.
- Thread pool tasks (`abstract_http_thread_task_cb`), loop socket callbacks (`http_loop_cb`), timer callbacks (`http_timer_cb`), and external `struct HttpLoopHooks` functions all return `enum c_abstract_http_error` and must never block the host reactor thread.
