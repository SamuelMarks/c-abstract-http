
/**
 * @file http_types.c
 * @brief Core HTTP request/response types, client dispatchers, cookies,
 * multipart, and OAuth2 helpers.
 */

/* clang-format off */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <c_abstract_http/actor.h>
#include <c_abstract_http/coroutine.h>
#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/log.h>
#include <c_abstract_http/process.h>
#include <c_abstract_http/thread_pool.h>
#include <c89stringutils_string_extras.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <dos.h>
#if defined(C_ABSTRACT_HTTP_DOS_WATT32)
#include <tcp.h>
#endif
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection flag for portable case-insensitive string compare */
extern int g_mock_strcasecmp_fail;
/** @brief Fault-injection flag for http_headers_init */
extern int g_mock_headers_init_fail;
/** @brief Fault-injection flag for http_parts_init */
extern int g_mock_parts_init_fail;
/** @brief Fault-injection flag for http_multi_request_init */
extern int g_mock_multi_init_fail;

/** @brief Fault-injection flag for mutex destruction failure */
extern int g_mock_mutex_free_fail;
/** @brief Fault-injection flag for condition variable destruction failure */
extern int g_mock_cond_free_fail;
/** @brief Fault-injection flag for http_headers_free */
extern int g_mock_headers_free_fail;
/** @brief Fault-injection flag for http_parts_free */
extern int g_mock_parts_free_fail;
/** @brief Fault-injection flag for http_multi_request_free */
extern int g_mock_multi_free_fail;
/** @brief Fault-injection flag for http_response_free */
extern int g_mock_response_free_fail;
/** @brief Fault-injection flag for http_config_free */
extern int g_mock_config_free_fail;

/** @brief Fault-injection countdown for c_abstract_http_strdup */
extern int g_mock_http_types_strdup_fail;
/** @brief Fault-injection countdown for sprintf_s_wrapper */
extern int g_mock_sprintf_s_wrapper_fail;
/** @brief Fault-injection countdown for urlencode_append */
extern int g_mock_urlencode_append_fail;
#endif

/**
 * @brief Compare two ASCII strings case-insensitively.
 * @param s1 First NUL-terminated string.
 * @param s2 Second NUL-terminated string.
 * @param out_diff Output pointer receiving lexical difference (0 if equal).
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error enum.
 */
static enum c_abstract_http_error
strcasecmp_portable(const char *s1, const char *s2, int *out_diff) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_strcasecmp_fail > 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (g_mock_strcasecmp_fail < 0 && ++g_mock_strcasecmp_fail == 0)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  while (*s1 && *s2) {
    int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (diff != 0) {
      *out_diff = diff;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    s1++;
    s2++;
  }
  *out_diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Format text into a bounded buffer starting at an offset using Safe CRT
 * when available.
 * @param out_written Output pointer receiving number of characters written.
 * @param buf Destination buffer base.
 * @param start Offset in buf where writing begins.
 * @param cap Total byte capacity of buf.
 * @param fmt Printf-style format string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on invalid bounds/failure.
 */
static enum c_abstract_http_error
sprintf_s_wrapper(size_t *out_written, char *buf, size_t start, size_t cap,
                  const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;

/**
 * @brief Format text into a bounded buffer starting at an offset using Safe CRT
 * when available.
 * @param out_written Output pointer receiving number of characters written.
 * @param buf Destination buffer base.
 * @param start Offset in buf where writing begins.
 * @param cap Total byte capacity of buf.
 * @param fmt Printf-style format string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on invalid bounds/failure.
 */
static enum c_abstract_http_error sprintf_s_wrapper(size_t *out_written,
                                                    char *buf, size_t start,
                                                    size_t cap, const char *fmt,
                                                    ...) {
  size_t written;
  va_list args;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_sprintf_s_wrapper_fail > 0) {
    if (--g_mock_sprintf_s_wrapper_fail == 0)
      return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  if (!out_written || !buf || start > cap)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  va_start(args, fmt);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  written = (size_t)vsprintf_s(buf + start, cap - start, fmt, args);
#else
  written =
      (size_t)c89stringutils_vsnprintf(buf + start, cap - start, fmt, args);
#endif
  va_end(args);
  *out_written = written;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an HTTP headers collection */
enum c_abstract_http_error http_headers_init(struct HttpHeaders *headers) {
  if (!headers)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_headers_init_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  headers->headers = NULL;
  headers->count = 0;
  headers->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free an HTTP headers collection */
enum c_abstract_http_error http_headers_free(struct HttpHeaders *headers) {
  size_t i;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_headers_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (!headers)
    return C_ABSTRACT_HTTP_SUCCESS;

  if (headers->headers) {
    for (i = 0; i < headers->count; ++i) {
      free(headers->headers[i].key);
      free(headers->headers[i].value);
    }
    free(headers->headers);
    headers->headers = NULL;
  }
  headers->count = 0;
  headers->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#undef c_abstract_http_strdup
/** @brief Duplicate a NUL-terminated string */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_strdup(const char *s, char **out_s);
/** @brief Duplicate a NUL-terminated string */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_strdup(const char *s, char **out_s) {
  size_t len;
  char *d;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_http_types_strdup_fail > 0) {
    if (--g_mock_http_types_strdup_fail == 0) {
      if (out_s)
        *out_s = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }
#endif
  if (!s)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  len = strlen(s);
  d = (char *)malloc(len + 1);
  if (!d) {
    if (out_s)
      *out_s = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(d, s, len + 1);
  if (out_s)
    *out_s = d;
  else
    free(d);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Test helper for unmocked c_abstract_http_strdup.
 * @param s Source NUL-terminated string.
 * @param out_s Optional output pointer receiving newly allocated copy.
 * @return Result from c_abstract_http_strdup.
 */
C_ABSTRACT_HTTP_API enum c_abstract_http_error
abstract_http_test_real_strdup(const char *s, char **out_s);
/**
 * @brief Test helper for unmocked c_abstract_http_strdup.
 * @param s Source NUL-terminated string.
 * @param out_s Optional output pointer receiving newly allocated copy.
 * @return Result from c_abstract_http_strdup.
 */
C_ABSTRACT_HTTP_API enum c_abstract_http_error
abstract_http_test_real_strdup(const char *s, char **out_s) {
  return c_abstract_http_strdup(s, out_s);
}

/** @brief Add a key-value pair to an HTTP headers collection */
enum c_abstract_http_error http_headers_add(struct HttpHeaders *headers,
                                            const char *key,
                                            const char *value) {
  enum c_abstract_http_error rc;
  if (!headers || !key || !value)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  if (headers->count >= headers->capacity) {
    const size_t new_cap = (headers->capacity == 0) ? 8 : headers->capacity * 2;
    struct HttpHeader *new_arr = (struct HttpHeader *)realloc(
        headers->headers, new_cap * sizeof(struct HttpHeader));
    if (!new_arr)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    headers->headers = new_arr;
    headers->capacity = new_cap;
  }

  rc = c_abstract_http_strdup(key, &headers->headers[headers->count].key);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  rc = c_abstract_http_strdup(value, &headers->headers[headers->count].value);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(headers->headers[headers->count].key);
    headers->headers[headers->count].key = NULL;
    return rc;
  }

  headers->count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Retrieve a header value by case-insensitive key */
enum c_abstract_http_error http_headers_get(const struct HttpHeaders *headers,
                                            const char *key, const char **out) {
  size_t i;
  enum c_abstract_http_error rc;
  if (!headers || !key || !out)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  for (i = 0; i < headers->count; ++i) {
    int diff = -1;
    rc = strcasecmp_portable(headers->headers[i].key, key, &diff);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    if (diff == 0) {
      *out = headers->headers[i].value;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  return C_ABSTRACT_HTTP_ERR_INVAL;
}

/** @brief Remove all headers matching a case-insensitive key */
enum c_abstract_http_error http_headers_remove(struct HttpHeaders *headers,
                                               const char *key) {
  size_t i, j;
  int found = 0;
  enum c_abstract_http_error rc;

  if (!headers || !key)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  for (i = 0; i < headers->count;) {
    int diff = -1;
    rc = strcasecmp_portable(headers->headers[i].key, key, &diff);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    if (diff == 0) {
      /* Free memory */
      free(headers->headers[i].key);
      free(headers->headers[i].value);

      /* Shift remaining elements left */
      for (j = i; j < headers->count - 1; ++j) {
        headers->headers[j] = headers->headers[j + 1];
      }

      headers->count--;
      found = 1;
      /* Do not increment i, as we need to check the shifted element */
    } else {
      i++;
    }
  }

  return found ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_INVAL;
}

/* --- Multipart Implementation --- */

/** @brief Initialize a multipart parts collection */
enum c_abstract_http_error http_parts_init(struct HttpParts *parts) {
  if (!parts)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_parts_init_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  parts->parts = NULL;
  parts->count = 0;
  parts->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free a multipart parts collection */
enum c_abstract_http_error http_parts_free(struct HttpParts *parts) {
  size_t i;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_parts_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (!parts)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (parts->parts) {
    for (i = 0; i < parts->count; ++i) {
      free(parts->parts[i].name);
      parts->parts[i].name = NULL;
      free(parts->parts[i].filename);
      parts->parts[i].filename = NULL;
      free(parts->parts[i].content_type);
      parts->parts[i].content_type = NULL;
      {
        enum c_abstract_http_error rc =
            http_headers_free(&parts->parts[i].headers);
        if (rc != C_ABSTRACT_HTTP_SUCCESS)
          return rc;
      }
      /* data ownership is typically external references for efficiency,
         but for safety in this generator we assume data pointers are managed
         by the source (e.g. read_to_file or literal).
         The generic part struct doesn't own data memory. */
    }
    free(parts->parts);
    parts->parts = NULL;
  }
  parts->count = 0;
  parts->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_request_add_part(struct HttpRequest *req, const char *name,
                      const char *filename, const char *content_type,
                      const void *data, size_t data_len) {
  enum c_abstract_http_error rc;
  struct HttpParts *p;
  if (!req || !name || (!data && data_len > 0))
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = &req->parts;
  if (p->count >= p->capacity) {
    size_t new_cap = (p->capacity == 0) ? 4 : p->capacity * 2;
    struct HttpPart *new_arr =
        (struct HttpPart *)realloc(p->parts, new_cap * sizeof(struct HttpPart));
    if (!new_arr)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    p->parts = new_arr;
    p->capacity = new_cap;
  }

  /* Zero new slot */
  memset(&p->parts[p->count], 0, sizeof(struct HttpPart));
  rc = http_headers_init(&p->parts[p->count].headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = c_abstract_http_strdup(name, &p->parts[p->count].name);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    enum c_abstract_http_error rch =
        http_headers_free(&p->parts[p->count].headers);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    return rc;
  }

  if (filename) {
    rc = c_abstract_http_strdup(filename, &p->parts[p->count].filename);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      enum c_abstract_http_error rch;
      free(p->parts[p->count].name);
      p->parts[p->count].name = NULL;
      rch = http_headers_free(&p->parts[p->count].headers);
      if (rch != C_ABSTRACT_HTTP_SUCCESS)
        return rch;
      return rc;
    }
  }

  if (content_type) {
    rc = c_abstract_http_strdup(content_type, &p->parts[p->count].content_type);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      enum c_abstract_http_error rch;
      if (p->parts[p->count].filename)
        free(p->parts[p->count].filename);
      free(p->parts[p->count].name);
      p->parts[p->count].filename = NULL;
      p->parts[p->count].name = NULL;
      rch = http_headers_free(&p->parts[p->count].headers);
      if (rch != C_ABSTRACT_HTTP_SUCCESS)
        return rch;
      return rc;
    }
  }

  /* Reference data, do not copy yet. Serialization will copy. */
  p->parts[p->count].data = (void *)(size_t)data;
  p->parts[p->count].data_len = data_len;

  p->count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_request_add_part_header_last(struct HttpRequest *req, const char *key,
                                  const char *value) {
  struct HttpPart *part;
  if (!req || !key || !value)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (req->parts.count == 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  part = &req->parts.parts[req->parts.count - 1];
  return http_headers_add(&part->headers, key, value);
}

/** @brief Flatten multipart parts into a serialized request body */
enum c_abstract_http_error http_request_flatten_parts(struct HttpRequest *req) {
  char boundary[64];
  size_t i;
  size_t estimated_size = 0;
  char *buffer = NULL;
  size_t pos = 0;
  enum c_abstract_http_error rc;

  if (!req || req->parts.count == 0)
    return C_ABSTRACT_HTTP_SUCCESS; /* Nothing to flatten */

  /* 1. Generate Boundary */
  if (req->body) {
    /* Already have a body, parts would conflict. Fail or warn? */
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  srand((unsigned int)time(NULL));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  /* MSVC Safe Path */
  sprintf_s(boundary, sizeof(boundary), "------------------------cddbound%08x",
            rand());
#else
  /* Standard POSIX / GCC Path */
  sprintf(boundary, "------------------------cddbound%08x", rand());
#endif

  /* 2. Calculate Size */
  for (i = 0; i < req->parts.count; ++i) {
    struct HttpPart *part = &req->parts.parts[i];
    size_t h;

    estimated_size += strlen(boundary) + 6; /* --bound\r\n */
    /* Content-Disposition: form-data; name="..." */
    estimated_size += 38 + strlen(part->name) + 4;
    if (part->filename) {
      /* ; filename="..." */
      estimated_size += 13 + strlen(part->filename) + 1;
    }
    estimated_size += 2; /* \r\n */

    if (part->content_type) {
      /* Content-Type: ...\r\n */
      estimated_size += 14 + strlen(part->content_type) + 2;
    } else if (part->filename) {
      /* Default to octet-stream for files */
      estimated_size +=
          14 + 24 + 2; /* Content-Type: application/octet-stream\r\n */
    }

    for (h = 0; h < part->headers.count; ++h) {
      const struct HttpHeader *hdr = &part->headers.headers[h];

      estimated_size += strlen(hdr->key) + 2 + strlen(hdr->value) + 2;
    }

    estimated_size += 2; /* \r\n (end of headers) */
    estimated_size += part->data_len;
    estimated_size += 2; /* \r\n (end of part) */
  }
  estimated_size += strlen(boundary) + 8; /* --bound--\r\n\0 */

  /* 3. Build Buffer */
  buffer = (char *)malloc(estimated_size);
  if (!buffer)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  for (i = 0; i < req->parts.count; ++i) {
    struct HttpPart *part = &req->parts.parts[i];
    size_t h;
    size_t written;

    /* Boundary start */
    /* Note: Using sprintf directly is unsafe if we didn't pre-calc, assume size
     * is ok */
    /* Using safe wrapper logic */
    /* --boundary\r\n */
    rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size, "--%s\r\n",
                           boundary);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buffer);
      return rc;
    }
    pos += written;

    /* Header */
    rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size,
                           "Content-Disposition: form-data; name=\"%s\"",
                           part->name);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buffer);
      return rc;
    }
    pos += written;

    if (part->filename) {
      rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size,
                             "; filename=\"%s\"", part->filename);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(buffer);
        return rc;
      }
      pos += written;
    }
    rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size, "\r\n");
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buffer);
      return rc;
    }
    pos += written;

    /* Content-Type */
    if (part->content_type) {
      rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size,
                             "Content-Type: %s\r\n", part->content_type);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(buffer);
        return rc;
      }
      pos += written;
    } else if (part->filename) {
      rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size,
                             "Content-Type: application/octet-stream\r\n");
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(buffer);
        return rc;
      }
      pos += written;
    }

    for (h = 0; h < part->headers.count; ++h) {
      const struct HttpHeader *hdr = &part->headers.headers[h];

      rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size,
                             "%s: %s\r\n", hdr->key, hdr->value);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(buffer);
        return rc;
      }
      pos += written;
    }

    /* End of headers */
    rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size, "\r\n");
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buffer);
      return rc;
    }
    pos += written;

    /* Data */
    if (part->data_len > 0) {
      memcpy(buffer + pos, part->data, part->data_len);
      pos += part->data_len;
    }

    /* End of part */
    rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size, "\r\n");
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buffer);
      return rc;
    }
    pos += written;
  }

  /* Final Boundary */
  {
    size_t written;
    rc = sprintf_s_wrapper(&written, buffer, pos, estimated_size, "--%s--\r\n",
                           boundary);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buffer);
      return rc;
    }
    pos += written;
  }

  /* 4. Update Request */
  req->body = buffer;
  req->body_len = pos;

  {
    char ct[128];
    enum c_abstract_http_error hdr_rc;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    /* MSVC Safe Path */
    sprintf_s(ct, sizeof(ct), "multipart/form-data; boundary=%s", boundary);
#else
    /* Standard POSIX / GCC Path */
    sprintf(ct, "multipart/form-data; boundary=%s", boundary);
#endif
    /* If header exists, this adds a duplicate. Typically client code shouldn't
     * set CT if using parts. */
    hdr_rc = http_headers_add(&req->headers, "Content-Type", ct);
    if (hdr_rc != 0) {
      free(buffer);
      req->body = NULL;
      req->body_len = 0;
      return hdr_rc;
    }
  }

  /* 5. Clear parts logic to avoid double-process (though body takes precedence)
   */
  /* We don't free the parts struct itself as caller might own data, but we
   * effectively consumed it into body */

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize a cookie jar */
enum c_abstract_http_error http_cookie_jar_init(struct HttpCookieJar *jar) {
  if (!jar)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  jar->cookies = NULL;
  jar->count = 0;
  jar->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free a cookie jar */
enum c_abstract_http_error http_cookie_jar_free(struct HttpCookieJar *jar) {
  size_t i;
  if (!jar)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (jar->cookies) {
    for (i = 0; i < jar->count; ++i) {
      free(jar->cookies[i].name);
      free(jar->cookies[i].value);
      free(jar->cookies[i].domain);
      free(jar->cookies[i].path);
      free(jar->cookies[i].same_site);
    }
    free(jar->cookies);
    jar->cookies = NULL;
  }
  jar->count = 0;
  jar->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set or update a cookie in the cookie jar */
enum c_abstract_http_error http_cookie_jar_set(struct HttpCookieJar *jar,
                                               const char *name,
                                               const char *value) {
  enum c_abstract_http_error rc;
  size_t i;

  if (!jar || !name || !value)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  /* Check for existing */
  for (i = 0; i < jar->count; ++i) {
    if (strcmp(jar->cookies[i].name, name) == 0) {
      char *new_val = NULL;
      rc = c_abstract_http_strdup(value, &new_val);
      if (rc != C_ABSTRACT_HTTP_SUCCESS)
        return rc;
      free(jar->cookies[i].value);
      jar->cookies[i].value = new_val;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }

  /* Insert new */
  if (jar->count >= jar->capacity) {
    size_t new_cap = (jar->capacity == 0) ? 4 : jar->capacity * 2;
    struct HttpCookie *new_arr = (struct HttpCookie *)realloc(
        jar->cookies, new_cap * sizeof(struct HttpCookie));
    if (!new_arr)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    jar->cookies = new_arr;
    jar->capacity = new_cap;
  }

  memset(&jar->cookies[jar->count], 0, sizeof(struct HttpCookie));

  rc = c_abstract_http_strdup(name, &jar->cookies[jar->count].name);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  rc = c_abstract_http_strdup(value, &jar->cookies[jar->count].value);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(jar->cookies[jar->count].name);
    jar->cookies[jar->count].name = NULL;
    return rc;
  }

  jar->count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Retrieve a cookie value by name */
enum c_abstract_http_error http_cookie_jar_get(const struct HttpCookieJar *jar,
                                               const char *name,
                                               const char **out) {
  size_t i;
  if (!jar || !name || !out)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  for (i = 0; i < jar->count; ++i) {
    if (strcmp(jar->cookies[i].name, name) == 0) {
      *out = jar->cookies[i].value;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  return C_ABSTRACT_HTTP_ERR_INVAL;
}

/**
 * @brief Duplicate a trimmed ASCII substring `[start, end)`.
 * @param start Inclusive start pointer.
 * @param end Exclusive end pointer.
 * @param out Output pointer receiving NUL-terminated heap string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
cookie_slice_dup(const char *start, const char *end, char **out) {
  size_t len;
  char *buf;
  while (start < end && (*start == ' ' || *start == '\t')) {
    start++;
  }
  while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
    end--;
  }
  len = (size_t)(end - start);
  buf = (char *)malloc(len + 1);
  if (!buf) {
    *out = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  if (len > 0) {
    memcpy(buf, start, len);
  }
  buf[len] = '\0';
  *out = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Pure predicate checking RFC 6265 domain suffix match between request
 * host and cookie domain.
 * @param host Request host slice start.
 * @param host_len Request host byte length.
 * @param cookie_dom Stored normalized cookie domain (without leading dot).
 * @return 1 on match, 0 otherwise.
 */
static int math_cookie_domain_matches(const char *host, size_t host_len,
                                      const char *cookie_dom) {
  size_t dom_len = strlen(cookie_dom);
  size_t i;
  const char *suffix;
  if (dom_len == 0 || host_len < dom_len) {
    return 0;
  }
  if (host_len > dom_len && host[host_len - dom_len - 1] != '.') {
    return 0;
  }
  suffix = host + (host_len - dom_len);
  for (i = 0; i < dom_len; ++i) {
    if (tolower((unsigned char)suffix[i]) !=
        tolower((unsigned char)cookie_dom[i])) {
      return 0;
    }
  }
  return 1;
}

/** @brief Parse an RFC 6265 Set-Cookie header into the cookie jar */
enum c_abstract_http_error
http_cookie_jar_parse_set_cookie(struct HttpCookieJar *jar,
                                 const char *set_cookie_header) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  const char *semi;
  const char *eq;
  const char *cursor;
  char *name = NULL;
  char *value = NULL;
  char *domain = NULL;
  char *path = NULL;
  char *same_site = NULL;
  long expires = 0;
  int has_max_age = 0;
  int secure = 0;
  int http_only = 0;
  size_t i;
  size_t target_idx = 0;
  int found = 0;

  if (!jar || !set_cookie_header) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  semi = strchr(set_cookie_header, ';');
  if (!semi) {
    semi = set_cookie_header + strlen(set_cookie_header);
  }
  eq = (const char *)memchr(set_cookie_header, '=',
                            (size_t)(semi - set_cookie_header));
  if (!eq) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = cookie_slice_dup(set_cookie_header, eq, &name);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup;
  }
  if (name[0] == '\0') {
    rc = C_ABSTRACT_HTTP_ERR_INVAL;
    goto cleanup;
  }
  rc = cookie_slice_dup(eq + 1, semi, &value);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup;
  }

  cursor = (*semi == ';') ? (semi + 1) : semi;
  while (*cursor != '\0') {
    static const char *const k_attrs[7] = {"Domain",  "Path",    "SameSite",
                                           "Max-Age", "Expires", "Secure",
                                           "HttpOnly"};
    const char *next_semi = strchr(cursor, ';');
    const char *seg_end = next_semi ? next_semi : (cursor + strlen(cursor));
    const char *attr_eq =
        (const char *)memchr(cursor, '=', (size_t)(seg_end - cursor));
    char *attr_key = NULL;
    char *attr_val = NULL;
    int matched = -1;
    size_t ai;

    if (attr_eq) {
      rc = cookie_slice_dup(cursor, attr_eq, &attr_key);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup;
      }
      rc = cookie_slice_dup(attr_eq + 1, seg_end, &attr_val);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(attr_key);
        goto cleanup;
      }
    } else {
      rc = cookie_slice_dup(cursor, seg_end, &attr_key);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup;
      }
    }

    for (ai = 0; ai < 7; ++ai) {
      int diff = 0;
      rc = strcasecmp_portable(attr_key, k_attrs[ai], &diff);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(attr_key);
        free(attr_val);
        goto cleanup;
      }
      if (diff == 0) {
        matched = (int)ai;
        break;
      }
    }
    free(attr_key);

    if (matched == 0 && attr_val) {
      const char *dom_src = (attr_val[0] == '.') ? (attr_val + 1) : attr_val;
      free(domain);
      domain = NULL;
      rc = c_abstract_http_strdup(dom_src, &domain);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(attr_val);
        goto cleanup;
      }
      free(attr_val);
    } else if (matched == 1 && attr_val) {
      free(path);
      path = attr_val;
    } else if (matched == 2 && attr_val) {
      free(same_site);
      same_site = attr_val;
    } else if (matched == 3 && attr_val) {
      long sec = atol(attr_val);
      has_max_age = 1;
      expires = (sec <= 0) ? 1L : ((long)time(NULL) + sec);
      free(attr_val);
    } else if (matched == 4 && attr_val) {
      if (!has_max_age) {
        long ep = atol(attr_val);
        if (ep > 0) {
          expires = ep;
        }
      }
      free(attr_val);
    } else {
      if (matched == 5) {
        secure = 1;
      } else if (matched == 6) {
        http_only = 1;
      }
      free(attr_val);
    }

    cursor = next_semi ? (next_semi + 1) : seg_end;
  }

  for (i = 0; i < jar->count; ++i) {
    int dom_eq = 0;
    int path_eq = 0;
    if (strcmp(jar->cookies[i].name, name) != 0) {
      continue;
    }
    if (!domain && !jar->cookies[i].domain) {
      dom_eq = 1;
    } else if (domain && jar->cookies[i].domain) {
      int d = 0;
      rc = strcasecmp_portable(domain, jar->cookies[i].domain, &d);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup;
      }
      dom_eq = (d == 0);
    }
    if (!path && !jar->cookies[i].path) {
      path_eq = 1;
    } else if (path && jar->cookies[i].path &&
               strcmp(path, jar->cookies[i].path) == 0) {
      path_eq = 1;
    }
    if (dom_eq && path_eq) {
      target_idx = i;
      found = 1;
      break;
    }
  }

  if (!found) {
    if (jar->count >= jar->capacity) {
      size_t new_cap = (jar->capacity == 0) ? 4 : jar->capacity * 2;
      struct HttpCookie *new_arr = (struct HttpCookie *)realloc(
          jar->cookies, new_cap * sizeof(struct HttpCookie));
      if (!new_arr) {
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        goto cleanup;
      }
      jar->cookies = new_arr;
      jar->capacity = new_cap;
    }
    target_idx = jar->count++;
    memset(&jar->cookies[target_idx], 0, sizeof(struct HttpCookie));
    jar->cookies[target_idx].name = name;
    name = NULL;
  } else {
    free(jar->cookies[target_idx].value);
    free(jar->cookies[target_idx].domain);
    free(jar->cookies[target_idx].path);
    free(jar->cookies[target_idx].same_site);
  }

  jar->cookies[target_idx].value = value;
  jar->cookies[target_idx].domain = domain;
  jar->cookies[target_idx].path = path;
  jar->cookies[target_idx].same_site = same_site;
  jar->cookies[target_idx].expires = expires;
  jar->cookies[target_idx].secure = secure;
  jar->cookies[target_idx].http_only = http_only;
  value = NULL;
  domain = NULL;
  path = NULL;
  same_site = NULL;

cleanup:
  free(name);
  free(value);
  free(domain);
  free(path);
  free(same_site);
  return rc;
}

/**
 * @brief Pure predicate checking if a stored cookie applies to a parsed
 * request target.
 * @param c Candidate cookie.
 * @param now_ts Current epoch timestamp.
 * @param is_https Non-zero if target scheme is HTTPS.
 * @param has_url Non-zero if target has host authority.
 * @param host_start Pointer to request host slice.
 * @param host_len Length of request host slice.
 * @param req_path Request path starting with '/'.
 * @return 1 if cookie should be sent, 0 otherwise.
 */
static int math_cookie_matches_request(const struct HttpCookie *c, long now_ts,
                                       int is_https, int has_url,
                                       const char *host_start, size_t host_len,
                                       const char *req_path) {
  if (c->expires > 0 && c->expires <= now_ts) {
    return 0;
  }
  if (c->secure && !is_https) {
    return 0;
  }
  if (c->domain && (!has_url || !math_cookie_domain_matches(
                                    host_start, host_len, c->domain))) {
    return 0;
  }
  if (c->path && strncmp(req_path, c->path, strlen(c->path)) != 0) {
    return 0;
  }
  return 1;
}

/** @brief Build an RFC 6265 Cookie header string matching a request URL */
enum c_abstract_http_error
http_cookie_jar_build_header(const struct HttpCookieJar *jar,
                             const char *request_url,
                             char **out_cookie_header) {
  size_t i;
  size_t total_len = 0;
  size_t match_count = 0;
  int is_https = 0;
  int has_url = 0;
  const char *host_start = NULL;
  size_t host_len = 0;
  const char *req_path = "/";
  long now_ts;
  char *buf;
  size_t pos = 0;

  if (!jar || !out_cookie_header) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_cookie_header = NULL;
  if (jar->count == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  if (request_url) {
    const char *scheme_sep = strstr(request_url, "://");
    if (scheme_sep) {
      const char *slash;
      const char *colon;
      size_t scheme_len = (size_t)(scheme_sep - request_url);
      has_url = 1;
      if (scheme_len == 5 && tolower((unsigned char)request_url[0]) == 'h' &&
          tolower((unsigned char)request_url[4]) == 's') {
        is_https = 1;
      }
      host_start = scheme_sep + 3;
      slash = strchr(host_start, '/');
      if (slash) {
        req_path = slash;
        host_len = (size_t)(slash - host_start);
      } else {
        host_len = strlen(host_start);
      }
      colon = (const char *)memchr(host_start, ':', host_len);
      if (colon) {
        host_len = (size_t)(colon - host_start);
      }
    } else if (request_url[0] == '/') {
      req_path = request_url;
    }
  }

  now_ts = (long)time(NULL);
  for (i = 0; i < jar->count; ++i) {
    const struct HttpCookie *c = &jar->cookies[i];
    if (!math_cookie_matches_request(c, now_ts, is_https, has_url, host_start,
                                     host_len, req_path)) {
      continue;
    }
    if (match_count > 0) {
      total_len += 2;
    }
    total_len += strlen(c->name) + 1 + strlen(c->value);
    match_count++;
  }

  if (match_count == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  buf = (char *)malloc(total_len + 1);
  if (!buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  match_count = 0;
  for (i = 0; i < jar->count; ++i) {
    const struct HttpCookie *c = &jar->cookies[i];
    size_t nlen;
    size_t vlen;
    if (!math_cookie_matches_request(c, now_ts, is_https, has_url, host_start,
                                     host_len, req_path)) {
      continue;
    }
    if (match_count > 0) {
      buf[pos++] = ';';
      buf[pos++] = ' ';
    }
    nlen = strlen(c->name);
    vlen = strlen(c->value);
    memcpy(buf + pos, c->name, nlen);
    pos += nlen;
    buf[pos++] = '=';
    memcpy(buf + pos, c->value, vlen);
    pos += vlen;
    match_count++;
  }
  buf[pos] = '\0';
  *out_cookie_header = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an HTTP configuration structure */
enum c_abstract_http_error http_config_init(struct HttpConfig *config) {
  enum c_abstract_http_error rc;
  if (!config)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  config->timeout_ms = 30000;
  config->connect_timeout_ms = 0;
  config->read_timeout_ms = 0;
  config->write_timeout_ms = 0;
  config->verify_peer = 1;
  config->verify_host = 1;
  config->follow_redirects = 1;
  config->proxy_url = NULL;
  config->proxy_username = NULL;
  config->proxy_password = NULL;
  config->cookie_jar = NULL;
  config->user_agent = NULL;

  /* Retry defaults */
  config->retry_count = 0;
  config->retry_policy = HTTP_RETRY_NONE;

  config->modality = MODALITY_SYNC;
  config->min_threads = 4;
  config->max_threads = 16;
  config->version_mask = HTTP_VERSION_DEFAULT;
  config->tls_version_mask = HTTP_TLS_VERSION_DEFAULT;
  config->http3_fallback = 1;
  config->ca_path = NULL;
  config->ca_buffer = NULL;
  config->ca_buffer_len = 0;
  config->client_cert_path = NULL;
  config->client_key_path = NULL;
  config->cert_pin_sha256 = NULL;

  rc = c_abstract_http_strdup("c_abstract_http/0.1.0", &config->user_agent);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free an HTTP configuration structure */
enum c_abstract_http_error http_config_free(struct HttpConfig *config) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_config_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (!config)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (config->user_agent) {
    free(config->user_agent);
    config->user_agent = NULL;
  }
  if (config->proxy_url) {
    free(config->proxy_url);
    config->proxy_url = NULL;
  }
  if (config->proxy_username) {
    free(config->proxy_username);
    config->proxy_username = NULL;
  }
  if (config->proxy_password) {
    free(config->proxy_password);
    config->proxy_password = NULL;
  }
  free(config->ca_path);
  config->ca_path = NULL;
  free(config->ca_buffer);
  config->ca_buffer = NULL;
  config->ca_buffer_len = 0;
  free(config->client_cert_path);
  config->client_cert_path = NULL;
  free(config->client_key_path);
  config->client_key_path = NULL;
  free(config->cert_pin_sha256);
  config->cert_pin_sha256 = NULL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an HTTP client */
enum c_abstract_http_error http_client_init(struct HttpClient *client) {
  if (!client)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  memset(client, 0, sizeof(struct HttpClient));
  return http_config_init(&client->config);
}

/** @brief Free an HTTP client */
enum c_abstract_http_error http_client_free(struct HttpClient *client) {
  if (!client)
    return C_ABSTRACT_HTTP_SUCCESS;

  {
    enum c_abstract_http_error rc = http_config_free(&client->config);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
  }

  if (client->base_url) {
    free(client->base_url);
    client->base_url = NULL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an HTTP request structure */
enum c_abstract_http_error http_request_init(struct HttpRequest *req) {
  enum c_abstract_http_error rc;
  if (!req)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  req->url = NULL;
  req->method = HTTP_GET;
  req->body = NULL;
  req->body_len = 0;
  req->on_chunk = NULL;
  req->on_chunk_user_data = NULL;
  req->read_chunk = NULL;
  req->read_chunk_user_data = NULL;
  req->expected_body_len = 0;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;
  rc = http_headers_init(&req->headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  rc = http_parts_init(&req->parts);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    enum c_abstract_http_error rch = http_headers_free(&req->headers);

    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free an HTTP request structure */
enum c_abstract_http_error http_request_free(struct HttpRequest *req) {
  if (!req)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (req->url) {
    free(req->url);
    req->url = NULL;
  }
  if (req->body) {
    free(req->body);
    req->body = NULL;
  }
  {
    enum c_abstract_http_error rch = http_headers_free(&req->headers);

    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
  }
  {
    enum c_abstract_http_error rcp = http_parts_free(&req->parts);
    if (rcp != C_ABSTRACT_HTTP_SUCCESS)
      return rcp;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize a modality execution context */
enum c_abstract_http_error
http_modality_context_init(struct ModalityContext *ctx) {
  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  ctx->modality = MODALITY_SYNC;
  ctx->internal_ctx = NULL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free a modality execution context */
enum c_abstract_http_error
http_modality_context_free(struct ModalityContext *ctx) {
  void *raw;
  if (!ctx)
    return C_ABSTRACT_HTTP_SUCCESS;
  raw = ctx->internal_ctx;
  ctx->internal_ctx = NULL;
  if (!raw)
    return C_ABSTRACT_HTTP_SUCCESS;

  switch ((int)ctx->modality) {
  case MODALITY_ASYNC: {
    return http_loop_free((struct ModalityEventLoop *)raw);
  }
  case MODALITY_THREAD_POOL: {
    return abstract_http_thread_pool_free((struct AbstractHttpThreadPool *)raw);
  }
  case MODALITY_GREENTHREAD: {
    return abstract_http_coroutine_free((struct AbstractHttpCoroutine *)raw);
  }
  case MODALITY_MESSAGE_PASSING: {
    return abstract_http_message_bus_free((struct AbstractHttpMessageBus *)raw);
  }
  case MODALITY_MULTIPROCESS: {
    return abstract_http_process_wait_and_free(
        (struct AbstractHttpProcess *)raw, NULL);
  }
  case MODALITY_SYNC:
  default:
    free(raw);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

/** @brief Internal synchronization and cancellation state for HttpFuture */
struct HttpFutureState {
  /** @brief Non-zero once cancellation was requested */
  int is_cancelled;
  /** @brief Mutex protecting state transitions */
  struct AbstractHttpMutex *mutex;
  /** @brief Condition variable woken on completion or cancellation */
  struct AbstractHttpCond *cond;
  /** @brief Intrusive singly-linked list pointer of active future states */
  struct HttpFutureState *next;
};

/** @brief Head of active library-owned HttpFutureState instances */
static struct HttpFutureState *g_active_future_states = NULL;

/**
 * @brief Pure predicate looking up a library-allocated `HttpFutureState`
 * without dereferencing unknown pointers.
 * @param ptr Candidate `future->internal_state` pointer.
 * @param out_state Output pointer receiving matching `struct HttpFutureState
 * *`.
 * @return 1 if found, 0 otherwise.
 */
static int math_find_future_state(const void *ptr,
                                  struct HttpFutureState **out_state) {
  struct HttpFutureState *cur;
  *out_state = NULL;
  if (!ptr) {
    return 0;
  }
  for (cur = g_active_future_states; cur != NULL; cur = cur->next) {
    if ((const void *)cur == ptr) {
      *out_state = cur;
      return 1;
    }
  }
  return 0;
}

/**
 * @brief Initialize a paired mutex and condition variable.
 * @param out_mutex Output mutex pointer.
 * @param out_cond Output condition variable pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
sync_pair_init(struct AbstractHttpMutex **out_mutex,
               struct AbstractHttpCond **out_cond) {
  enum c_abstract_http_error rc;
  rc = abstract_http_mutex_init(out_mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = abstract_http_cond_init(out_cond);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    struct AbstractHttpMutex *m = *out_mutex;
    enum c_abstract_http_error m_rc;
    *out_mutex = NULL;
    m_rc = abstract_http_mutex_free(m);
    if (m_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return m_rc;
    }
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free a paired condition variable and mutex, percolating errors.
 * @param cond Condition variable to free.
 * @param mutex Mutex to free.
 * @return C_ABSTRACT_HTTP_SUCCESS or first failure enum.
 */
static enum c_abstract_http_error
sync_pair_free(struct AbstractHttpCond *cond, struct AbstractHttpMutex *mutex) {
  enum c_abstract_http_error c_rc;
  enum c_abstract_http_error m_rc;
  c_rc = abstract_http_cond_free(cond);
  if (c_rc != C_ABSTRACT_HTTP_SUCCESS) {
    m_rc = abstract_http_mutex_free(mutex);
    if (m_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return m_rc;
    }
    return c_rc;
  }
  m_rc = abstract_http_mutex_free(mutex);
  if (m_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return m_rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Broadcast a condition variable (if `do_broadcast` is non-zero) and
 * unlock its associated mutex.
 * @param mutex Locked mutex to unlock.
 * @param cond Condition variable to broadcast.
 * @param do_broadcast Non-zero to broadcast before unlocking.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
sync_broadcast_and_unlock(struct AbstractHttpMutex *mutex,
                          struct AbstractHttpCond *cond, int do_broadcast) {
  enum c_abstract_http_error u_rc;
  if (do_broadcast) {
    enum c_abstract_http_error b_rc;
    b_rc = abstract_http_cond_broadcast(cond);
    if (b_rc != C_ABSTRACT_HTTP_SUCCESS) {
      u_rc = abstract_http_mutex_unlock(mutex);
      if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return u_rc;
      }
      return b_rc;
    }
  }
  u_rc = abstract_http_mutex_unlock(mutex);
  if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return u_rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Ensure `future->internal_state` is allocated and initialized.
 * @param future Target future.
 * @param out_state Output pointer receiving internal state struct.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on allocation/sync init
 * failure.
 */
static enum c_abstract_http_error
http_future_ensure_state(struct HttpFuture *future,
                         struct HttpFutureState **out_state) {
  struct HttpFutureState *st = NULL;
  enum c_abstract_http_error rc;
  if (math_find_future_state(future->internal_state, &st)) {
    *out_state = st;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  st = (struct HttpFutureState *)calloc(1, sizeof(*st));
  if (!st) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = sync_pair_init(&st->mutex, &st->cond);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(st);
    return rc;
  }
  st->next = g_active_future_states;
  g_active_future_states = st;
  future->internal_state = st;
  *out_state = st;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an asynchronous HTTP future */
enum c_abstract_http_error http_future_init(struct HttpFuture *future) {
  if (!future)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  future->is_ready = 0;
  future->error_code = 0;
  future->response = NULL;
  future->internal_state = NULL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free an asynchronous HTTP future */
enum c_abstract_http_error http_future_free(struct HttpFuture *future) {
  struct HttpFutureState *st = NULL;
  if (!future)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (math_find_future_state(future->internal_state, &st)) {
    struct HttpFutureState **pp = &g_active_future_states;
    enum c_abstract_http_error rc;
    while (*pp) {
      if (*pp == st) {
        *pp = st->next;
        break;
      }
      pp = &(*pp)->next;
    }
    future->internal_state = NULL;
    future->response = NULL;
    rc = sync_pair_free(st->cond, st->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(st);
      return rc;
    }
    free(st);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  future->internal_state = NULL;
  future->response = NULL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Wait for a deferred HttpFuture to complete */
enum c_abstract_http_error http_future_await(struct HttpFuture *future) {
  struct HttpFutureState *st = NULL;
  if (!future)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (math_find_future_state(future->internal_state, &st)) {
    enum c_abstract_http_error rc;
    enum c_abstract_http_error u_rc;
    rc = abstract_http_mutex_lock(st->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    while (!future->is_ready) {
      rc = abstract_http_cond_wait(st->cond, st->mutex);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        u_rc = abstract_http_mutex_unlock(st->mutex);
        if (u_rc != C_ABSTRACT_HTTP_SUCCESS)
          return u_rc;
        return rc;
      }
    }
    u_rc = abstract_http_mutex_unlock(st->mutex);
    if (u_rc != C_ABSTRACT_HTTP_SUCCESS)
      return u_rc;
  } else if (!future->is_ready) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return (enum c_abstract_http_error)future->error_code;
}

/** @brief Mark an HttpFuture as cancelled and wake waiting callers */
enum c_abstract_http_error http_future_cancel(struct HttpFuture *future) {
  struct HttpFutureState *st = NULL;
  enum c_abstract_http_error rc;
  if (!future)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  rc = http_future_ensure_state(future, &st);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  rc = abstract_http_mutex_lock(st->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  st->is_cancelled = 1;
  if (!future->is_ready) {
    future->error_code = (int)C_ABSTRACT_HTTP_ERR_TIMEOUT;
    future->is_ready = 1;
  }
  return sync_broadcast_and_unlock(st->mutex, st->cond, 1);
}

/** @brief Initialize a multi-request batch */
enum c_abstract_http_error
http_multi_request_init(struct HttpMultiRequest *multi) {
  if (!multi)
    return C_ABSTRACT_HTTP_ERR_INVAL;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_multi_init_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  multi->requests = NULL;
  multi->count = 0;
  multi->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Free a multi-request batch */
enum c_abstract_http_error
http_multi_request_free(struct HttpMultiRequest *multi) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_multi_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (!multi)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (multi->requests) {
    /* Does not free the actual HttpRequest objects, just the array */
    free(multi->requests);
    multi->requests = NULL;
  }
  multi->count = 0;
  multi->capacity = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Add an HTTP request to a multi-request batch */
enum c_abstract_http_error
http_multi_request_add(struct HttpMultiRequest *multi,
                       struct HttpRequest *req) {
  if (!multi || !req)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  if (multi->count >= multi->capacity) {
    size_t new_cap = (multi->capacity == 0) ? 4 : multi->capacity * 2;
    struct HttpRequest **new_arr = (struct HttpRequest **)realloc(
        multi->requests, new_cap * sizeof(struct HttpRequest *));
    if (!new_arr)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    multi->requests = new_arr;
    multi->capacity = new_cap;
  }

  multi->requests[multi->count++] = req;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set Bearer token Authorization header on an HTTP request */
enum c_abstract_http_error http_request_set_auth_bearer(struct HttpRequest *req,
                                                        const char *token) {
  char *val = NULL;
  size_t len;
  enum c_abstract_http_error rc;

  if (!req || !token)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  len = strlen(token) + 8;
  val = (char *)malloc(len);
  if (!val)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  /* MSVC Safe Path */
  sprintf_s(val, len, "Bearer %s", token);
#else
  /* Standard POSIX / GCC Path */
  sprintf(val, "Bearer %s", token);
#endif

  rc = http_headers_add(&req->headers, "Authorization", val);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(val);
    return rc;
  }
  free(val);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set Basic token Authorization header on an HTTP request */
enum c_abstract_http_error http_request_set_auth_basic(struct HttpRequest *req,
                                                       const char *token) {
  char *val = NULL;
  size_t len;
  enum c_abstract_http_error rc;

  if (!req || !token)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  len = strlen(token) + 7;
  val = (char *)malloc(len);
  if (!val)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  /* MSVC Safe Path */
  sprintf_s(val, len, "Basic %s", token);
#else
  /* Standard POSIX / GCC Path */
  sprintf(val, "Basic %s", token);
#endif

  rc = http_headers_add(&req->headers, "Authorization", val);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(val);
    return rc;
  }
  free(val);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Encode binary bytes into a newly allocated Base64 string.
 * @param src Source byte buffer.
 * @param len Byte length of src.
 * @param out Output pointer receiving allocated Base64 string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
http_types_base64_encode(const unsigned char *src, size_t len, char **out) {
  static const char b64[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *res;
  size_t olen = 4 * ((len + 2) / 3);
  size_t i, j;

  res = (char *)malloc(olen + 1);
  if (!res)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  i = 0;
  j = 0;
  while (i < len) {
    unsigned long octet_a = src[i++];
    unsigned long octet_b = i < len ? src[i++] : 0;
    unsigned long octet_c = i < len ? src[i++] : 0;
    unsigned long triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    res[j++] = b64[(triple >> 3 * 6) & 0x3F];
    res[j++] = b64[(triple >> 2 * 6) & 0x3F];
    res[j++] = b64[(triple >> 1 * 6) & 0x3F];
    res[j++] = b64[(triple >> 0 * 6) & 0x3F];
  }

  if (len % 3 == 1) {
    res[olen - 1] = '=';
    res[olen - 2] = '=';
  } else if (len % 3 == 2) {
    res[olen - 1] = '=';
  }
  res[olen] = '\0';
  *out = res;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Set Basic username/password Authorization header on an HTTP request
 */
enum c_abstract_http_error http_request_set_auth_basic_userpwd(
    struct HttpRequest *req, const char *username, const char *password) {
  enum c_abstract_http_error rc;
  char *raw;
  char *encoded = NULL;
  size_t len;
  if (!req || !username || !password)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  len = strlen(username) + strlen(password) + 2;
  raw = (char *)malloc(len);
  if (!raw)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  /* MSVC Safe Path */
  sprintf_s(raw, len, "%s:%s", username, password);
#else
  /* Standard POSIX / GCC Path */
  sprintf(raw, "%s:%s", username, password);
#endif

  rc = http_types_base64_encode((const unsigned char *)raw, len - 1, &encoded);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(raw);
    return rc;
  }
  free(raw);

  rc = http_request_set_auth_basic(req, encoded);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(encoded);
    return rc;
  }
  free(encoded);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Compute encoded byte length for application/x-www-form-urlencoded
 * text.
 * @param src NUL-terminated input string.
 * @return Encoded length excluding NUL terminator.
 */
static size_t math_urlencode_len(const char *src) {
  size_t len = 0;
  const char *p;

  for (p = src; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      len++;
    } else if (c == ' ') {
      len++;
    } else {
      len += 3;
    }
  }
  return len;
}

/**
 * @brief Percent-encode a string into a cursor buffer and advance the cursor.
 * @param dest Pointer to destination cursor pointer.
 * @param src NUL-terminated input string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on invalid input/failure.
 */
static enum c_abstract_http_error urlencode_append(char **dest,
                                                   const char *src) {
  static const char *hex = "0123456789ABCDEF";
  const char *p;
  char *q;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_urlencode_append_fail > 0) {
    if (--g_mock_urlencode_append_fail == 0)
      return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  if (!dest || !*dest)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  if (!src)
    return C_ABSTRACT_HTTP_SUCCESS;

  q = *dest;
  for (p = src; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      *q++ = (char)c;
    } else if (c == ' ') {
      *q++ = '+';
    } else {
      *q++ = '%';
      *q++ = hex[c >> 4];
      *q++ = hex[c & 15];
    }
  }
  *dest = q;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Resource Owner Password Credentials request */
enum c_abstract_http_error http_request_init_oauth2_password_grant(
    struct HttpRequest *req, const char *token_endpoint_url,
    const char *username, const char *password, const char *client_id,
    const char *client_secret, const char *scope) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;
  const char *grant_type = "password";

  if (!req || !token_endpoint_url || !username || !password)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(token_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* Calculate length */
  body_len += 11 + math_urlencode_len(grant_type); /* grant_type=... */
  body_len += 10 + math_urlencode_len(username);   /* &username=... */
  body_len += 10 + math_urlencode_len(password);   /* &password=... */

  if (client_id)
    body_len += 11 + math_urlencode_len(client_id); /* &client_id=... */
  if (client_secret)
    body_len += 15 + math_urlencode_len(client_secret); /* &client_secret=... */
  if (scope)
    body_len += 7 + math_urlencode_len(scope); /* &scope=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  /* grant_type=password */
  memcpy(p, "grant_type=", 11);
  p += 11;
  rc = urlencode_append(&p, grant_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  /* &username=... */
  memcpy(p, "&username=", 10);
  p += 10;
  rc = urlencode_append(&p, username);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  /* &password=... */
  memcpy(p, "&password=", 10);
  p += 10;
  rc = urlencode_append(&p, password);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (client_id) {
    memcpy(p, "&client_id=", 11);
    p += 11;
    rc = urlencode_append(&p, client_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_secret) {
    memcpy(p, "&client_secret=", 15);
    p += 15;
    rc = urlencode_append(&p, client_secret);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (scope) {
    memcpy(p, "&scope=", 7);
    p += 7;
    rc = urlencode_append(&p, scope);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Refresh Token grant request */
enum c_abstract_http_error http_request_init_oauth2_refresh_token_grant(
    struct HttpRequest *req, const char *token_endpoint_url,
    const char *refresh_token, const char *client_id, const char *client_secret,
    const char *scope) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;
  const char *grant_type = "refresh_token";

  if (!req || !token_endpoint_url || !refresh_token)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(token_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* Calculate length */
  body_len += 11 + math_urlencode_len(grant_type);    /* grant_type=... */
  body_len += 15 + math_urlencode_len(refresh_token); /* &refresh_token=... */

  if (client_id)
    body_len += 11 + math_urlencode_len(client_id); /* &client_id=... */
  if (client_secret)
    body_len += 15 + math_urlencode_len(client_secret); /* &client_secret=... */
  if (scope)
    body_len += 7 + math_urlencode_len(scope); /* &scope=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  /* grant_type=refresh_token */
  memcpy(p, "grant_type=", 11);
  p += 11;
  rc = urlencode_append(&p, grant_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  /* &refresh_token=... */
  memcpy(p, "&refresh_token=", 15);
  p += 15;
  rc = urlencode_append(&p, refresh_token);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (client_id) {
    memcpy(p, "&client_id=", 11);
    p += 11;
    rc = urlencode_append(&p, client_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_secret) {
    memcpy(p, "&client_secret=", 15);
    p += 15;
    rc = urlencode_append(&p, client_secret);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (scope) {
    memcpy(p, "&scope=", 7);
    p += 7;
    rc = urlencode_append(&p, scope);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Authorization Code grant request */
enum c_abstract_http_error http_request_init_oauth2_authorization_code_grant(
    struct HttpRequest *req, const char *token_endpoint_url, const char *code,
    const char *redirect_uri, const char *client_id, const char *client_secret,
    const char *code_verifier) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;
  const char *grant_type = "authorization_code";

  if (!req || !token_endpoint_url || !code)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(token_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* Calculate length */
  body_len += 11 + math_urlencode_len(grant_type); /* grant_type=... */
  body_len += 6 + math_urlencode_len(code);        /* &code=... */

  if (redirect_uri)
    body_len += 14 + math_urlencode_len(redirect_uri); /* &redirect_uri=... */
  if (client_id)
    body_len += 11 + math_urlencode_len(client_id); /* &client_id=... */
  if (client_secret)
    body_len += 15 + math_urlencode_len(client_secret); /* &client_secret=... */
  if (code_verifier)
    body_len += 15 + math_urlencode_len(code_verifier); /* &code_verifier=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  /* grant_type=authorization_code */
  memcpy(p, "grant_type=", 11);
  p += 11;
  rc = urlencode_append(&p, grant_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  /* &code=... */
  memcpy(p, "&code=", 6);
  p += 6;
  rc = urlencode_append(&p, code);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (redirect_uri) {
    memcpy(p, "&redirect_uri=", 14);
    p += 14;
    rc = urlencode_append(&p, redirect_uri);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_id) {
    memcpy(p, "&client_id=", 11);
    p += 11;
    rc = urlencode_append(&p, client_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_secret) {
    memcpy(p, "&client_secret=", 15);
    p += 15;
    rc = urlencode_append(&p, client_secret);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (code_verifier) {
    memcpy(p, "&code_verifier=", 15);
    p += 15;
    rc = urlencode_append(&p, code_verifier);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Client Credentials grant request */
enum c_abstract_http_error http_request_init_oauth2_client_credentials_grant(
    struct HttpRequest *req, const char *token_endpoint_url,
    const char *client_id, const char *client_secret, const char *scope) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;
  const char *grant_type = "client_credentials";

  if (!req || !token_endpoint_url)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(token_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* Calculate length */
  body_len += 11 + math_urlencode_len(grant_type); /* grant_type=... */

  if (client_id)
    body_len += 11 + math_urlencode_len(client_id); /* &client_id=... */
  if (client_secret)
    body_len += 15 + math_urlencode_len(client_secret); /* &client_secret=... */
  if (scope)
    body_len += 7 + math_urlencode_len(scope); /* &scope=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  /* grant_type=client_credentials */
  memcpy(p, "grant_type=", 11);
  p += 11;
  rc = urlencode_append(&p, grant_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (client_id) {
    memcpy(p, "&client_id=", 11);
    p += 11;
    rc = urlencode_append(&p, client_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_secret) {
    memcpy(p, "&client_secret=", 15);
    p += 15;
    rc = urlencode_append(&p, client_secret);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (scope) {
    memcpy(p, "&scope=", 7);
    p += 7;
    rc = urlencode_append(&p, scope);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 JWT Bearer grant request */
enum c_abstract_http_error http_request_init_oauth2_jwt_bearer_grant(
    struct HttpRequest *req, const char *token_endpoint_url,
    const char *assertion, const char *scope) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;
  const char *grant_type = "urn:ietf:params:oauth:grant-type:jwt-bearer";

  if (!req || !token_endpoint_url || !assertion)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(token_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  /* Calculate length */
  body_len += 11 + math_urlencode_len(grant_type); /* grant_type=... */
  body_len += 11 + math_urlencode_len(assertion);  /* &assertion=... */

  if (scope)
    body_len += 7 + math_urlencode_len(scope); /* &scope=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  /* grant_type=... */
  memcpy(p, "grant_type=", 11);
  p += 11;
  rc = urlencode_append(&p, grant_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  /* &assertion=... */
  memcpy(p, "&assertion=", 11);
  p += 11;
  rc = urlencode_append(&p, assertion);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (scope) {
    memcpy(p, "&scope=", 7);
    p += 7;
    rc = urlencode_append(&p, scope);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_request_init_oauth2_device_authorization_request(
    struct HttpRequest *req, const char *device_endpoint_url,
    const char *client_id, const char *scope) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;

  if (!req || !device_endpoint_url || !client_id)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(device_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  body_len += 10 + math_urlencode_len(client_id); /* client_id=... */
  if (scope)
    body_len += 7 + math_urlencode_len(scope); /* &scope=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  memcpy(p, "client_id=", 10);
  p += 10;
  rc = urlencode_append(&p, client_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (scope) {
    memcpy(p, "&scope=", 7);
    p += 7;
    rc = urlencode_append(&p, scope);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Device Access Token poll request */
enum c_abstract_http_error http_request_init_oauth2_device_access_token_request(
    struct HttpRequest *req, const char *token_endpoint_url,
    const char *client_id, const char *device_code) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;
  const char *grant_type = "urn:ietf:params:oauth:grant-type:device_code";

  if (!req || !token_endpoint_url || !client_id || !device_code)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(token_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  body_len += 11 + math_urlencode_len(grant_type);  /* grant_type=... */
  body_len += 11 + math_urlencode_len(client_id);   /* &client_id=... */
  body_len += 13 + math_urlencode_len(device_code); /* &device_code=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  memcpy(p, "grant_type=", 11);
  p += 11;
  rc = urlencode_append(&p, grant_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  memcpy(p, "&client_id=", 11);
  p += 11;
  rc = urlencode_append(&p, client_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  memcpy(p, "&device_code=", 13);
  p += 13;
  rc = urlencode_append(&p, device_code);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Token Revocation request */
enum c_abstract_http_error http_request_init_oauth2_token_revocation(
    struct HttpRequest *req, const char *revocation_endpoint_url,
    const char *token, const char *token_type_hint, const char *client_id,
    const char *client_secret) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;

  if (!req || !revocation_endpoint_url || !token)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(revocation_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  body_len += 6 + math_urlencode_len(token); /* token=... */

  if (token_type_hint)
    body_len +=
        17 + math_urlencode_len(token_type_hint); /* &token_type_hint=... */
  if (client_id)
    body_len += 11 + math_urlencode_len(client_id); /* &client_id=... */
  if (client_secret)
    body_len += 15 + math_urlencode_len(client_secret); /* &client_secret=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  memcpy(p, "token=", 6);
  p += 6;
  rc = urlencode_append(&p, token);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (token_type_hint) {
    memcpy(p, "&token_type_hint=", 17);
    p += 17;
    rc = urlencode_append(&p, token_type_hint);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_id) {
    memcpy(p, "&client_id=", 11);
    p += 11;
    rc = urlencode_append(&p, client_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_secret) {
    memcpy(p, "&client_secret=", 15);
    p += 15;
    rc = urlencode_append(&p, client_secret);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize an OAuth2 Token Introspection request */
enum c_abstract_http_error http_request_init_oauth2_token_introspection(
    struct HttpRequest *req, const char *introspection_endpoint_url,
    const char *token, const char *token_type_hint, const char *client_id,
    const char *client_secret) {
  enum c_abstract_http_error rc;
  size_t body_len = 0;
  char *body = NULL;
  char *p = NULL;

  if (!req || !introspection_endpoint_url || !token)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  req->url = NULL;
  rc = c_abstract_http_strdup(introspection_endpoint_url, &req->url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  req->method = HTTP_POST;
  req->ws_ctx = NULL;
  req->sse_ctx = NULL;

  rc = http_headers_add(&req->headers, "Content-Type",
                        "application/x-www-form-urlencoded");
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;

  body_len += 6 + math_urlencode_len(token); /* token=... */

  if (token_type_hint)
    body_len +=
        17 + math_urlencode_len(token_type_hint); /* &token_type_hint=... */
  if (client_id)
    body_len += 11 + math_urlencode_len(client_id); /* &client_id=... */
  if (client_secret)
    body_len += 15 + math_urlencode_len(client_secret); /* &client_secret=... */

  body = (char *)malloc(body_len + 1);
  if (!body)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = body;
  memcpy(p, "token=", 6);
  p += 6;
  rc = urlencode_append(&p, token);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(body);
    return rc;
  }

  if (token_type_hint) {
    memcpy(p, "&token_type_hint=", 17);
    p += 17;
    rc = urlencode_append(&p, token_type_hint);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_id) {
    memcpy(p, "&client_id=", 11);
    p += 11;
    rc = urlencode_append(&p, client_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  if (client_secret) {
    memcpy(p, "&client_secret=", 15);
    p += 15;
    rc = urlencode_append(&p, client_secret);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body);
      return rc;
    }
  }

  *p = '\0';
  req->body = body;
  req->body_len = body_len;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Build an OAuth2 Authorization URL */
enum c_abstract_http_error http_oauth2_build_authorization_url(
    const char *auth_endpoint, const char *client_id, const char *response_type,
    const char *redirect_uri, const char *scope, const char *state,
    const char *code_challenge, const char *code_challenge_method,
    char **out_url) {
  enum c_abstract_http_error rc;
  size_t len = 0;
  char *url = NULL;
  char *p = NULL;
  int has_query = 0;

  if (!auth_endpoint || !client_id || !response_type || !out_url)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  has_query = (strchr(auth_endpoint, '?') != NULL) ? 1 : 0;

  len = strlen(auth_endpoint);
  len += 1 + 14 + math_urlencode_len(response_type); /* ?response_type=... */
  len += 11 + math_urlencode_len(client_id);         /* &client_id=... */

  if (redirect_uri)
    len += 14 + math_urlencode_len(redirect_uri); /* &redirect_uri=... */
  if (scope)
    len += 7 + math_urlencode_len(scope); /* &scope=... */
  if (state)
    len += 7 + math_urlencode_len(state); /* &state=... */
  if (code_challenge)
    len += 16 + math_urlencode_len(code_challenge); /* &code_challenge=... */
  if (code_challenge_method)
    len += 23 + math_urlencode_len(
                    code_challenge_method); /* &code_challenge_method=... */

  url = (char *)malloc(len + 1);
  if (!url)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = url;
  memcpy(p, auth_endpoint, strlen(auth_endpoint));
  p += strlen(auth_endpoint);

  *p++ = has_query ? '&' : '?';
  memcpy(p, "response_type=", 14);
  p += 14;
  rc = urlencode_append(&p, response_type);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(url);
    return rc;
  }

  memcpy(p, "&client_id=", 11);
  p += 11;
  rc = urlencode_append(&p, client_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(url);
    return rc;
  }

  if (redirect_uri) {
    memcpy(p, "&redirect_uri=", 14);
    p += 14;
    rc = urlencode_append(&p, redirect_uri);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(url);
      return rc;
    }
  }

  if (scope) {
    memcpy(p, "&scope=", 7);
    p += 7;
    rc = urlencode_append(&p, scope);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(url);
      return rc;
    }
  }

  if (state) {
    memcpy(p, "&state=", 7);
    p += 7;
    rc = urlencode_append(&p, state);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(url);
      return rc;
    }
  }

  if (code_challenge) {
    memcpy(p, "&code_challenge=", 16);
    p += 16;
    rc = urlencode_append(&p, code_challenge);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(url);
      return rc;
    }
  }

  if (code_challenge_method) {
    memcpy(p, "&code_challenge_method=", 23);
    p += 23;
    rc = urlencode_append(&p, code_challenge_method);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(url);
      return rc;
    }
  }

  *p = '\0';
  *out_url = url;

  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(_WIN32)
/** @brief Portable socket close function alias on Windows */
#define ABSTRACT_HTTP_CLOSESOCKET closesocket
/** @brief Portable socket handle type on Windows */
typedef SOCKET abstract_http_socket_t;
/** @brief Invalid socket sentinel value on Windows */
#define ABSTRACT_HTTP_INVALID_SOCKET_VAL INVALID_SOCKET
/** @brief Socket error sentinel value on Windows */
#define ABSTRACT_HTTP_SOCKET_ERROR_VAL SOCKET_ERROR
#else
/** @brief Portable socket close function alias on POSIX */
#define ABSTRACT_HTTP_CLOSESOCKET close
/** @brief Portable socket descriptor type on POSIX */
typedef int abstract_http_socket_t;
/** @brief Invalid socket sentinel value on POSIX */
#define ABSTRACT_HTTP_INVALID_SOCKET_VAL -1
/** @brief Socket error sentinel value on POSIX */
#define ABSTRACT_HTTP_SOCKET_ERROR_VAL -1
#endif

/**
 * @brief URL-decode a bounded substring into a newly allocated NUL-terminated
 * string.
 * @param src Input percent-encoded string.
 * @param src_len Number of bytes in src to decode.
 * @param out Output pointer receiving decoded string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error urldecode_alloc(const char *src,
                                                  size_t src_len, char **out);

/**
 * @brief Test wrapper exposing urldecode_alloc for unit testing.
 * @param src Input percent-encoded string.
 * @param src_len Number of bytes in src to decode.
 * @param out Output pointer receiving decoded string.
 * @return Result from urldecode_alloc.
 */
C_ABSTRACT_HTTP_API enum c_abstract_http_error
abstract_http_test_urldecode_alloc(const char *src, size_t src_len, char **out);
/**
 * @brief Test wrapper exposing urldecode_alloc for unit testing.
 * @param src Input percent-encoded string.
 * @param src_len Number of bytes in src to decode.
 * @param out Output pointer receiving decoded string.
 * @return Result from urldecode_alloc.
 */
C_ABSTRACT_HTTP_API enum c_abstract_http_error
abstract_http_test_urldecode_alloc(const char *src, size_t src_len,
                                   char **out) {
  return urldecode_alloc(src, src_len, out);
}

/**
 * @brief URL-decode a bounded substring into a newly allocated NUL-terminated
 * string.
 * @param src Input percent-encoded string.
 * @param src_len Number of bytes in src to decode.
 * @param out Output pointer receiving decoded string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error urldecode_alloc(const char *src,
                                                  size_t src_len, char **out) {
  char *dst = (char *)malloc(src_len + 1);
  size_t i, j = 0;

  if (!dst)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  for (i = 0; i < src_len; i++) {
    if (src[i] == '%') {
      if (i + 2 < src_len) {
        int a = (unsigned char)src[i + 1];
        int b = (unsigned char)src[i + 2];
        a = (a >= '0' && a <= '9') ? a - '0'

                                   : (tolower(a) - 'a' + 10);
        b = (b >= '0' && b <= '9') ? b - '0'

                                   : (tolower(b) - 'a' + 10);
        dst[j++] = (char)((a << 4) | b);
        i += 2;
      }
    } else if (src[i] == '+') {
      dst[j++] = ' ';
    } else {
      dst[j++] = src[i];
    }
  }
  dst[j] = '\0';
  *out = dst;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse an OAuth2 redirect HTTP request line, URL, or query string.
 * @param input Raw HTTP request (`GET /cb?... HTTP/1.1`), redirect URL, or
 * query string.
 * @param require_http_get Non-zero to require `"GET "` prefix (socket mode).
 * @param out_code Optional output pointer for `code`.
 * @param out_state Optional output pointer for `state`.
 * @param out_error Optional output pointer for `error`.
 * @param out_error_desc Optional output pointer for `error_description`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
parse_oauth2_redirect_payload(const char *input, int require_http_get,
                              char **out_code, char **out_state,
                              char **out_error, char **out_error_desc) {
  const char *p;
  enum c_abstract_http_error rc;

  if (require_http_get || strncmp(input, "GET ", 4) == 0) {
    if (strncmp(input, "GET ", 4) != 0) {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    p = input + 4;
    while (*p && *p != '?' && *p != ' ' && *p != '\r' && *p != '\n') {
      p++;
    }
    if (*p != '?') {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    p++;
  } else {
    const char *q = strchr(input, '?');
    if (q) {
      p = q + 1;
    } else if (strchr(input, '=')) {
      p = input;
    } else {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
  }

  while (*p && *p != ' ' && *p != '\r' && *p != '\n' && *p != '#') {
    const char *key = p;
    const char *val = NULL;
    size_t key_len = 0;
    size_t val_len = 0;

    while (*p && *p != '=' && *p != '&' && *p != ' ' && *p != '\r' &&
           *p != '\n' && *p != '#') {
      p++;
    }
    key_len = (size_t)(p - key);

    if (*p == '=') {
      p++;
      val = p;
      while (*p && *p != '&' && *p != ' ' && *p != '\r' && *p != '\n' &&
             *p != '#') {
        p++;
      }
      val_len = (size_t)(p - val);
    }

    if (key_len == 4 && strncmp(key, "code", 4) == 0 && out_code && val) {
      free(*out_code);
      *out_code = NULL;
      rc = urldecode_alloc(val, val_len, out_code);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    } else if (key_len == 5 && strncmp(key, "state", 5) == 0 && out_state &&
               val) {
      free(*out_state);
      *out_state = NULL;
      rc = urldecode_alloc(val, val_len, out_state);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    } else if (key_len == 5 && strncmp(key, "error", 5) == 0 && out_error &&
               val) {
      free(*out_error);
      *out_error = NULL;
      rc = urldecode_alloc(val, val_len, out_error);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    } else if (key_len == 17 && strncmp(key, "error_description", 17) == 0 &&
               out_error_desc && val) {
      free(*out_error_desc);
      *out_error_desc = NULL;
      rc = urldecode_alloc(val, val_len, out_error_desc);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    }

    if (*p == '&') {
      p++;
    }
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Optional test-only override for `CAH_OAUTH2_REDIRECT` */
static const char *g_test_oauth2_redirect_override = NULL;

/**
 * @brief Configure test-only override value for `CAH_OAUTH2_REDIRECT`.
 * @param val Override string, or NULL to clear.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_test_set_oauth2_redirect_env(const char *val);

C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_test_set_oauth2_redirect_env(const char *val) {
  g_test_oauth2_redirect_override = val;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Read an environment variable into a heap-duplicated string safely.
 * @param name Environment variable name.
 * @param out_val Output pointer receiving heap string, or NULL if unset/empty.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error dup_env_var(const char *name,
                                              char **out_val) {
  *out_val = NULL;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_test_oauth2_redirect_override &&
      strcmp(name, "CAH_OAUTH2_REDIRECT") == 0) {
    if (g_test_oauth2_redirect_override[0] == '\0') {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    return c_abstract_http_strdup(g_test_oauth2_redirect_override, out_val);
  }
#endif
  {
    char *buf = NULL;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name) == 0 && buf != NULL) {
      if (buf[0] == '\0') {
        free(buf);
        return C_ABSTRACT_HTTP_SUCCESS;
      }
      *out_val = buf;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#else
  {
    const char *v = getenv(name);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_test_oauth2_redirect_override &&
        strcmp(name, "CAH_OAUTH2_REDIRECT") == 0) {
      v = g_test_oauth2_redirect_override;
    }
#endif
    if (!v || v[0] == '\0') {
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    return c_abstract_http_strdup(v, out_val);
  }
#endif
}

enum c_abstract_http_error
http_oauth2_localhost_intercept(unsigned short port, const char *html_response,
                                char **out_code, char **out_state,
                                char **out_error, char **out_error_desc) {
  char *env_redir = NULL;
  enum c_abstract_http_error env_rc;

  if (out_code)
    *out_code = NULL;
  if (out_state)
    *out_state = NULL;
  if (out_error)
    *out_error = NULL;
  if (out_error_desc)
    *out_error_desc = NULL;

  env_rc = dup_env_var("CAH_OAUTH2_REDIRECT", &env_redir);
  if (env_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return env_rc;
  }
  if (env_redir) {
    enum c_abstract_http_error prc;
    prc = parse_oauth2_redirect_payload(env_redir, 0, out_code, out_state,
                                        out_error, out_error_desc);
    if (prc != C_ABSTRACT_HTTP_SUCCESS) {
      free(env_redir);
      return prc;
    }
    free(env_redir);
    return C_ABSTRACT_HTTP_SUCCESS;
  }

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#if defined(C_ABSTRACT_HTTP_DOS_WATT32)
  {
    tcp_Socket sock;
    char buf[2048];
    int status = 0;
    int n = 0;
    enum c_abstract_http_error rc;

    if (sock_init() != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (!tcp_listen(&sock, port, 0, 0, NULL, 0)) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    sock_wait_established(&sock, sock_delay, NULL, &status);
    sock_tick(&sock, &status);
    while (!sock_dataready(&sock)) {
      sock_tick(&sock, &status);
    }
    n = sock_fastread(&sock, (unsigned char *)buf, (int)sizeof(buf) - 1);
    if (n <= 0) {
      sock_close(&sock);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    buf[n] = '\0';
    if (html_response) {
      sock_puts(&sock, (const unsigned char *)html_response);
    }
    sock_close(&sock);
    rc = parse_oauth2_redirect_payload(buf, 1, out_code, out_state, out_error,
                                       out_error_desc);
    return rc;
  }
#else
  {
    char *stdio_mode = NULL;
    enum c_abstract_http_error src;
    (void)port;
    (void)html_response;
    src = dup_env_var("CAH_OAUTH2_STDIO", &stdio_mode);
    if (src != C_ABSTRACT_HTTP_SUCCESS) {
      return src;
    }
    if (stdio_mode) {
      char line[1024];
      free(stdio_mode);
      if (!fgets(line, (int)sizeof(line), stdin)) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      return parse_oauth2_redirect_payload(line, 0, out_code, out_state,
                                           out_error, out_error_desc);
    }
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
#endif
#else
  {
    abstract_http_socket_t srv_sock = ABSTRACT_HTTP_INVALID_SOCKET_VAL,
                           cli_sock = ABSTRACT_HTTP_INVALID_SOCKET_VAL;
    struct sockaddr_in saddr;
    char buf[4096];
    enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
    int n;
    int opt = 1;

#if defined(_WIN32)
    {
      WSADATA wsa;
      if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return C_ABSTRACT_HTTP_ERR_IO;
    }
#endif

    srv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_sock == ABSTRACT_HTTP_INVALID_SOCKET_VAL) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
               sizeof(opt));

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = (unsigned short)((((unsigned short)(port) >> 8) & 0xff) |
                                      (((unsigned short)(port) & 0xff) << 8));
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(srv_sock, (struct sockaddr *)&saddr, sizeof(saddr)) ==
        ABSTRACT_HTTP_SOCKET_ERROR_VAL) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    if (listen(srv_sock, 1) == ABSTRACT_HTTP_SOCKET_ERROR_VAL) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    cli_sock = accept(srv_sock, NULL, NULL);
    if (cli_sock == ABSTRACT_HTTP_INVALID_SOCKET_VAL) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    n = (int)recv(cli_sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
    buf[n] = '\0';

    if (html_response) {
#if defined(_WIN32)
      send(cli_sock, html_response, (int)strlen(html_response), 0);
#else
      send(cli_sock, html_response, (size_t)strlen(html_response), 0);
#endif
    }

    rc = parse_oauth2_redirect_payload(buf, 1, out_code, out_state, out_error,
                                       out_error_desc);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup;
    }

  cleanup:
    if (cli_sock != ABSTRACT_HTTP_INVALID_SOCKET_VAL)
      ABSTRACT_HTTP_CLOSESOCKET(cli_sock);
    if (srv_sock != ABSTRACT_HTTP_INVALID_SOCKET_VAL)
      ABSTRACT_HTTP_CLOSESOCKET(srv_sock);
#if defined(_WIN32)
    WSACleanup();
#endif
    return rc;
  }
#endif
}

/** @brief Initialize an HTTP response structure */
enum c_abstract_http_error http_response_init(struct HttpResponse *res) {
  if (!res)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  res->status_code = 0;
  res->body = NULL;
  res->body_len = 0;
  return http_headers_init(&res->headers);
}

/** @brief Free an HTTP response structure */
enum c_abstract_http_error http_response_free(struct HttpResponse *res) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_response_free_fail)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (!res)
    return C_ABSTRACT_HTTP_SUCCESS;
  if (res->body) {
    free(res->body);
    res->body = NULL;
  }
  {
    enum c_abstract_http_error rch = http_headers_free(&res->headers);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Save an HTTP response body to a file */
enum c_abstract_http_error
http_response_save_to_file(const struct HttpResponse *res, const char *path) {
  FILE *f = NULL;
  size_t written;

  if (!res || !path)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  if (!res->body && res->body_len > 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  /* MSVC Safe Path */
  fopen_s(&f, path, "wb");
#else
  /* Standard POSIX / GCC Path */
  f = fopen(path, "wb");
#endif
  if (!f)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (res->body_len > 0) {
    written = fwrite(res->body, 1, res->body_len, f);
    if (written != res->body_len) {
      fclose(f);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  if (fclose(f) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;

  return C_ABSTRACT_HTTP_SUCCESS;
}

#ifndef CAH_MATH_TIME_DECLARED
/** @brief Guard macro for math_get_current_time_ms declaration */
#define CAH_MATH_TIME_DECLARED 1
/** @brief Return current monotonic/wall time in milliseconds */
abstract_http_int64_t math_get_current_time_ms(void);
/** @brief Real monotonic/wall time implementation */
C_ABSTRACT_HTTP_API abstract_http_int64_t real_math_get_current_time_ms(void);
#ifndef math_get_current_time_ms
/** @brief Default mapping of math_get_current_time_ms to real implementation */
#define math_get_current_time_ms real_math_get_current_time_ms
#endif
#endif

/**
 * @brief Build a normalized joined URL from `base_url` and relative `rel_url`.
 * @param base_url Base URL prefix.
 * @param rel_url Relative path suffix.
 * @param out_url Output pointer receiving newly allocated joined URL.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error client_join_base_url(const char *base_url,
                                                       const char *rel_url,
                                                       char **out_url) {
  size_t blen = strlen(base_url);
  size_t rlen;
  char *buf;
  while (blen > 0 && base_url[blen - 1] == '/') {
    blen--;
  }
  while (*rel_url == '/') {
    rel_url++;
  }
  rlen = strlen(rel_url);
  buf = (char *)malloc(blen + 1 + rlen + 1);
  if (!buf) {
    *out_url = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(buf, base_url, blen);
  buf[blen] = '/';
  memcpy(buf + blen + 1, rel_url, rlen);
  buf[blen + 1 + rlen] = '\0';
  *out_url = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Pure helper computing retry delay in milliseconds from policy and
 * response headers.
 * @param policy Configured retry policy.
 * @param attempt Zero-based retry attempt index.
 * @param res Optional HTTP response inspected for `Retry-After` seconds.
 * @return Non-negative delay in milliseconds.
 */
static long math_compute_retry_delay_ms(enum HttpRetryPolicy policy,
                                        size_t attempt,
                                        const struct HttpResponse *res) {
  if (res) {
    size_t i;
    for (i = 0; i < res->headers.count; ++i) {
      const char *k = res->headers.headers[i].key;
      const char *v = res->headers.headers[i].value;
      if (k && v && strlen(k) == 11 && tolower((unsigned char)k[0]) == 'r' &&
          tolower((unsigned char)k[6]) == 'a') {
        long sec = atol(v);
        if (sec > 0) {
          return sec * 1000L;
        }
      }
    }
  }
  if (policy == HTTP_RETRY_EXPONENTIAL) {
    size_t shift = attempt < 6 ? attempt : 6;
    return (long)(1u << shift);
  }
  return (long)(attempt + 1);
}

/** @brief Send a single HTTP request through the unified client pipeline */
enum c_abstract_http_error http_client_send(struct HttpClient *client,
                                            struct HttpRequest *req,
                                            struct HttpResponse **res) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *orig_url = NULL;
  char *resolved_url = NULL;
  char *cookie_hdr = NULL;
  int added_cookie = 0;
  size_t max_retries = 0;
  size_t attempt = 0;
  struct HttpResponse *attempt_res = NULL;

  if (!client || !req || !res || !req->url) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = NULL;
  if (!client->send) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  orig_url = req->url;
  if (client->base_url && !strstr(req->url, "://")) {
    rc = client_join_base_url(client->base_url, req->url, &resolved_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup;
    }
    req->url = resolved_url;
  }

  if (req->parts.count > 0 && !req->body) {
    rc = http_request_flatten_parts(req);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup;
    }
  }

  if (client->config.cookie_jar) {
    rc = http_cookie_jar_build_header(client->config.cookie_jar, req->url,
                                      &cookie_hdr);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup;
    }
    if (cookie_hdr) {
      size_t hi;
      int has_cookie_hdr = 0;
      for (hi = 0; hi < req->headers.count; ++hi) {
        int diff = 0;
        rc = strcasecmp_portable(req->headers.headers[hi].key, "Cookie", &diff);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          goto cleanup;
        }
        if (diff == 0) {
          has_cookie_hdr = 1;
          break;
        }
      }
      if (!has_cookie_hdr) {
        rc = http_headers_add(&req->headers, "Cookie", cookie_hdr);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          goto cleanup;
        }
        added_cookie = 1;
      }
    }
  }

  if (client->config.retry_policy != HTTP_RETRY_NONE &&
      client->config.retry_count > 0) {
    max_retries = (size_t)client->config.retry_count;
  }

  for (attempt = 0; attempt <= max_retries; ++attempt) {
    int should_retry = 0;
    attempt_res = NULL;
    rc = client->send(client->transport, req, &attempt_res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      if (attempt < max_retries &&
          (rc == C_ABSTRACT_HTTP_ERR_IO || rc == C_ABSTRACT_HTTP_ERR_TIMEOUT)) {
        should_retry = 1;
      } else {
        goto cleanup;
      }
    } else if (attempt < max_retries && attempt_res &&
               (attempt_res->status_code == 429 ||
                attempt_res->status_code == 502 ||
                attempt_res->status_code == 503 ||
                attempt_res->status_code == 504)) {
      should_retry = 1;
    }
    if (should_retry) {
      long delay_ms = math_compute_retry_delay_ms(client->config.retry_policy,
                                                  attempt, attempt_res);
      abstract_http_int64_t t0 = math_get_current_time_ms();
      if (attempt_res) {
        rc = http_response_free(attempt_res);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(attempt_res);
          attempt_res = NULL;
          goto cleanup;
        }
        free(attempt_res);
        attempt_res = NULL;
      }
      while (math_get_current_time_ms() - t0 <
             (abstract_http_int64_t)delay_ms) {
      }
      continue;
    }
    break;
  }

  if (rc == C_ABSTRACT_HTTP_SUCCESS && attempt_res &&
      client->config.cookie_jar) {
    size_t hi;
    for (hi = 0; hi < attempt_res->headers.count; ++hi) {
      int diff = 0;
      rc = strcasecmp_portable(attempt_res->headers.headers[hi].key,
                               "Set-Cookie", &diff);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup;
      }
      if (diff == 0 && attempt_res->headers.headers[hi].value) {
        rc = http_cookie_jar_parse_set_cookie(
            client->config.cookie_jar, attempt_res->headers.headers[hi].value);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          goto cleanup;
        }
      }
    }
  }

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    *res = attempt_res;
    attempt_res = NULL;
  }

cleanup:
  if (added_cookie && req->headers.count > 0) {
    size_t ci = req->headers.count - 1;
    free(req->headers.headers[ci].key);
    free(req->headers.headers[ci].value);
    req->headers.count--;
  }
  req->url = orig_url;
  free(resolved_url);
  free(cookie_hdr);
  if (attempt_res) {
    enum c_abstract_http_error rf_rc;
    rf_rc = http_response_free(attempt_res);
    if (rf_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(attempt_res);
      return rf_rc;
    }
    free(attempt_res);
  }
  return rc;
}

/**
 * @brief Pure predicate checking whether an HttpFuture was pre-cancelled.
 * @param future Future slot to inspect.
 * @return 1 if cancelled, 0 otherwise.
 */
static int math_is_future_cancelled(const struct HttpFuture *future) {
  if (future) {
    struct HttpFutureState *st = NULL;
    if (math_find_future_state(future->internal_state, &st)) {
      return st->is_cancelled;
    }
  }
  return 0;
}

/**
 * @brief Record completion on an HttpFuture slot, signaling any awaiters.
 * @param future Target future slot.
 * @param res Resulting response pointer (NULL on error).
 * @param req_rc Resulting request status enum.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization error enum.
 */
static enum c_abstract_http_error
http_future_complete_slot(struct HttpFuture *future, struct HttpResponse *res,
                          enum c_abstract_http_error req_rc) {
  struct HttpFutureState *st = NULL;
  if (math_find_future_state(future->internal_state, &st)) {
    enum c_abstract_http_error rc;
    rc = abstract_http_mutex_lock(st->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    future->response = res;
    future->error_code = (int)req_rc;
    future->is_ready = 1;
    return sync_broadcast_and_unlock(st->mutex, st->cond, 1);
  }
  future->response = res;
  future->error_code = (int)req_rc;
  future->is_ready = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Dispatch a single batch request through middleware or raw transport.
 * @param client HTTP client instance.
 * @param req Request to send.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or transport error enum.
 */
static enum c_abstract_http_error
multi_dispatch_slot_send(struct HttpClient *client, struct HttpRequest *req,
                         struct HttpResponse **res) {
  if (req && req->url &&
      (client->base_url || client->config.cookie_jar || req->parts.count > 0 ||
       (client->config.retry_policy != HTTP_RETRY_NONE &&
        client->config.retry_count > 0))) {
    return http_client_send(client, req, res);
  }
  if (!client->send) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  return client->send(client->transport, req, res);
}

/**
 * @brief Complete an `HttpFuture` slot and optionally propagate request
 * failure.
 * @param future Target future slot.
 * @param res Resulting response pointer (NULL on error).
 * @param req_rc Request execution status enum.
 * @param fail_fast Non-zero to return `req_rc` when `req_rc != SUCCESS`.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization/request error enum.
 */
static enum c_abstract_http_error
multi_complete_slot_checked(struct HttpFuture *future, struct HttpResponse *res,
                            enum c_abstract_http_error req_rc, int fail_fast) {
  enum c_abstract_http_error slot_rc;
  slot_rc = http_future_complete_slot(future, res, req_rc);
  if (slot_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return slot_rc;
  }
  if (fail_fast && req_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return req_rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Execute a single request slot for batch modalities, honoring
 * cancellation and client middleware.
 * @param client HTTP client instance.
 * @param req Request to execute.
 * @param future Target future slot.
 * @param fail_fast Non-zero to return request failure enum immediately.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization/request error enum.
 */
static enum c_abstract_http_error
multi_execute_single_slot(struct HttpClient *client, struct HttpRequest *req,
                          struct HttpFuture *future, int fail_fast) {
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error req_rc;
  if (math_is_future_cancelled(future)) {
    return multi_complete_slot_checked(future, NULL,
                                       C_ABSTRACT_HTTP_ERR_TIMEOUT, fail_fast);
  }
  req_rc = multi_dispatch_slot_send(client, req, &res);
  if (req_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_complete_slot_checked(future, res, req_rc, fail_fast);
  }
  return multi_complete_slot_checked(future, res, C_ABSTRACT_HTTP_SUCCESS, 0);
}

/** @brief Shared completion barrier for thread-pool batch execution */
struct MultiPoolBarrier {
  /** @brief Mutex protecting remaining counter and worker_err */
  struct AbstractHttpMutex *mutex;
  /** @brief Condition variable signaled when remaining reaches 0 */
  struct AbstractHttpCond *cond;
  /** @brief Number of outstanding tasks */
  size_t remaining;
  /** @brief First synchronization error observed on workers */
  enum c_abstract_http_error worker_err;
};

/** @brief Per-request work item for thread-pool, coroutine, and actor batches
 */
struct MultiBatchItem {
  /** @brief Client executing the request */
  struct HttpClient *client;
  /** @brief Request pointer */
  struct HttpRequest *req;
  /** @brief Future receiving result */
  struct HttpFuture *future;
  /** @brief Optional thread-pool barrier */
  struct MultiPoolBarrier *barrier;
};

/**
 * @brief Decrement thread-pool batch barrier and broadcast on final completion.
 * @param barrier Shared completion barrier.
 * @param slot_rc Synchronization status from worker slot execution.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization error enum.
 */
static enum c_abstract_http_error
multi_pool_finish_barrier(struct MultiPoolBarrier *barrier,
                          enum c_abstract_http_error slot_rc) {
  enum c_abstract_http_error l_rc;
  int notify_done;
  l_rc = abstract_http_mutex_lock(barrier->mutex);
  if (l_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return l_rc;
  }
  if (slot_rc != C_ABSTRACT_HTTP_SUCCESS &&
      barrier->worker_err == C_ABSTRACT_HTTP_SUCCESS) {
    barrier->worker_err = slot_rc;
  }
  barrier->remaining--;
  notify_done = (barrier->remaining == 0);
  return sync_broadcast_and_unlock(barrier->mutex, barrier->cond, notify_done);
}

/**
 * @brief Worker task callback for `MODALITY_THREAD_POOL` batch execution.
 * @param arg Pointer to `struct MultiBatchItem`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error multi_pool_task_cb(void *arg) {
  struct MultiBatchItem *item = (struct MultiBatchItem *)arg;
  enum c_abstract_http_error slot_rc;
  enum c_abstract_http_error b_rc;
  slot_rc = multi_execute_single_slot(item->client, item->req, item->future, 0);
  if (slot_rc != C_ABSTRACT_HTTP_SUCCESS) {
    b_rc = multi_pool_finish_barrier(item->barrier, slot_rc);
    if (b_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return b_rc;
    }
    return slot_rc;
  }
  return multi_pool_finish_barrier(item->barrier, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Subtract unpushed task count from barrier when `thread_pool_push`
 * fails.
 * @param barrier Shared completion barrier.
 * @param unpushed Number of tasks that were never enqueued.
 * @return C_ABSTRACT_HTTP_SUCCESS or mutex error enum.
 */
static enum c_abstract_http_error
multi_pool_abort_unpushed(struct MultiPoolBarrier *barrier, size_t unpushed) {
  enum c_abstract_http_error rc;
  rc = abstract_http_mutex_lock(barrier->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  barrier->remaining =
      (barrier->remaining > unpushed) ? (barrier->remaining - unpushed) : 0;
  return abstract_http_mutex_unlock(barrier->mutex);
}

/**
 * @brief Wait until all enqueued tasks on `barrier` complete.
 * @param barrier Shared completion barrier.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization/worker error enum.
 */
static enum c_abstract_http_error
multi_pool_wait_barrier(struct MultiPoolBarrier *barrier) {
  enum c_abstract_http_error rc;
  enum c_abstract_http_error u_rc;
  enum c_abstract_http_error w_err;
  rc = abstract_http_mutex_lock(barrier->mutex);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  while (barrier->remaining > 0) {
    rc = abstract_http_cond_wait(barrier->cond, barrier->mutex);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      u_rc = abstract_http_mutex_unlock(barrier->mutex);
      if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return u_rc;
      }
      return rc;
    }
  }
  w_err = barrier->worker_err;
  u_rc = abstract_http_mutex_unlock(barrier->mutex);
  if (u_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return u_rc;
  }
  return w_err;
}

/**
 * @brief Release thread-pool batch items and barrier sync primitives.
 * @param barrier Barrier to destroy.
 * @param items Work-item array to free.
 * @param orig_rc Primary status to return if cleanup succeeds.
 * @return Cleanup error enum if cleanup fails, otherwise `orig_rc`.
 */
static enum c_abstract_http_error
multi_pool_cleanup(struct MultiPoolBarrier *barrier,
                   struct MultiBatchItem *items,
                   enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error f_rc;
  free(items);
  f_rc = sync_pair_free(barrier->cond, barrier->mutex);
  if (f_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return f_rc;
  }
  return orig_rc;
}

/**
 * @brief Dispatch a multi-request batch concurrently on `client->thread_pool`.
 * @param client HTTP client with active `thread_pool`.
 * @param requests Array of request pointers.
 * @param num_requests Number of requests.
 * @param futures Output futures array.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
multi_run_thread_pool(struct HttpClient *client,
                      struct HttpRequest *const *requests, size_t num_requests,
                      struct HttpFuture **futures) {
  struct MultiPoolBarrier barrier;
  struct MultiBatchItem *items = NULL;
  enum c_abstract_http_error rc;
  size_t i;
  size_t pushed = 0;

  memset(&barrier, 0, sizeof(barrier));
  items = (struct MultiBatchItem *)calloc(num_requests, sizeof(*items));
  if (!items) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = sync_pair_init(&barrier.mutex, &barrier.cond);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(items);
    return rc;
  }

  barrier.remaining = num_requests;
  barrier.worker_err = C_ABSTRACT_HTTP_SUCCESS;
  for (i = 0; i < num_requests; ++i) {
    items[i].client = client;
    items[i].req = requests[i];
    items[i].future = futures[i];
    items[i].barrier = &barrier;
    rc = abstract_http_thread_pool_push(client->thread_pool, multi_pool_task_cb,
                                        &items[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      enum c_abstract_http_error a_rc;
      enum c_abstract_http_error w_rc;
      a_rc = multi_pool_abort_unpushed(&barrier, num_requests - pushed);
      if (a_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return multi_pool_cleanup(&barrier, items, a_rc);
      }
      w_rc = multi_pool_wait_barrier(&barrier);
      if (w_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return multi_pool_cleanup(&barrier, items, w_rc);
      }
      return multi_pool_cleanup(&barrier, items, rc);
    }
    pushed++;
  }

  rc = multi_pool_wait_barrier(&barrier);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_pool_cleanup(&barrier, items, rc);
  }
  return multi_pool_cleanup(&barrier, items, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Coroutine entry point for `MODALITY_GREENTHREAD` batch execution.
 * @param arg Pointer to `struct MultiBatchItem`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error multi_coroutine_task_cb(void *arg) {
  struct MultiBatchItem *item = (struct MultiBatchItem *)arg;
  enum c_abstract_http_error rc;
  rc = abstract_http_coroutine_yield();
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return multi_execute_single_slot(item->client, item->req, item->future, 0);
}

/**
 * @brief Recursively free coroutines in `cos[idx..created)` and release scratch
 * arrays.
 * @param cos Array of coroutine pointers.
 * @param idx Current coroutine index.
 * @param created Number of initialized coroutines.
 * @param items Scratch batch items array (freed at base case).
 * @param orig_rc Primary status enum to return if all frees succeed.
 * @return Cleanup error if any `coroutine_free` fails, otherwise `orig_rc`.
 */
static enum c_abstract_http_error
multi_free_coroutines_from(struct AbstractHttpCoroutine **cos, size_t idx,
                           size_t created, struct MultiBatchItem *items,
                           enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error f_rc;
  if (idx >= created) {
    free(items);
    free(cos);
    return orig_rc;
  }
  f_rc = abstract_http_coroutine_free(cos[idx]);
  if (f_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_free_coroutines_from(cos, idx + 1, created, items, f_rc);
  }
  return multi_free_coroutines_from(cos, idx + 1, created, items, orig_rc);
}

/**
 * @brief Schedule and drive cooperative coroutines for `MODALITY_GREENTHREAD`.
 * @param client HTTP client instance.
 * @param requests Array of request pointers.
 * @param num_requests Number of requests.
 * @param futures Output futures array.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
multi_run_greenthreads(struct HttpClient *client,
                       struct HttpRequest *const *requests, size_t num_requests,
                       struct HttpFuture **futures) {
  struct AbstractHttpCoroutine **cos = NULL;
  struct MultiBatchItem *items = NULL;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  size_t i;
  size_t created = 0;

  cos = (struct AbstractHttpCoroutine **)calloc(num_requests, sizeof(*cos));
  if (!cos) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  items = (struct MultiBatchItem *)calloc(num_requests, sizeof(*items));
  if (!items) {
    free(cos);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  for (i = 0; i < num_requests; ++i) {
    items[i].client = client;
    items[i].req = requests[i];
    items[i].future = futures[i];
    rc = abstract_http_coroutine_init(&cos[i], 0, multi_coroutine_task_cb,
                                      &items[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return multi_free_coroutines_from(cos, 0, created, items, rc);
    }
    created++;
  }

  for (;;) {
    size_t done_count = 0;
    for (i = 0; i < created; ++i) {
      int is_done = 0;
      rc = abstract_http_coroutine_is_done(cos[i], &is_done);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return multi_free_coroutines_from(cos, 0, created, items, rc);
      }
      if (is_done) {
        done_count++;
        continue;
      }
      rc = abstract_http_coroutine_resume(cos[i]);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return multi_free_coroutines_from(cos, 0, created, items, rc);
      }
      rc = abstract_http_coroutine_is_done(cos[i], &is_done);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return multi_free_coroutines_from(cos, 0, created, items, rc);
      }
      if (is_done) {
        done_count++;
      }
    }
    if (done_count == created) {
      break;
    }
  }

  return multi_free_coroutines_from(cos, 0, created, items,
                                    C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Actor handler callback for `MODALITY_MESSAGE_PASSING` batch execution.
 * @param actor Receiving actor handle.
 * @param msg Message carrying `struct MultiBatchItem *` payload.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
multi_actor_handler_cb(struct AbstractHttpActor *actor,
                       struct AbstractHttpMessage *msg) {
  struct MultiBatchItem *item;
  (void)actor;
  item = (struct MultiBatchItem *)msg->payload;
  return multi_execute_single_slot(item->client, item->req, item->future, 0);
}

/**
 * @brief Dispatch requests via Actor Message Bus for
 * `MODALITY_MESSAGE_PASSING`.
 * @param client HTTP client instance.
 * @param requests Array of request pointers.
 * @param num_requests Number of requests.
 * @param futures Output futures array.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
multi_run_message_passing(struct HttpClient *client,
                          struct HttpRequest *const *requests,
                          size_t num_requests, struct HttpFuture **futures) {
  struct AbstractHttpMessageBus *bus = client->message_bus;
  struct AbstractHttpMessageBus *owned_bus = NULL;
  struct AbstractHttpActor *worker = NULL;
  struct MultiBatchItem *items = NULL;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  int processed = 0;
  size_t i;

  items = (struct MultiBatchItem *)calloc(num_requests, sizeof(*items));
  if (!items) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (!bus) {
    rc = abstract_http_message_bus_init(&owned_bus);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(items);
      return rc;
    }
    bus = owned_bus;
  }

  rc = abstract_http_actor_spawn(bus, "cah_multi_worker",
                                 multi_actor_handler_cb, NULL, &worker);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup;
  }

  for (i = 0; i < num_requests; ++i) {
    struct AbstractHttpMessage msg;
    items[i].client = client;
    items[i].req = requests[i];
    items[i].future = futures[i];
    memset(&msg, 0, sizeof(msg));
    msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND;
    msg.payload = &items[i];
    msg.sender = worker;
    msg.receiver = worker;
    rc = abstract_http_actor_send(bus, &msg);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup;
    }
  }

  rc = abstract_http_message_bus_process(bus, &processed);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup;
  }

cleanup:
  free(items);
  if (owned_bus) {
    enum c_abstract_http_error b_rc;
    b_rc = abstract_http_message_bus_free(owned_bus);
    if (b_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return b_rc;
    }
  }
  return rc;
}

/**
 * @brief Free response on IPC error (if non-NULL) and complete future slot.
 * @param res Response object (freed if `status != C_ABSTRACT_HTTP_SUCCESS`).
 * @param future Target future slot.
 * @param status Final IPC status.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization error enum.
 */
static enum c_abstract_http_error
multi_ipc_finish_slot(struct HttpResponse *res, struct HttpFuture *future,
                      enum c_abstract_http_error status) {
  if (status != C_ABSTRACT_HTTP_SUCCESS && res) {
    enum c_abstract_http_error r_rc;
    r_rc = http_response_free(res);
    if (r_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(res);
      return http_future_complete_slot(future, NULL, r_rc);
    }
    free(res);
    res = NULL;
  }
  return http_future_complete_slot(future, res, status);
}

/**
 * @brief Close child-to-parent IPC pipe, free response if failed, and complete
 * future.
 * @param c2p Child-to-parent pipe.
 * @param res Response object (freed if status != SUCCESS).
 * @param future Target future slot.
 * @param status Accumulated IPC status.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization error enum.
 */
static enum c_abstract_http_error
multi_ipc_close_c2p(struct AbstractHttpIpcPipe *c2p, struct HttpResponse *res,
                    struct HttpFuture *future,
                    enum c_abstract_http_error status) {
  enum c_abstract_http_error p_rc;
  p_rc = abstract_http_ipc_pipe_free(c2p);
  if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_finish_slot(res, future, p_rc);
  }
  return multi_ipc_finish_slot(res, future, status);
}

/**
 * @brief Close both IPC pipes and complete future slot.
 * @param p2c Parent-to-child pipe.
 * @param c2p Child-to-parent pipe.
 * @param res Response object.
 * @param future Target future slot.
 * @param status Accumulated IPC status.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization error enum.
 */
static enum c_abstract_http_error
multi_ipc_close_pipes(struct AbstractHttpIpcPipe *p2c,
                      struct AbstractHttpIpcPipe *c2p, struct HttpResponse *res,
                      struct HttpFuture *future,
                      enum c_abstract_http_error status) {
  enum c_abstract_http_error p_rc;
  p_rc = abstract_http_ipc_pipe_free(p2c);
  if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_close_c2p(c2p, res, future, p_rc);
  }
  return multi_ipc_close_c2p(c2p, res, future, status);
}

/**
 * @brief Clean up IPC child-process and pipes, completing the future slot.
 * @param proc Child process handle (may be NULL).
 * @param p2c Parent-to-child pipe.
 * @param c2p Child-to-parent pipe.
 * @param req_buf Serialized request buffer to free.
 * @param res_buf Serialized response buffer to free.
 * @param res Response object (owned by future on success, freed on error).
 * @param future Target future slot.
 * @param orig_rc Primary IPC execution status.
 * @return C_ABSTRACT_HTTP_SUCCESS or synchronization/cleanup error enum.
 */
static enum c_abstract_http_error multi_ipc_cleanup_and_complete(
    struct AbstractHttpProcess *proc, struct AbstractHttpIpcPipe *p2c,
    struct AbstractHttpIpcPipe *c2p, char *req_buf, char *res_buf,
    struct HttpResponse *res, struct HttpFuture *future,
    enum c_abstract_http_error orig_rc) {
  free(req_buf);
  free(res_buf);
  if (proc) {
    enum c_abstract_http_error p_rc;
    int exit_code = 0;
    p_rc = abstract_http_process_wait_and_free(proc, &exit_code);
    if (p_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return multi_ipc_close_pipes(p2c, c2p, res, future,
                                   orig_rc != C_ABSTRACT_HTTP_SUCCESS ? orig_rc
                                                                      : p_rc);
    }
  }
  return multi_ipc_close_pipes(p2c, c2p, res, future, orig_rc);
}

/**
 * @brief Execute a single request over IPC child-process serialization for
 * `MODALITY_MULTIPROCESS`.
 * @param req Request to serialize and send.
 * @param future Future slot receiving deserialized response.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
static enum c_abstract_http_error
multi_ipc_execute_one(const struct HttpRequest *req,
                      struct HttpFuture *future) {
  struct AbstractHttpIpcPipe p2c;
  struct AbstractHttpIpcPipe c2p;
  struct AbstractHttpProcess *proc = NULL;
  char *req_buf = NULL;
  size_t req_len = 0;
  char *res_buf = NULL;
  size_t res_len = 0;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  if (math_is_future_cancelled(future)) {
    return http_future_complete_slot(future, NULL, C_ABSTRACT_HTTP_ERR_TIMEOUT);
  }

  memset(&p2c, 0, sizeof(p2c));
  memset(&c2p, 0, sizeof(c2p));
  rc = abstract_http_ipc_pipe_init(&p2c);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return http_future_complete_slot(future, NULL, rc);
  }
  rc = abstract_http_ipc_pipe_init(&c2p);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(NULL, &p2c, &c2p, NULL, NULL, NULL,
                                          future, rc);
  }
  rc = abstract_http_process_spawn(&proc, &p2c, &c2p);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(NULL, &p2c, &c2p, NULL, NULL, NULL,
                                          future, rc);
  }
  rc = abstract_http_ipc_serialize_request(req, &req_buf, &req_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future, rc);
  }
  rc = abstract_http_ipc_write(p2c.write_handle, &req_len, sizeof(req_len));
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future, rc);
  }
  rc = abstract_http_ipc_write(p2c.write_handle, req_buf, req_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future, rc);
  }
  rc = abstract_http_ipc_read(c2p.read_handle, &res_len, sizeof(res_len));
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future, rc);
  }
  res_buf = (char *)malloc(res_len > 0 ? res_len : 1);
  if (!res_buf) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, NULL, NULL,
                                          future, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  rc = abstract_http_ipc_read(c2p.read_handle, res_buf, res_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future, rc);
  }
  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  if (!res) {
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future,
                                          C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  rc = abstract_http_ipc_deserialize_response(res_buf, res_len, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(res);
    return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf,
                                          NULL, future, rc);
  }
  return multi_ipc_cleanup_and_complete(proc, &p2c, &c2p, req_buf, res_buf, res,
                                        future, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Evaluate `progress_cb` and `fail_fast` across completed batch futures.
 * @param futures Array of futures.
 * @param num_requests Number of requests in batch.
 * @param progress_cb Optional progress callback.
 * @param user_data User context for progress callback.
 * @param fail_fast Non-zero to return first future error.
 * @return C_ABSTRACT_HTTP_SUCCESS or first failure/abort enum.
 */
static enum c_abstract_http_error
multi_finalize_batch(struct HttpFuture **futures, size_t num_requests,
                     http_multi_progress_cb progress_cb, void *user_data,
                     int fail_fast) {
  size_t i;
  size_t completed = 0;
  enum c_abstract_http_error first_err = C_ABSTRACT_HTTP_SUCCESS;
  for (i = 0; i < num_requests; ++i) {
    if (futures[i] && futures[i]->is_ready) {
      completed++;
      if (first_err == C_ABSTRACT_HTTP_SUCCESS &&
          futures[i]->error_code != (int)C_ABSTRACT_HTTP_SUCCESS) {
        first_err = (enum c_abstract_http_error)futures[i]->error_code;
      }
    }
  }
  if (progress_cb) {
    enum c_abstract_http_error cb_rc;
    cb_rc = progress_cb(completed, num_requests, user_data);
    if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return cb_rc;
    }
  }
  if (fail_fast && first_err != C_ABSTRACT_HTTP_SUCCESS) {
    return first_err;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Send multiple HTTP requests using the client's configured modality */
enum c_abstract_http_error http_client_send_multi(
    struct HttpClient *client, struct HttpRequest *const *requests,
    size_t num_requests, struct HttpFuture **futures,
    http_multi_progress_cb progress_cb, void *user_data, int fail_fast) {
  size_t i;
  enum c_abstract_http_error rc;
  struct HttpMultiRequest multi;

  if (!client || !requests || num_requests == 0 || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = http_multi_request_init(&multi);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  for (i = 0; i < num_requests; ++i) {
    rc = http_multi_request_add(&multi, requests[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup_multi;
    }
  }

  /* Dispatch based on modality */
  switch ((int)client->config.modality) {
  case MODALITY_ASYNC: {
    if (client->send_multi && client->loop) {
      for (i = 0; i < num_requests; ++i) {
        if (futures[i] && !math_is_future_cancelled(futures[i])) {
          futures[i]->is_ready = 0;
          futures[i]->error_code = 0;
          futures[i]->response = NULL;
        }
      }
      rc = client->send_multi(client->transport, client->loop, &multi, futures);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup_multi;
      }
      goto finalize_multi;
    }
    rc = C_ABSTRACT_HTTP_ERR_NOTSUP;
    goto cleanup_multi;
  }

  case MODALITY_THREAD_POOL: {
    if (client->thread_pool) {
      rc = multi_run_thread_pool(client, requests, num_requests, futures);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup_multi;
      }
      goto finalize_multi;
    }
    goto sequential_exec;
  }

  case MODALITY_GREENTHREAD: {
    rc = multi_run_greenthreads(client, requests, num_requests, futures);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup_multi;
    }
    goto finalize_multi;
  }

  case MODALITY_MESSAGE_PASSING: {
    rc = multi_run_message_passing(client, requests, num_requests, futures);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      goto cleanup_multi;
    }
    goto finalize_multi;
  }

  case MODALITY_MULTIPROCESS: {
    if (client->use_multiprocess_ipc) {
      rc = C_ABSTRACT_HTTP_SUCCESS;
      for (i = 0; i < num_requests; ++i) {
        rc = multi_ipc_execute_one(requests[i], futures[i]);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          goto cleanup_multi;
        }
        if (progress_cb) {
          rc = progress_cb(i + 1, num_requests, user_data);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            goto cleanup_multi;
          }
        }
        if (fail_fast &&
            futures[i]->error_code != (int)C_ABSTRACT_HTTP_SUCCESS) {
          rc = (enum c_abstract_http_error)futures[i]->error_code;
          goto cleanup_multi;
        }
      }
      goto cleanup_multi;
    }
    goto sequential_exec;
  }

  case MODALITY_SYNC:
  default:
  sequential_exec: {
    int direct_ff = (fail_fast && !progress_cb) ? 1 : 0;
    rc = C_ABSTRACT_HTTP_SUCCESS;
    for (i = 0; i < num_requests; ++i) {
      rc =
          multi_execute_single_slot(client, requests[i], futures[i], direct_ff);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup_multi;
      }

      if (progress_cb) {
        rc = progress_cb(i + 1, num_requests, user_data);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          goto cleanup_multi;
        }
      }

      if (futures[i]->error_code != (int)C_ABSTRACT_HTTP_SUCCESS && fail_fast) {
        rc = (enum c_abstract_http_error)futures[i]->error_code;
        goto cleanup_multi;
      }
    }
    goto cleanup_multi;
  }
  }

finalize_multi:;
  rc = multi_finalize_batch(futures, num_requests, progress_cb, user_data,
                            fail_fast);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup_multi;
  }

cleanup_multi: {
  enum c_abstract_http_error rch;
  rch = http_multi_request_free(&multi);
  if (rch != C_ABSTRACT_HTTP_SUCCESS)
    return rch;
}
  return rc;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
C_ABSTRACT_HTTP_API void
c_abstract_http_log_debug(const char *fmt, ...) {
#ifdef DEBUG
  va_list args;
  if (!fmt)
    return;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
#else
  if (!fmt)
    return;
#endif
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Expose internal http_types helpers for test coverage.
 *
 * @param step Test case selector (0..6).
 * @return Error code returned by the exercised internal helper.
 */
enum c_abstract_http_error c_abstract_http_test_http_types_helpers(int step);

enum c_abstract_http_error c_abstract_http_test_http_types_helpers(int step) {
  size_t written = 0;
  char buf[32];
  char *p = buf;
  char *null_p = NULL;

  switch (step) {
  case 0: {
    return sprintf_s_wrapper(NULL, buf, 0, sizeof(buf), "test");
  }
  case 1: {
    return sprintf_s_wrapper(&written, NULL, 0, sizeof(buf), "test");
  }
  case 2: {
    return sprintf_s_wrapper(&written, buf, 10, 5, "test");
  }
  case 3: {
    return urlencode_append(NULL, "test");
  }
  case 4: {
    return urlencode_append(&null_p, "test");
  }
  case 5: {
    return urlencode_append(&p, NULL);
  }
  default: {
    c_abstract_http_log_debug(NULL);
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  }
}
#endif
