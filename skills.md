# Contributor & Agent Skills

This document outlines standard operating procedures (SOPs) and technical "skills" for developers or autonomous agents contributing to `c-abstract-http`.

## 1. Extending the Backend (Adding a new Transport)

If you are implementing a new transport backend (e.g., `http_lwip.c` for embedded systems)
:
- **Define Context:** Create a `struct HttpTransportContext` in your implementation file (
`src/http_lwip.c`).
- **Implement Interface:** You must supply initialization logic, configuration logic, and 
a `send` implementation conforming to the `http_send_fn` typedef inside `http_types.h`.   
- **Memory Handling:** Your `send` function must populate a `struct HttpResponse **res`. E
nsure you gracefully handle the case where reading the socket fails halfway through, clean
ing up whatever `res` struct you started to allocate.
- **Wire up Transport:** Add the new backend dispatch logic inside `src/transport.c` guard
ed by the appropriate preprocessor macro (e.g., `#elif defined(LWIP)` or `#elif defined(C_ABSTRACT_HTTP_USE_LIBSOUP3)`). Add it to `CMakeLi
sts.txt`.

## 2. Navigating the `int` Return Error Code Pattern

When implementing string manipulations, getters, or memory reads, standard C patterns often return a pointer or NULL on failure. In this library, you MUST NOT return pointers.
Instead, return a standardized POSIX error code (defined in `<errno.h>`).

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
int get_header(struct HttpHeaders* h, const char* k, const char** out) {
    if (!h || !k || !out) return EINVAL;
    /* ... logic ... */
    if (not_found) return ENOENT;
    *out = val;
    return 0; /* SUCCESS */
}
```

## 3. Safe MSVC Extensions

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

## 4. Handling `size_t` Formatting

Printing `size_t` requires caution to not break MSVC C89 mode. Use the globally defined `NUM_FORMAT` macro.

```c
/* Do this: */
sprintf(buf, "Size=" NUM_FORMAT, len);

/* Never this: */
sprintf(buf, "Size=%zu", len);
```

## 5. Refactoring `<windows.h>`

If you are pulling in Windows APIs, never include `<windows.h>`.
Identify the precise SDK headers needed (e.g., `<windef.h>`, `<winnt.h>`, `<winbase.h>`) and include those in order. `windef.h` almost always goes first. If necessary, wrap in `#ifdef _WIN32`.