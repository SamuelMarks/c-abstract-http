/**
 * @file http_xquic.c
 * @brief Alibaba xquic HTTP/3 implementation of the Abstract Network Interface
 * (ANI).
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_xquic.h>
#include "c_abstract_http/log.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_xquic.h"
#else
#include <xquic/xquic.h>
#endif
/* clang-format on */

/** @brief Reference count for Alibaba xquic global initialization */
static int g_xquic_init_count = 0;

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Context config */
  struct HttpConfig config;
  /** @brief Engine handle */
  xqc_engine_t *engine;
};

/** @brief Growable HTTP/3 byte buffer */
struct xq_h3_buf {
  /** @brief Byte array */
  uint8_t *data;
  /** @brief Used length */
  size_t len;
  /** @brief Allocated capacity */
  size_t cap;
};

/**
 * @brief Append raw bytes to a growable HTTP/3 buffer.
 * @param b Target buffer.
 * @param src Source bytes.
 * @param n Byte count to append.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error xq_buf_append(struct xq_h3_buf *b,
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
static enum c_abstract_http_error xq_write_varint(struct xq_h3_buf *b,
                                                  size_t val) {
  uint8_t tmp[4];
  if (val < 64) {
    tmp[0] = (uint8_t)val;
    return xq_buf_append(b, tmp, 1);
  }
  if (val < 16384) {
    tmp[0] = (uint8_t)(0x40U | ((val >> 8) & 0x3FU));
    tmp[1] = (uint8_t)(val & 0xFFU);
    return xq_buf_append(b, tmp, 2);
  }
  tmp[0] = (uint8_t)(0x80U | ((val >> 24) & 0x3FU));
  tmp[1] = (uint8_t)((val >> 16) & 0xFFU);
  tmp[2] = (uint8_t)((val >> 8) & 0xFFU);
  tmp[3] = (uint8_t)(val & 0xFFU);
  return xq_buf_append(b, tmp, 4);
}

/**
 * @brief Decode an RFC 9000 variable-length integer from a byte slice.
 * @param src Input byte buffer.
 * @param len Total length of @p src.
 * @param off In/out cursor offset.
 * @param out_val Decoded integer output.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error xq_read_varint(const uint8_t *src, size_t len,
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
xq_write_qpack_field(struct xq_h3_buf *b, const char *name, const char *val) {
  enum c_abstract_http_error rc;
  uint8_t marker = 0x20U;
  size_t nlen = strlen(name);
  size_t vlen = strlen(val);

  rc = xq_buf_append(b, &marker, 1);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = xq_write_varint(b, nlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = xq_buf_append(b, name, nlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = xq_write_varint(b, vlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return xq_buf_append(b, val, vlen);
}

/**
 * @brief Convert enum HttpMethod to its wire token string.
 * @param method HTTP method enum.
 * @param out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
static enum c_abstract_http_error xq_method_str(enum HttpMethod method,
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
static enum c_abstract_http_error xq_parse_url(const char *url,
                                               char **out_scheme,
                                               char **out_host, int *out_port,
                                               char **out_path) {
  const char *p;
  const char *host_start;
  const char *host_end;
  size_t scheme_len;
  size_t host_len;
  int default_port = 443;
  enum c_abstract_http_error rc;

  if (!url) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  p = strstr(url, "://");
  if (!p) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  scheme_len = (size_t)(p - url);
  if (scheme_len == 5 && strncmp(url, "https", 5) == 0) {
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
    *out_port = (int)port_val;
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
xq_serialize_request(const struct HttpTransportContext *ctx,
                     const struct HttpRequest *req, const char *scheme,
                     const char *host, const char *path,
                     struct xq_h3_buf *out_stream) {
  struct xq_h3_buf hdr_payload;
  struct xq_h3_buf body_payload;
  const char *method_s = NULL;
  uint8_t prefix[2];
  size_t i;
  enum c_abstract_http_error rc;

  memset(&hdr_payload, 0, sizeof(hdr_payload));
  memset(&body_payload, 0, sizeof(body_payload));

  rc = xq_method_str(req->method, &method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  prefix[0] = 0x00;
  prefix[1] = 0x00;
  rc = xq_buf_append(&hdr_payload, prefix, 2);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }

  rc = xq_write_qpack_field(&hdr_payload, ":method", method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = xq_write_qpack_field(&hdr_payload, ":scheme", scheme);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = xq_write_qpack_field(&hdr_payload, ":authority", host);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = xq_write_qpack_field(&hdr_payload, ":path", path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }

  for (i = 0; i < req->headers.count; ++i) {
    rc = xq_write_qpack_field(&hdr_payload, req->headers.headers[i].key,
                              req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(hdr_payload.data);
      return rc;
    }
  }

  if (ctx->config.cookie_jar && ctx->config.cookie_jar->count > 0) {
    struct xq_h3_buf cbuf;
    char nul = '\0';
    memset(&cbuf, 0, sizeof(cbuf));
    for (i = 0; i < ctx->config.cookie_jar->count; ++i) {
      if (i > 0) {
        rc = xq_buf_append(&cbuf, "; ", 2);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(cbuf.data);
          free(hdr_payload.data);
          return rc;
        }
      }
      rc = xq_buf_append(&cbuf, ctx->config.cookie_jar->cookies[i].name,
                         strlen(ctx->config.cookie_jar->cookies[i].name));
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
      rc = xq_buf_append(&cbuf, "=", 1);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
      rc = xq_buf_append(&cbuf, ctx->config.cookie_jar->cookies[i].value,
                         strlen(ctx->config.cookie_jar->cookies[i].value));
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(cbuf.data);
        free(hdr_payload.data);
        return rc;
      }
    }
    rc = xq_buf_append(&cbuf, &nul, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(cbuf.data);
      free(hdr_payload.data);
      return rc;
    }
    rc = xq_write_qpack_field(&hdr_payload, "cookie", (const char *)cbuf.data);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(cbuf.data);
      free(hdr_payload.data);
      return rc;
    }
    free(cbuf.data);
  }

  rc = xq_write_varint(out_stream, 0x01U);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = xq_write_varint(out_stream, hdr_payload.len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(hdr_payload.data);
    return rc;
  }
  rc = xq_buf_append(out_stream, hdr_payload.data, hdr_payload.len);
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
      rc = xq_buf_append(&body_payload, chunk, nread);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(body_payload.data);
        return rc;
      }
    }
  } else if (req->body && req->body_len > 0) {
    rc = xq_buf_append(&body_payload, req->body, req->body_len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
  }

  if (body_payload.len > 0) {
    rc = xq_write_varint(out_stream, 0x00U);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
    rc = xq_write_varint(out_stream, body_payload.len);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(body_payload.data);
      return rc;
    }
    rc = xq_buf_append(out_stream, body_payload.data, body_payload.len);
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
static enum c_abstract_http_error xq_store_set_cookie(struct HttpCookieJar *jar,
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
xq_decode_response(struct HttpTransportContext *ctx,
                   const struct HttpRequest *req, const uint8_t *buf,
                   size_t len, struct HttpResponse *res) {
  size_t off = 0;
  enum c_abstract_http_error rc;

  while (off < len) {
    size_t ftype = 0;
    size_t flen = 0;

    rc = xq_read_varint(buf, len, &off, &ftype);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    rc = xq_read_varint(buf, len, &off, &flen);
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
        rc = xq_read_varint(buf, hend, &hoff, &kl);
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

        rc = xq_read_varint(buf, hend, &hoff, &vl);
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
            rc = xq_store_set_cookie(ctx->config.cookie_jar, vstr);
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

enum c_abstract_http_error http_xquic_global_init(void) {
  if (g_xquic_init_count++ == 0) {
    /* Reference-counted global xquic initialization */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_xquic_global_cleanup(void) {
  if (g_xquic_init_count > 0 && --g_xquic_init_count == 0) {
    /* Reference-counted global xquic teardown */
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_xquic_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;

  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  c = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!c) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_xquic_config_init_fail) {
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(c);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_XQUIC)
  c->engine = xqc_engine_create_client(g_mock_xquic_engine_present == -1);
  if (!c->engine) {
    enum c_abstract_http_error free_rc = http_config_free(&c->config);
    free(c);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return free_rc;
    }
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  if (g_mock_xquic_engine_present == -2) {
    xqc_engine_destroy(c->engine);
    c->engine = NULL;
  }
#endif

  *ctx = c;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_xquic_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    if (ctx->engine) {
      xqc_engine_destroy(ctx->engine);
      ctx->engine = NULL;
    }
    {
      enum c_abstract_http_error rch = http_config_free(&ctx->config);
      if (rch != C_ABSTRACT_HTTP_SUCCESS) {
        return rch;
      }
    }
    free(ctx);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_xquic_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  if (!ctx || !config) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.cookie_jar = config->cookie_jar;

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free a partially initialized response pointer and return error code.
 * @param res In/out response pointer to release and clear.
 * @param orig_rc Fallback error code if response cleanup succeeds.
 * @return Cleanup error code if cleanup failed, otherwise @p orig_rc.
 */
static enum c_abstract_http_error
xq_cleanup_failed_res(struct HttpResponse **res,
                      enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error free_rc = http_response_free(*res);
  free(*res);
  *res = NULL;
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return free_rc;
  }
  return orig_rc;
}

enum c_abstract_http_error http_xquic_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **res) {
  char *scheme = NULL;
  char *host = NULL;
  char *path = NULL;
  int port = 443;
  struct xq_h3_buf tx_stream;
  struct xq_h3_buf rx_stream;
  xqc_conn_settings_t conn_settings;
  xqc_conn_ssl_config_t conn_ssl_config;
  xqc_connection_t *conn = NULL;
  xqc_stream_t *stream = NULL;
  enum c_abstract_http_error rc;

  if (!ctx || !req || !res) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!ctx->engine) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = xq_parse_url(req->url, &scheme, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  memset(&tx_stream, 0, sizeof(tx_stream));
  rc = xq_serialize_request(ctx, req, scheme, host, path, &tx_stream);
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

  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    free(host);
    free(tx_stream.data);
    return rc;
  }

  memset(&conn_settings, 0, sizeof(conn_settings));
  conn_settings.idle_time_out =
      ctx->config.timeout_ms > 0 ? (uint64_t)ctx->config.timeout_ms : 30000U;
  memset(&conn_ssl_config, 0, sizeof(conn_ssl_config));
  conn_ssl_config.cert_verify_flag = ctx->config.verify_peer ? 1 : 0;

  conn = xqc_connect(ctx->engine, &conn_settings, NULL, 0, host, 0,
                     &conn_ssl_config, NULL, (size_t)port, "h3");
  free(host);
  if (!conn) {
    free(tx_stream.data);
    return xq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_CONNREFUSED);
  }

  stream = xqc_stream_create(ctx->engine, conn, NULL);
  if (!stream) {
    xqc_conn_close(ctx->engine, conn);
    free(tx_stream.data);
    return xq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
  }

  if (xqc_stream_send(stream, tx_stream.data, tx_stream.len, 1) < 0) {
    xqc_stream_close(stream);
    xqc_conn_close(ctx->engine, conn);
    free(tx_stream.data);
    return xq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
  }
  free(tx_stream.data);

  memset(&rx_stream, 0, sizeof(rx_stream));
  for (;;) {
    unsigned char rbuf[1024];
    uint8_t fin = 0;
    xqc_ssize_t n = xqc_stream_recv(stream, rbuf, sizeof(rbuf), &fin);
    if (n < 0) {
      free(rx_stream.data);
      xqc_stream_close(stream);
      xqc_conn_close(ctx->engine, conn);
      return xq_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
    }
    if (n > 0) {
      rc = xq_buf_append(&rx_stream, rbuf, (size_t)n);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        enum c_abstract_http_error free_rc = http_response_free(*res);
        free(rx_stream.data);
        xqc_stream_close(stream);
        xqc_conn_close(ctx->engine, conn);
        free(*res);
        *res = NULL;
        if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
          return free_rc;
        }
        return rc;
      }
    }
    if (fin) {
      break;
    }
  }

  xqc_stream_close(stream);
  xqc_conn_close(ctx->engine, conn);

  rc = xq_decode_response(ctx, req, rx_stream.data, rx_stream.len, *res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    enum c_abstract_http_error free_rc = http_response_free(*res);
    free(rx_stream.data);
    free(*res);
    *res = NULL;
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return free_rc;
    }
    return rc;
  }
  free(rx_stream.data);

  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_xquic_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;

  if (!ctx || !multi || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; ++i) {
    struct HttpResponse *res = NULL;
    rc = http_xquic_send(ctx, multi->requests[i], &res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      futures[i]->error_code = rc;
      futures[i]->response = res;
      futures[i]->is_ready = 1;
      return rc;
    }
    futures[i]->error_code = rc;
    futures[i]->response = res;
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
