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
#else
#include <netinet/in.h>
#include <sys/socket.h>
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
  /** @brief Serialized request HTTP/3 stream bytes */
  struct lsq_h3_buf tx;
  /** @brief Transmitted offset */
  size_t tx_off;
  /** @brief Accumulated response HTTP/3 stream bytes */
  struct lsq_h3_buf rx;
  /** @brief Stream completion flag */
  int done;
  /** @brief Stream error code */
  enum c_abstract_http_error err;
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
static enum c_abstract_http_error lsq_buf_append(struct lsq_h3_buf *b,
                                                 const void *src, size_t n) {
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
static enum c_abstract_http_error lsq_write_varint(struct lsq_h3_buf *b,
                                                   size_t val) {
  uint8_t tmp[4];
  if (val < 64) {
    tmp[0] = (uint8_t)val;
    return lsq_buf_append(b, tmp, 1);
  }
  if (val < 16384) {
    tmp[0] = (uint8_t)(0x40U | ((val >> 8) & 0x3FU));
    tmp[1] = (uint8_t)(val & 0xFFU);
    return lsq_buf_append(b, tmp, 2);
  }
  tmp[0] = (uint8_t)(0x80U | ((val >> 24) & 0x3FU));
  tmp[1] = (uint8_t)((val >> 16) & 0xFFU);
  tmp[2] = (uint8_t)((val >> 8) & 0xFFU);
  tmp[3] = (uint8_t)(val & 0xFFU);
  return lsq_buf_append(b, tmp, 4);
}

/**
 * @brief Decode an RFC 9000 variable-length integer from a byte slice.
 * @param src Input byte buffer.
 * @param len Total length of @p src.
 * @param off In/out cursor offset.
 * @param out_val Decoded integer output.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error
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
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Write a literal QPACK header field without name reference.
 * @param b Target buffer.
 * @param name Header field name.
 * @param val Header field value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
lsq_write_qpack_field(struct lsq_h3_buf *b, const char *name, const char *val) {
  enum c_abstract_http_error rc;
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
static enum c_abstract_http_error lsq_method_str(enum HttpMethod method,
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
static enum c_abstract_http_error
lsq_parse_url(const char *url, char **out_scheme, char **out_host,
              unsigned short *out_port, char **out_path) {
  const char *p;
  const char *host_start;
  const char *host_end;
  size_t scheme_len;
  size_t host_len;
  unsigned short default_port = 443;
  enum c_abstract_http_error rc;

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
static enum c_abstract_http_error
lsq_build_proxy_auth(const char *user, const char *pass, char **out_hdr) {
  const char *pw = pass ? pass : "";
  size_t ulen = strlen(user);
  size_t plen = strlen(pw);
  char *raw;
  char *b64 = NULL;
  size_t b64_len = 0;
  char *hdr;
  enum c_abstract_http_error rc;

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
static enum c_abstract_http_error
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
  enum c_abstract_http_error rc;

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

  if (req->read_chunk) {
    for (;;) {
      uint8_t chunk[4096];
      size_t nread = 0;
      rc = req->read_chunk(req->read_chunk_user_data, chunk, sizeof(chunk),
                           &nread);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(body_payload.data);
        return rc;
      }
      if (nread == 0) {
        break;
      }
      rc = lsq_buf_append(&body_payload, chunk, nread);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(body_payload.data);
        return rc;
      }
    }
  } else if (req->body && req->body_len > 0) {
    rc = lsq_buf_append(&body_payload, req->body, req->body_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
  }

  if (body_payload.len > 0) {
    rc = lsq_write_varint(out_stream, 0x00U);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
    rc = lsq_write_varint(out_stream, body_payload.len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
    rc = lsq_buf_append(out_stream, body_payload.data, body_payload.len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
  }
  free(body_payload.data);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse a Set-Cookie header value and store it in the cookie jar.
 * @param jar Target cookie jar.
 * @param header_val Raw Set-Cookie header value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
lsq_store_set_cookie(struct HttpCookieJar *jar, const char *header_val) {
  const char *eq;
  const char *semi;
  size_t nlen;
  size_t vlen;
  char *cname;
  char *cval;
  enum c_abstract_http_error rc;

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
static enum c_abstract_http_error
lsq_decode_response(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req, const uint8_t *buf,
                    size_t len, struct HttpResponse *res) {
  size_t off = 0;
  enum c_abstract_http_error rc;

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
static enum c_abstract_http_error
lsq_on_new_stream_impl(struct HttpTransportContext *ctx, lsquic_stream_t *s,
                       lsquic_stream_ctx_t **out_st) {
  lsquic_stream_ctx_t *st;
  int prev_w;
  if (!ctx || ctx->active_batch.next_idx >= ctx->active_batch.count) {
    *out_st = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  st = &ctx->active_batch.streams[ctx->active_batch.next_idx++];
  prev_w = lsquic_stream_wantwrite(s, 1);
  (void)prev_w;
  *out_st = st;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Handle lsquic stream write readiness logic.
 * @param s QUIC stream handle.
 * @param st Stream context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error lsq_on_write_impl(lsquic_stream_t *s,
                                                    lsquic_stream_ctx_t *st) {
  int prev_w;
  int prev_r;
  int cls_rc;
  while (st->tx_off < st->tx.len) {
    long nw = lsquic_stream_write(s, st->tx.data + st->tx_off,
                                  st->tx.len - st->tx_off);
    if (nw <= 0) {
      st->err = C_ABSTRACT_HTTP_ERR_IO;
      st->done = 1;
      cls_rc = lsquic_stream_close(s);
      (void)cls_rc;
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    st->tx_off += (size_t)nw;
  }
  prev_w = lsquic_stream_wantwrite(s, 0);
  (void)prev_w;
  if (lsquic_stream_shutdown(s, 1) != 0) {
    st->err = C_ABSTRACT_HTTP_ERR_IO;
    st->done = 1;
    cls_rc = lsquic_stream_close(s);
    (void)cls_rc;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  prev_r = lsquic_stream_wantread(s, 1);
  (void)prev_r;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Handle lsquic stream read readiness logic.
 * @param s QUIC stream handle.
 * @param st Stream context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error lsq_on_read_impl(lsquic_stream_t *s,
                                                   lsquic_stream_ctx_t *st) {
  int prev_r;
  int cls_rc;
  enum c_abstract_http_error rc;
  for (;;) {
    uint8_t buf[2048];
    long nr = lsquic_stream_read(s, buf, sizeof(buf));
    if (nr < 0) {
      st->err = C_ABSTRACT_HTTP_ERR_IO;
      st->done = 1;
      cls_rc = lsquic_stream_close(s);
      (void)cls_rc;
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    if (nr == 0) {
      prev_r = lsquic_stream_wantread(s, 0);
      (void)prev_r;
      st->done = 1;
      cls_rc = lsquic_stream_close(s);
      (void)cls_rc;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    rc = lsq_buf_append(&st->rx, buf, (size_t)nr);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      st->err = rc;
      st->done = 1;
      cls_rc = lsquic_stream_close(s);
      (void)cls_rc;
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
  enum c_abstract_http_error rc;
  rc = lsq_on_new_stream_impl(ctx, s, &st);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NULL;
  }
  return st;
}

/**
 * @brief LiteSpeed QUIC C-API stream-write callback trampoline.
 * @param s Stream handle.
 * @param h Stream context pointer.
 */
static void lsq_cb_on_write(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  enum c_abstract_http_error rc;
  if (!h) {
    return;
  }
  rc = lsq_on_write_impl(s, h);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("lsq_cb_on_write: stream write error");
  }
}

/**
 * @brief LiteSpeed QUIC C-API stream-read callback trampoline.
 * @param s Stream handle.
 * @param h Stream context pointer.
 */
static void lsq_cb_on_read(lsquic_stream_t *s, lsquic_stream_ctx_t *h) {
  enum c_abstract_http_error rc;
  if (!h) {
    return;
  }
  rc = lsq_on_read_impl(s, h);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("lsq_cb_on_read: stream read error");
  }
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

enum c_abstract_http_error http_lsquic_global_init(void) {
  if (g_lsquic_init_count == 0) {
    if (lsquic_global_init(LSQUIC_GLOBAL_CLIENT) != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  g_lsquic_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_lsquic_global_cleanup(void) {
  if (g_lsquic_init_count > 0 && --g_lsquic_init_count == 0) {
    lsquic_global_cleanup();
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_lsquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  struct lsquic_engine_api eapi;
  enum c_abstract_http_error rc;

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
  eapi.ea_alpn = "h3";

  c->engine = lsquic_engine_new(LSENG_HTTP, &eapi);
  if (!c->engine) {
    enum c_abstract_http_error free_rc = http_config_free(&c->config);
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

enum c_abstract_http_error
http_lsquic_context_free(struct HttpTransportContext *ctx) {
  enum c_abstract_http_error rch;
  if (!ctx) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  lsquic_engine_destroy(ctx->engine);
  ctx->engine = NULL;
  free(ctx->proxy_host);
  ctx->proxy_host = NULL;
  rch = http_config_free(&ctx->config);
  if (rch != C_ABSTRACT_HTTP_SUCCESS) {
    return rch;
  }
  free(ctx);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Replace a heap-allocated string field with an optional duplicate.
 * @param dst Target string pointer to replace.
 * @param src Optional source string to duplicate.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error lsq_replace_str(char **dst, const char *src) {
  free(*dst);
  *dst = NULL;
  if (!src) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_strdup(src, dst);
}

enum c_abstract_http_error
http_lsquic_config_apply(struct HttpTransportContext *ctx,
                         const struct HttpConfig *config) {
  enum c_abstract_http_error rc;
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

  ctx->verify_peer = config->verify_peer;
  ctx->timeout_ms = (unsigned int)config->timeout_ms;
  if (config->timeout_ms > 0) {
    ctx->settings.es_idle_conn_to = (unsigned)(config->timeout_ms * 1000);
  }
  if (config->connect_timeout_ms > 0) {
    ctx->settings.es_handshake_to =
        (unsigned)(config->connect_timeout_ms * 1000);
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.cookie_jar = config->cookie_jar;
  ctx->config.http3_fallback = config->http3_fallback;
  ctx->config.version_mask = config->version_mask;
  ctx->config.tls_version_mask = config->tls_version_mask;
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
static enum c_abstract_http_error
lsq_prepare_effective_request(const struct HttpRequest *req,
                              struct HttpRequest *flat_req, int *out_flattened,
                              const struct HttpRequest **out_eff) {
  size_t i;
  enum c_abstract_http_error rc;

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
      enum c_abstract_http_error frc;
      frc = http_headers_free(&flat_req->headers);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        return frc;
      }
      return rc;
    }
  }
  rc = http_request_flatten_parts(flat_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    enum c_abstract_http_error frc;
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
static enum c_abstract_http_error
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
static enum c_abstract_http_error
lsq_cleanup_failed_res(struct HttpResponse **res,
                       enum c_abstract_http_error orig_rc) {
  struct HttpResponse *tmp;
  enum c_abstract_http_error free_rc;
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
static enum c_abstract_http_error
lsq_raw_ctx_cleanup(struct HttpTransportContext *raw_ctx,
                    struct HttpResponse **res,
                    enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error frc;
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
static enum c_abstract_http_error
lsq_fallback_tcp(const struct HttpConfig *cfg,
                 const struct HttpRequest *eff_req, struct HttpResponse **res) {
  struct HttpTransportContext *raw_ctx = NULL;
  enum c_abstract_http_error rc;

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
static enum c_abstract_http_error
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
  enum c_abstract_http_error rc;

  rc = lsq_parse_url(req->url, &scheme, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  memset(&st, 0, sizeof(st));
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
static enum c_abstract_http_error
lsq_finish_send(struct HttpRequest *flat_req, int flattened,
                struct HttpResponse **res, enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error cl_rc;
  cl_rc = lsq_cleanup_effective_request(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return lsq_cleanup_failed_res(res, cl_rc);
  }
  return orig_rc;
}

enum c_abstract_http_error http_lsquic_send(struct HttpTransportContext *ctx,
                                            const struct HttpRequest *req,
                                            struct HttpResponse **res) {
  struct HttpRequest flat_req;
  const struct HttpRequest *eff_req = NULL;
  int flattened = 0;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res || !ctx->is_configured || !ctx->engine) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = lsq_prepare_effective_request(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = lsq_send_quic_once(ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (ctx->config.http3_fallback && (rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED ||
                                       rc == C_ABSTRACT_HTTP_ERR_IO)) {
      enum c_abstract_http_error fb_rc;
      fb_rc = lsq_fallback_tcp(&ctx->config, eff_req, res);
      if (fb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return lsq_finish_send(&flat_req, flattened, res, fb_rc);
      }
      return lsq_finish_send(&flat_req, flattened, res,
                             C_ABSTRACT_HTTP_SUCCESS);
    }
    return lsq_finish_send(&flat_req, flattened, res, rc);
  }

  return lsq_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
}

enum c_abstract_http_error http_lsquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
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

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    rc = http_lsquic_send(ctx, multi->requests[i], &res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      futures[i]->response = res;
      futures[i]->error_code = rc;
      futures[i]->is_ready = 1;
      return rc;
    }
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }

  if (loop) {
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
