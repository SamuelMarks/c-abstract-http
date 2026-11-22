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
  enum c_abstract_http_error err;
};

/**
 * @brief Convert enum HttpMethod to its wire token string.
 * @param method HTTP method enum.
 * @param out Output string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
static enum c_abstract_http_error ngh3_method_str(enum HttpMethod method,
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
ngh3_parse_url(const char *url, char **out_scheme, char **out_host,
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
static enum c_abstract_http_error
ngh3_build_proxy_auth(const char *user, const char *pass, char **out_hdr) {
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
 * @brief Format cookie jar entries into a single Cookie header value.
 * @param jar Source cookie jar.
 * @param out_cookie Newly allocated Cookie string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
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
static enum c_abstract_http_error
ngh3_store_set_cookie(struct HttpCookieJar *jar, const char *header_val) {
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
 * @brief Handle received HTTP/3 response header field.
 * @param st Per-stream state.
 * @param kbuf Header name buffer.
 * @param vbuf Header value buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
ngh3_recv_header_impl(struct ngh3_stream_state *st, nghttp3_vec kbuf,
                      nghttp3_vec vbuf) {
  char *kstr;
  char *vstr;
  enum c_abstract_http_error rc;

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
static enum c_abstract_http_error
ngh3_recv_data_impl(struct ngh3_stream_state *st, const uint8_t *data,
                    size_t datalen) {
  enum c_abstract_http_error rc;
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
static enum c_abstract_http_error
ngh3_read_data_impl(struct ngh3_stream_state *st, nghttp3_vec *vec,
                    uint32_t *pflags, size_t *out_cnt) {
  enum c_abstract_http_error rc;
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
  enum c_abstract_http_error rc;
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)flags;
  (void)conn_user_data;
  kbuf = nghttp3_rcbuf_get_buf(name);
  vbuf = nghttp3_rcbuf_get_buf(value);
  rc = ngh3_recv_header_impl(st, kbuf, vbuf);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
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
  enum c_abstract_http_error rc;
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  rc = ngh3_recv_data_impl(st, data, datalen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return 0;
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
  enum c_abstract_http_error rc;
  (void)conn;
  (void)stream_id;
  (void)veccnt;
  (void)conn_user_data;
  rc = ngh3_read_data_impl(st, vec, pflags, &cnt);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }
  return (nghttp3_ssize)cnt;
}

enum c_abstract_http_error http_nghttp3_global_init(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_nghttp3_global_init_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  g_nghttp3_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_nghttp3_global_cleanup(void) {
  if (g_nghttp3_init_count > 0) {
    g_nghttp3_init_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_nghttp3_context_init(struct HttpTransportContext **ctx) {
  struct HttpTransportContext *c;
  enum c_abstract_http_error rc;

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

enum c_abstract_http_error
http_nghttp3_context_free(struct HttpTransportContext *ctx) {
  enum c_abstract_http_error rch;
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
static enum c_abstract_http_error ngh3_replace_str(char **dst,
                                                   const char *src) {
  free(*dst);
  *dst = NULL;
  if (!src) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_strdup(src, dst);
}

enum c_abstract_http_error
http_nghttp3_config_apply(struct HttpTransportContext *ctx,
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
ngh3_prepare_effective_request(const struct HttpRequest *req,
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
static enum c_abstract_http_error
ngh3_cleanup_failed_res(struct HttpResponse **res,
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
ngh3_raw_ctx_cleanup(struct HttpTransportContext *raw_ctx,
                     struct HttpResponse **res,
                     enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error frc;
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
static enum c_abstract_http_error
ngh3_fallback_tcp(const struct HttpConfig *cfg,
                  const struct HttpRequest *eff_req,
                  struct HttpResponse **res) {
  struct HttpTransportContext *raw_ctx = NULL;
  enum c_abstract_http_error rc;

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

/**
 * @brief Open and bind ngtcp2 + nghttp3 client connection & control streams.
 * @param ctx Transport context.
 * @param conn_host Target QUIC authority/proxy hostname.
 * @param conn_port Target QUIC UDP port.
 * @param out_qconn Output ngtcp2 connection handle.
 * @param out_h3conn Output nghttp3 connection handle.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
ngh3_open_h3_session(struct HttpTransportContext *ctx, const char *conn_host,
                     unsigned short conn_port, ngtcp2_conn **out_qconn,
                     nghttp3_conn **out_h3conn) {
  nghttp3_callbacks cbs;
  ngtcp2_conn *qconn = NULL;
  nghttp3_conn *h3conn = NULL;
  int64_t ctrl_id = 0;
  int64_t qenc_id = 0;
  int64_t qdec_id = 0;

  if (ngtcp2_conn_client_new(&qconn, conn_host, conn_port, &ctx->q_settings,
                             &ctx->q_params) != 0) {
    return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
  }

  memset(&cbs, 0, sizeof(cbs));
  cbs.recv_header = ngh3_cb_recv_header;
  cbs.recv_data = ngh3_cb_recv_data;
  cbs.end_stream = ngh3_cb_end_stream;

  if (nghttp3_conn_client_new(&h3conn, &cbs, &ctx->h3_settings, NULL, ctx) !=
      0) {
    ngtcp2_conn_del(qconn);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  if (ngtcp2_conn_open_uni_stream(qconn, &ctrl_id, NULL) != 0 ||
      ngtcp2_conn_open_uni_stream(qconn, &qenc_id, NULL) != 0 ||
      ngtcp2_conn_open_uni_stream(qconn, &qdec_id, NULL) != 0 ||
      nghttp3_conn_bind_control_stream(h3conn, ctrl_id) != 0 ||
      nghttp3_conn_bind_qpack_streams(h3conn, qenc_id, qdec_id) != 0) {
    nghttp3_conn_del(h3conn);
    ngtcp2_conn_del(qconn);
    return C_ABSTRACT_HTTP_ERR_IO;
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
static enum c_abstract_http_error
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
  char *auth_hdr = NULL;
  char *cookie_hdr = NULL;
  int64_t stream_id = 0;
  nghttp3_data_reader dr;
  int sub_rc;
  enum c_abstract_http_error rc;

  rc = ngh3_method_str(st->req->method, &method_s);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  nv_cap = st->req->headers.count + 8;
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
static enum c_abstract_http_error
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
  enum c_abstract_http_error rc;

  rc = ngh3_parse_url(req->url, &scheme, &host, &port, &path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
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
  free(scheme);
  free(host);
  free(path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    nghttp3_conn_del(h3conn);
    ngtcp2_conn_del(qconn);
    return ngh3_cleanup_failed_res(res, rc);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                       \
    !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)
  drv_rc = mock_nghttp3_drive_loopback(h3conn, qconn);
#endif

  nghttp3_conn_del(h3conn);
  ngtcp2_conn_del(qconn);

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
static enum c_abstract_http_error
ngh3_finish_send(struct HttpRequest *flat_req, int flattened,
                 struct HttpResponse **res,
                 enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error cl_rc;
  cl_rc = ngh3_cleanup_effective_request(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return ngh3_cleanup_failed_res(res, cl_rc);
  }
  return orig_rc;
}

enum c_abstract_http_error http_nghttp3_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
  struct HttpRequest flat_req;
  const struct HttpRequest *eff_req = NULL;
  int flattened = 0;
  enum c_abstract_http_error rc;

  cah_cppcheck_mut_ptr((void *)ctx);
  if (!ctx || !req || !res || !ctx->is_configured) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = ngh3_prepare_effective_request(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = ngh3_send_quic_once(ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (ctx->config.http3_fallback && (rc == C_ABSTRACT_HTTP_ERR_CONNREFUSED ||
                                       rc == C_ABSTRACT_HTTP_ERR_IO)) {
      enum c_abstract_http_error fb_rc;
      fb_rc = ngh3_fallback_tcp(&ctx->config, eff_req, res);
      if (fb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        return ngh3_finish_send(&flat_req, flattened, res, fb_rc);
      }
      return ngh3_finish_send(&flat_req, flattened, res,
                              C_ABSTRACT_HTTP_SUCCESS);
    }
    return ngh3_finish_send(&flat_req, flattened, res, rc);
  }

  return ngh3_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
}

enum c_abstract_http_error http_nghttp3_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  cah_cppcheck_mut_ptr((void *)ctx);

  if (!ctx || !multi || !futures || !ctx->is_configured) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    rc = http_nghttp3_send(ctx, multi->requests[i], &res);
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
