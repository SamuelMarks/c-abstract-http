/**
 * @file http_lsquic.c
 * @brief LiteSpeed QUIC (lsquic) HTTP/3 implementation of the Abstract Network
 * Interface (ANI).
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <c_abstract_http/http_lsquic.h>
#include <c_abstract_http/http_raw.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "crypto_utils.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
#include "mock_stubs/mock_lsquic.h"
#else
#include <lsquic.h>
#endif
/* clang-format on */

/** @brief Reference count for lsquic global initialization */
static int g_lsquic_init_count = 0;

/** @brief Growable HTTP/3 byte buffer */
struct lsq_h3_buf {
  /** @brief Byte array */
  uint8_t *data;
  /** @brief Used length */
  size_t len;
  /** @brief Allocated capacity */
  size_t cap;
};

/** @brief Per-stream state for lsquic HTTP/3 transactions */
struct lsquic_stream_ctx {
  /** @brief Originating request descriptor for streaming uploads */
  const struct HttpRequest *req;
  /** @brief Serialized request HTTP/3 stream bytes */
  struct lsq_h3_buf tx;
  /** @brief Transmitted offset */
  size_t tx_off;
  /** @brief Accumulated response HTTP/3 stream bytes */
  struct lsq_h3_buf rx;
  /** @brief Non-zero once streaming read_chunk reaches EOF */
  int chunk_eof;
  /** @brief Stream completion flag */
  int done;
  /** @brief Stream error code */
  c_abstract_http_error_t err;
};

/** @brief Active batch queue bound to `ea_stream_if_ctx` during dispatch */
struct lsq_batch_state {
  /** @brief Array of stream contexts */
  struct lsquic_stream_ctx *streams;
  /** @brief Total stream count */
  size_t count;
  /** @brief Next unassigned stream index */
  size_t next_idx;
};

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief LiteSpeed QUIC engine handle */
  lsquic_engine_t *engine;
  /** @brief Engine settings */
  struct lsquic_engine_settings settings;
  /** @brief Active batch state for stream callbacks */
  struct lsq_batch_state active_batch;
  /** @brief Configuration applied flag */
  int is_configured;
  /** @brief Peer verification flag */
  int verify_peer;
  /** @brief Timeout in milliseconds */
  unsigned int timeout_ms;
  /** @brief Parsed MASQUE/HTTP3 proxy hostname */
  char *proxy_host;
  /** @brief Parsed MASQUE/HTTP3 proxy UDP port */
  unsigned short proxy_port;
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
  /** @brief Active UDP socket descriptor during QUIC dispatch */
  int udp_fd;
#endif
  /** @brief Configuration struct */
  struct HttpConfig config;
};

/**
 * @brief Append raw bytes to a growable HTTP/3 buffer.
 * @param b Target buffer.
 * @param src Source bytes.
 * @param n Byte count to append.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_buf_append(struct lsq_h3_buf *b, const void *src, size_t n) {
  if (n == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_body_alloc_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  if (b->len + n > b->cap) {
    uint8_t *nb;
    size_t new_cap = b->cap == 0 ? 16 : b->cap * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_alloc_fail) {
      new_cap = b->len + n;
    }
#endif
    while (new_cap < b->len + n) {
      new_cap *= 2;
    }
    nb = (uint8_t *)realloc(b->data, new_cap);
    if (!nb) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    b->data = nb;
    b->cap = new_cap;
  }
  memcpy(b->data + b->len, src, n);
  b->len += n;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Encode an RFC 9000 variable-length integer into an HTTP/3 buffer.
 * @param b Target buffer.
 * @param val Non-negative integer value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t lsq_write_varint(struct lsq_h3_buf *b,
                                                           size_t val) {
  uint8_t tmp[8];
  if (val < 64) {
    tmp[0] = (uint8_t)val;
    return lsq_buf_append(b, tmp, 1);
  }
  if (val < 16384) {
    tmp[0] = (uint8_t)(0x40U | ((val >> 8) & 0x3FU));
    tmp[1] = (uint8_t)(val & 0xFFU);
    return lsq_buf_append(b, tmp, 2);
  }
  if (val < 1073741824UL) {
    tmp[0] = (uint8_t)(0x80U | ((val >> 24) & 0x3FU));
    tmp[1] = (uint8_t)((val >> 16) & 0xFFU);
    tmp[2] = (uint8_t)((val >> 8) & 0xFFU);
    tmp[3] = (uint8_t)(val & 0xFFU);
    return lsq_buf_append(b, tmp, 4);
  }
  {
    abstract_http_uint64_t v64 = (abstract_http_uint64_t)val;
    tmp[0] = (uint8_t)(0xC0U | ((v64 >> 56) & 0x3FU));
    tmp[1] = (uint8_t)((v64 >> 48) & 0xFFU);
    tmp[2] = (uint8_t)((v64 >> 40) & 0xFFU);
    tmp[3] = (uint8_t)((v64 >> 32) & 0xFFU);
    tmp[4] = (uint8_t)((v64 >> 24) & 0xFFU);
    tmp[5] = (uint8_t)((v64 >> 16) & 0xFFU);
    tmp[6] = (uint8_t)((v64 >> 8) & 0xFFU);
    tmp[7] = (uint8_t)(v64 & 0xFFU);
    return lsq_buf_append(b, tmp, 8);
  }
}

/**
 * @brief Decode an RFC 9000 variable-length integer from a byte slice.
 * @param src Input byte buffer.
 * @param len Total length of @p src.
 * @param off In/out cursor offset.
 * @param out_val Decoded integer output.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_read_varint(const uint8_t *src, size_t len, size_t *off, size_t *out_val) {
  uint8_t fb;
  unsigned int prefix;
  if (*off >= len) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  fb = src[*off];
  prefix = (unsigned int)(fb >> 6);
  if (prefix == 0) {
    *out_val = (size_t)(fb & 0x3FU);
    *off += 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (prefix == 1) {
    if (*off + 2 > len) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    *out_val = ((size_t)(fb & 0x3FU) << 8) | (size_t)src[*off + 1];
    *off += 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (prefix == 2) {
    if (*off + 4 > len) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    *out_val = ((size_t)(fb & 0x3FU) << 24) | ((size_t)src[*off + 1] << 16) |
               ((size_t)src[*off + 2] << 8) | (size_t)src[*off + 3];
    *off += 4;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (*off + 8 > len) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  {
    abstract_http_uint64_t v64 = ((abstract_http_uint64_t)(fb & 0x3FU) << 56) |
                                 ((abstract_http_uint64_t)src[*off + 1] << 48) |
                                 ((abstract_http_uint64_t)src[*off + 2] << 40) |
                                 ((abstract_http_uint64_t)src[*off + 3] << 32) |
                                 ((abstract_http_uint64_t)src[*off + 4] << 24) |
                                 ((abstract_http_uint64_t)src[*off + 5] << 16) |
                                 ((abstract_http_uint64_t)src[*off + 6] << 8) |
                                 (abstract_http_uint64_t)src[*off + 7];
    *out_val = (size_t)v64;
    *off += 8;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

/**
 * @brief Write a literal QPACK header field without name reference.
 * @param b Target buffer.
 * @param name Header field name.
 * @param val Header field value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_write_qpack_field(struct lsq_h3_buf *b, const char *name, const char *val) {
  c_abstract_http_error_t rc;
  uint8_t marker = 0x20U;
  size_t nlen = strlen(name);
  size_t vlen = strlen(val);

  rc = lsq_buf_append(b, &marker, 1);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = lsq_write_varint(b, nlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = lsq_buf_append(b, name, nlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = lsq_write_varint(b, vlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return lsq_buf_append(b, val, vlen);
}

/**
 * @brief Convert enum HttpMethod to its wire token string.
 * @param method HTTP method enum.
 * @param out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t lsq_method_str(enum HttpMethod method,
                                                         const char **out) {
  switch ((int)method) {
  case HTTP_GET:
    *out = "GET";
    break;
  case HTTP_POST:
    *out = "POST";
    break;
  case HTTP_PUT:
    *out = "PUT";
    break;
  case HTTP_DELETE:
    *out = "DELETE";
    break;
  case HTTP_PATCH:
    *out = "PATCH";
    break;
  case HTTP_HEAD:
    *out = "HEAD";
    break;
  case HTTP_OPTIONS:
    *out = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out = "TRACE";
    break;
  case HTTP_CONNECT:
    *out = "CONNECT";
    break;
  default:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse an HTTP(S)/MASQUE URL into scheme, host, port, and path.
 * @param url Input URL string.
 * @param out_scheme Newly allocated scheme string.
 * @param out_host Newly allocated host string.
 * @param out_port Parsed port number.
 * @param out_path Newly allocated path string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_parse_url(const char *url, char **out_scheme, char **out_host,
              unsigned short *out_port, char **out_path) {
  const char *p;
  const char *host_start;
  const char *host_end;
  size_t scheme_len;
  size_t host_len;
  unsigned short default_port = 443;
  c_abstract_http_error_t rc;

  if (!url) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  p = strstr(url, "://");
  if (!p) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  scheme_len = (size_t)(p - url);
  if ((scheme_len == 5 && strncmp(url, "https", 5) == 0) ||
      (scheme_len == 6 && strncmp(url, "masque", 6) == 0)) {
    default_port = 443;
  } else if (scheme_len == 4 && strncmp(url, "http", 4) == 0) {
    default_port = 80;
  } else {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *out_scheme = (char *)malloc(scheme_len + 1);
  if (!*out_scheme) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(*out_scheme, url, scheme_len);
  (*out_scheme)[scheme_len] = '\0';

  host_start = p + 3;
  host_end = host_start;
  while (*host_end && *host_end != ':' && *host_end != '/') {
    host_end++;
  }
  host_len = (size_t)(host_end - host_start);
  if (host_len == 0) {
    free(*out_scheme);
    *out_scheme = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *out_host = (char *)malloc(host_len + 1);
  if (!*out_host) {
    free(*out_scheme);
    *out_scheme = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(*out_host, host_start, host_len);
  (*out_host)[host_len] = '\0';

  *out_port = default_port;
  p = host_end;
  if (*p == ':') {
    unsigned int port_val = 0;
    p++;
    while (*p >= '0' && *p <= '9') {
      port_val = port_val * 10U + (unsigned int)(*p - '0');
      p++;
    }
    if (port_val == 0 || port_val > 65535U) {
      free(*out_scheme);
      free(*out_host);
      *out_scheme = NULL;
      *out_host = NULL;
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    *out_port = (unsigned short)port_val;
  }

  if (*p == '/') {
    rc = c_abstract_http_strdup(p, out_path);
  } else {
    rc = c_abstract_http_strdup("/", out_path);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*out_scheme);
    free(*out_host);
    *out_scheme = NULL;
    *out_host = NULL;
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Case-insensitive ASCII comparison for header names.
 * @param a First string.
 * @param b Second string.
 * @return 0 if equal ignoring ASCII case, non-zero otherwise.
 */
static int math_lsq_strcasecmp(const char *a, const char *b) {
  unsigned char ca;
  unsigned char cb;
  while (*a && *b) {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (unsigned char)(ca + ('a' - 'A'));
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (unsigned char)(cb + ('a' - 'A'));
    }
    if (ca != cb) {
      return (int)ca - (int)cb;
    }
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/**
 * @brief Build a Basic Proxy-Authorization header value from credentials.
 * @param user Proxy username.
 * @param pass Optional proxy password.
 * @param out_hdr Newly allocated "Basic <base64>" string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_build_proxy_auth(const char *user, const char *pass, char **out_hdr) {
  const char *pw = pass ? pass : "";
  size_t ulen = strlen(user);
  size_t plen = strlen(pw);
  char *raw;
  char *b64 = NULL;
  size_t b64_len = 0;
  char *hdr;
  c_abstract_http_error_t rc;

  raw = (char *)malloc(ulen + 1 + plen + 1);
  if (!raw) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(raw, user, ulen);
  raw[ulen] = ':';
  memcpy(raw + ulen + 1, pw, plen);
  raw[ulen + 1 + plen] = '\0';

  rc = base64_encode((const unsigned char *)raw, ulen + 1 + plen, &b64,
                     &b64_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(raw);
    return rc;
  }
  free(raw);

  hdr = (char *)malloc(6 + b64_len + 1);
  if (!hdr) {
    free(b64);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(hdr, "Basic ", 6);
  memcpy(hdr + 6, b64, b64_len + 1);
  free(b64);
  *out_hdr = hdr;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Serialize an HttpRequest into HTTP/3 HEADERS and DATA frames.
 * @param ctx Transport context.
 * @param req Request to serialize.
 * @param scheme Parsed scheme string.
 * @param host Parsed authority/host string.
 * @param path Parsed path string.
 * @param out_stream Output buffer receiving encoded HTTP/3 stream bytes.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_serialize_request(const struct HttpTransportContext *ctx,
                      const struct HttpRequest *req, const char *scheme,
                      const char *host, const char *path,
                      struct lsq_h3_buf *out_stream) {
  struct lsq_h3_buf hdr_payload;
  struct lsq_h3_buf body_payload;
  const char *method_s = NULL;
  uint8_t prefix[2];
  size_t i;
  int has_ua = 0;
  c_abstract_http_error_t rc;

  memset(&hdr_payload, 0, sizeof(hdr_payload));
  memset(&body_payload, 0, sizeof(body_payload));

  rc = lsq_method_str(req->method, &method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  prefix[0] = 0x00;
  prefix[1] = 0x00;
  rc = lsq_buf_append(&hdr_payload, prefix, 2);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }

  rc = lsq_write_qpack_field(&hdr_payload, ":method", method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = lsq_write_qpack_field(&hdr_payload, ":scheme", scheme);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = lsq_write_qpack_field(&hdr_payload, ":authority", host);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = lsq_write_qpack_field(&hdr_payload, ":path", path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }

  for (i = 0; i < req->headers.count; ++i) {
    if (math_lsq_strcasecmp(req->headers.headers[i].key, "User-Agent") == 0) {
      has_ua = 1;
    }
    rc = lsq_write_qpack_field(&hdr_payload, req->headers.headers[i].key,
                               req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
  }

  if (!has_ua && ctx->config.user_agent) {
    rc = lsq_write_qpack_field(&hdr_payload, "user-agent",
                               ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
  }
  {
    char clen_buf[32];
    if (math_c_abstract_http_format_clen(req, clen_buf)) {
      rc = lsq_write_qpack_field(&hdr_payload, "content-length", clen_buf);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(hdr_payload.data);
        return rc;
      }
    }
  }

  if (ctx->proxy_host) {
    rc = lsq_write_qpack_field(&hdr_payload, "x-masque-target", host);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
    if (ctx->config.proxy_username) {
      char *auth_hdr = NULL;
      rc = lsq_build_proxy_auth(ctx->config.proxy_username,
                                ctx->config.proxy_password, &auth_hdr);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(hdr_payload.data);
        return rc;
      }
      rc = lsq_write_qpack_field(&hdr_payload, "proxy-authorization", auth_hdr);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(auth_hdr);
        free(hdr_payload.data);
        return rc;
      }
      free(auth_hdr);
    }
  }

  if (ctx->config.cookie_jar && ctx->config.cookie_jar->count > 0) {
    struct lsq_h3_buf cbuf;
    char nul = '\0';
    memset(&cbuf, 0, sizeof(cbuf));
    for (i = 0; i < ctx->config.cookie_jar->count; ++i) {
      if (i > 0) {
        rc = lsq_buf_append(&cbuf, "; ", 2);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(cbuf.data);
          free(hdr_payload.data);
          return rc;
        }
      }
      rc = lsq_buf_append(&cbuf, ctx->config.cookie_jar->cookies[i].name,
                          strlen(ctx->config.cookie_jar->cookies[i].name));
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
      rc = lsq_buf_append(&cbuf, "=", 1);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
      rc = lsq_buf_append(&cbuf, ctx->config.cookie_jar->cookies[i].value,
                          strlen(ctx->config.cookie_jar->cookies[i].value));
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
    }
    rc = lsq_buf_append(&cbuf, &nul, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(cbuf.data);
      free(hdr_payload.data);
      return rc;
    }
    rc = lsq_write_qpack_field(&hdr_payload, "cookie", (const char *)cbuf.data);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(cbuf.data);
      free(hdr_payload.data);
      return rc;
    }
    free(cbuf.data);
  }

  rc = lsq_write_varint(out_stream, 0x01U);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = lsq_write_varint(out_stream, hdr_payload.len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = lsq_buf_append(out_stream, hdr_payload.data, hdr_payload.len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  free(hdr_payload.data);

  if (!req->read_chunk && req->body && req->body_len > 0) {
    rc = lsq_write_varint(out_stream, 0x00U);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = lsq_write_varint(out_stream, req->body_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = lsq_buf_append(out_stream, req->body, req->body_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  (void)body_payload;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse a Set-Cookie header value and store it in the cookie jar.
 * @param jar Target cookie jar.
 * @param header_val Raw Set-Cookie header value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_store_set_cookie(struct HttpCookieJar *jar, const char *header_val) {
  const char *eq;
  const char *semi;
  size_t nlen;
  size_t vlen;
  char *cname;
  char *cval;
  c_abstract_http_error_t rc;

  eq = strchr(header_val, '=');
  if (!eq || eq == header_val) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  nlen = (size_t)(eq - header_val);
  semi = strchr(eq + 1, ';');
  vlen = semi ? (size_t)(semi - (eq + 1)) : strlen(eq + 1);

  cname = (char *)malloc(nlen + 1);
  if (!cname) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  cval = (char *)malloc(vlen + 1);
  if (!cval) {
    free(cname);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(cname, header_val, nlen);
  cname[nlen] = '\0';
  memcpy(cval, eq + 1, vlen);
  cval[vlen] = '\0';

  rc = http_cookie_jar_set(jar, cname, cval);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(cname);
    free(cval);
    return rc;
  }
  free(cname);
  free(cval);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Decode HTTP/3 response frames into an HttpResponse structure.
 * @param ctx Transport context.
 * @param req Originating request (for optional on_chunk callback).
 * @param buf Raw HTTP/3 response stream bytes.
 * @param len Length of @p buf.
 * @param res Output response structure.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_decode_response(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req, const uint8_t *buf,
                    size_t len, struct HttpResponse *res) {
  size_t off = 0;
  c_abstract_http_error_t rc;

  while (off < len) {
    size_t ftype = 0;
    size_t flen = 0;

    rc = lsq_read_varint(buf, len, &off, &ftype);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = lsq_read_varint(buf, len, &off, &flen);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    if (off + flen > len) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }

    if (ftype == 0x01U) {
      size_t hoff;
      size_t hend = off + flen;
      if (flen < 2) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      hoff = off + 2;
      while (hoff < hend) {
        size_t kl = 0;
        size_t vl = 0;
        char *kstr;
        char *vstr;

        if (buf[hoff++] != 0x20U) {
          return C_ABSTRACT_HTTP_ERR_IO;
        }
        rc = lsq_read_varint(buf, hend, &hoff, &kl);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return rc;
        }
        if (hoff + kl > hend) {
          return C_ABSTRACT_HTTP_ERR_IO;
        }
        kstr = (char *)malloc(kl + 1);
        if (!kstr) {
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        memcpy(kstr, buf + hoff, kl);
        kstr[kl] = '\0';
        hoff += kl;

        rc = lsq_read_varint(buf, hend, &hoff, &vl);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(kstr);
          return rc;
        }
        if (hoff + vl > hend) {
          free(kstr);
          return C_ABSTRACT_HTTP_ERR_IO;
        }
        vstr = (char *)malloc(vl + 1);
        if (!vstr) {
          free(kstr);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        memcpy(vstr, buf + hoff, vl);
        vstr[vl] = '\0';
        hoff += vl;

        if (strcmp(kstr, ":status") == 0) {
          res->status_code = atoi(vstr);
        } else {
          rc = http_headers_add(&res->headers, kstr, vstr);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            free(kstr);
            free(vstr);
            return rc;
          }
          if (ctx->config.cookie_jar && (strcmp(kstr, "set-cookie") == 0 ||
                                         strcmp(kstr, "Set-Cookie") == 0)) {
            rc = lsq_store_set_cookie(ctx->config.cookie_jar, vstr);
            if (rc != C_ABSTRACT_HTTP_SUCCESS) {
              free(kstr);
              free(vstr);
              return rc;
            }
          }
        }
        free(kstr);
        free(vstr);
      }
    } else if (ftype == 0x00U && flen > 0) {
      if (req->on_chunk) {
        rc = req->on_chunk(req->on_chunk_user_data, buf + off, flen);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return rc;
        }
      } else {
        char *nb = (char *)realloc(res->body, res->body_len + flen + 1);
        if (!nb) {
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        memcpy(nb + res->body_len, buf + off, flen);
        res->body = nb;
        res->body_len += flen;
        nb[res->body_len] = '\0';
      }
    }
    off += flen;
  }

  if (res->status_code == 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Handle lsquic stream initialization logic.
 * @param ctx Transport context owning active batch state.
 * @param s QUIC stream handle.
 * @param out_st Output stream context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_on_new_stream_impl(struct HttpTransportContext *ctx, lsquic_stream_t *s,
                       lsquic_stream_ctx_t **out_st) {
  lsquic_stream_ctx_t *st;
  if (!ctx || ctx->active_batch.next_idx >= ctx->active_batch.count) {
    *out_st = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  st = &ctx->active_batch.streams[ctx->active_batch.next_idx++];
  if (lsquic_stream_wantwrite(s, 1) < 0) {
    LOG_DEBUG("lsq_on_new_stream_impl: lsquic_stream_wantwrite failed");
  }
  *out_st = st;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Handle lsquic stream write readiness logic.
 * @param s QUIC stream handle.
 * @param st Stream context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_on_write_impl(lsquic_stream_t *s, lsquic_stream_ctx_t *st) {
  for (;;) {
    while (st->tx_off < st->tx.len) {
      long nw = lsquic_stream_write(s, st->tx.data + st->tx_off,
                                    st->tx.len - st->tx_off);
      if (nw <= 0) {
        st->err = C_ABSTRACT_HTTP_ERR_IO;
        st->done = 1;
        if (lsquic_stream_close(s) != 0) {
          LOG_DEBUG("lsq_on_write_impl: lsquic_stream_close failed");
        }
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      st->tx_off += (size_t)nw;
    }
    if (st->req && st->req->read_chunk && !st->chunk_eof) {
      uint8_t chunk[192];
      size_t nread = 0;
      c_abstract_http_error_t rc = st->req->read_chunk(
          st->req->read_chunk_user_data, chunk, sizeof(chunk), &nread);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        st->err = rc;
        st->done = 1;
        if (lsquic_stream_close(s) != 0) {
          LOG_DEBUG("lsq_on_write_impl: lsquic_stream_close failed");
        }
        return rc;
      }
      if (nread == 0) {
        st->chunk_eof = 1;
        break;
      }
      st->tx.data[0] = 0x00U;
      if (nread < 64) {
        st->tx.data[1] = (uint8_t)nread;
        st->tx.len = 2;
      } else {
        st->tx.data[1] = (uint8_t)(0x40U | ((nread >> 8) & 0x3FU));
        st->tx.data[2] = (uint8_t)(nread & 0xFFU);
        st->tx.len = 3;
      }
      memcpy(st->tx.data + st->tx.len, chunk, nread);
      st->tx.len += nread;
      st->tx_off = 0;
      continue;
    }
    break;
  }
  if (lsquic_stream_wantwrite(s, 0) < 0) {
    LOG_DEBUG("lsq_on_write_impl: lsquic_stream_wantwrite(0) failed");
  }
  if (lsquic_stream_shutdown(s, 1) != 0) {
    st->err = C_ABSTRACT_HTTP_ERR_IO;
    st->done = 1;
    if (lsquic_stream_close(s) != 0) {
      LOG_DEBUG("lsq_on_write_impl: lsquic_stream_close failed");
    }
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (lsquic_stream_wantread(s, 1) < 0) {
    LOG_DEBUG("lsq_on_write_impl: lsquic_stream_wantread(1) failed");
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Handle lsquic stream read readiness logic.
 * @param s QUIC stream handle.
 * @param st Stream context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_on_read_impl(lsquic_stream_t *s, lsquic_stream_ctx_t *st) {
  c_abstract_http_error_t rc;
  for (;;) {
    uint8_t buf[2048];
    long nr = lsquic_stream_read(s, buf, sizeof(buf));
    if (nr < 0) {
      st->err = C_ABSTRACT_HTTP_ERR_IO;
      st->done = 1;
      if (lsquic_stream_close(s) != 0) {
        LOG_DEBUG("lsq_on_read_impl: lsquic_stream_close failed");
      }
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (nr == 0) {
      if (lsquic_stream_wantread(s, 0) < 0) {
        LOG_DEBUG("lsq_on_read_impl: lsquic_stream_wantread(0) failed");
      }
      st->done = 1;
      if (lsquic_stream_close(s) != 0) {
        LOG_DEBUG("lsq_on_read_impl: lsquic_stream_close failed");
      }
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    rc = lsq_buf_append(&st->rx, buf, (size_t)nr);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      st->err = rc;
      st->done = 1;
      if (lsquic_stream_close(s) != 0) {
        LOG_DEBUG("lsq_on_read_impl: lsquic_stream_close failed");
      }
      return rc;
    }
  }
}

/**
 * @brief LiteSpeed QUIC C-API new-connection callback trampoline.
 * @param stream_if_ctx Engine stream interface user pointer.
 * @param c Connection handle.
 * @return Opaque connection context pointer.
 */
static lsquic_conn_ctx_t *lsq_cb_on_new_conn(void *stream_if_ctx,
                                             lsquic_conn_t *c) {
  (void)c;
  return (lsquic_conn_ctx_t *)stream_if_ctx;
}

/**
 * @brief LiteSpeed QUIC C-API connection-closed callback trampoline.
 * @param c Connection handle.
 */
static void lsq_cb_on_conn_closed(lsquic_conn_t *c) { (void)c; }

/**
 * @brief Dispatch an enum-returning lsquic helper inside a pointer C-ABI
 * callback.
 * @param expr Expression returning `c_abstract_http_error_t`.
 * @param ok_ptr Pointer returned on `C_ABSTRACT_HTTP_SUCCESS`.
 */
#define LSQ_PTR_CB_DISPATCH(expr, ok_ptr)                                      \
  do {                                                                         \
    c_abstract_http_error_t _cb_rc = (expr);                                   \
    if (_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      return NULL;                                                             \
    }                                                                          \
    return (ok_ptr);                                                           \
  } while (0)

/**
 * @brief Dispatch an enum-returning lsquic helper inside a void C-ABI callback.
 * @param expr Expression returning `c_abstract_http_error_t`.
 */
#define LSQ_VOID_CB_DISPATCH(expr)                                             \
  do {                                                                         \
    c_abstract_http_error_t _cb_rc = (expr);                                   \
    if (_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("lsquic_cb: helper returned %d", (int)_cb_rc);                 \
    }                                                                          \
  } while (0)

/**
 * @brief LiteSpeed QUIC C-API new-stream callback trampoline.
 * @param stream_if_ctx Engine stream interface user pointer.
 * @param s Stream handle.
 * @return Stream context pointer.
 */
static lsquic_stream_ctx_t *lsq_cb_on_new_stream(void *stream_if_ctx,
                                                 lsquic_stream_t *s) {
  struct HttpTransportContext *ctx =
      (struct HttpTransportContext *)stream_if_ctx;
  lsquic_stream_ctx_t *st = NULL;
  LSQ_PTR_CB_DISPATCH(lsq_on_new_stream_impl(ctx, s, &st), st);
}

/**
 * @brief LiteSpeed QUIC C-API stream-write callback trampoline.
 * @param s Stream handle.
 * @param h Stream context pointer.
 */
static void lsq_cb_on_write(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  if (!h) {
    return;
  }
  LSQ_VOID_CB_DISPATCH(lsq_on_write_impl(s, h));
}

/**
 * @brief LiteSpeed QUIC C-API stream-read callback trampoline.
 * @param s Stream handle.
 * @param h Stream context pointer.
 */
static void lsq_cb_on_read(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  if (!h) {
    return;
  }
  LSQ_VOID_CB_DISPATCH(lsq_on_read_impl(s, h));
}

/**
 * @brief LiteSpeed QUIC C-API stream-close callback trampoline.
 * @param s Stream handle.
 * @param h Stream context pointer.
 */
static void lsq_cb_on_close(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  (void)s;
  if (h) {
    h->done = 1;
  }
}

/** @brief Static LiteSpeed QUIC stream callback dispatch table */
static const struct lsquic_stream_if g_lsq_stream_if = {
    lsq_cb_on_new_conn, lsq_cb_on_conn_closed, lsq_cb_on_new_stream,
    lsq_cb_on_read,     lsq_cb_on_write,       lsq_cb_on_close};

NO_DISCARD c_abstract_http_error_t http_lsquic_global_init(void) {
  if (g_lsquic_init_count == 0) {
    if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT) != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  g_lsquic_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t http_lsquic_global_cleanup(void) {
  if (g_lsquic_init_count > 0 && --g_lsquic_init_count == 0) {
    lsquic_global_cleanup();
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
/**
 * @brief Transmit outbound QUIC packets generated by lsquic over UDP.
 * @param packets_out_ctx Engine packet-out user pointer (`ctx`).
 * @param specs Array of outbound packet specifications.
 * @param n_specs Count of entries in @p specs.
 * @return Number of packets sent or -1 on error.
 */
static int lsq_packets_out_cb(void *packets_out_ctx,
                              const struct lsquic_out_spec *specs,
                              unsigned n_specs) {
  struct HttpTransportContext *ctx =
      (struct HttpTransportContext *)packets_out_ctx;
  unsigned n;
  if (!ctx || ctx->udp_fd < 0) {
    return -1;
  }
  for (n = 0; n < n_specs; ++n) {
    uint8_t dg[1500];
    size_t dg_len = 0;
    size_t v;
    for (v = 0; v < specs[n].iovlen; ++v) {
      if (dg_len + specs[n].iov[v].iov_len > sizeof(dg)) {
        break;
      }
      memcpy(dg + dg_len, specs[n].iov[v].iov_base, specs[n].iov[v].iov_len);
      dg_len += specs[n].iov[v].iov_len;
    }
    if (sendto(ctx->udp_fd, (const char *)dg, (int)dg_len, 0, specs[n].dest_sa,
               specs[n].dest_sa->sa_family == AF_INET6
                   ? (int)sizeof(struct sockaddr_in6)
                   : (int)sizeof(struct sockaddr_in)) < 0) {
      return (int)n;
    }
  }
  return (int)n_specs;
}
#endif

NO_DISCARD c_abstract_http_error_t
http_lsquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  struct lsquic_engine_api eapi;
  c_abstract_http_error_t rc;

  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_config_init_fail) {
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(c);
    return rc;
  }

  lsquic_engine_init_settings(&c->settings, LSENG_HTTP);
  memset(&eapi, 0, sizeof(eapi));
  eapi.ea_settings = &c->settings;
  eapi.ea_stream_if = &g_lsq_stream_if;
  eapi.ea_stream_if_ctx = c;
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
  c->udp_fd = -1;
  eapi.ea_packets_out = lsq_packets_out_cb;
  eapi.ea_packets_out_ctx = c;
#endif
  eapi.ea_alpn = "h3";

  c->engine = lsquic_engine_new(LSENG_HTTP, &eapi);
  if (!c->engine) {
    c_abstract_http_error_t free_rc = http_config_free(&c->config);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(c);
      return free_rc;
    }
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  *ctx = c;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_lsquic_context_free(struct HttpTransportContext *ctx) {
  c_abstract_http_error_t rch;
  if (!ctx) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  lsquic_engine_destroy(ctx->engine);
  ctx->engine = NULL;
  free(ctx->proxy_host);
  ctx->proxy_host = NULL;
  rch = http_config_free(&ctx->config);
  free(ctx);
  if (rch != C_ABSTRACT_HTTP_SUCCESS) {
    return rch;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Replace a heap-allocated string field with an optional duplicate.
 * @param dst Target string pointer to replace.
 * @param src Optional source string to duplicate.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t lsq_replace_str(char **dst,
                                                          const char *src) {
  free(*dst);
  *dst = NULL;
  if (!src) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_strdup(src, dst);
}

NO_DISCARD c_abstract_http_error_t http_lsquic_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  c_abstract_http_error_t rc;
  if (!ctx || !config) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  free(ctx->proxy_host);
  ctx->proxy_host = NULL;
  ctx->proxy_port = 0;

  if (config->proxy_url) {
    const char *sep = strstr(config->proxy_url, "://");
    char *pscheme = NULL;
    char *ppath = NULL;
    if (!sep) {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    if (!((sep - config->proxy_url == 4 &&
           strncmp(config->proxy_url, "http", 4) == 0) ||
          (sep - config->proxy_url == 5 &&
           strncmp(config->proxy_url, "https", 5) == 0) ||
          (sep - config->proxy_url == 6 &&
           strncmp(config->proxy_url, "masque", 6) == 0))) {
      return C_ABSTRACT_HTTP_ERR_NOTSUP;
    }
    rc = lsq_parse_url(config->proxy_url, &pscheme, &ctx->proxy_host,
                       &ctx->proxy_port, &ppath);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(pscheme);
      free(ppath);
      return rc;
    }
    free(pscheme);
    free(ppath);
  }

  rc = lsq_replace_str(&ctx->config.user_agent, config->user_agent);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = lsq_replace_str(&ctx->config.proxy_url, config->proxy_url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = lsq_replace_str(&ctx->config.proxy_username, config->proxy_username);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = lsq_replace_str(&ctx->config.proxy_password, config->proxy_password);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = c_abstract_http_h3_apply_tls_config(&ctx->config, config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  ctx->verify_peer = config->verify_peer;
  ctx->timeout_ms = (unsigned int)config->timeout_ms;
  if (config->timeout_ms > 0) {
    ctx->settings.es_idle_conn_to = (unsigned)(config->timeout_ms * 1000);
  }
  if (config->connect_timeout_ms > 0) {
    ctx->settings.es_handshake_to =
        (unsigned)(config->connect_timeout_ms * 1000);
  }
  ctx->is_configured = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Flatten multipart parts into a temporary request copy when needed.
 * @param req Input caller request.
 * @param flat_req Scratch request structure to populate if parts are present.
 * @param out_flattened Set to 1 if @p flat_req owns flattened resources.
 * @param out_eff Output pointer to the effective request to transmit.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_prepare_effective_request(const struct HttpRequest *req,
                              struct HttpRequest *flat_req, int *out_flattened,
                              const struct HttpRequest **out_eff) {
  size_t i;
  c_abstract_http_error_t rc;

  *out_flattened = 0;
  *out_eff = req;

  if (req->parts.count == 0 || req->body || req->read_chunk) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!req->parts.parts) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(flat_req, 0, sizeof(*flat_req));
  flat_req->method = req->method;
  flat_req->url = req->url;
  flat_req->parts = req->parts;
  flat_req->on_chunk = req->on_chunk;
  flat_req->on_chunk_user_data = req->on_chunk_user_data;

  rc = http_headers_init(&flat_req->headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  for (i = 0; i < req->headers.count; ++i) {
    rc = http_headers_add(&flat_req->headers, req->headers.headers[i].key,
                          req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t frc;
      frc = http_headers_free(&flat_req->headers);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        return frc;
      }
      return rc;
    }
  }
  rc = http_request_flatten_parts(flat_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t frc;
    free(flat_req->body);
    flat_req->body = NULL;
    frc = http_headers_free(&flat_req->headers);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return frc;
    }
    return rc;
  }
  *out_flattened = 1;
  *out_eff = flat_req;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Release flattened temporary request resources if active.
 * @param flat_req Scratch request structure.
 * @param flattened Non-zero when @p flat_req owns allocated headers/body.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_cleanup_effective_request(struct HttpRequest *flat_req, int flattened) {
  if (!flattened) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  free(flat_req->body);
  flat_req->body = NULL;
  return http_headers_free(&flat_req->headers);
}

/**
 * @brief Free a partially initialized response pointer and return error code.
 * @param res In/out response pointer to release and clear.
 * @param orig_rc Fallback error code if response cleanup succeeds.
 * @return Cleanup error code if cleanup failed, otherwise @p orig_rc.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_cleanup_failed_res(struct HttpResponse **res,
                       c_abstract_http_error_t orig_rc) {
  struct HttpResponse *tmp;
  c_abstract_http_error_t free_rc;
  if (!*res) {
    return orig_rc;
  }
  tmp = *res;
  *res = NULL;
  free_rc = http_response_free(tmp);
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(tmp);
    return free_rc;
  }
  free(tmp);
  return orig_rc;
}

/**
 * @brief Release temporary raw TCP fallback context and return final status.
 * @param raw_ctx Raw transport context to free.
 * @param res In/out response pointer.
 * @param orig_rc Primary status from fallback send.
 * @return Cleanup error if context cleanup fails, otherwise @p orig_rc.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_raw_ctx_cleanup(struct HttpTransportContext *raw_ctx,
                    struct HttpResponse **res,
                    c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t frc;
  frc = http_raw_context_free(raw_ctx);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    return lsq_cleanup_failed_res(res, frc);
  }
  return orig_rc;
}

/**
 * @brief Downgrade failed HTTP/3 QUIC request to TCP HTTP/1.1 raw transport.
 * @param cfg Active transport configuration.
 * @param eff_req Effective HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or fallback error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_fallback_tcp(const struct HttpConfig *cfg,
                 const struct HttpRequest *eff_req, struct HttpResponse **res) {
  struct HttpTransportContext *raw_ctx = NULL;
  c_abstract_http_error_t rc;

  rc = http_raw_context_init(&raw_ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = http_raw_config_apply(raw_ctx, cfg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return lsq_raw_ctx_cleanup(raw_ctx, res, rc);
  }
  rc = http_raw_send(raw_ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return lsq_raw_ctx_cleanup(raw_ctx, res, rc);
  }
  return lsq_raw_ctx_cleanup(raw_ctx, res, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Convert a 16-bit port from host byte order to network byte order.
 * @param hostshort Port in host byte order.
 * @return Port in network byte order.
 */
static unsigned short math_lsq_htons(unsigned short hostshort) {
  unsigned short out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)((hostshort >> 8) & 0xFFU);
  p[1] = (unsigned char)(hostshort & 0xFFU);
  return out;
}

/**
 * @brief Execute a single HTTP/3 transaction over lsquic UDP QUIC.
 * @param ctx Transport context.
 * @param req Effective HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_send_quic_once(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, struct HttpResponse **res) {
  char *scheme = NULL;
  char *host = NULL;
  char *path = NULL;
  unsigned short port = 443;
  unsigned short conn_port;
  const char *conn_host;
  struct lsquic_stream_ctx st;
  struct sockaddr_in peer_sa;
  lsquic_conn_t *conn = NULL;
  c_abstract_http_error_t rc;

  rc = lsq_parse_url(req->url, &scheme, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = c_abstract_http_h3_verify_peer_tls(&ctx->config, scheme, host);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(scheme);
    free(host);
    free(path);
    return rc;
  }

  memset(&st, 0, sizeof(st));
  st.req = req;
  rc = lsq_serialize_request(ctx, req, scheme, host, path, &st.tx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(scheme);
    free(path);
    free(host);
    free(st.tx.data);
    return rc;
  }
  free(scheme);
  free(path);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_res_alloc_fail) {
    free(host);
    free(st.tx.data);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  if (!*res) {
    free(host);
    free(st.tx.data);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_lsquic_res_init_fail) {
    free(*res);
    *res = NULL;
    free(host);
    free(st.tx.data);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    free(host);
    free(st.tx.data);
    return rc;
  }

  conn_host = ctx->proxy_host ? ctx->proxy_host : host;
  conn_port = ctx->proxy_host ? ctx->proxy_port : port;

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
  {
    struct addrinfo hints;
    struct addrinfo *ai = NULL;
    struct sockaddr_storage local_ss;
    socklen_t local_len = (socklen_t)sizeof(local_ss);
    char port_str[16];
    int sfd;
    unsigned int elapsed = 0;
    unsigned int budget = ctx->timeout_ms > 0 ? ctx->timeout_ms : 15000U;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(port_str, sizeof(port_str), "%u", (unsigned int)conn_port);
#else
    c89stringutils_snprintf(port_str, sizeof(port_str), "%u",
                            (unsigned int)conn_port);
#endif
    if (getaddrinfo(conn_host, port_str, &hints, &ai) != 0 || !ai) {
      free(host);
      free(st.tx.data);
      return lsq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_HOSTUNREACH);
    }
    sfd = (int)socket(ai->ai_family, SOCK_DGRAM, 0);
    if (sfd < 0) {
      freeaddrinfo(ai);
      free(host);
      free(st.tx.data);
      return lsq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
    }
    memset(&local_ss, 0, sizeof(local_ss));
    local_ss.ss_family = ai->ai_family;
    if (bind(sfd, (struct sockaddr *)&local_ss, (socklen_t)ai->ai_addrlen) !=
            0 ||
        getsockname(sfd, (struct sockaddr *)&local_ss, &local_len) != 0) {
#if defined(_WIN32)
      closesocket(sfd);
#else
      close(sfd);
#endif
      freeaddrinfo(ai);
      free(host);
      free(st.tx.data);
      return lsq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
    }
    ctx->udp_fd = sfd;
    ctx->active_batch.streams = &st;
    ctx->active_batch.count = 1;
    ctx->active_batch.next_idx = 0;

    conn = lsquic_engine_connect(ctx->engine, N_LSQVER,
                                 (struct sockaddr *)&local_ss, ai->ai_addr, ctx,
                                 NULL, conn_host, 0, NULL, 0, NULL, 0);
    free(host);
    if (!conn) {
      ctx->active_batch.streams = NULL;
      ctx->udp_fd = -1;
#if defined(_WIN32)
      closesocket(sfd);
#else
      close(sfd);
#endif
      freeaddrinfo(ai);
      free(st.tx.data);
      return lsq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
    }

    lsquic_conn_make_stream(conn);
    lsquic_engine_process_conns(ctx->engine);
    if (lsquic_engine_has_unsent_packets(ctx->engine)) {
      lsquic_engine_send_unsent_packets(ctx->engine);
    }

    while (!st.done && st.err == C_ABSTRACT_HTTP_SUCCESS && elapsed < budget) {
      fd_set rfds;
      struct timeval tv;
      int diff_us = 50000;
      int has_tick;
      int sel;
      has_tick = lsquic_engine_earliest_adv_tick(ctx->engine, &diff_us);
      if (!has_tick) {
        diff_us = 25000;
      } else if (diff_us < 1000) {
        diff_us = 1000;
      } else if (diff_us > 50000) {
        diff_us = 50000;
      }
      FD_ZERO(&rfds);
      FD_SET(sfd, &rfds);
      tv.tv_sec = diff_us / 1000000;
      tv.tv_usec = diff_us % 1000000;
      sel = select(sfd + 1, &rfds, NULL, NULL, &tv);
      if (sel > 0 && FD_ISSET(sfd, &rfds)) {
        uint8_t rbuf[1500];
        struct sockaddr_storage peer_from;
        socklen_t from_len = (socklen_t)sizeof(peer_from);
        long nr = (long)recvfrom(sfd, (char *)rbuf, sizeof(rbuf), 0,
                                 (struct sockaddr *)&peer_from, &from_len);
        if (nr > 0) {
          int pkt_rc = lsquic_engine_packet_in(
              ctx->engine, rbuf, (size_t)nr, (struct sockaddr *)&local_ss,
              (struct sockaddr *)&peer_from, ctx, 0);
          if (pkt_rc < 0) {
            LOG_DEBUG("http_lsquic_send: packet_in returned %d", pkt_rc);
          }
        }
      } else {
        elapsed += (unsigned int)((diff_us + 999) / 1000);
      }
      lsquic_engine_process_conns(ctx->engine);
      if (lsquic_engine_has_unsent_packets(ctx->engine)) {
        lsquic_engine_send_unsent_packets(ctx->engine);
      }
    }

    lsquic_conn_close(conn);
    lsquic_engine_process_conns(ctx->engine);
    ctx->udp_fd = -1;
#if defined(_WIN32)
    closesocket(sfd);
#else
    close(sfd);
#endif
    freeaddrinfo(ai);
    if (!st.done && st.err == C_ABSTRACT_HTTP_SUCCESS) {
      st.err = C_ABSTRACT_HTTP_ERR_TIMEOUT;
    }
  }
#else
  memset(&peer_sa, 0, sizeof(peer_sa));
  peer_sa.sin_family = AF_INET;
  peer_sa.sin_port = math_lsq_htons(conn_port);

  ctx->active_batch.streams = &st;
  ctx->active_batch.count = 1;
  ctx->active_batch.next_idx = 0;

  conn =
      lsquic_engine_connect(ctx->engine, 0, NULL, (struct sockaddr *)&peer_sa,
                            ctx, NULL, conn_host, 0, NULL, 0, NULL, 0);
  free(host);
  if (!conn) {
    ctx->active_batch.streams = NULL;
    free(st.tx.data);
    return lsq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  }

  lsquic_conn_make_stream(conn);
  lsquic_engine_process_conns(ctx->engine);
  lsquic_conn_close(conn);
#endif

  ctx->active_batch.streams = NULL;
  free(st.tx.data);

  if (st.err != C_ABSTRACT_HTTP_SUCCESS) {
    free(st.rx.data);
    return lsq_cleanup_failed_res(res, st.err);
  }

  rc = lsq_decode_response(ctx, req, st.rx.data, st.rx.len, *res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(st.rx.data);
    return lsq_cleanup_failed_res(res, rc);
  }
  free(st.rx.data);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up flattened request scratch state and finalize
 * `http_lsquic_send`.
 * @param flat_req Scratch request structure.
 * @param flattened Non-zero when @p flat_req owns allocated buffers.
 * @param res In/out response pointer.
 * @param orig_rc Primary send result.
 * @return Final status enum.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_finish_send(struct HttpRequest *flat_req, int flattened,
                struct HttpResponse **res, c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t cl_rc;
  cl_rc = lsq_cleanup_effective_request(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return lsq_cleanup_failed_res(res, cl_rc);
  }
  return orig_rc;
}

/**
 * @brief Execute one hop over lsquic QUIC with optional TCP fallback.
 * @param ctx Transport context.
 * @param eff_req Effective request for current hop.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_send_hop_once(struct HttpTransportContext *ctx,
                  const struct HttpRequest *eff_req,
                  struct HttpResponse **res) {
  c_abstract_http_error_t rc = lsq_send_quic_once(ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (ctx->config.http3_fallback && (rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED ||
                                       rc == C_ABSTRACT_HTTP_ERR_IO)) {
      return lsq_fallback_tcp(&ctx->config, eff_req, res);
    }
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_lsquic_send(struct HttpTransportContext *ctx,
                 const struct HttpRequest *req, struct HttpResponse **res) {
  struct HttpRequest flat_req;
  struct HttpRequest hop_req;
  const struct HttpRequest *eff_req = NULL;
  char *redir_url = NULL;
  int flattened = 0;
  int hop;
  c_abstract_http_error_t rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res || !ctx->is_configured || !ctx->engine) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = lsq_prepare_effective_request(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  hop_req = *eff_req;
  for (hop = 0;; ++hop) {
    const char *loc = NULL;
    rc = lsq_send_hop_once(ctx, &hop_req, res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(redir_url);
      return lsq_finish_send(&flat_req, flattened, res, rc);
    }
    if (!math_c_abstract_http_should_redirect(ctx->config.follow_redirects,
                                              *res, &loc)) {
      break;
    }
    rc = c_abstract_http_h3_advance_redirect(&hop_req, &redir_url, res, loc,
                                             hop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(redir_url);
      return lsq_finish_send(&flat_req, flattened, res,
                             lsq_cleanup_failed_res(res, rc));
    }
  }

  free(redir_url);
  return lsq_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
}

/** @brief Per-slot parsed URL & response state for `http_lsquic_send_multi` */
struct lsq_multi_slot {
  /** @brief Parsed scheme */
  char *scheme;
  /** @brief Parsed host */
  char *host;
  /** @brief Parsed path */
  char *path;
  /** @brief Parsed UDP port */
  unsigned short port;
  /** @brief Allocated response */
  struct HttpResponse *res;
};

/** @brief Batch coordinator driven by `ModalityEventLoop` */
struct lsq_multi_batch {
  /** @brief Transport context */
  struct HttpTransportContext *ctx;
  /** @brief Multi-request descriptor */
  const struct HttpMultiRequest *multi;
  /** @brief Output futures array */
  struct HttpFuture **futures;
  /** @brief Per-slot metadata array */
  struct lsq_multi_slot *slots;
  /** @brief Concurrent `lsquic_stream_ctx` array */
  struct lsquic_stream_ctx *streams;
  /** @brief Non-zero when caller supplied loop */
  int caller_loop;
  /** @brief First observed error */
  c_abstract_http_error_t first_err;
};

/**
 * @brief Record first batch error and stop caller `ModalityEventLoop` if set.
 * @param loop Active event loop.
 * @param b Batch coordinator.
 * @param err Failure status to record.
 * @return Loop-stop error if non-zero, otherwise @p err.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_stop_batch_on_err(struct ModalityEventLoop *loop, struct lsq_multi_batch *b,
                      c_abstract_http_error_t err) {
  b->first_err = err;
  if (b->caller_loop) {
    c_abstract_http_error_t src = http_loop_stop(loop);
    if (src != C_ABSTRACT_HTTP_SUCCESS) {
      b->first_err = src;
      return src;
    }
  }
  return err;
}

/**
 * @brief Settle one slot via single-request `http_lsquic_send` fallback/path.
 * @param loop Active event loop.
 * @param b Batch coordinator.
 * @param idx Slot index.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_settle_single_slot(struct ModalityEventLoop *loop,
                       struct lsq_multi_batch *b, size_t idx) {
  struct HttpResponse *single_res = NULL;
  c_abstract_http_error_t rc =
      http_lsquic_send(b->ctx, b->multi->requests[idx], &single_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    b->futures[idx]->response = single_res;
    b->futures[idx]->error_code = rc;
    b->futures[idx]->is_ready = 1;
    return lsq_stop_batch_on_err(loop, b, rc);
  }
  b->futures[idx]->response = single_res;
  b->futures[idx]->error_code = C_ABSTRACT_HTTP_SUCCESS;
  b->futures[idx]->is_ready = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free stream buffers across `[start_slot .. end_slot]`, settle
 * `futures[fail_idx]`, and stop caller loop if set.
 * @param loop Active event loop.
 * @param b Batch coordinator.
 * @param start_slot Group start index.
 * @param end_slot Inclusive end slot index to clean.
 * @param fail_idx Failing slot index.
 * @param primary_rc Primary error enum.
 * @return Cleanup or loop-stop error if non-zero, otherwise @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_abort_mux_slot(struct ModalityEventLoop *loop, struct lsq_multi_batch *b,
                   size_t start_slot, size_t end_slot, size_t fail_idx,
                   c_abstract_http_error_t primary_rc) {
  size_t k;
  c_abstract_http_error_t out_rc = primary_rc;
  for (k = start_slot; k <= end_slot; ++k) {
    struct lsq_multi_slot *sk = &b->slots[k];
    struct lsquic_stream_ctx *stk = &b->streams[k - start_slot];
    free(stk->tx.data);
    stk->tx.data = NULL;
    free(stk->rx.data);
    stk->rx.data = NULL;
    free(sk->scheme);
    free(sk->host);
    free(sk->path);
    sk->scheme = sk->host = sk->path = NULL;
    if (sk->res) {
      c_abstract_http_error_t frc = http_response_free(sk->res);
      free(sk->res);
      sk->res = NULL;
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        out_rc = frc;
      }
    }
  }
  b->futures[fail_idx]->response = NULL;
  b->futures[fail_idx]->error_code = out_rc;
  b->futures[fail_idx]->is_ready = 1;
  return lsq_stop_batch_on_err(loop, b, out_rc);
}

/**
 * @brief Event-loop tick callback multiplexing `N` concurrent streams via
 * `lsquic_conn_make_stream` and `lsquic_engine_earliest_adv_tick`.
 * @param loop Active event loop.
 * @param timer_id Fired timer ID.
 * @param user_data Pointer to `struct lsq_multi_batch`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_multi_tick_cb(struct ModalityEventLoop *loop, int timer_id,
                  void *user_data) {
  struct lsq_multi_batch *b = (struct lsq_multi_batch *)user_data;
  size_t i = 0;
  c_abstract_http_error_t rc;
  (void)timer_id;

  while (i < b->multi->count) {
    const struct HttpRequest *req_i = b->multi->requests[i];
    struct lsq_multi_slot *s_i = &b->slots[i];

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)
    if (!req_i || req_i->parts.count > 0) {
      rc = lsq_settle_single_slot(loop, b, i);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      i++;
      continue;
    }

    rc = lsq_parse_url(req_i->url, &s_i->scheme, &s_i->host, &s_i->port,
                       &s_i->path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return lsq_abort_mux_slot(loop, b, i, i, i, rc);
    }
    rc = c_abstract_http_h3_verify_peer_tls(&b->ctx->config, s_i->scheme,
                                            s_i->host);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return lsq_abort_mux_slot(loop, b, i, i, i, rc);
    }

    {
      const char *conn_host =
          b->ctx->proxy_host ? b->ctx->proxy_host : s_i->host;
      unsigned short conn_port =
          b->ctx->proxy_host ? b->ctx->proxy_port : s_i->port;
      struct sockaddr_in peer_sa;
      lsquic_conn_t *conn;
      size_t j;
      size_t mux_cnt = 0;

      for (j = i; j < b->multi->count; ++j) {
        const struct HttpRequest *req_j = b->multi->requests[j];
        struct lsq_multi_slot *s_j = &b->slots[j];
        struct lsquic_stream_ctx *st_j = &b->streams[mux_cnt];
        const char *ch_j;
        unsigned short cp_j;

        if (j > i) {
          if (!req_j || req_j->parts.count > 0) {
            break;
          }
          rc = lsq_parse_url(req_j->url, &s_j->scheme, &s_j->host, &s_j->port,
                             &s_j->path);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return lsq_abort_mux_slot(loop, b, i, j, j, rc);
          }
          rc = c_abstract_http_h3_verify_peer_tls(&b->ctx->config, s_j->scheme,
                                                  s_j->host);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return lsq_abort_mux_slot(loop, b, i, j, j, rc);
          }
        }
        ch_j = b->ctx->proxy_host ? b->ctx->proxy_host : s_j->host;
        cp_j = b->ctx->proxy_host ? b->ctx->proxy_port : s_j->port;
        if (strcmp(ch_j, conn_host) != 0 || cp_j != conn_port) {
          free(s_j->scheme);
          free(s_j->host);
          free(s_j->path);
          s_j->scheme = s_j->host = s_j->path = NULL;
          break;
        }
        memset(st_j, 0, sizeof(*st_j));
        st_j->req = req_j;
        rc = lsq_serialize_request(b->ctx, req_j, s_j->scheme, s_j->host,
                                   s_j->path, &st_j->tx);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return lsq_abort_mux_slot(loop, b, i, j, j, rc);
        }
        s_j->res = (struct HttpResponse *)calloc(1, sizeof(*s_j->res));
        if (!s_j->res) {
          return lsq_abort_mux_slot(loop, b, i, j, j,
                                    C_ABSTRACT_HTTP_ERR_NOMEM);
        }
        rc = http_response_init(s_j->res);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return lsq_abort_mux_slot(loop, b, i, j, j, rc);
        }
        mux_cnt++;
      }

      memset(&peer_sa, 0, sizeof(peer_sa));
      peer_sa.sin_family = AF_INET;
      peer_sa.sin_port = math_lsq_htons(conn_port);
      b->ctx->active_batch.streams = b->streams;
      b->ctx->active_batch.count = mux_cnt;
      b->ctx->active_batch.next_idx = 0;

      conn = lsquic_engine_connect(b->ctx->engine, 0, NULL,
                                   (struct sockaddr *)&peer_sa, b->ctx, NULL,
                                   conn_host, 0, NULL, 0, NULL, 0);
      if (conn) {
        int adv_diff = 0;
        size_t k;
        for (k = 0; k < mux_cnt; ++k) {
          lsquic_conn_make_stream(conn);
        }
        if (lsquic_engine_earliest_adv_tick(b->ctx->engine, &adv_diff) > 0) {
          lsquic_engine_process_conns(b->ctx->engine);
        }
        lsquic_conn_close(conn);
      }
      b->ctx->active_batch.streams = NULL;

      for (j = 0; j < mux_cnt; ++j) {
        struct lsq_multi_slot *s_j = &b->slots[i + j];
        struct lsquic_stream_ctx *st_j = &b->streams[j];
        const char *loc = NULL;
        int decoded_ok = 0;

        free(st_j->tx.data);
        st_j->tx.data = NULL;
        free(s_j->scheme);
        free(s_j->host);
        free(s_j->path);
        s_j->scheme = s_j->host = s_j->path = NULL;

        if (conn && st_j->err == C_ABSTRACT_HTTP_SUCCESS) {
          c_abstract_http_error_t dec_rc =
              lsq_decode_response(b->ctx, b->multi->requests[i + j],
                                  st_j->rx.data, st_j->rx.len, s_j->res);
          if (dec_rc != C_ABSTRACT_HTTP_SUCCESS) {
            return lsq_abort_mux_slot(loop, b, i, i + mux_cnt - 1, i + j,
                                      dec_rc);
          }
          decoded_ok = 1;
        }
        free(st_j->rx.data);
        st_j->rx.data = NULL;

        if (decoded_ok &&
            !math_c_abstract_http_should_redirect(
                b->ctx->config.follow_redirects, s_j->res, &loc)) {
          b->futures[i + j]->response = s_j->res;
          s_j->res = NULL;
          b->futures[i + j]->error_code = C_ABSTRACT_HTTP_SUCCESS;
          b->futures[i + j]->is_ready = 1;
        } else {
          c_abstract_http_error_t cl_rc =
              lsq_cleanup_failed_res(&s_j->res, C_ABSTRACT_HTTP_SUCCESS);
          if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
            return lsq_abort_mux_slot(loop, b, i, i + mux_cnt - 1, i + j,
                                      cl_rc);
          }
          rc = lsq_settle_single_slot(loop, b, i + j);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return lsq_abort_mux_slot(loop, b, i, i + mux_cnt - 1, i + j, rc);
          }
        }
      }
      i += mux_cnt;
    }
#else
    (void)req_i;
    (void)s_i;
    rc = lsq_settle_single_slot(loop, b, i);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    i++;
#endif
  }

  if (b->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Tear down owned or caller loop and batch arrays after lsquic pump.
 * @param owned_loop Owned loop or NULL.
 * @param caller_loop Caller loop or NULL.
 * @param slots Slot metadata array.
 * @param streams Stream context array.
 * @param primary_rc Primary return code.
 * @return Cleanup error if non-zero, otherwise @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t lsq_multi_finish_loop(
    struct ModalityEventLoop *owned_loop, struct ModalityEventLoop *caller_loop,
    struct lsq_multi_slot *slots, struct lsquic_stream_ctx *streams,
    c_abstract_http_error_t primary_rc) {
  free(streams);
  free(slots);
  if (owned_loop) {
    c_abstract_http_error_t frc = http_loop_free(owned_loop);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return frc;
    }
  } else if (caller_loop) {
    c_abstract_http_error_t urc =
        abstract_http_event_loop_test_unstop(caller_loop);
    if (urc != C_ABSTRACT_HTTP_SUCCESS) {
      return urc;
    }
  }
  return primary_rc;
}

/**
 * @brief Schedule and drive `lsq_multi_tick_cb` on `active_loop` (falling back
 * to an internal `owned_loop` when caller supplied a wakeup-only external
 * loop).
 * @param loop Caller loop or NULL.
 * @param batch Initialized batch coordinator.
 * @param slots Slot metadata array (freed before returning).
 * @param streams Stream context array (freed before returning).
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
lsq_run_multi_loop(struct ModalityEventLoop *loop,
                   struct lsq_multi_batch *batch, struct lsq_multi_slot *slots,
                   struct lsquic_stream_ctx *streams) {
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop = loop;
  int timer_id = 0;
  c_abstract_http_error_t rc;
  c_abstract_http_error_t run_rc;

  if (!active_loop) {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(streams);
      free(slots);
      return rc;
    }
    active_loop = owned_loop;
  }

  rc = http_loop_add_timer(active_loop, 0, lsq_multi_tick_cb, batch, &timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP && loop != NULL) {
      c_abstract_http_error_t t_rc;
      c_abstract_http_error_t init_rc = http_loop_init(&owned_loop);
      if (init_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(streams);
        free(slots);
        return init_rc;
      }
      batch->caller_loop = 0;
      t_rc = http_loop_add_timer(owned_loop, 0, lsq_multi_tick_cb, batch,
                                 &timer_id);
      if (t_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return lsq_multi_finish_loop(owned_loop, NULL, slots, streams, t_rc);
      }
      active_loop = owned_loop;
    } else {
      return lsq_multi_finish_loop(owned_loop, loop, slots, streams, rc);
    }
  }

  run_rc = http_loop_run(active_loop);
  if (run_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return lsq_multi_finish_loop(
        owned_loop, batch->caller_loop ? loop : NULL, slots, streams,
        batch->first_err != C_ABSTRACT_HTTP_SUCCESS ? batch->first_err
                                                    : run_rc);
  }
  return lsq_multi_finish_loop(owned_loop, batch->caller_loop ? loop : NULL,
                               slots, streams, batch->first_err);
}

NO_DISCARD c_abstract_http_error_t http_lsquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct lsq_multi_slot *slots;
  struct lsquic_stream_ctx *streams;
  struct lsq_multi_batch batch;
  c_abstract_http_error_t rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !multi || !futures || !ctx->is_configured || !ctx->engine) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  slots = (struct lsq_multi_slot *)calloc(multi->count, sizeof(*slots));
  if (!slots) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  streams = (struct lsquic_stream_ctx *)calloc(multi->count, sizeof(*streams));
  if (!streams) {
    free(slots);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  memset(&batch, 0, sizeof(batch));
  batch.ctx = ctx;
  batch.multi = multi;
  batch.futures = futures;
  batch.slots = slots;
  batch.streams = streams;
  batch.caller_loop = (loop != NULL) ? 1 : 0;
  batch.first_err = C_ABSTRACT_HTTP_SUCCESS;

  rc = lsq_run_multi_loop(loop, &batch, slots, streams);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (loop) {
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Internal test hook verifying 8-byte RFC 9000 varint round-trip.
 * @return C_ABSTRACT_HTTP_SUCCESS on exact round-trip match.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_lsquic_test_varint8(void);
NO_DISCARD c_abstract_http_error_t c_abstract_http_lsquic_test_varint8(void) {
  struct lsq_h3_buf b;
  uint8_t tmp[8];
  size_t off = 0;
  size_t dec = 0;
  c_abstract_http_error_t rc;
  memset(&b, 0, sizeof(b));
  rc = lsq_write_varint(&b, (size_t)1073741824UL);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  memcpy(tmp, b.data, 8);
  free(b.data);
  return lsq_read_varint(tmp, 8, &off, &dec);
}
#endif
