# Contributor & Agent Skills

This document outlines standard operating procedures (SOPs) and technical "skills" for developers or autonomous agents contributing to `c-abstract-http`.

## 1. Extending the Backend (Adding a new Transport)

Before implementing a new transport backend, consider how it interacts with the OS defaults. The library is structured to automatically use the best native APIs per platform (WinHTTP on Windows, CFNetwork on Apple, libcurl on Linux, etc.), unless explicitly overridden via a CMake `-D` flag (e.g., `-DC_ABSTRACT_HTTP_USE_LIBUV=ON`).

If you are implementing a new transport backend (e.g., `http_lwip.c` for embedded systems):
- **Define Context:** Create a `struct HttpTransportContext` in your implementation file (`src/http_lwip.c`).
- **Implement Interface:** You must supply initialization logic, configuration logic, and a `send` implementation conforming to the `http_send_fn` typedef inside `http_types.h`.
- **Memory Handling:** Your `send` function must populate a `struct HttpResponse **res`. Ensure you gracefully handle the case where reading the socket fails halfway through, cleaning up whatever `res` struct you started to allocate.
- **Wire up Transport:** Add the new backend dispatch logic inside `src/transport.c` guarded by the appropriate preprocessor macro (e.g., `#elif defined(LWIP)` or `#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)`). Add it to `CMakeLists.txt`.

## 2. Extending Cryptography (Adding a new TLS Backend)

If you are adding another cryptographic library (e.g., `NSS`):
- Expose an `option(C_ABSTRACT_HTTP_USE_NSS ...)` in the root `CMakeLists.txt`.
- Add its `FetchContent` or `vcpkg`/`PkgConfig` resolution routing inside `c_abstract_http_apply_crypto`.
- Map the choice downward to any HTTP backends that optionally wrap it. For example, if you chose to inject `NSS`, add a matching `set(CURL_USE_NSS ON CACHE BOOL "" FORCE)` inside the `c_abstract_http_configure_curl` macro so the transport natively embraces it.
- Finally, surface the capability into `vcpkg.json`'s `features` block.

## 3. Navigating the `enum c_abstract_http_error` Return Error Code Pattern
When implementing string manipulations, getters, or memory reads, standard C patterns often return a pointer or NULL on failure. In this library, you MUST NOT return pointers.
Instead, return a standardized error code `enum c_abstract_http_error` (defined in `c_abstract_http/http_types.h`).

### Bad (Do Not Do This)
```c
const char* get_header(struct HttpHeaders* h, const char* k) {
    /* ... logic ... */
    if (not_found) return NULL;
    return val;
}
```

### Good (Do This)
```c
enum c_abstract_http_error get_header(struct HttpHeaders* h, const char* k, const char** out) {
    if (!h || !k || !out) return HTTP_ERROR_INVALID_ARGUMENT;
    /* ... logic ... */
    if (not_found) return HTTP_ERROR_NOT_FOUND;
    *out = val;
    return HTTP_ERROR_NONE; /* SUCCESS */
}
```

## 4. Safe MSVC Extensions

Legacy MSVC and security policies demand the usage of `*_s` functions instead of POSIX standards where buffer overflows are a risk. However, you cannot unconditionally apply `_s` functions because they do not exist in standard GCC/Clang on Linux.

When writing to buffers using functions like `sprintf`, `strcpy`, `strcat`, or opening files with `fopen`, you must format it like so:

```c
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    sprintf_s(buf, buf_len, "Value: %d", 42);
    fopen_s(&fh, "file.txt", "r");
#else
    /* Standard POSIX / GCC Path */
    sprintf(buf, "Value: %d", 42);
    fh = fopen("file.txt", "r");
#endif
```

## 5. Handling `size_t` Formatting

Printing `size_t` requires caution to not break MSVC C89 mode. Use the globally defined `NUM_FORMAT` macro.

```c
/* Do this: */
sprintf(buf, "Size=" NUM_FORMAT, len);

/* Never this: */
sprintf(buf, "Size=%zu", len);
```

## 6. Refactoring `<windows.h>`

If you are pulling in Windows APIs, never include `<windows.h>`.
Identify the precise SDK headers needed (e.g., `<windef.h>`, `<winnt.h>`, `<winbase.h>`) and include those in order. `windef.h` almost always goes first. If necessary, wrap in `#ifdef _WIN32`.
## 5. FetchContent Policy
Always use FetchContent_MakeAvailable over the deprecated FetchContent_Populate pattern in modern CMake logic to ensure robust, warning-free dependency resolution.

## 6. Header Guards & C++ Interop
Every public header MUST be wrapped with #ifdef __cplusplus and extern "C" exactly once. Furthermore, strictly guard POSIX/C99 headers like <stdint.h>, <stdbool.h>, and <unistd.h> using #if defined(_MSC_VER) with local typedef fallbacks to support compilation on legacy environments like MSVC 2005.

## 7. Clang-Format Safety
All #include blocks MUST be wrapped in /* clang-format off */ and /* clang-format on */ (maximum once per file). This strictly preserves include ordering and dependency hierarchies during automated formatting.

## 8. Pluggable Execution Modalities (Actors, Threads, Event Loops)
When extending or bridging the concurrency capabilities of the library to an external framework (such as `c-multiplatform`), you must implement the respective `*Hooks` structure (e.g., `HttpLoopHooks` in `event_loop.h`, `CddThreadPoolHooks` in `thread_pool.h`).
- **Do not modify the generic C-side headers.** The hook structures (`*Hooks`) are specifically designed to enable Inversion-of-Control.
- Ensure that memory allocated inside a hook proxy is correctly cleaned up inside the corresponding `free` hook.
- When adapting an event loop (`HttpLoopHooks`), the custom `add_fd` and `mod_fd` functions should never block the main loop of the host framework.
