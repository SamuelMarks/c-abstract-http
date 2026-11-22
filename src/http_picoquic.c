/**
 * @file http_picoquic.c
 * @brief Picoquic HTTP/3 implementation of the Abstract Network Interface
 * (ANI).
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <c_abstract_http/http_picoquic.h>
#include <c_abstract_http/http_raw.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "crypto_utils.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_picoquic.h"
#else
#include <picoquic.h>
#endif
/* clang-format on */

/** @brief Reference count for picoquic global initialization */
static int g_picoquic_init_count = 0;

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Picoquic handle */
  picoquic_quic_t *quic;
  /** @brief Configuration state */
  int is_configured;
  /** @brief Peer verification */
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

/** @brief Growable HTTP/3 byte buffer */
struct pq_h3_buf {
  /** @brief Byte array */
  uint8_t *data;
  /** @brief Used length */
  size_t len;
  /** @brief Allocated capacity */
  size_t cap;
};

/** @brief Stream state for receiving HTTP/3 frames from picoquic */
struct pq_stream_state {
  /** @brief Accumulated raw HTTP/3 stream bytes */
  struct pq_h3_buf rx;
  /** @brief Completion flag */
  int fin;
  /** @brief Transport error flag */
  enum c_abstract_http_error err;
};

/**
 * @brief Append raw bytes to a growable HTTP/3 buffer.
 * @param b Target buffer.
 * @param src Source bytes.
 * @param n Byte count to append.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error pq_buf_append(struct pq_h3_buf *b,
                                                const void *src, size_t n) {
  if (n == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
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
static enum c_abstract_http_error pq_write_varint(struct pq_h3_buf *b,
                                                  size_t val) {
  uint8_t tmp[4];
  if (val < 64) {
    tmp[0] = (uint8_t)val;
    return pq_buf_append(b, tmp, 1);
  }
  if (val < 16384) {
    tmp[0] = (uint8_t)(0x40U | ((val >> 8) & 0x3FU));
    tmp[1] = (uint8_t)(val & 0xFFU);
    return pq_buf_append(b, tmp, 2);
  }
  tmp[0] = (uint8_t)(0x80U | ((val >> 24) & 0x3FU));
  tmp[1] = (uint8_t)((val >> 16) & 0xFFU);
  tmp[2] = (uint8_t)((val >> 8) & 0xFFU);
  tmp[3] = (uint8_t)(val & 0xFFU);
  return pq_buf_append(b, tmp, 4);
}

/**
 * @brief Decode an RFC 9000 variable-length integer from a byte slice.
 * @param src Input byte buffer.
 * @param len Total length of @p src.
 * @param off In/out cursor offset.
 * @param out_val Decoded integer output.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error pq_read_varint(const uint8_t *src, size_t len,
                                                 size_t *off, size_t *out_val) {
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
pq_write_qpack_field(struct pq_h3_buf *b, const char *name, const char *val) {
  enum c_abstract_http_error rc;
  uint8_t marker = 0x20U;
  size_t nlen = strlen(name);
  size_t vlen = strlen(val);

  rc = pq_buf_append(b, &marker, 1);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = pq_write_varint(b, nlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = pq_buf_append(b, name, nlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = pq_write_varint(b, vlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return pq_buf_append(b, val, vlen);
}

/**
 * @brief Convert enum HttpMethod to its wire token string.
 * @param method HTTP method enum.
 * @param out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
static enum c_abstract_http_error pq_method_str(enum HttpMethod method,
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
 * @brief Parse an HTTP(S) URL into scheme, host, port, and path components.
 * @param url Input URL string.
 * @param out_scheme Newly allocated scheme string.
 * @param out_host Newly allocated host string.
 * @param out_port Parsed port number.
 * @param out_path Newly allocated path string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code on invalid URL / OOM.
 */
static enum c_abstract_http_error
pq_parse_url(const char *url, char **out_scheme, char **out_host,
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
static int math_pq_strcasecmp(const char *a, const char *b) {
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
pq_build_proxy_auth(const char *user, const char *pass, char **out_hdr) {
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
pq_serialize_request(const struct HttpTransportContext *ctx,
                     const struct HttpRequest *req, const char *scheme,
                     const char *host, const char *path,
                     struct pq_h3_buf *out_stream) {
  struct pq_h3_buf hdr_payload;
  struct pq_h3_buf body_payload;
  const char *method_s = NULL;
  uint8_t prefix[2];
  size_t i;
  int has_ua = 0;
  enum c_abstract_http_error rc;

  memset(&hdr_payload, 0, sizeof(hdr_payload));
  memset(&body_payload, 0, sizeof(body_payload));

  rc = pq_method_str(req->method, &method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  prefix[0] = 0x00;
  prefix[1] = 0x00;
  rc = pq_buf_append(&hdr_payload, prefix, 2);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }

  rc = pq_write_qpack_field(&hdr_payload, ":method", method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = pq_write_qpack_field(&hdr_payload, ":scheme", scheme);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = pq_write_qpack_field(&hdr_payload, ":authority", host);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = pq_write_qpack_field(&hdr_payload, ":path", path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }

  for (i = 0; i < req->headers.count; ++i) {
    if (math_pq_strcasecmp(req->headers.headers[i].key, "User-Agent") == 0) {
      has_ua = 1;
    }
    rc = pq_write_qpack_field(&hdr_payload, req->headers.headers[i].key,
                              req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
  }

  if (!has_ua && ctx->config.user_agent) {
    rc = pq_write_qpack_field(&hdr_payload, "user-agent",
                              ctx->config.user_agent);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
  }

  if (ctx->proxy_host) {
    rc = pq_write_qpack_field(&hdr_payload, "x-masque-target", host);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
    if (ctx->config.proxy_username) {
      char *auth_hdr = NULL;
      rc = pq_build_proxy_auth(ctx->config.proxy_username,
                               ctx->config.proxy_password, &auth_hdr);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(hdr_payload.data);
        return rc;
      }
      rc = pq_write_qpack_field(&hdr_payload, "proxy-authorization", auth_hdr);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(auth_hdr);
        free(hdr_payload.data);
        return rc;
      }
      free(auth_hdr);
    }
  }

  if (ctx->config.cookie_jar && ctx->config.cookie_jar->count > 0) {
    struct pq_h3_buf cbuf;
    char nul = '\0';
    memset(&cbuf, 0, sizeof(cbuf));
    for (i = 0; i < ctx->config.cookie_jar->count; ++i) {
      if (i > 0) {
        rc = pq_buf_append(&cbuf, "; ", 2);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(cbuf.data);
          free(hdr_payload.data);
          return rc;
        }
      }
      rc = pq_buf_append(&cbuf, ctx->config.cookie_jar->cookies[i].name,
                         strlen(ctx->config.cookie_jar->cookies[i].name));
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
      rc = pq_buf_append(&cbuf, "=", 1);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
      rc = pq_buf_append(&cbuf, ctx->config.cookie_jar->cookies[i].value,
                         strlen(ctx->config.cookie_jar->cookies[i].value));
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
    }
    rc = pq_buf_append(&cbuf, &nul, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(cbuf.data);
      free(hdr_payload.data);
      return rc;
    }
    rc = pq_write_qpack_field(&hdr_payload, "cookie", (const char *)cbuf.data);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(cbuf.data);
      free(hdr_payload.data);
      return rc;
    }
    free(cbuf.data);
  }

  /* Emit HEADERS frame (0x01) */
  rc = pq_write_varint(out_stream, 0x01U);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = pq_write_varint(out_stream, hdr_payload.len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = pq_buf_append(out_stream, hdr_payload.data, hdr_payload.len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  free(hdr_payload.data);

  /* Assemble optional request body / streaming upload */
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
      rc = pq_buf_append(&body_payload, chunk, nread);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(body_payload.data);
        return rc;
      }
    }
  } else if (req->body && req->body_len > 0) {
    rc = pq_buf_append(&body_payload, req->body, req->body_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
  }

  if (body_payload.len > 0) {
    rc = pq_write_varint(out_stream, 0x00U);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
    rc = pq_write_varint(out_stream, body_payload.len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
    rc = pq_buf_append(out_stream, body_payload.data, body_payload.len);
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
static enum c_abstract_http_error pq_store_set_cookie(struct HttpCookieJar *jar,
                                                      const char *header_val) {
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
pq_decode_response(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, const uint8_t *buf,
                   size_t len, struct HttpResponse *res) {
  size_t off = 0;
  enum c_abstract_http_error rc;

  while (off < len) {
    size_t ftype = 0;
    size_t flen = 0;

    rc = pq_read_varint(buf, len, &off, &ftype);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = pq_read_varint(buf, len, &off, &flen);
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
        rc = pq_read_varint(buf, hend, &hoff, &kl);
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

        rc = pq_read_varint(buf, hend, &hoff, &vl);
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
            rc = pq_store_set_cookie(ctx->config.cookie_jar, vstr);
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

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Accumulate stream payload bytes and status events from picoquic.
 * @param st Stream state structure.
 * @param bytes Stream payload chunk.
 * @param length Chunk byte length.
 * @param fin_or_event Picoquic stream event code.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
pq_stream_cb_impl(struct pq_stream_state *st, const uint8_t *bytes,
                  size_t length, picoquic_call_back_event_t fin_or_event) {
  enum c_abstract_http_error rc;
  if (fin_or_event == picoquic_callback_close ||
      fin_or_event == picoquic_callback_application_close) {
    st->err = C_ABSTRACT_HTTP_ERR_IO;
    st->fin = 1;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (bytes && length > 0) {
    rc = pq_buf_append(&st->rx, bytes, length);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      st->err = rc;
      st->fin = 1;
      return rc;
    }
  }
  if (fin_or_event == picoquic_callback_stream_fin) {
    st->fin = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Picoquic C-API stream data callback trampoline.
 * @param cnx Connection pointer.
 * @param stream_id QUIC stream identifier.
 * @param bytes Stream payload bytes.
 * @param length Byte count.
 * @param fin_or_event Stream event indicator.
 * @param callback_ctx User context (`struct pq_stream_state *`).
 * @param stream_ctx Per-stream context.
 * @return 0 on success, non-zero error code on failure.
 */
static int pq_stream_cb(picoquic_cnx_t *cnx, uint64_t stream_id, uint8_t *bytes,
                        size_t length, picoquic_call_back_event_t fin_or_event,
                        void *callback_ctx, void *stream_ctx) {
  struct pq_stream_state *st = (struct pq_stream_state *)callback_ctx;
  enum c_abstract_http_error rc;
  (void)cnx;
  (void)stream_id;
  (void)stream_ctx;
  rc = pq_stream_cb_impl(st, bytes, length, fin_or_event);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return (int)rc;
  }
  return 0;
}

enum c_abstract_http_error http_picoquic_global_init(void) {
  if (g_picoquic_init_count++ == 0) {
    /* Reference-counted global QUIC initialization */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_picoquic_global_cleanup(void) {
  if (g_picoquic_init_count > 0 && --g_picoquic_init_count == 0) {
    /* Reference-counted global QUIC teardown */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_picoquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;
  uint8_t reset_seed[16];

  memset(reset_seed, 0, sizeof(reset_seed));

  LOG_DEBUG("http_picoquic_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_picoquic_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    LOG_DEBUG("http_picoquic_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_config_init_fail) {
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(c);
    return rc;
  }

  c->quic = picoquic_create(8, NULL, NULL, NULL, "h3", NULL, NULL, NULL, NULL,
                            reset_seed, 0, NULL, NULL, NULL, 0);
  if (!c->quic) {
    enum c_abstract_http_error free_rc = http_config_free(&c->config);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(c);
      return free_rc;
    }
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_quic_null_on_free == 2) {
    picoquic_free(c->quic);
    c->quic = NULL;
  }
#endif

  *ctx = c;
  LOG_DEBUG("http_picoquic_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_picoquic_context_free(struct HttpTransportContext *ctx) {
  enum c_abstract_http_error rch;
  LOG_DEBUG("http_picoquic_context_free: Entering");
  if (!ctx) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_quic_null_on_free == 1) {
    picoquic_free(ctx->quic);
    ctx->quic = NULL;
  }
#endif
  if (ctx->quic) {
    picoquic_free(ctx->quic);
    ctx->quic = NULL;
  }
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
static enum c_abstract_http_error pq_replace_str(char **dst, const char *src) {
  free(*dst);
  *dst = NULL;
  if (!src) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_strdup(src, dst);
}

enum c_abstract_http_error
http_picoquic_config_apply(struct HttpTransportContext *ctx,
                           const struct HttpConfig *config) {
  enum c_abstract_http_error rc;
  LOG_DEBUG("http_picoquic_config_apply: Entering");
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
    rc = pq_parse_url(config->proxy_url, &pscheme, &ctx->proxy_host,
                      &ctx->proxy_port, &ppath);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(pscheme);
      free(ppath);
      return rc;
    }
    free(pscheme);
    free(ppath);
  }

  rc = pq_replace_str(&ctx->config.user_agent, config->user_agent);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = pq_replace_str(&ctx->config.proxy_url, config->proxy_url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = pq_replace_str(&ctx->config.proxy_username, config->proxy_username);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = pq_replace_str(&ctx->config.proxy_password, config->proxy_password);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  ctx->verify_peer = config->verify_peer;
  ctx->timeout_ms = (unsigned int)config->timeout_ms;

  if (config->version_mask != HTTP_VERSION_DEFAULT &&
      !(config->version_mask & HTTP_VERSION_3)) {
    LOG_DEBUG("http_picoquic_config_apply: Non-H3 mask requested");
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
pq_prepare_effective_request(const struct HttpRequest *req,
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
pq_cleanup_effective_request(struct HttpRequest *flat_req, int flattened) {
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
pq_cleanup_failed_res(struct HttpResponse **res,
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
 * @param res In/out response pointer (freed if context cleanup fails after
 * success).
 * @param orig_rc Primary status from fallback send.
 * @return Cleanup error if context cleanup fails, otherwise @p orig_rc.
 */
static enum c_abstract_http_error
pq_raw_ctx_cleanup(struct HttpTransportContext *raw_ctx,
                   struct HttpResponse **res,
                   enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error frc;
  frc = http_raw_context_free(raw_ctx);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    return pq_cleanup_failed_res(res, frc);
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
pq_fallback_tcp(const struct HttpConfig *cfg, const struct HttpRequest *eff_req,
                struct HttpResponse **res) {
  struct HttpTransportContext *raw_ctx = NULL;
  enum c_abstract_http_error rc;

  rc = http_raw_context_init(&raw_ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = http_raw_config_apply(raw_ctx, cfg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return pq_raw_ctx_cleanup(raw_ctx, res, rc);
  }
  rc = http_raw_send(raw_ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return pq_raw_ctx_cleanup(raw_ctx, res, rc);
  }
  return pq_raw_ctx_cleanup(raw_ctx, res, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Convert a 16-bit port from host byte order to network byte order.
 * @param hostshort Port in host byte order.
 * @return Port in network byte order.
 */
static unsigned short math_pq_htons(unsigned short hostshort) {
  unsigned short out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)((hostshort >> 8) & 0xFFU);
  p[1] = (unsigned char)(hostshort & 0xFFU);
  return out;
}

/**
 * @brief Execute a single HTTP/3 transaction over picoquic UDP QUIC.
 * @param ctx Transport context.
 * @param req Effective HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
pq_send_quic_once(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req, struct HttpResponse **res) {
  char *scheme = NULL;
  char *host = NULL;
  char *path = NULL;
  unsigned short port = 443;
  unsigned short conn_port;
  const char *conn_host;
  struct pq_h3_buf tx_stream;
  struct pq_stream_state st;
  struct sockaddr_in server_address;
  picoquic_cnx_t *cnx = NULL;
  enum c_abstract_http_error rc;

  rc = pq_parse_url(req->url, &scheme, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  memset(&tx_stream, 0, sizeof(tx_stream));
  rc = pq_serialize_request(ctx, req, scheme, host, path, &tx_stream);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(scheme);
    free(path);
    free(host);
    free(tx_stream.data);
    return rc;
  }
  free(scheme);
  free(path);

  *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  if (!*res) {
    free(host);
    free(tx_stream.data);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_picoquic_response_init_fail) {
    free(*res);
    *res = NULL;
    free(host);
    free(tx_stream.data);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    free(host);
    free(tx_stream.data);
    return rc;
  }

  conn_host = ctx->proxy_host ? ctx->proxy_host : host;
  conn_port = ctx->proxy_host ? ctx->proxy_port : port;

  memset(&st, 0, sizeof(st));
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = math_pq_htons(conn_port);

  cnx =
      picoquic_create_client_cnx(ctx->quic, (struct sockaddr *)&server_address,
                                 0, 0, conn_host, "h3", pq_stream_cb, &st);
  free(host);
  if (!cnx) {
    free(tx_stream.data);
    return pq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  }

  if (picoquic_start_client_cnx(cnx) != 0) {
    picoquic_delete_cnx(cnx);
    free(tx_stream.data);
    return pq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
  }

  if (picoquic_add_to_stream(cnx, 0, tx_stream.data, tx_stream.len, 1) != 0) {
    picoquic_delete_cnx(cnx);
    free(tx_stream.data);
    return pq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
  }
  free(tx_stream.data);

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_PICOQUIC)
  picoquic_MockDriveLoopback(cnx);
#endif

  picoquic_close(cnx, 0);
  picoquic_delete_cnx(cnx);

  if (st.err != C_ABSTRACT_HTTP_SUCCESS) {
    free(st.rx.data);
    return pq_cleanup_failed_res(res, st.err);
  }

  rc = pq_decode_response(ctx, req, st.rx.data, st.rx.len, *res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(st.rx.data);
    return pq_cleanup_failed_res(res, rc);
  }
  free(st.rx.data);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up flattened request scratch state and finalize
 * `http_picoquic_send`.
 * @param flat_req Scratch request structure.
 * @param flattened Non-zero when @p flat_req owns allocated buffers.
 * @param res In/out response pointer.
 * @param orig_rc Primary send result.
 * @return Final status enum.
 */
static enum c_abstract_http_error
pq_finish_send(struct HttpRequest *flat_req, int flattened,
               struct HttpResponse **res, enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error cl_rc;
  cl_rc = pq_cleanup_effective_request(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return pq_cleanup_failed_res(res, cl_rc);
  }
  return orig_rc;
}

enum c_abstract_http_error http_picoquic_send(struct HttpTransportContext *ctx,
                                              const struct HttpRequest *req,
                                              struct HttpResponse **res) {
  struct HttpRequest flat_req;
  const struct HttpRequest *eff_req = NULL;
  int flattened = 0;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res || !ctx->is_configured || !ctx->quic) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = pq_prepare_effective_request(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = pq_send_quic_once(ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (ctx->config.http3_fallback && (rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED ||
                                       rc == C_ABSTRACT_HTTP_ERR_IO)) {
      enum c_abstract_http_error fb_rc;
      fb_rc = pq_fallback_tcp(&ctx->config, eff_req, res);
      if (fb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return pq_finish_send(&flat_req, flattened, res, fb_rc);
      }
      return pq_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
    }
    return pq_finish_send(&flat_req, flattened, res, rc);
  }

  return pq_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
}

enum c_abstract_http_error http_picoquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !multi || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    rc = http_picoquic_send(ctx, multi->requests[i], &res);
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
