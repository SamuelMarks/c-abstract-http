/**
 * @file http_nghttp3.c
 * @brief nghttp3 + ngtcp2 HTTP/3 implementation of the Abstract Network
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
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <c_abstract_http/http_nghttp3.h>
#include <c_abstract_http/http_raw.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "crypto_utils.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
#include "mock_stubs/mock_nghttp3.h"
#else
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#if defined(__has_include)
#if (defined(C_ABSTRACT_HTTP_USE_OPENSSL) ||                                   \
     defined(C_ABSTRACT_HTTP_USE_BORINGSSL) ||                                 \
     defined(C_ABSTRACT_HTTP_USE_WOLFSSL)) &&                                  \
    __has_include(<ngtcp2/ngtcp2_crypto_ossl.h>)
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
/** @brief Active ngtcp2 OpenSSL 3.5+ native QUIC TLS 1.3 backend indicator */
#define CAH_NGTCP2_HAVE_OSSL 1
#elif (defined(C_ABSTRACT_HTTP_USE_OPENSSL) ||                                 \
       defined(C_ABSTRACT_HTTP_USE_BORINGSSL) ||                               \
       defined(C_ABSTRACT_HTTP_USE_WOLFSSL)) &&                                \
    __has_include(<ngtcp2/ngtcp2_crypto_quictls.h>)
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_quictls.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
/** @brief Active ngtcp2 QuicTLS/OpenSSL TLS 1.3 backend indicator */
#define CAH_NGTCP2_HAVE_QUICTLS 1
#elif defined(C_ABSTRACT_HTTP_USE_GNUTLS) &&                                   \
    __has_include(<ngtcp2/ngtcp2_crypto_gnutls.h>)
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_gnutls.h>
/** @brief Active ngtcp2 GnuTLS TLS 1.3 backend indicator */
#define CAH_NGTCP2_HAVE_GNUTLS 1
#elif __has_include(<ngtcp2/ngtcp2_crypto_picotls.h>) &&                       \
    __has_include(<picotls.h>)
#include <picotls.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_picotls.h>
/** @brief Active ngtcp2 picotls TLS 1.3 backend indicator */
#define CAH_NGTCP2_HAVE_PICOTLS 1
#endif
#endif
#endif
/* clang-format on */

/** @brief Reference count for nghttp3 + ngtcp2 global initialization */
static int g_nghttp3_init_count = 0;

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief nghttp3 settings template */
  nghttp3_settings h3_settings;
  /** @brief ngtcp2 settings template */
  ngtcp2_settings q_settings;
  /** @brief ngtcp2 transport parameters template */
  ngtcp2_transport_params q_params;
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
  /** @brief Active real UDP + TLS 1.3 QUIC session pointer */
  void *active_real_sess;
  /** @brief Active transport configuration */
  struct HttpConfig config;
};

/** @brief Per-stream HTTP/3 transaction state for nghttp3 callbacks */
struct ngh3_stream_state {
  /** @brief Parent transport context */
  struct HttpTransportContext *ctx;
  /** @brief Originating request */
  const struct HttpRequest *req;
  /** @brief Target response structure */
  struct HttpResponse *res;
  /** @brief Static body read cursor */
  size_t body_off;
  /** @brief Scratch chunk upload buffer */
  uint8_t chunk_buf[4096];
  /** @brief Completion flag */
  int done;
  /** @brief Callback error state */
  c_abstract_http_error_t err;
};

/**
 * @brief Convert enum HttpMethod to its wire token string.
 * @param method HTTP method enum.
 * @param out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_method_str(enum HttpMethod method, const char **out) {
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
ngh3_parse_url(const char *url, char **out_scheme, char **out_host,
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
static int math_ngh3_strcasecmp(const char *a, const char *b) {
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
ngh3_build_proxy_auth(const char *user, const char *pass, char **out_hdr) {
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
 * @brief Format cookie jar entries into a single Cookie header value.
 * @param jar Source cookie jar.
 * @param out_cookie Newly allocated Cookie string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_build_cookie_hdr(const struct HttpCookieJar *jar, char **out_cookie) {
  size_t total = 1;
  size_t i;
  size_t pos = 0;
  char *buf;

  for (i = 0; i < jar->count; ++i) {
    if (i > 0) {
      total += 2;
    }
    total += strlen(jar->cookies[i].name) + 1 + strlen(jar->cookies[i].value);
  }
  buf = (char *)malloc(total);
  if (!buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  for (i = 0; i < jar->count; ++i) {
    size_t nl = strlen(jar->cookies[i].name);
    size_t vl = strlen(jar->cookies[i].value);
    if (i > 0) {
      memcpy(buf + pos, "; ", 2);
      pos += 2;
    }
    memcpy(buf + pos, jar->cookies[i].name, nl);
    pos += nl;
    buf[pos++] = '=';
    memcpy(buf + pos, jar->cookies[i].value, vl);
    pos += vl;
  }
  buf[pos] = '\0';
  *out_cookie = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse a Set-Cookie header value and store it in the cookie jar.
 * @param jar Target cookie jar.
 * @param header_val Raw Set-Cookie header value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_store_set_cookie(struct HttpCookieJar *jar, const char *header_val) {
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
 * @brief Handle received HTTP/3 response header field.
 * @param st Per-stream state.
 * @param kbuf Header name buffer.
 * @param vbuf Header value buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_recv_header_impl(struct ngh3_stream_state *st, nghttp3_vec kbuf,
                      nghttp3_vec vbuf) {
  char *kstr;
  char *vstr;
  c_abstract_http_error_t rc;

  kstr = (char *)malloc(kbuf.len + 1);
  if (!kstr) {
    st->err = C_ABSTRACT_HTTP_ERR_NOMEM;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  vstr = (char *)malloc(vbuf.len + 1);
  if (!vstr) {
    free(kstr);
    st->err = C_ABSTRACT_HTTP_ERR_NOMEM;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(kstr, kbuf.base, kbuf.len);
  kstr[kbuf.len] = '\0';
  memcpy(vstr, vbuf.base, vbuf.len);
  vstr[vbuf.len] = '\0';

  if (strcmp(kstr, ":status") == 0) {
    st->res->status_code = atoi(vstr);
  } else {
    rc = http_headers_add(&st->res->headers, kstr, vstr);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(kstr);
      free(vstr);
      st->err = rc;
      return rc;
    }
    if (st->ctx->config.cookie_jar &&
        (strcmp(kstr, "set-cookie") == 0 || strcmp(kstr, "Set-Cookie") == 0)) {
      rc = ngh3_store_set_cookie(st->ctx->config.cookie_jar, vstr);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(kstr);
        free(vstr);
        st->err = rc;
        return rc;
      }
    }
  }
  free(kstr);
  free(vstr);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Handle received HTTP/3 response DATA payload chunk.
 * @param st Per-stream state.
 * @param data Payload bytes.
 * @param datalen Payload byte count.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_recv_data_impl(struct ngh3_stream_state *st, const uint8_t *data,
                    size_t datalen) {
  c_abstract_http_error_t rc;
  if (datalen == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->req->on_chunk) {
    rc = st->req->on_chunk(st->req->on_chunk_user_data, data, datalen);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      st->err = rc;
      return rc;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  {
    char *nb = (char *)realloc(st->res->body, st->res->body_len + datalen + 1);
    if (!nb) {
      st->err = C_ABSTRACT_HTTP_ERR_NOMEM;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(nb + st->res->body_len, data, datalen);
    st->res->body = nb;
    st->res->body_len += datalen;
    nb[st->res->body_len] = '\0';
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Supply request upload payload chunks to nghttp3.
 * @param st Per-stream state.
 * @param vec Output vector array (at least 1 element).
 * @param pflags Output nghttp3 data flags.
 * @param out_cnt Output vector count (0 or 1).
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_read_data_impl(struct ngh3_stream_state *st, nghttp3_vec *vec,
                    uint32_t *pflags, size_t *out_cnt) {
  c_abstract_http_error_t rc;
  *pflags = NGHTTP3_DATA_FLAG_NONE;
  *out_cnt = 0;

  if (st->req->read_chunk) {
    size_t nread = 0;
    rc = st->req->read_chunk(st->req->read_chunk_user_data, st->chunk_buf,
                             sizeof(st->chunk_buf), &nread);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      st->err = rc;
      return rc;
    }
    if (nread == 0) {
      *pflags = NGHTTP3_DATA_FLAG_EOF;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    vec[0].base = st->chunk_buf;
    vec[0].len = nread;
    *out_cnt = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  {
    size_t rem = st->req->body_len - st->body_off;
    vec[0].base =
        (uint8_t *)(size_t)((const uint8_t *)st->req->body + st->body_off);
    vec[0].len = rem;
    st->body_off += rem;
    *pflags = NGHTTP3_DATA_FLAG_EOF;
    *out_cnt = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

/**
 * @brief Dispatch an enum-returning nghttp3 helper inside an int C-ABI
 * callback.
 * @param expr Expression returning `c_abstract_http_error_t`.
 * @param ok_ret Return value on `C_ABSTRACT_HTTP_SUCCESS`.
 */
#define NGH3_CB_DISPATCH(expr, ok_ret)                                         \
  do {                                                                         \
    c_abstract_http_error_t _cb_rc = (expr);                                   \
    if (_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      return NGHTTP3_ERR_CALLBACK_FAILURE;                                     \
    }                                                                          \
    return (ok_ret);                                                           \
  } while (0)

/**
 * @brief nghttp3 C-API recv_header callback trampoline.
 * @param conn HTTP/3 connection handle.
 * @param stream_id QUIC stream identifier.
 * @param token QPACK token.
 * @param name Header name rcbuf.
 * @param value Header value rcbuf.
 * @param flags Header flags.
 * @param conn_user_data Connection user pointer.
 * @param stream_user_data Stream user pointer (`struct ngh3_stream_state *`).
 * @return 0 on success, NGHTTP3_ERR_CALLBACK_FAILURE on error.
 */
static int ngh3_cb_recv_header(nghttp3_conn *conn, int64_t stream_id,
                               int32_t token, nghttp3_rcbuf *name,
                               nghttp3_rcbuf *value, uint8_t flags,
                               void *conn_user_data, void *stream_user_data) {
  struct ngh3_stream_state *st = (struct ngh3_stream_state *)stream_user_data;
  nghttp3_vec kbuf;
  nghttp3_vec vbuf;
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)flags;
  (void)conn_user_data;
  kbuf = nghttp3_rcbuf_get_buf(name);
  vbuf = nghttp3_rcbuf_get_buf(value);
  NGH3_CB_DISPATCH(ngh3_recv_header_impl(st, kbuf, vbuf), 0);
}

/**
 * @brief nghttp3 C-API recv_data callback trampoline.
 * @param conn HTTP/3 connection handle.
 * @param stream_id QUIC stream identifier.
 * @param data Payload slice.
 * @param datalen Payload length.
 * @param conn_user_data Connection user pointer.
 * @param stream_user_data Stream user pointer (`struct ngh3_stream_state *`).
 * @return 0 on success, NGHTTP3_ERR_CALLBACK_FAILURE on error.
 */
static int ngh3_cb_recv_data(nghttp3_conn *conn, int64_t stream_id,
                             const uint8_t *data, size_t datalen,
                             void *conn_user_data, void *stream_user_data) {
  struct ngh3_stream_state *st = (struct ngh3_stream_state *)stream_user_data;
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  NGH3_CB_DISPATCH(ngh3_recv_data_impl(st, data, datalen), 0);
}

/**
 * @brief nghttp3 C-API end_stream callback trampoline.
 * @param conn HTTP/3 connection handle.
 * @param stream_id QUIC stream identifier.
 * @param conn_user_data Connection user pointer.
 * @param stream_user_data Stream user pointer (`struct ngh3_stream_state *`).
 * @return 0 on success.
 */
static int ngh3_cb_end_stream(nghttp3_conn *conn, int64_t stream_id,
                              void *conn_user_data, void *stream_user_data) {
  struct ngh3_stream_state *st = (struct ngh3_stream_state *)stream_user_data;
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  st->done = 1;
  return 0;
}

/**
 * @brief nghttp3 C-API upload data reader callback trampoline.
 * @param conn HTTP/3 connection handle.
 * @param stream_id QUIC stream identifier.
 * @param vec Output vector array.
 * @param veccnt Vector array capacity.
 * @param pflags Output data flags.
 * @param conn_user_data Connection user pointer.
 * @param stream_user_data Stream user pointer (`struct ngh3_stream_state *`).
 * @return Number of populated vectors, or NGHTTP3_ERR_CALLBACK_FAILURE.
 */
static nghttp3_ssize ngh3_cb_read_data(nghttp3_conn *conn, int64_t stream_id,
                                       nghttp3_vec *vec, size_t veccnt,
                                       uint32_t *pflags, void *conn_user_data,
                                       void *stream_user_data) {
  struct ngh3_stream_state *st = (struct ngh3_stream_state *)stream_user_data;
  size_t cnt = 0;
  (void)conn;
  (void)stream_id;
  (void)veccnt;
  (void)conn_user_data;
  NGH3_CB_DISPATCH(ngh3_read_data_impl(st, vec, pflags, &cnt),
                   (nghttp3_ssize)cnt);
}

NO_DISCARD c_abstract_http_error_t http_nghttp3_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  g_nghttp3_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t http_nghttp3_global_cleanup(void) {
  if (g_nghttp3_init_count > 0) {
    g_nghttp3_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_nghttp3_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  c_abstract_http_error_t rc;

  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  c = (struct HttpTransportContext *)calloc(1, sizeof(*c));
  if (!c) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_config_init_fail) {
    free(c);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(c);
    return rc;
  }

  nghttp3_settings_default(&c->h3_settings);
  ngtcp2_settings_default(&c->q_settings);
  ngtcp2_transport_params_default(&c->q_params);

  *ctx = c;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_nghttp3_context_free(struct HttpTransportContext *ctx) {
  c_abstract_http_error_t rch;
  if (!ctx) {
    return C_ABSTRACT_HTTP_SUCCESS;
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
NO_DISCARD static c_abstract_http_error_t ngh3_replace_str(char **dst,
                                                           const char *src) {
  free(*dst);
  *dst = NULL;
  if (!src) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_strdup(src, dst);
}

NO_DISCARD c_abstract_http_error_t http_nghttp3_config_apply(
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
    rc = ngh3_parse_url(config->proxy_url, &pscheme, &ctx->proxy_host,
                        &ctx->proxy_port, &ppath);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(pscheme);
      free(ppath);
      return rc;
    }
    free(pscheme);
    free(ppath);
  }

  rc = ngh3_replace_str(&ctx->config.user_agent, config->user_agent);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = ngh3_replace_str(&ctx->config.proxy_url, config->proxy_url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = ngh3_replace_str(&ctx->config.proxy_username, config->proxy_username);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = ngh3_replace_str(&ctx->config.proxy_password, config->proxy_password);
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
    ctx->q_params.max_idle_timeout =
        (uint64_t)config->timeout_ms * (uint64_t)1000000UL;
  }
  if (config->connect_timeout_ms > 0) {
    ctx->q_settings.handshake_timeout =
        (uint64_t)config->connect_timeout_ms * (uint64_t)1000000UL;
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
ngh3_prepare_effective_request(const struct HttpRequest *req,
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
ngh3_cleanup_effective_request(struct HttpRequest *flat_req, int flattened) {
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
ngh3_cleanup_failed_res(struct HttpResponse **res,
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
ngh3_raw_ctx_cleanup(struct HttpTransportContext *raw_ctx,
                     struct HttpResponse **res,
                     c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t frc;
  frc = http_raw_context_free(raw_ctx);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_cleanup_failed_res(res, frc);
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
ngh3_fallback_tcp(const struct HttpConfig *cfg,
                  const struct HttpRequest *eff_req,
                  struct HttpResponse **res) {
  struct HttpTransportContext *raw_ctx = NULL;
  c_abstract_http_error_t rc;

  rc = http_raw_context_init(&raw_ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = http_raw_config_apply(raw_ctx, cfg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_raw_ctx_cleanup(raw_ctx, res, rc);
  }
  rc = http_raw_send(raw_ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_raw_ctx_cleanup(raw_ctx, res, rc);
  }
  return ngh3_raw_ctx_cleanup(raw_ctx, res, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Populate an nghttp3_nv entry from NUL-terminated strings.
 * @param nv_ptr Pointer to target nghttp3_nv structure.
 * @param k_str Header name string.
 * @param v_str Header value string.
 */
#define NGH3_SET_NV(nv_ptr, k_str, v_str)                                      \
  do {                                                                         \
    nghttp3_nv *_nv = (nv_ptr);                                                \
    const char *_ks = (k_str);                                                 \
    const char *_vs = (v_str);                                                 \
    _nv->name = (const uint8_t *)_ks;                                          \
    _nv->namelen = strlen(_ks);                                                \
    _nv->value = (const uint8_t *)_vs;                                         \
    _nv->valuelen = strlen(_vs);                                               \
    _nv->flags = NGHTTP3_NV_FLAG_NONE;                                         \
  } while (0)

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)

#if defined(_WIN32)
/** @brief Platform UDP socket descriptor type */
typedef SOCKET ngh3_udp_sock_t;
/** @brief Invalid UDP socket sentinel */
#define NGH3_INVALID_SOCK INVALID_SOCKET
/** @brief Close UDP socket descriptor */
#define NGH3_CLOSE_SOCK(s) closesocket(s)
#else
/** @brief Platform UDP socket descriptor type */
typedef int ngh3_udp_sock_t;
/** @brief Invalid UDP socket sentinel */
#define NGH3_INVALID_SOCK (-1)
/** @brief Close UDP socket descriptor */
#define NGH3_CLOSE_SOCK(s) close(s)
#endif

/** @brief Active real-network UDP + TLS 1.3 QUIC session state */
struct ngh3_real_udp_sess {
#if defined(CAH_NGTCP2_HAVE_OSSL) || defined(CAH_NGTCP2_HAVE_QUICTLS) ||       \
    defined(CAH_NGTCP2_HAVE_GNUTLS) || defined(CAH_NGTCP2_HAVE_PICOTLS)
  /** @brief ngtcp2_crypto connection reference (must be first for user_data) */
  ngtcp2_crypto_conn_ref conn_ref;
#endif
  /** @brief Non-blocking UDP socket */
  ngh3_udp_sock_t udp_fd;
  /** @brief Bound local socket address */
  struct sockaddr_storage local_addr;
  /** @brief Byte length of `local_addr` */
  ngtcp2_socklen local_len;
  /** @brief Target remote socket address */
  struct sockaddr_storage remote_addr;
  /** @brief Byte length of `remote_addr` */
  ngtcp2_socklen remote_len;
  /** @brief Active ngtcp2 connection */
  ngtcp2_conn *qconn;
  /** @brief Active nghttp3 connection */
  nghttp3_conn *h3conn;
  /** @brief Owning transport context */
  struct HttpTransportContext *ctx;
#if defined(CAH_NGTCP2_HAVE_OSSL)
  /** @brief OpenSSL context */
  SSL_CTX *ssl_ctx;
  /** @brief OpenSSL session */
  SSL *ssl;
  /** @brief ngtcp2 OpenSSL 3.5+ wrapper context */
  ngtcp2_crypto_ossl_ctx *ossl_ctx;
#elif defined(CAH_NGTCP2_HAVE_QUICTLS)
  /** @brief QuicTLS SSL_CTX */
  SSL_CTX *ssl_ctx;
  /** @brief QuicTLS SSL handle */
  SSL *ssl;
#elif defined(CAH_NGTCP2_HAVE_GNUTLS)
  /** @brief GnuTLS credentials */
  gnutls_certificate_credentials_t gtls_cred;
  /** @brief GnuTLS session */
  gnutls_session_t gtls_sess;
  /** @brief Non-zero once `gtls_cred` initialized */
  int has_gtls_cred;
  /** @brief Non-zero once `gtls_sess` initialized */
  int has_gtls_sess;
#elif defined(CAH_NGTCP2_HAVE_PICOTLS)
  /** @brief ngtcp2 picotls wrapper context */
  ngtcp2_crypto_picotls_ctx ptls_cctx;
  /** @brief picotls client context */
  ptls_context_t ptls_ctx;
  /** @brief Non-zero once `ptls_cctx` initialized */
  int has_ptls;
#endif
};

/**
 * @brief Query current wall/monotonic clock in nanoseconds for `ngtcp2_tstamp`.
 * @param out_ns Output timestamp in nanoseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_now_ns(ngtcp2_tstamp *out_ns) {
#if defined(_WIN32)
  FILETIME ft;
  ULARGE_INTEGER uli;
  if (!out_ns) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  GetSystemTimeAsFileTime(&ft);
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  *out_ns = (ngtcp2_tstamp)uli.QuadPart * (ngtcp2_tstamp)100U;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  struct timeval tv;
  ngtcp2_tstamp ns_per_s = (ngtcp2_tstamp)1000000UL * (ngtcp2_tstamp)1000UL;
  if (!out_ns) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  gettimeofday(&tv, NULL);
  *out_ns = (ngtcp2_tstamp)tv.tv_sec * ns_per_s +
            (ngtcp2_tstamp)tv.tv_usec * (ngtcp2_tstamp)1000UL;
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Put UDP socket @p fd into non-blocking mode.
 * @param fd Open UDP socket descriptor.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_set_nonblock(ngh3_udp_sock_t fd) {
#if defined(_WIN32)
  u_long nb = 1UL;
  return ioctlsocket(fd, FIONBIO, &nb) == 0 ? C_ABSTRACT_HTTP_SUCCESS
                                            : C_ABSTRACT_HTTP_ERR_IO;
#else
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Populate random bytes requested by ngtcp2 callbacks.
 * @param dest Destination buffer.
 * @param destlen Requested byte length.
 * @param rand_ctx Optional ngtcp2 random context.
 */
static void ngh3_real_rand_cb(uint8_t *dest, size_t destlen,
                              const ngtcp2_rand_ctx *rand_ctx) {
  c_abstract_http_error_t rc;
  if (rand_ctx) {
  }
  rc = c_abstract_http_random_bytes(dest, destlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    memset(dest, 0x42, destlen);
  }
}

#if defined(NGTCP2_VERSION_NUM) && NGTCP2_VERSION_NUM >= 0x011600
/**
 * @brief Generate a new connection ID and stateless reset token (`ngtcp2`
 * >= 1.22).
 * @param conn Active ngtcp2 connection.
 * @param cid Output connection ID structure.
 * @param token Output stateless reset token structure.
 * @param cidlen Requested CID byte length.
 * @param user_data Connection user pointer.
 * @return 0 on success, negative ngtcp2 error code on failure.
 */
static int ngh3_real_get_new_cid2_cb(ngtcp2_conn *conn, ngtcp2_cid *cid,
                                     ngtcp2_stateless_reset_token *token,
                                     size_t cidlen, void *user_data) {
  c_abstract_http_error_t rc;
  if (conn || user_data) {
  }
  rc = c_abstract_http_random_bytes(cid->data, cidlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  rc = c_abstract_http_random_bytes(token->data, sizeof(token->data));
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  cid->datalen = cidlen;
  return 0;
}
#else
/**
 * @brief Generate a new connection ID and stateless reset token (`ngtcp2`
 * < 1.22).
 * @param conn Active ngtcp2 connection.
 * @param cid Output connection ID structure.
 * @param token Output stateless reset token buffer.
 * @param cidlen Requested CID byte length.
 * @param user_data Connection user pointer.
 * @return 0 on success, negative ngtcp2 error code on failure.
 */
static int ngh3_real_get_new_cid_cb(ngtcp2_conn *conn, ngtcp2_cid *cid,
                                    uint8_t *token, size_t cidlen,
                                    void *user_data) {
  c_abstract_http_error_t rc;
  if (conn || user_data) {
  }
  rc = c_abstract_http_random_bytes(cid->data, cidlen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  rc = c_abstract_http_random_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  cid->datalen = cidlen;
  return 0;
}
#endif

/**
 * @brief Forward incoming QUIC STREAM payload frames from `ngtcp2` into
 * `nghttp3_conn_read_stream`.
 * @param conn Active ngtcp2 connection.
 * @param flags Stream data flags (`NGTCP2_STREAM_DATA_FLAG_FIN`, etc.).
 * @param stream_id QUIC stream identifier.
 * @param offset Byte offset within the stream.
 * @param data Stream payload buffer.
 * @param datalen Stream payload byte length.
 * @param user_data Session pointer (`struct ngh3_real_udp_sess *`).
 * @param stream_user_data Per-stream user pointer.
 * @return 0 on success, or `NGTCP2_ERR_CALLBACK_FAILURE` on error.
 */
static int ngh3_real_recv_stream_data_cb(ngtcp2_conn *conn, uint32_t flags,
                                         int64_t stream_id, uint64_t offset,
                                         const uint8_t *data, size_t datalen,
                                         void *user_data,
                                         void *stream_user_data) {
  struct ngh3_real_udp_sess *s = (struct ngh3_real_udp_sess *)user_data;
  int fin = (flags & NGTCP2_STREAM_DATA_FLAG_FIN) ? 1 : 0;
  nghttp3_ssize nconsumed;
  if (offset || stream_user_data) {
  }
  if (!s || !s->h3conn) {
    return 0;
  }
  nconsumed =
      nghttp3_conn_read_stream(s->h3conn, stream_id, data, datalen, fin);
  if (nconsumed < 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  ngtcp2_conn_extend_max_stream_offset(conn, stream_id, (uint64_t)nconsumed);
  ngtcp2_conn_extend_max_offset(conn, (uint64_t)nconsumed);
  return 0;
}

/**
 * @brief Notify `nghttp3` of acknowledged QUIC stream data bytes.
 * @param conn Active ngtcp2 connection.
 * @param stream_id QUIC stream identifier.
 * @param offset Acknowledged byte offset.
 * @param datalen Acknowledged byte count.
 * @param user_data Session pointer (`struct ngh3_real_udp_sess *`).
 * @param stream_user_data Per-stream user pointer.
 * @return 0 on success, or `NGTCP2_ERR_CALLBACK_FAILURE` on error.
 */
static int ngh3_real_acked_stream_data_cb(ngtcp2_conn *conn, int64_t stream_id,
                                          uint64_t offset, uint64_t datalen,
                                          void *user_data,
                                          void *stream_user_data) {
  struct ngh3_real_udp_sess *s = (struct ngh3_real_udp_sess *)user_data;
  if (conn || offset || stream_user_data) {
  }
  if (s && s->h3conn &&
      nghttp3_conn_add_ack_offset(s->h3conn, stream_id, datalen) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

/**
 * @brief Notify `nghttp3` when a QUIC stream closes.
 * @param conn Active ngtcp2 connection.
 * @param flags Stream close flags.
 * @param stream_id Closed QUIC stream identifier.
 * @param app_error_code Application error code.
 * @param user_data Session pointer (`struct ngh3_real_udp_sess *`).
 * @param stream_user_data Per-stream user pointer.
 * @return 0 on success, or `NGTCP2_ERR_CALLBACK_FAILURE` on error.
 */
static int ngh3_real_stream_close_cb(ngtcp2_conn *conn, uint32_t flags,
                                     int64_t stream_id, uint64_t app_error_code,
                                     void *user_data, void *stream_user_data) {
  struct ngh3_real_udp_sess *s = (struct ngh3_real_udp_sess *)user_data;
  if (conn || stream_user_data) {
  }
  if (!(flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET)) {
    app_error_code = NGHTTP3_H3_NO_ERROR;
  }
  if (s && s->h3conn) {
    int rv = nghttp3_conn_close_stream(s->h3conn, stream_id, app_error_code);
    if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }
  }
  return 0;
}

/**
 * @brief Unblock flow-controlled `nghttp3` stream when `MAX_STREAM_DATA` grows.
 * @param conn Active ngtcp2 connection.
 * @param stream_id QUIC stream identifier.
 * @param max_data Updated maximum stream data credit.
 * @param user_data Session pointer (`struct ngh3_real_udp_sess *`).
 * @param stream_user_data Per-stream user pointer.
 * @return 0 on success, or `NGTCP2_ERR_CALLBACK_FAILURE` on error.
 */
static int ngh3_real_extend_max_stream_data_cb(ngtcp2_conn *conn,
                                               int64_t stream_id,
                                               uint64_t max_data,
                                               void *user_data,
                                               void *stream_user_data) {
  struct ngh3_real_udp_sess *s = (struct ngh3_real_udp_sess *)user_data;
  if (conn || max_data || stream_user_data) {
  }
  if (s && s->h3conn &&
      nghttp3_conn_unblock_stream(s->h3conn, stream_id) != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
  return 0;
}

#if defined(CAH_NGTCP2_HAVE_OSSL) || defined(CAH_NGTCP2_HAVE_QUICTLS) ||       \
    defined(CAH_NGTCP2_HAVE_GNUTLS) || defined(CAH_NGTCP2_HAVE_PICOTLS)
/**
 * @brief Resolve `ngtcp2_conn *` from `ngtcp2_crypto_conn_ref`.
 * @param conn_ref Crypto connection reference.
 * @return Bound `ngtcp2_conn *` handle.
 */
static ngtcp2_conn *ngh3_real_get_conn_cb(ngtcp2_crypto_conn_ref *conn_ref) {
  struct ngh3_real_udp_sess *s =
      (struct ngh3_real_udp_sess *)conn_ref->user_data;
  return s->qconn;
}

/**
 * @brief Wire standard `ngtcp2_crypto_*_cb` TLS 1.3 callbacks into @p qcbs.
 * @param qcbs Destination `ngtcp2_callbacks` table.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_wire_crypto_cbs(ngtcp2_callbacks *qcbs) {
  qcbs->client_initial = ngtcp2_crypto_client_initial_cb;
  qcbs->recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
  qcbs->encrypt = ngtcp2_crypto_encrypt_cb;
  qcbs->decrypt = ngtcp2_crypto_decrypt_cb;
  qcbs->hp_mask = ngtcp2_crypto_hp_mask_cb;
  qcbs->recv_retry = ngtcp2_crypto_recv_retry_cb;
  qcbs->update_key = ngtcp2_crypto_update_key_cb;
  qcbs->delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
  qcbs->delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
#if defined(NGTCP2_VERSION_NUM) && NGTCP2_VERSION_NUM >= 0x011600
  qcbs->get_path_challenge_data2 = ngtcp2_crypto_get_path_challenge_data2_cb;
#else
  qcbs->get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
#endif
  qcbs->version_negotiation = ngtcp2_crypto_version_negotiation_cb;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Release a real UDP + TLS 1.3 QUIC session and close its UDP socket.
 * @param ctx Owning transport context.
 * @param qconn Active ngtcp2 connection or NULL.
 * @param h3conn Active nghttp3 connection or NULL.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_free_sess(struct HttpTransportContext *ctx, ngtcp2_conn *qconn,
                    nghttp3_conn *h3conn) {
  struct ngh3_real_udp_sess *s =
      (struct ngh3_real_udp_sess *)ctx->active_real_sess;
  ctx->active_real_sess = NULL;
  if (h3conn) {
    nghttp3_conn_del(h3conn);
  }
  if (qconn) {
    ngtcp2_conn_del(qconn);
  }
  if (s) {
#if defined(CAH_NGTCP2_HAVE_OSSL)
    if (s->ssl) {
      SSL_set_app_data(s->ssl, NULL);
      SSL_free(s->ssl);
    }
    if (s->ossl_ctx) {
      ngtcp2_crypto_ossl_ctx_del(s->ossl_ctx);
    }
    if (s->ssl_ctx) {
      SSL_CTX_free(s->ssl_ctx);
    }
#elif defined(CAH_NGTCP2_HAVE_QUICTLS)
    if (s->ssl) {
      SSL_set_app_data(s->ssl, NULL);
      SSL_free(s->ssl);
    }
    if (s->ssl_ctx) {
      SSL_CTX_free(s->ssl_ctx);
    }
#elif defined(CAH_NGTCP2_HAVE_GNUTLS)
    if (s->has_gtls_sess) {
      gnutls_deinit(s->gtls_sess);
    }
    if (s->has_gtls_cred) {
      gnutls_certificate_free_credentials(s->gtls_cred);
    }
#elif defined(CAH_NGTCP2_HAVE_PICOTLS)
    if (s->has_ptls) {
      if (s->ptls_cctx.ptls) {
        ptls_free(s->ptls_cctx.ptls);
        s->ptls_cctx.ptls = NULL;
      }
      ngtcp2_crypto_picotls_deinit(&s->ptls_cctx);
    }
#endif
    if (s->udp_fd != NGH3_INVALID_SOCK) {
      NGH3_CLOSE_SOCK(s->udp_fd);
    }
    free(s);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Release a real UDP + TLS 1.3 QUIC session while preserving
 * @p primary_rc.
 * @param ctx Owning transport context.
 * @param qconn Active ngtcp2 connection or NULL.
 * @param h3conn Active nghttp3 connection or NULL.
 * @param primary_rc Primary operation status to preserve if non-zero.
 * @return @p primary_rc if non-zero, otherwise `ngh3_real_free_sess` status.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_free_sess_with_err(struct HttpTransportContext *ctx,
                             ngtcp2_conn *qconn, nghttp3_conn *h3conn,
                             c_abstract_http_error_t primary_rc) {
  c_abstract_http_error_t frc = ngh3_real_free_sess(ctx, qconn, h3conn);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    return primary_rc != C_ABSTRACT_HTTP_SUCCESS ? primary_rc : frc;
  }
  return primary_rc;
}

/**
 * @brief Initialize TLS 1.3 native handle (`ossl` / `quictls` / `gnutls` /
 * `picotls`) and bind onto `s->qconn`.
 * @param s Active real UDP QUIC session.
 * @param cfg HTTP configuration (custom CA, mTLS, verify_peer).
 * @param conn_host Target server SNI hostname.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_bind_tls13(struct ngh3_real_udp_sess *s, const struct HttpConfig *cfg,
                     const char *conn_host) {
#if defined(CAH_NGTCP2_HAVE_OSSL) || defined(CAH_NGTCP2_HAVE_QUICTLS)
  static const unsigned char k_h3_alpn[] = {2, 'h', '3'};
  if (conn_host || cfg) {
  }
#if defined(CAH_NGTCP2_HAVE_OSSL)
  if (ngtcp2_crypto_ossl_init() != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#else
  if (ngtcp2_crypto_quictls_init() != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  s->ssl_ctx = SSL_CTX_new(TLS_client_method());
  if (!s->ssl_ctx) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if !defined(CAH_NGTCP2_HAVE_OSSL)
  if (ngtcp2_crypto_quictls_configure_client_context(s->ssl_ctx) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  SSL_CTX_set_verify(
      s->ssl_ctx,
      (cfg && !cfg->verify_peer) ? SSL_VERIFY_NONE : SSL_VERIFY_PEER, NULL);
  if (cfg && cfg->ca_path) {
    if (SSL_CTX_load_verify_locations(s->ssl_ctx, cfg->ca_path, NULL) != 1) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  } else if (!cfg || !cfg->ca_buffer || cfg->ca_buffer_len == 0U) {
    SSL_CTX_set_default_verify_paths(s->ssl_ctx);
  }
#if !defined(C_ABSTRACT_HTTP_USE_WOLFSSL)
  if (cfg && cfg->ca_buffer && cfg->ca_buffer_len > 0U) {
    BIO *cbio =
        BIO_new_mem_buf((const void *)cfg->ca_buffer, (int)cfg->ca_buffer_len);
    X509_STORE *st = SSL_CTX_get_cert_store(s->ssl_ctx);
    int added = 0;
    if (!cbio || !st) {
      if (cbio) {
        BIO_free(cbio);
      }
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    for (;;) {
      X509 *xc = PEM_read_bio_X509(cbio, NULL, NULL, NULL);
      if (!xc) {
        break;
      }
      if (X509_STORE_add_cert(st, xc) == 1) {
        added = 1;
      }
      X509_free(xc);
    }
    BIO_free(cbio);
    if (!added) {
      const unsigned char *dp = cfg->ca_buffer;
      X509 *dxc = d2i_X509(NULL, &dp, (long)cfg->ca_buffer_len);
      if (!dxc) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
      added = (X509_STORE_add_cert(st, dxc) == 1) ? 1 : 0;
      X509_free(dxc);
      if (!added) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
    }
  }
#endif
  if (cfg && cfg->client_cert_path && cfg->client_key_path) {
    if (SSL_CTX_use_certificate_chain_file(s->ssl_ctx, cfg->client_cert_path) !=
            1 ||
        SSL_CTX_use_PrivateKey_file(s->ssl_ctx, cfg->client_key_path,
                                    SSL_FILETYPE_PEM) != 1) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  }
  s->ssl = SSL_new(s->ssl_ctx);
  if (!s->ssl) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(CAH_NGTCP2_HAVE_OSSL)
  if (ngtcp2_crypto_ossl_ctx_new(&s->ossl_ctx, s->ssl) != 0 ||
      ngtcp2_crypto_ossl_configure_client_session(s->ssl) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  s->conn_ref.get_conn = ngh3_real_get_conn_cb;
  s->conn_ref.user_data = s;
  SSL_set_app_data(s->ssl, &s->conn_ref);
  SSL_set_connect_state(s->ssl);
  SSL_set_alpn_protos(s->ssl, k_h3_alpn, (unsigned int)sizeof(k_h3_alpn));
  if (conn_host && conn_host[0] != '\0') {
    SSL_set_tlsext_host_name(s->ssl, conn_host);
  }
#if defined(CAH_NGTCP2_HAVE_OSSL)
  ngtcp2_conn_set_tls_native_handle(s->qconn, s->ossl_ctx);
#else
  ngtcp2_conn_set_tls_native_handle(s->qconn, s->ssl);
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(CAH_NGTCP2_HAVE_GNUTLS)
  gnutls_datum_t alpn_h3;
  if (gnutls_certificate_allocate_credentials(&s->gtls_cred) != 0) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  s->has_gtls_cred = 1;
  if (cfg && cfg->ca_path) {
    if (gnutls_certificate_set_x509_trust_file(s->gtls_cred, cfg->ca_path,
                                               GNUTLS_X509_FMT_PEM) < 0) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  } else if (!cfg || !cfg->ca_buffer || cfg->ca_buffer_len == 0U) {
    gnutls_certificate_set_x509_system_trust(s->gtls_cred);
  }
  if (cfg && cfg->ca_buffer && cfg->ca_buffer_len > 0U) {
    gnutls_datum_t ca_mem;
    ca_mem.data = cfg->ca_buffer;
    ca_mem.size = (unsigned int)cfg->ca_buffer_len;
    if (gnutls_certificate_set_x509_trust_mem(s->gtls_cred, &ca_mem,
                                              GNUTLS_X509_FMT_PEM) < 0 &&
        gnutls_certificate_set_x509_trust_mem(s->gtls_cred, &ca_mem,
                                              GNUTLS_X509_FMT_DER) < 0) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  }
  if (cfg && cfg->client_cert_path && cfg->client_key_path) {
    if (gnutls_certificate_set_x509_key_file(
            s->gtls_cred, cfg->client_cert_path, cfg->client_key_path,
            GNUTLS_X509_FMT_PEM) < 0) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  }
  if (gnutls_init(&s->gtls_sess, GNUTLS_CLIENT | GNUTLS_ENABLE_EARLY_DATA |
                                     GNUTLS_NO_END_OF_EARLY_DATA) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  s->has_gtls_sess = 1;
  if (ngtcp2_crypto_gnutls_configure_client_session(s->gtls_sess) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  s->conn_ref.get_conn = ngh3_real_get_conn_cb;
  s->conn_ref.user_data = s;
  gnutls_session_set_ptr(s->gtls_sess, &s->conn_ref);
  gnutls_credentials_set(s->gtls_sess, GNUTLS_CRD_CERTIFICATE, s->gtls_cred);
  alpn_h3.data = (unsigned char *)"h3";
  alpn_h3.size = 2U;
  gnutls_alpn_set_protocols(s->gtls_sess, &alpn_h3, 1, GNUTLS_ALPN_MANDATORY);
  if (conn_host && conn_host[0] != '\0') {
    gnutls_server_name_set(s->gtls_sess, GNUTLS_NAME_DNS, conn_host,
                           strlen(conn_host));
  }
  ngtcp2_conn_set_tls_native_handle(s->qconn, s->gtls_sess);
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(CAH_NGTCP2_HAVE_PICOTLS)
  if (cfg) {
  }
  if (ngtcp2_crypto_picotls_init() != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  ngtcp2_crypto_picotls_ctx_init(&s->ptls_cctx);
  s->has_ptls = 1;
  memset(&s->ptls_ctx, 0, sizeof(s->ptls_ctx));
  s->ptls_ctx.random_bytes =
      (void (*)(void *, size_t))c_abstract_http_random_bytes;
  s->ptls_ctx.get_time = &ptls_get_time;
  if (ngtcp2_crypto_picotls_configure_client_context(&s->ptls_ctx) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  s->ptls_cctx.ptls = ptls_client_new(&s->ptls_ctx);
  if (!s->ptls_cctx.ptls) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  if (ngtcp2_crypto_picotls_configure_client_session(&s->ptls_cctx, s->qconn) !=
      0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  s->conn_ref.get_conn = ngh3_real_get_conn_cb;
  s->conn_ref.user_data = s;
  *ptls_get_data_ptr(s->ptls_cctx.ptls) = &s->conn_ref;
  if (conn_host && conn_host[0] != '\0') {
    ptls_set_server_name(s->ptls_cctx.ptls, conn_host, strlen(conn_host));
  }
  ngtcp2_conn_set_tls_native_handle(s->qconn, &s->ptls_cctx);
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  if (s || cfg || conn_host) {
  }
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#endif
}

/**
 * @brief Verify post-handshake peer X.509 certificate SPKI SHA-256 pin when
 * configured.
 * @param s Active real UDP QUIC session.
 * @param cfg HTTP configuration.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_ACCES.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_verify_spki_pin(struct ngh3_real_udp_sess *s,
                          const struct HttpConfig *cfg) {
  if (!cfg || !cfg->cert_pin_sha256 || cfg->cert_pin_sha256[0] == '\0') {
    if (s) {
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#if defined(CAH_NGTCP2_HAVE_OSSL) || defined(CAH_NGTCP2_HAVE_QUICTLS)
  {
    X509 *peer = SSL_get_peer_certificate(s->ssl);
    unsigned char *spki_der = NULL;
    int spki_len;
    c_abstract_http_error_t prc;
    if (!peer) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
    spki_len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(peer), &spki_der);
    X509_free(peer);
    if (spki_len <= 0 || !spki_der) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
    prc = c_abstract_http_verify_spki_pin_sha256(spki_der, (size_t)spki_len,
                                                 cfg->cert_pin_sha256);
    if (prc != C_ABSTRACT_HTTP_SUCCESS) {
      OPENSSL_free(spki_der);
      return prc;
    }
    OPENSSL_free(spki_der);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#elif defined(CAH_NGTCP2_HAVE_GNUTLS)
  {
    unsigned int list_size = 0;
    const gnutls_datum_t *certs =
        gnutls_certificate_get_peers(s->gtls_sess, &list_size);
    gnutls_x509_crt_t crt = NULL;
    gnutls_pubkey_t pubkey = NULL;
    gnutls_datum_t spki = {NULL, 0};
    c_abstract_http_error_t prc;
    if (!certs || list_size == 0U || gnutls_x509_crt_init(&crt) < 0) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
    if (gnutls_x509_crt_import(crt, &certs[0], GNUTLS_X509_FMT_DER) < 0 ||
        gnutls_pubkey_init(&pubkey) < 0) {
      gnutls_x509_crt_deinit(crt);
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
    if (gnutls_pubkey_import_x509(pubkey, crt, 0) < 0 ||
        gnutls_pubkey_export2(pubkey, GNUTLS_X509_FMT_DER, &spki) < 0) {
      gnutls_pubkey_deinit(pubkey);
      gnutls_x509_crt_deinit(crt);
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
    prc = c_abstract_http_verify_spki_pin_sha256(spki.data, (size_t)spki.size,
                                                 cfg->cert_pin_sha256);
    if (prc != C_ABSTRACT_HTTP_SUCCESS) {
      gnutls_free(spki.data);
      gnutls_pubkey_deinit(pubkey);
      gnutls_x509_crt_deinit(crt);
      return prc;
    }
    gnutls_free(spki.data);
    gnutls_pubkey_deinit(pubkey);
    gnutls_x509_crt_deinit(crt);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#else
  if (s) {
  }
  return C_ABSTRACT_HTTP_ERR_ACCES;
#endif
}

/**
 * @brief Perform non-blocking UDP `sendto` flush, timer expiry check, and
 * `recvfrom` drain on @p s without sleeping.
 * @param s Active real UDP QUIC session.
 * @param h3_ready Non-zero once HTTP/3 control streams are bound.
 * @param deadline_ns Absolute timeout deadline in nanoseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_TIMEOUT, or I/O error.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_io_nonblock(struct ngh3_real_udp_sess *s, int h3_ready,
                      ngtcp2_tstamp deadline_ns) {
  uint8_t pkt[1452];
  ngtcp2_path_storage ps;
  ngtcp2_pkt_info pi;
  ngtcp2_tstamp ts = 0U;
  c_abstract_http_error_t rc = ngh3_real_now_ns(&ts);

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (ts >= deadline_ns) {
    return C_ABSTRACT_HTTP_ERR_TIMEOUT;
  }

  if (ts >= ngtcp2_conn_get_expiry(s->qconn)) {
    int hrc = ngtcp2_conn_handle_expiry(s->qconn, ts);
    if (hrc != 0) {
      return hrc == NGTCP2_ERR_IDLE_CLOSE ? C_ABSTRACT_HTTP_ERR_TIMEOUT
                                          : C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    }
  }

  for (;;) {
    uint8_t rx[2048];
    struct sockaddr_storage rx_remote;
    socklen_t rx_remote_len = (socklen_t)sizeof(rx_remote);
    ngtcp2_path rx_path;
    ngtcp2_tstamp rx_ts = 0U;
    int nr = (int)recvfrom(s->udp_fd, (char *)rx, sizeof(rx), 0,
                           (struct sockaddr *)&rx_remote, &rx_remote_len);
    if (nr < 0) {
#if defined(_WIN32)
      int werr = WSAGetLastError();
      if (werr == WSAEWOULDBLOCK) {
        break;
      }
      return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
#else
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      if (errno == EINTR) {
        continue;
      }
      return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
#endif
    }
    if (nr == 0) {
      break;
    }
    rc = ngh3_real_now_ns(&rx_ts);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    memset(&rx_path, 0, sizeof(rx_path));
    rx_path.local.addr = (ngtcp2_sockaddr *)&s->local_addr;
    rx_path.local.addrlen = s->local_len;
    rx_path.remote.addr = (ngtcp2_sockaddr *)&rx_remote;
    rx_path.remote.addrlen = (ngtcp2_socklen)rx_remote_len;
    memset(&pi, 0, sizeof(pi));
    if (ngtcp2_conn_read_pkt(s->qconn, &rx_path, &pi, rx, (size_t)nr, rx_ts) !=
        0) {
      return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    }
  }

  ngtcp2_path_storage_zero(&ps);
  for (;;) {
    nghttp3_vec h3v[8];
    nghttp3_ssize vcnt = 0;
    int64_t stream_id = -1;
    int fin = 0;
    uint32_t wflags = NGTCP2_WRITE_STREAM_FLAG_NONE;
    ngtcp2_ssize ndatalen = -1;
    ngtcp2_ssize nwrite;
    ngtcp2_tstamp tx_ts = 0U;
    const struct sockaddr *dst_sa;
    socklen_t dst_len;

    if (h3_ready && s->h3conn) {
      vcnt = nghttp3_conn_writev_stream(s->h3conn, &stream_id, &fin, h3v, 8);
      if (vcnt < 0) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      if (fin) {
        wflags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
      }
      if (vcnt > 0 || stream_id != -1) {
        wflags |= NGTCP2_WRITE_STREAM_FLAG_MORE;
      }
    }

    rc = ngh3_real_now_ns(&tx_ts);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    memset(&pi, 0, sizeof(pi));
    nwrite = ngtcp2_conn_writev_stream(
        s->qconn, &ps.path, &pi, pkt, sizeof(pkt), &ndatalen, wflags, stream_id,
        (const ngtcp2_vec *)h3v, (size_t)(vcnt > 0 ? vcnt : 0), tx_ts);
    if (nwrite < 0) {
      if (nwrite == NGTCP2_ERR_WRITE_MORE && h3_ready && ndatalen >= 0) {
        if (nghttp3_conn_add_write_offset(s->h3conn, stream_id,
                                          (size_t)ndatalen) != 0) {
          return C_ABSTRACT_HTTP_ERR_IO;
        }
        continue;
      }
      if (nwrite == NGTCP2_ERR_STREAM_DATA_BLOCKED && h3_ready) {
        nghttp3_conn_block_stream(s->h3conn, stream_id);
        continue;
      }
      if (nwrite == NGTCP2_ERR_STREAM_SHUT_WR && h3_ready) {
        nghttp3_conn_shutdown_stream_write(s->h3conn, stream_id);
        continue;
      }
      return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    }
    if (h3_ready && ndatalen >= 0) {
      if (nghttp3_conn_add_write_offset(s->h3conn, stream_id,
                                        (size_t)ndatalen) != 0) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
    }
    if (nwrite == 0) {
      break;
    }
    dst_sa = ps.path.remote.addrlen > 0
                 ? (const struct sockaddr *)ps.path.remote.addr
                 : (const struct sockaddr *)&s->remote_addr;
    dst_len = ps.path.remote.addrlen > 0 ? (socklen_t)ps.path.remote.addrlen
                                         : (socklen_t)s->remote_len;
    if (sendto(s->udp_fd, (const char *)pkt, (int)nwrite, 0, dst_sa, dst_len) <
        0) {
#if defined(_WIN32)
      if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
#endif
      {
        return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
      }
      break;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Pump one round of outbound QUIC + HTTP/3 UDP datagrams via `sendto`,
 * wait up to QUIC expiry, and drain inbound UDP datagrams via `recvfrom`.
 * @param s Active real UDP QUIC session.
 * @param h3_ready Non-zero once HTTP/3 control streams are bound.
 * @param deadline_ns Absolute timeout deadline in nanoseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_TIMEOUT, or I/O error.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_step_udp(struct ngh3_real_udp_sess *s, int h3_ready,
                   ngtcp2_tstamp deadline_ns) {
  c_abstract_http_error_t rc = ngh3_real_io_nonblock(s, h3_ready, deadline_ns);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  /* Wait on UDP readability up to min(QUIC timer expiry, deadline) */
  {
    ngtcp2_tstamp exp_ns = ngtcp2_conn_get_expiry(s->qconn);
    ngtcp2_tstamp now_ns = 0U;
    ngtcp2_tstamp wait_ns;
    fd_set rfds;
    struct timeval tv;
    int sel_rc;

    rc = ngh3_real_now_ns(&now_ns);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    wait_ns = deadline_ns > now_ns ? (deadline_ns - now_ns) : 0U;

    if (exp_ns != UINT64_MAX) {
      ngtcp2_tstamp rem_exp = exp_ns > now_ns ? (exp_ns - now_ns) : 0U;
      if (rem_exp < wait_ns) {
        wait_ns = rem_exp;
      }
    }
    if (wait_ns > (ngtcp2_tstamp)50000000UL) {
      wait_ns = (ngtcp2_tstamp)50000000UL;
    }
    FD_ZERO(&rfds);
    FD_SET(s->udp_fd, &rfds);
    {
      ngtcp2_tstamp ns_per_s = (ngtcp2_tstamp)1000000UL * (ngtcp2_tstamp)1000UL;
      tv.tv_sec = (long)(wait_ns / ns_per_s);
      tv.tv_usec = (int)((wait_ns % ns_per_s) / (ngtcp2_tstamp)1000UL);
    }
    sel_rc = select((int)s->udp_fd + 1, &rfds, NULL, NULL, &tv);
    if (sel_rc < 0) {
#if !defined(_WIN32)
      if (errno == EINTR) {
        return C_ABSTRACT_HTTP_SUCCESS;
      }
#endif
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  return ngh3_real_io_nonblock(s, h3_ready, deadline_ns);
}

/**
 * @brief Compute absolute QUIC operation deadline in nanoseconds.
 * @param ctx Owning HTTP transport context.
 * @param out_deadline Output deadline timestamp in nanoseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS or clock error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_deadline_ns(const struct HttpTransportContext *ctx,
                      ngtcp2_tstamp *out_deadline) {
  unsigned int t_ms = ctx->timeout_ms > 0U ? ctx->timeout_ms : 10000U;
  ngtcp2_tstamp now_ns = 0U;
  c_abstract_http_error_t rc;
  if (!out_deadline) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (ctx->config.connect_timeout_ms > 0 &&
      (unsigned int)ctx->config.connect_timeout_ms < t_ms) {
    t_ms = (unsigned int)ctx->config.connect_timeout_ms;
  }
  rc = ngh3_real_now_ns(&now_ns);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  *out_deadline = now_ns + (ngtcp2_tstamp)t_ms * (ngtcp2_tstamp)1000000UL;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Open and bind ngtcp2 + nghttp3 client connection & control streams.
 * @param ctx Transport context.
 * @param conn_host Target QUIC authority/proxy hostname.
 * @param conn_port Target QUIC UDP port.
 * @param out_qconn Output ngtcp2 connection handle.
 * @param out_h3conn Output nghttp3 connection handle.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_open_h3_session(struct HttpTransportContext *ctx, const char *conn_host,
                     unsigned short conn_port, ngtcp2_conn **out_qconn,
                     nghttp3_conn **out_h3conn) {
  nghttp3_callbacks cbs;
  ngtcp2_conn *qconn = NULL;
  nghttp3_conn *h3conn = NULL;
  int64_t ctrl_id = 0;
  int64_t qenc_id = 0;
  int64_t qdec_id = 0;

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
  {
    struct addrinfo hints;
    struct addrinfo *ai = NULL;
    struct ngh3_real_udp_sess *s;
    char port_str[16];
    ngtcp2_cid dcid;
    ngtcp2_cid scid;
    ngtcp2_path path;
    ngtcp2_callbacks qcbs;
    int qrc;
    c_abstract_http_error_t rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(port_str, sizeof(port_str), "%u", (unsigned int)conn_port);
#else
    sprintf(port_str, "%u", (unsigned int)conn_port);
#endif
    if (getaddrinfo(conn_host, port_str, &hints, &ai) != 0 || !ai) {
      return C_ABSTRACT_HTTP_ERR_HOSTUNREACH;
    }

    s = (struct ngh3_real_udp_sess *)calloc(1, sizeof(*s));
    if (!s) {
      freeaddrinfo(ai);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    s->udp_fd = NGH3_INVALID_SOCK;
    s->ctx = ctx;

    s->udp_fd = socket(ai->ai_family, SOCK_DGRAM, 0);
    if (s->udp_fd == NGH3_INVALID_SOCK) {
      freeaddrinfo(ai);
      free(s);
      return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    }
    {
      struct sockaddr_storage bind_sa;
      socklen_t bind_len = 0;
      memset(&bind_sa, 0, sizeof(bind_sa));
      if (ai->ai_family == AF_INET) {
        struct sockaddr_in *b4 = (struct sockaddr_in *)&bind_sa;
        b4->sin_family = AF_INET;
        bind_len = (socklen_t)sizeof(*b4);
      } else {
        bind_sa.ss_family = ai->ai_family;
        bind_len = (socklen_t)ai->ai_addrlen;
      }
      if (bind(s->udp_fd, (const struct sockaddr *)&bind_sa, bind_len) != 0) {
        freeaddrinfo(ai);
        NGH3_CLOSE_SOCK(s->udp_fd);
        free(s);
        return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
      }
    }
    rc = ngh3_real_set_nonblock(s->udp_fd);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      freeaddrinfo(ai);
      NGH3_CLOSE_SOCK(s->udp_fd);
      free(s);
      return rc;
    }

    memcpy(&s->remote_addr, ai->ai_addr, ai->ai_addrlen);
    s->remote_len = (ngtcp2_socklen)ai->ai_addrlen;
    s->local_len = (ngtcp2_socklen)sizeof(s->local_addr);
    if (getsockname(s->udp_fd, (struct sockaddr *)&s->local_addr,
                    (socklen_t *)&s->local_len) != 0) {
      s->local_addr.ss_family = ai->ai_family;
      s->local_len = (ngtcp2_socklen)ai->ai_addrlen;
    }
    freeaddrinfo(ai);

    memset(&dcid, 0, sizeof(dcid));
    memset(&scid, 0, sizeof(scid));
    dcid.datalen = 16;
    scid.datalen = 16;
    rc = c_abstract_http_random_bytes(dcid.data, dcid.datalen);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      NGH3_CLOSE_SOCK(s->udp_fd);
      free(s);
      return rc;
    }
    rc = c_abstract_http_random_bytes(scid.data, scid.datalen);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      NGH3_CLOSE_SOCK(s->udp_fd);
      free(s);
      return rc;
    }
    memset(&path, 0, sizeof(path));
    path.local.addr = (ngtcp2_sockaddr *)&s->local_addr;
    path.local.addrlen = s->local_len;
    path.remote.addr = (ngtcp2_sockaddr *)&s->remote_addr;
    path.remote.addrlen = s->remote_len;
    memset(&qcbs, 0, sizeof(qcbs));
    qcbs.rand = ngh3_real_rand_cb;
#if defined(NGTCP2_VERSION_NUM) && NGTCP2_VERSION_NUM >= 0x011600
    qcbs.get_new_connection_id2 = ngh3_real_get_new_cid2_cb;
#else
    qcbs.get_new_connection_id = ngh3_real_get_new_cid_cb;
#endif
    qcbs.recv_stream_data = ngh3_real_recv_stream_data_cb;
    qcbs.acked_stream_data_offset = ngh3_real_acked_stream_data_cb;
    qcbs.stream_close = ngh3_real_stream_close_cb;
    qcbs.extend_max_stream_data = ngh3_real_extend_max_stream_data_cb;
#if defined(CAH_NGTCP2_HAVE_OSSL) || defined(CAH_NGTCP2_HAVE_QUICTLS) ||       \
    defined(CAH_NGTCP2_HAVE_GNUTLS) || defined(CAH_NGTCP2_HAVE_PICOTLS)
    rc = ngh3_real_wire_crypto_cbs(&qcbs);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      NGH3_CLOSE_SOCK(s->udp_fd);
      free(s);
      return rc;
    }
#endif
    rc = ngh3_real_now_ns(&ctx->q_settings.initial_ts);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      NGH3_CLOSE_SOCK(s->udp_fd);
      free(s);
      return rc;
    }
    ctx->q_params.original_dcid = dcid;
    ctx->q_params.original_dcid_present = 1;
    ctx->q_params.initial_max_streams_uni = 3;
    ctx->q_params.initial_max_streams_bidi = 32;
    ctx->q_params.initial_max_data = 1048576U;
    ctx->q_params.initial_max_stream_data_bidi_local = 262144U;
    ctx->q_params.initial_max_stream_data_bidi_remote = 262144U;
    ctx->q_params.initial_max_stream_data_uni = 262144U;
    qrc = ngtcp2_conn_client_new(&qconn, &dcid, &scid, &path,
                                 NGTCP2_PROTO_VER_V1, &qcbs, &ctx->q_settings,
                                 &ctx->q_params, NULL, s);
    if (qrc != 0) {
      NGH3_CLOSE_SOCK(s->udp_fd);
      free(s);
      return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
    }
    s->qconn = qconn;
    ctx->active_real_sess = s;

    rc = ngh3_real_bind_tls13(s, &ctx->config, conn_host);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ngh3_real_free_sess_with_err(ctx, qconn, NULL, rc);
    }

    /* Drive non-blocking UDP TLS 1.3 handshake until peer uni stream credits
     * arrive */
    {
      ngtcp2_tstamp dl_ns = 0U;
      rc = ngh3_real_deadline_ns(ctx, &dl_ns);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return ngh3_real_free_sess_with_err(ctx, qconn, NULL, rc);
      }
      while (!ngtcp2_conn_get_handshake_completed(qconn) ||
             ngtcp2_conn_get_streams_uni_left(qconn) < 3U) {
        rc = ngh3_real_step_udp(s, 0, dl_ns);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return ngh3_real_free_sess_with_err(ctx, qconn, NULL, rc);
        }
      }
      rc = ngh3_real_verify_spki_pin(s, &ctx->config);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return ngh3_real_free_sess_with_err(ctx, qconn, NULL, rc);
      }
    }
  }
#else
  if (ngtcp2_conn_client_new(&qconn, conn_host, conn_port, &ctx->q_settings,
                             &ctx->q_params) != 0) {
    return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
  }
#endif

  memset(&cbs, 0, sizeof(cbs));
  cbs.recv_header = ngh3_cb_recv_header;
  cbs.recv_data = ngh3_cb_recv_data;
  cbs.end_stream = ngh3_cb_end_stream;

  if (nghttp3_conn_client_new(&h3conn, &cbs, &ctx->h3_settings, NULL, ctx) !=
      0) {
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
    return ngh3_real_free_sess_with_err(ctx, qconn, NULL,
                                        C_ABSTRACT_HTTP_ERR_NOMEM);
#else
    ngtcp2_conn_del(qconn);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
#endif
  }

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
  if (ctx->active_real_sess) {
    ((struct ngh3_real_udp_sess *)ctx->active_real_sess)->h3conn = h3conn;
  }
#endif

  if (ngtcp2_conn_open_uni_stream(qconn, &ctrl_id, NULL) != 0 ||
      ngtcp2_conn_open_uni_stream(qconn, &qenc_id, NULL) != 0 ||
      ngtcp2_conn_open_uni_stream(qconn, &qdec_id, NULL) != 0 ||
      nghttp3_conn_bind_control_stream(h3conn, ctrl_id) != 0 ||
      nghttp3_conn_bind_qpack_streams(h3conn, qenc_id, qdec_id) != 0) {
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
    return ngh3_real_free_sess_with_err(ctx, qconn, h3conn,
                                        C_ABSTRACT_HTTP_ERR_IO);
#else
    nghttp3_conn_del(h3conn);
    ngtcp2_conn_del(qconn);
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  }

  *out_qconn = qconn;
  *out_h3conn = h3conn;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Submit one HTTP/3 request stream onto an active nghttp3 connection.
 * @param ctx Transport context.
 * @param qconn Active ngtcp2 connection.
 * @param h3conn Active nghttp3 connection.
 * @param st Per-stream state (pre-populated with req and res).
 * @param scheme Request scheme string.
 * @param host Request authority string.
 * @param path Request path string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_submit_stream(const struct HttpTransportContext *ctx, ngtcp2_conn *qconn,
                   nghttp3_conn *h3conn, struct ngh3_stream_state *st,
                   const char *scheme, const char *host, const char *path) {
  const char *method_s = NULL;
  nghttp3_nv *nva = NULL;
  size_t nv_cap;
  size_t nv_len = 0;
  size_t i;
  int has_ua = 0;
  int has_body = 0;
  char clen_buf[32];
  char *auth_hdr = NULL;
  char *cookie_hdr = NULL;
  int64_t stream_id = 0;
  nghttp3_data_reader dr;
  int sub_rc;
  c_abstract_http_error_t rc;

  rc = ngh3_method_str(st->req->method, &method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  nv_cap = st->req->headers.count + 9;
  nva = (nghttp3_nv *)calloc(nv_cap, sizeof(*nva));
  if (!nva) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  NGH3_SET_NV(&nva[nv_len++], ":method", method_s);
  NGH3_SET_NV(&nva[nv_len++], ":scheme", scheme);
  NGH3_SET_NV(&nva[nv_len++], ":authority", host);
  NGH3_SET_NV(&nva[nv_len++], ":path", path);

  for (i = 0; i < st->req->headers.count; ++i) {
    if (math_ngh3_strcasecmp(st->req->headers.headers[i].key, "User-Agent") ==
        0) {
      has_ua = 1;
    }
    NGH3_SET_NV(&nva[nv_len++], st->req->headers.headers[i].key,
                st->req->headers.headers[i].value);
  }

  if (!has_ua && ctx->config.user_agent) {
    NGH3_SET_NV(&nva[nv_len++], "user-agent", ctx->config.user_agent);
  }
  if (math_c_abstract_http_format_clen(st->req, clen_buf)) {
    NGH3_SET_NV(&nva[nv_len++], "content-length", clen_buf);
  }

  if (ctx->proxy_host) {
    NGH3_SET_NV(&nva[nv_len++], "x-masque-target", host);
    if (ctx->config.proxy_username) {
      rc = ngh3_build_proxy_auth(ctx->config.proxy_username,
                                 ctx->config.proxy_password, &auth_hdr);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(nva);
        return rc;
      }
      NGH3_SET_NV(&nva[nv_len++], "proxy-authorization", auth_hdr);
    }
  }

  if (ctx->config.cookie_jar && ctx->config.cookie_jar->count > 0) {
    rc = ngh3_build_cookie_hdr(ctx->config.cookie_jar, &cookie_hdr);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(auth_hdr);
      free(nva);
      return rc;
    }
    NGH3_SET_NV(&nva[nv_len++], "cookie", cookie_hdr);
  }

  if (ngtcp2_conn_open_bidi_stream(qconn, &stream_id, st) != 0) {
    free(cookie_hdr);
    free(auth_hdr);
    free(nva);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  has_body = (st->req->read_chunk != NULL) ||
             (st->req->body != NULL && st->req->body_len > 0);
  memset(&dr, 0, sizeof(dr));
  dr.read_data = ngh3_cb_read_data;

  sub_rc = nghttp3_conn_submit_request(h3conn, stream_id, nva, nv_len,
                                       has_body ? &dr : NULL, st);
  free(cookie_hdr);
  free(auth_hdr);
  free(nva);

  if (sub_rc != 0) {
    if (st->err != C_ABSTRACT_HTTP_SUCCESS) {
      return st->err;
    }
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Execute a single HTTP/3 transaction over nghttp3 + ngtcp2.
 * @param ctx Transport context.
 * @param req Effective HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_send_quic_once(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req, struct HttpResponse **res) {
  char *scheme = NULL;
  char *host = NULL;
  char *path = NULL;
  unsigned short port = 443;
  const char *conn_host;
  unsigned short conn_port;
  ngtcp2_conn *qconn = NULL;
  nghttp3_conn *h3conn = NULL;
  struct ngh3_stream_state st;
  int drv_rc = 0;
  c_abstract_http_error_t rc;

  rc = ngh3_parse_url(req->url, &scheme, &host, &port, &path);
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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_response_alloc_fail) {
    free(scheme);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  *res = (struct HttpResponse *)calloc(1, sizeof(**res));
  if (!*res) {
    free(scheme);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_response_init_fail) {
    free(*res);
    *res = NULL;
    free(scheme);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    free(scheme);
    free(host);
    free(path);
    return rc;
  }

  conn_host = ctx->proxy_host ? ctx->proxy_host : host;
  conn_port = ctx->proxy_host ? ctx->proxy_port : port;

  rc = ngh3_open_h3_session(ctx, conn_host, conn_port, &qconn, &h3conn);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(scheme);
    free(host);
    free(path);
    return ngh3_cleanup_failed_res(res, rc);
  }

  memset(&st, 0, sizeof(st));
  st.ctx = ctx;
  st.req = req;
  st.res = *res;

  rc = ngh3_submit_stream(ctx, qconn, h3conn, &st, scheme, host, path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(scheme);
    free(host);
    free(path);
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
    {
      c_abstract_http_error_t frc = ngh3_real_free_sess(ctx, qconn, h3conn);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        return ngh3_cleanup_failed_res(res, frc);
      }
    }
#else
    nghttp3_conn_del(h3conn);
    ngtcp2_conn_del(qconn);
#endif
    return ngh3_cleanup_failed_res(res, rc);
  }
  free(scheme);
  free(host);
  free(path);

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
  drv_rc = mock_nghttp3_drive_loopback(h3conn, qconn);
  nghttp3_conn_del(h3conn);
  ngtcp2_conn_del(qconn);
#else
  {
    struct ngh3_real_udp_sess *s =
        (struct ngh3_real_udp_sess *)ctx->active_real_sess;
    ngtcp2_tstamp dl_ns = 0U;
    c_abstract_http_error_t frc;
    frc = ngh3_real_deadline_ns(ctx, &dl_ns);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t f_rc =
          ngh3_real_free_sess_with_err(ctx, qconn, h3conn, frc);
      return ngh3_cleanup_failed_res(res, f_rc);
    }
    while (!st.done && st.err == C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t step_rc = ngh3_real_step_udp(s, 1, dl_ns);
      if (step_rc != C_ABSTRACT_HTTP_SUCCESS) {
        c_abstract_http_error_t f_rc =
            ngh3_real_free_sess_with_err(ctx, qconn, h3conn, step_rc);
        return ngh3_cleanup_failed_res(
            res, st.err != C_ABSTRACT_HTTP_SUCCESS ? st.err : f_rc);
      }
    }
    frc = ngh3_real_free_sess(ctx, qconn, h3conn);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return ngh3_cleanup_failed_res(res, frc);
    }
  }
#endif

  if (st.err != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_cleanup_failed_res(res, st.err);
  }
  if (drv_rc != 0) {
    return ngh3_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_IO);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up flattened request scratch state and finalize
 * `http_nghttp3_send`.
 * @param flat_req Scratch request structure.
 * @param flattened Non-zero when @p flat_req owns allocated buffers.
 * @param res In/out response pointer.
 * @param orig_rc Primary send result.
 * @return Final status enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_finish_send(struct HttpRequest *flat_req, int flattened,
                 struct HttpResponse **res, c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t cl_rc;
  cl_rc = ngh3_cleanup_effective_request(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_cleanup_failed_res(res, cl_rc);
  }
  return orig_rc;
}

/**
 * @brief Execute one hop over QUIC with optional HTTP/1.1 TCP fallback.
 * @param ctx Transport context.
 * @param eff_req Effective request for current hop.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_send_hop_once(struct HttpTransportContext *ctx,
                   const struct HttpRequest *eff_req,
                   struct HttpResponse **res) {
  c_abstract_http_error_t rc = ngh3_send_quic_once(ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (ctx->config.http3_fallback && (rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED ||
                                       rc == C_ABSTRACT_HTTP_ERR_IO)) {
      return ngh3_fallback_tcp(&ctx->config, eff_req, res);
    }
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_nghttp3_send(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req, struct HttpResponse **res) {
  struct HttpRequest flat_req;
  struct HttpRequest hop_req;
  const struct HttpRequest *eff_req = NULL;
  char *redir_url = NULL;
  int flattened = 0;
  int hop;
  c_abstract_http_error_t rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res || !ctx->is_configured) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = ngh3_prepare_effective_request(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  hop_req = *eff_req;
  for (hop = 0;; ++hop) {
    const char *loc = NULL;
    rc = ngh3_send_hop_once(ctx, &hop_req, res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(redir_url);
      return ngh3_finish_send(&flat_req, flattened, res, rc);
    }
    if (!math_c_abstract_http_should_redirect(ctx->config.follow_redirects,
                                              *res, &loc)) {
      break;
    }
    rc = c_abstract_http_h3_advance_redirect(&hop_req, &redir_url, res, loc,
                                             hop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(redir_url);
      return ngh3_finish_send(&flat_req, flattened, res,
                              ngh3_cleanup_failed_res(res, rc));
    }
  }

  free(redir_url);
  return ngh3_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
}

/** @brief Per-slot state for multi-stream `http_nghttp3_send_multi` */
struct ngh3_multi_slot {
  /** @brief Parsed scheme */
  char *scheme;
  /** @brief Parsed host */
  char *host;
  /** @brief Parsed path */
  char *path;
  /** @brief Parsed port */
  unsigned short port;
  /** @brief Allocated response */
  struct HttpResponse *res;
  /** @brief Per-stream nghttp3 callback state */
  struct ngh3_stream_state st;
  /** @brief Non-zero when submitted onto shared QUIC session */
  int muxed;
};

/** @brief Batch coordinator driven by `ModalityEventLoop` */
struct ngh3_multi_batch {
  /** @brief Transport context */
  struct HttpTransportContext *ctx;
  /** @brief Multi-request descriptor */
  const struct HttpMultiRequest *multi;
  /** @brief Output futures array */
  struct HttpFuture **futures;
  /** @brief Per-slot state array */
  struct ngh3_multi_slot *slots;
  /** @brief Non-zero when caller provided external loop */
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
ngh3_stop_batch_on_err(struct ModalityEventLoop *loop,
                       struct ngh3_multi_batch *b,
                       c_abstract_http_error_t err) {
  b->first_err = err;
  if (b->caller_loop) {
    c_abstract_http_error_t src = http_loop_stop(loop);
    if (src != C_ABSTRACT_HTTP_SUCCESS) {
      return src;
    }
  }
  return err;
}

/**
 * @brief Settle one slot via single-request `http_nghttp3_send` fallback/path.
 * @param loop Active event loop.
 * @param b Batch coordinator.
 * @param idx Slot index.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_settle_single_slot(struct ModalityEventLoop *loop,
                        struct ngh3_multi_batch *b, size_t idx) {
  struct HttpResponse *single_res = NULL;
  c_abstract_http_error_t rc =
      http_nghttp3_send(b->ctx, b->multi->requests[idx], &single_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    b->futures[idx]->response = single_res;
    b->futures[idx]->error_code = rc;
    b->futures[idx]->is_ready = 1;
    return ngh3_stop_batch_on_err(loop, b, rc);
  }
  b->futures[idx]->response = single_res;
  b->futures[idx]->error_code = C_ABSTRACT_HTTP_SUCCESS;
  b->futures[idx]->is_ready = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Release partially prepared multi-stream slots, close session handles,
 * mark future @p fail_idx failed, and stop caller loop if set.
 * @param loop Active event loop.
 * @param b Batch coordinator.
 * @param qconn Open ngtcp2 connection or NULL.
 * @param h3conn Open nghttp3 connection or NULL.
 * @param start_idx First slot index in current group.
 * @param end_idx Inclusive end slot index to clean.
 * @param fail_idx Failing slot index.
 * @param err Primary error code.
 * @return Loop-stop or cleanup error if non-zero, otherwise @p err.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_abort_mux_slot(struct ModalityEventLoop *loop, struct ngh3_multi_batch *b,
                    ngtcp2_conn *qconn, nghttp3_conn *h3conn, size_t start_idx,
                    size_t end_idx, size_t fail_idx,
                    c_abstract_http_error_t err) {
  size_t k;
  c_abstract_http_error_t out_rc = err;

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
  if (h3conn || qconn) {
    c_abstract_http_error_t frc = ngh3_real_free_sess(b->ctx, qconn, h3conn);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return ngh3_stop_batch_on_err(loop, b, frc);
    }
  }
#else
  if (h3conn) {
    nghttp3_conn_del(h3conn);
  }
  if (qconn) {
    ngtcp2_conn_del(qconn);
  }
#endif
  for (k = start_idx; k <= end_idx; ++k) {
    struct ngh3_multi_slot *sk = &b->slots[k];
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
  return ngh3_stop_batch_on_err(loop, b, out_rc);
}

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) ||                                      \
    defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
/** @brief Context driving a multiplexed real-UDP QUIC group on
 * `ModalityEventLoop` */
struct ngh3_real_mux_drive {
  /** @brief Active real UDP QUIC session */
  struct ngh3_real_udp_sess *sess;
  /** @brief Parent multi-request batch */
  struct ngh3_multi_batch *batch;
  /** @brief Starting slot index */
  size_t start_idx;
  /** @brief Number of multiplexed streams */
  size_t mux_cnt;
  /** @brief Absolute deadline in nanoseconds */
  ngtcp2_tstamp deadline_ns;
  /** @brief Active retransmit timer ID (-1 if none) */
  int timer_id;
  /** @brief Recorded I/O failure flag */
  int failed;
};

NO_DISCARD static c_abstract_http_error_t
ngh3_real_mux_timer_cb(struct ModalityEventLoop *loop, int timer_id,
                       void *user_data);

/**
 * @brief Free an internal `ModalityEventLoop` while preserving @p primary_rc.
 * @param loop Event loop handle to free.
 * @param primary_rc Primary status to preserve if non-zero.
 * @return @p primary_rc if non-zero, otherwise `http_loop_free` status.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_loop_free_with_err(struct ModalityEventLoop *loop,
                        c_abstract_http_error_t primary_rc) {
  c_abstract_http_error_t frc = http_loop_free(loop);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    return primary_rc != C_ABSTRACT_HTTP_SUCCESS ? primary_rc : frc;
  }
  return primary_rc;
}

/**
 * @brief Step a multiplexed real-UDP QUIC group and re-arm its retransmit
 * timer on @p loop.
 * @param loop Dedicated `ModalityEventLoop`.
 * @param drv Multiplexed group driver state.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_mux_step(struct ModalityEventLoop *loop,
                   struct ngh3_real_mux_drive *drv) {
  size_t m;
  int all_done = 1;
  c_abstract_http_error_t io_rc =
      ngh3_real_io_nonblock(drv->sess, 1, drv->deadline_ns);
  if (io_rc != C_ABSTRACT_HTTP_SUCCESS) {
    drv->failed = 1;
    return http_loop_stop(loop);
  }
  for (m = drv->start_idx; m < drv->start_idx + drv->mux_cnt; ++m) {
    if (!drv->batch->slots[m].st.done &&
        drv->batch->slots[m].st.err == C_ABSTRACT_HTTP_SUCCESS) {
      all_done = 0;
      break;
    }
  }
  if (all_done) {
    return http_loop_stop(loop);
  }
  {
    ngtcp2_tstamp exp_ns = ngtcp2_conn_get_expiry(drv->sess->qconn);
    ngtcp2_tstamp now_ns = 0U;
    uint64_t wait_ms = 25U;
    c_abstract_http_error_t nrc = ngh3_real_now_ns(&now_ns);
    if (nrc != C_ABSTRACT_HTTP_SUCCESS) {
      return nrc;
    }
    if (exp_ns != UINT64_MAX) {
      ngtcp2_tstamp rem_ns = exp_ns > now_ns ? (exp_ns - now_ns) : 0U;
      wait_ms = (uint64_t)(rem_ns / (ngtcp2_tstamp)1000000UL);
      if (wait_ms == 0U) {
        wait_ms = 1U;
      } else if (wait_ms > 50U) {
        wait_ms = 50U;
      }
    }
    return http_loop_add_timer(loop, wait_ms, ngh3_real_mux_timer_cb, drv,
                               &drv->timer_id);
  }
}

/**
 * @brief `ModalityEventLoop` UDP socket readability callback for multiplexed
 * HTTP/3 streams.
 * @param loop Active event loop.
 * @param fd Ready UDP socket descriptor.
 * @param events Fired I/O event mask.
 * @param user_data Pointer to `struct ngh3_real_mux_drive`.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_mux_io_cb(struct ModalityEventLoop *loop, int fd, int events,
                    void *user_data) {
  struct ngh3_real_mux_drive *drv = (struct ngh3_real_mux_drive *)user_data;
  if (fd || events) {
  }
  if (drv->timer_id >= 0) {
    c_abstract_http_error_t crc = http_loop_cancel_timer(loop, drv->timer_id);
    drv->timer_id = -1;
    if (crc != C_ABSTRACT_HTTP_SUCCESS) {
      return crc;
    }
  }
  return ngh3_real_mux_step(loop, drv);
}

/**
 * @brief `ModalityEventLoop` retransmit timer callback for multiplexed HTTP/3
 * streams.
 * @param loop Active event loop.
 * @param timer_id Fired timer identifier.
 * @param user_data Pointer to `struct ngh3_real_mux_drive`.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_mux_timer_cb(struct ModalityEventLoop *loop, int timer_id,
                       void *user_data) {
  struct ngh3_real_mux_drive *drv = (struct ngh3_real_mux_drive *)user_data;
  if (timer_id) {
  }
  drv->timer_id = -1;
  return ngh3_real_mux_step(loop, drv);
}

/**
 * @brief Drive a multiplexed group of HTTP/3 streams on `s->udp_fd` using
 * `ModalityEventLoop` (`http_loop_add_fd` + retransmit timer).
 * @param b Parent multi-request batch.
 * @param start_idx First multiplexed slot index.
 * @param mux_cnt Number of multiplexed streams.
 * @param out_drv_rc Receives 0 on clean completion, -1 on transport failure.
 * @return C_ABSTRACT_HTTP_SUCCESS or event-loop allocation error.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_real_drive_mux_group(struct ngh3_multi_batch *b, size_t start_idx,
                          size_t mux_cnt, int *out_drv_rc) {
  struct ModalityEventLoop *sub_loop = NULL;
  struct ngh3_real_mux_drive drv;
  c_abstract_http_error_t rc;

  *out_drv_rc = 0;
  memset(&drv, 0, sizeof(drv));
  drv.sess = (struct ngh3_real_udp_sess *)b->ctx->active_real_sess;
  drv.batch = b;
  drv.start_idx = start_idx;
  drv.mux_cnt = mux_cnt;
  drv.timer_id = -1;
  rc = ngh3_real_deadline_ns(b->ctx, &drv.deadline_ns);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *out_drv_rc = -1;
    return rc;
  }

  rc = http_loop_init(&sub_loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *out_drv_rc = -1;
    return rc;
  }
  rc = http_loop_add_fd(sub_loop, (int)drv.sess->udp_fd, HTTP_LOOP_READ,
                        ngh3_real_mux_io_cb, &drv);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *out_drv_rc = -1;
    return ngh3_loop_free_with_err(sub_loop, rc);
  }
  rc = http_loop_add_timer(sub_loop, 0U, ngh3_real_mux_timer_cb, &drv,
                           &drv.timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *out_drv_rc = -1;
    return ngh3_loop_free_with_err(sub_loop, rc);
  }
  rc = http_loop_run(sub_loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *out_drv_rc = -1;
    return ngh3_loop_free_with_err(sub_loop, rc);
  }
  rc = http_loop_free(sub_loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *out_drv_rc = -1;
    return rc;
  }
  if (drv.failed) {
    *out_drv_rc = -1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/**
 * @brief Event-loop retransmit/pump callback multiplexing concurrent
 * bidirectional HTTP/3 streams (`ngtcp2_conn_open_bidi_stream`) per authority.
 * @param loop Active event loop.
 * @param timer_id Fired timer ID.
 * @param user_data Pointer to `struct ngh3_multi_batch`.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_multi_pump_cb(struct ModalityEventLoop *loop, int timer_id,
                   void *user_data) {
  struct ngh3_multi_batch *b = (struct ngh3_multi_batch *)user_data;
  size_t i = 0;
  c_abstract_http_error_t rc;
  (void)timer_id;

  while (i < b->multi->count) {
    const struct HttpRequest *req_i = b->multi->requests[i];
    struct ngh3_multi_slot *s_i = &b->slots[i];

    if (!req_i || req_i->parts.count > 0) {
      rc = ngh3_settle_single_slot(loop, b, i);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      i++;
      continue;
    }

    rc = ngh3_parse_url(req_i->url, &s_i->scheme, &s_i->host, &s_i->port,
                        &s_i->path);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ngh3_abort_mux_slot(loop, b, NULL, NULL, i, i, i, rc);
    }
    rc = c_abstract_http_h3_verify_peer_tls(&b->ctx->config, s_i->scheme,
                                            s_i->host);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return ngh3_abort_mux_slot(loop, b, NULL, NULL, i, i, i, rc);
    }

    {
      const char *conn_host =
          b->ctx->proxy_host ? b->ctx->proxy_host : s_i->host;
      unsigned short conn_port =
          b->ctx->proxy_host ? b->ctx->proxy_port : s_i->port;
      ngtcp2_conn *qconn = NULL;
      nghttp3_conn *h3conn = NULL;
      size_t j;
      size_t mux_cnt = 0;
      int drv_rc = 0;
      c_abstract_http_error_t open_rc =
          ngh3_open_h3_session(b->ctx, conn_host, conn_port, &qconn, &h3conn);
      if (open_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return ngh3_abort_mux_slot(loop, b, NULL, NULL, i, i, i, open_rc);
      }

      for (j = i; j < b->multi->count && mux_cnt < 32; ++j) {
        const struct HttpRequest *req_j = b->multi->requests[j];
        struct ngh3_multi_slot *s_j = &b->slots[j];
        const char *ch_j;
        unsigned short cp_j;

        if (j > i) {
          if (!req_j || req_j->parts.count > 0) {
            break;
          }
          rc = ngh3_parse_url(req_j->url, &s_j->scheme, &s_j->host, &s_j->port,
                              &s_j->path);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return ngh3_abort_mux_slot(loop, b, qconn, h3conn, i, j, j, rc);
          }
          rc = c_abstract_http_h3_verify_peer_tls(&b->ctx->config, s_j->scheme,
                                                  s_j->host);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return ngh3_abort_mux_slot(loop, b, qconn, h3conn, i, j, j, rc);
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

        s_j->res = (struct HttpResponse *)calloc(1, sizeof(*s_j->res));
        if (!s_j->res) {
          return ngh3_abort_mux_slot(loop, b, qconn, h3conn, i, j, j,
                                     C_ABSTRACT_HTTP_ERR_NOMEM);
        }
        rc = http_response_init(s_j->res);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return ngh3_abort_mux_slot(loop, b, qconn, h3conn, i, j, j, rc);
        }
        memset(&s_j->st, 0, sizeof(s_j->st));
        s_j->st.ctx = b->ctx;
        s_j->st.req = req_j;
        s_j->st.res = s_j->res;
        rc = ngh3_submit_stream(b->ctx, qconn, h3conn, &s_j->st, s_j->scheme,
                                s_j->host, s_j->path);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return ngh3_abort_mux_slot(loop, b, qconn, h3conn, i, j, j, rc);
        }
        s_j->muxed = 1;
        mux_cnt++;
      }

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
      drv_rc = mock_nghttp3_drive_loopback(h3conn, qconn);
      nghttp3_conn_del(h3conn);
      ngtcp2_conn_del(qconn);
#else
      {
        c_abstract_http_error_t mrc =
            ngh3_real_drive_mux_group(b, i, mux_cnt, &drv_rc);
        if (mrc != C_ABSTRACT_HTTP_SUCCESS) {
          return ngh3_abort_mux_slot(loop, b, qconn, h3conn, i,
                                     i + mux_cnt - 1U, i, mrc);
        }
        mrc = ngh3_real_free_sess(b->ctx, qconn, h3conn);
        if (mrc != C_ABSTRACT_HTTP_SUCCESS) {
          return ngh3_abort_mux_slot(loop, b, NULL, NULL, i, i + mux_cnt - 1U,
                                     i, mrc);
        }
      }
#endif

      for (j = i; j < i + mux_cnt; ++j) {
        struct ngh3_multi_slot *s_j = &b->slots[j];
        const char *loc = NULL;
        free(s_j->scheme);
        free(s_j->host);
        free(s_j->path);
        s_j->scheme = s_j->host = s_j->path = NULL;
        if (drv_rc == 0 && s_j->st.err == C_ABSTRACT_HTTP_SUCCESS &&
            !math_c_abstract_http_should_redirect(
                b->ctx->config.follow_redirects, s_j->res, &loc)) {
          b->futures[j]->response = s_j->res;
          s_j->res = NULL;
          b->futures[j]->error_code = C_ABSTRACT_HTTP_SUCCESS;
          b->futures[j]->is_ready = 1;
        } else {
          rc = ngh3_cleanup_failed_res(&s_j->res, C_ABSTRACT_HTTP_SUCCESS);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return ngh3_abort_mux_slot(loop, b, NULL, NULL, j + 1,
                                       i + mux_cnt - 1, j, rc);
          }
          rc = ngh3_settle_single_slot(loop, b, j);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return ngh3_abort_mux_slot(loop, b, NULL, NULL, j + 1,
                                       i + mux_cnt - 1, j, rc);
          }
        }
      }
      i += mux_cnt;
    }
  }

  if (b->caller_loop) {
    return http_loop_stop(loop);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Finalize owned or caller `ModalityEventLoop` after multi-stream pump.
 * @param owned_loop Owned loop or NULL.
 * @param caller_loop Caller loop or NULL.
 * @param slots Slot array to free.
 * @param primary_rc Primary return code.
 * @return Cleanup error if non-zero, else @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t ngh3_multi_finish_loop(
    struct ModalityEventLoop *owned_loop, struct ModalityEventLoop *caller_loop,
    struct ngh3_multi_slot *slots, c_abstract_http_error_t primary_rc) {
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
 * @brief Schedule and run `ngh3_multi_pump_cb` on `active_loop` (falling back
 * to an internal `owned_loop` when caller supplied a wakeup-only external
 * loop).
 * @param loop Caller loop or NULL.
 * @param batch Initialized batch coordinator.
 * @param slots Allocated slot array (freed before returning).
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ngh3_run_multi_loop(struct ModalityEventLoop *loop,
                    struct ngh3_multi_batch *batch,
                    struct ngh3_multi_slot *slots) {
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop = loop;
  int timer_id = 0;
  c_abstract_http_error_t rc;
  c_abstract_http_error_t run_rc;

  if (!active_loop) {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slots);
      return rc;
    }
    active_loop = owned_loop;
  }

  rc =
      http_loop_add_timer(active_loop, 0, ngh3_multi_pump_cb, batch, &timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (rc == C_ABSTRACT_HTTP_ERR_NOTSUP && loop != NULL) {
      c_abstract_http_error_t t_rc;
      c_abstract_http_error_t init_rc = http_loop_init(&owned_loop);
      if (init_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(slots);
        return init_rc;
      }
      batch->caller_loop = 0;
      t_rc = http_loop_add_timer(owned_loop, 0, ngh3_multi_pump_cb, batch,
                                 &timer_id);
      if (t_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return ngh3_multi_finish_loop(owned_loop, NULL, slots, t_rc);
      }
      active_loop = owned_loop;
    } else {
      return ngh3_multi_finish_loop(owned_loop, loop, slots, rc);
    }
  }

  run_rc = http_loop_run(active_loop);
  if (run_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_multi_finish_loop(
        owned_loop, batch->caller_loop ? loop : NULL, slots,
        batch->first_err != C_ABSTRACT_HTTP_SUCCESS ? batch->first_err
                                                    : run_rc);
  }
  return ngh3_multi_finish_loop(owned_loop, batch->caller_loop ? loop : NULL,
                                slots, batch->first_err);
}

NO_DISCARD c_abstract_http_error_t http_nghttp3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct ngh3_multi_slot *slots = NULL;
  struct ngh3_multi_batch batch;
  c_abstract_http_error_t rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !multi || !futures || !ctx->is_configured) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (multi->count > 0) {
    slots = (struct ngh3_multi_slot *)calloc(multi->count, sizeof(*slots));
    if (!slots) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memset(&batch, 0, sizeof(batch));
    batch.ctx = ctx;
    batch.multi = multi;
    batch.futures = futures;
    batch.slots = slots;
    batch.caller_loop = (loop != NULL) ? 1 : 0;
    batch.first_err = C_ABSTRACT_HTTP_SUCCESS;

    rc = ngh3_run_multi_loop(loop, &batch, slots);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

  if (loop) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_nghttp3_loop_wakeup_fail) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#endif
    rc = http_loop_wakeup(loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
