/**
 * @file mock_lsquic.h
 * @brief Internal test-only LiteSpeed QUIC (lsquic) C-library loopback stubs
 * for HTTP/3 framing, multi-stream multiplexing, and fault-injection tests.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_LSQUIC_H
#define C_ABSTRACT_HTTP_TEST_MOCK_LSQUIC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <c_abstract_http/http_types.h>
/* clang-format on */

extern int g_mock_alloc_fail;
extern int g_mock_lsquic_global_init_fail;
extern int g_mock_lsquic_context_init_fail;
extern int g_mock_lsquic_config_init_fail;
extern int g_mock_lsquic_engine_new_fail;
extern int g_mock_lsquic_res_alloc_fail;
extern int g_mock_lsquic_res_init_fail;
extern int g_mock_lsquic_body_alloc_fail;
extern int g_mock_lsquic_read_fail;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC)

#define LSQUIC_GLOBAL_CLIENT (1 << 0)
#define LSENG_HTTP (1 << 1)

struct lsquic_engine;
typedef struct lsquic_engine lsquic_engine_t;

struct lsquic_conn;
typedef struct lsquic_conn lsquic_conn_t;

struct lsquic_stream;
typedef struct lsquic_stream lsquic_stream_t;

typedef struct lsquic_conn_ctx lsquic_conn_ctx_t;
typedef struct lsquic_stream_ctx lsquic_stream_ctx_t;

struct lsquic_engine_settings {
  unsigned es_idle_conn_to;
  unsigned es_handshake_to;
  int es_ql_bits;
};

struct lsquic_stream_if {
  lsquic_conn_ctx_t *(*on_new_conn)(void *stream_if_ctx, lsquic_conn_t *c);
  void (*on_conn_closed)(lsquic_conn_t *c);
  lsquic_stream_ctx_t *(*on_new_stream)(void *stream_if_ctx,
                                        lsquic_stream_t *s);
  void (*on_read)(lsquic_stream_t *s, lsquic_stream_ctx_t *h);
  void (*on_write)(lsquic_stream_t *s, lsquic_stream_ctx_t *h);
  void (*on_close)(lsquic_stream_t *s, lsquic_stream_ctx_t *h);
};

struct lsquic_engine_api {
  const struct lsquic_engine_settings *ea_settings;
  const struct lsquic_stream_if *ea_stream_if;
  void *ea_stream_if_ctx;
  const char *ea_alpn;
};

struct lsquic_engine {
  struct lsquic_engine_settings settings;
  const struct lsquic_stream_if *stream_if;
  void *stream_if_ctx;
  lsquic_conn_t *active_conn;
};

struct lsquic_stream {
  lsquic_conn_t *conn;
  lsquic_stream_ctx_t *st_ctx;
  uint8_t *tx_buf;
  size_t tx_len;
  uint8_t *rx_buf;
  size_t rx_len;
  size_t rx_off;
  int want_write;
  int want_read;
  int write_fin;
  int closed;
  lsquic_stream_t *next;
};

struct lsquic_conn {
  lsquic_engine_t *engine;
  unsigned short port;
  char sni[128];
  lsquic_conn_ctx_t *conn_ctx;
  int pending_streams;
  int closed;
  lsquic_stream_t *streams;
};

static int lsquic_global_init(int flags) {
  (void)flags;
  if (g_mock_lsquic_global_init_fail) {
    return -1;
  }
  return 0;
}

static void lsquic_global_cleanup(void) {}

static void lsquic_engine_init_settings(struct lsquic_engine_settings *s,
                                        unsigned flags) {
  (void)flags;
  if (s) {
    memset(s, 0, sizeof(*s));
    s->es_idle_conn_to = 30000000U;
    s->es_handshake_to = 10000000U;
  }
}

static lsquic_engine_t *lsquic_engine_new(unsigned flags,
                                          const struct lsquic_engine_api *api) {
  lsquic_engine_t *eng;
  (void)flags;
  if (g_mock_lsquic_engine_new_fail || !api) {
    return NULL;
  }
  eng = (lsquic_engine_t *)calloc(1, sizeof(*eng));
  if (!eng) {
    return NULL;
  }
  if (api->ea_settings) {
    eng->settings = *api->ea_settings;
  }
  eng->stream_if = api->ea_stream_if;
  eng->stream_if_ctx = api->ea_stream_if_ctx;
  return eng;
}

static void lsquic_engine_destroy(lsquic_engine_t *eng) { free(eng); }

static lsquic_conn_t *lsquic_engine_connect(
    lsquic_engine_t *eng, int version, const struct sockaddr *local_sa,
    const struct sockaddr *peer_sa, void *peer_ctx, lsquic_conn_ctx_t *conn_ctx,
    const char *sni, unsigned short base_plpmtu, const unsigned char *sess_res,
    size_t sess_res_sz, const unsigned char *token, size_t token_sz) {
  lsquic_conn_t *c;
  struct sockaddr_in sin;
  unsigned short port = 443;
  (void)version;
  (void)local_sa;
  (void)peer_ctx;
  (void)conn_ctx;
  (void)base_plpmtu;
  (void)sess_res;
  (void)sess_res_sz;
  (void)token;
  (void)token_sz;
  if (peer_sa) {
    memcpy(&sin, peer_sa, sizeof(sin));
    port = ntohs(sin.sin_port);
  }
  if (!eng || port == 59999 || (sni && strcmp(sni, "cnx-fail.invalid") == 0)) {
    return NULL;
  }
  c = (lsquic_conn_t *)calloc(1, sizeof(*c));
  if (!c) {
    return NULL;
  }
  c->engine = eng;
  c->port = port;
  if (sni) {
    size_t slen = strlen(sni);
    if (slen >= sizeof(c->sni)) {
      slen = sizeof(c->sni) - 1;
    }
    memcpy(c->sni, sni, slen);
    c->sni[slen] = '\0';
  }
  eng->active_conn = c;
  if (eng->stream_if && eng->stream_if->on_new_conn) {
    c->conn_ctx = eng->stream_if->on_new_conn(eng->stream_if_ctx, c);
  }
  return c;
}

static void lsquic_conn_make_stream(lsquic_conn_t *c) {
  if (c && !c->closed) {
    c->pending_streams++;
    if (c->port == 59996) {
      c->pending_streams++;
    }
  }
}

static int lsquic_stream_wantwrite(lsquic_stream_t *s, int is_want) {
  int prev = 0;
  if (s) {
    prev = s->want_write;
    s->want_write = is_want;
  }
  return prev;
}

static int lsquic_stream_wantread(lsquic_stream_t *s, int is_want) {
  int prev = 0;
  if (s) {
    prev = s->want_read;
    s->want_read = is_want;
  }
  return prev;
}

static long lsquic_stream_write(lsquic_stream_t *s, const void *buf,
                                size_t len) {
  uint8_t *nb;
  if (!s || (s->conn && s->conn->port == 59997)) {
    return -1;
  }
  if (len == 0) {
    return 0;
  }
  nb = (uint8_t *)realloc(s->tx_buf, s->tx_len + len);
  if (!nb) {
    return -1;
  }
  memcpy(nb + s->tx_len, buf, len);
  s->tx_buf = nb;
  s->tx_len += len;
  return (long)len;
}

static int lsquic_stream_shutdown(lsquic_stream_t *s, int how) {
  (void)how;
  if (!s || (s->conn && s->conn->port == 59998)) {
    return -1;
  }
  s->write_fin = 1;
  return 0;
}

static long lsquic_stream_read(lsquic_stream_t *s, void *buf, size_t len) {
  size_t avail;
  size_t take;
  if (!s || g_mock_lsquic_read_fail) {
    return -1;
  }
  if (s->rx_off >= s->rx_len) {
    return 0;
  }
  avail = s->rx_len - s->rx_off;
  take = avail < len ? avail : len;
  memcpy(buf, s->rx_buf + s->rx_off, take);
  s->rx_off += take;
  return (long)take;
}

static int lsquic_stream_close(lsquic_stream_t *s) {
  if (!s) {
    return -1;
  }
  if (!s->closed) {
    s->closed = 1;
    if (s->conn && s->conn->engine && s->conn->engine->stream_if &&
        s->conn->engine->stream_if->on_close) {
      s->conn->engine->stream_if->on_close(s, s->st_ctx);
    }
  }
  return 0;
}

static void lsquic_conn_close(lsquic_conn_t *c) {
  lsquic_stream_t *st;
  lsquic_stream_t *nxt;
  if (!c) {
    return;
  }
  if (!c->closed) {
    c->closed = 1;
    if (c->engine && c->engine->stream_if &&
        c->engine->stream_if->on_conn_closed) {
      c->engine->stream_if->on_conn_closed(c);
    }
  }
  st = c->streams;
  while (st) {
    nxt = st->next;
    free(st->tx_buf);
    free(st->rx_buf);
    free(st);
    st = nxt;
  }
  c->streams = NULL;
  if (c->engine && c->engine->active_conn == c) {
    c->engine->active_conn = NULL;
  }
  free(c);
}

static void mock_lsq_write_varint(uint8_t *dst, size_t *pos, size_t val) {
  if (val < 64) {
    dst[(*pos)++] = (uint8_t)val;
  } else if (val < 16384) {
    dst[(*pos)++] = (uint8_t)(0x40U | ((val >> 8) & 0x3FU));
    dst[(*pos)++] = (uint8_t)(val & 0xFFU);
  } else {
    dst[(*pos)++] = (uint8_t)(0x80U | ((val >> 24) & 0x3FU));
    dst[(*pos)++] = (uint8_t)((val >> 16) & 0xFFU);
    dst[(*pos)++] = (uint8_t)((val >> 8) & 0xFFU);
    dst[(*pos)++] = (uint8_t)(val & 0xFFU);
  }
}

static void mock_lsq_write_hdr(uint8_t *dst, size_t *pos, const char *k,
                               const char *v) {
  size_t kl = strlen(k);
  size_t vl = strlen(v);
  dst[(*pos)++] = 0x20U;
  mock_lsq_write_varint(dst, pos, kl);
  memcpy(dst + *pos, k, kl);
  *pos += kl;
  mock_lsq_write_varint(dst, pos, vl);
  memcpy(dst + *pos, v, vl);
  *pos += vl;
}

static int mock_lsq_read_varint(const uint8_t *src, size_t len, size_t *off,
                                size_t *val) {
  uint8_t fb;
  if (*off >= len) {
    return -1;
  }
  fb = src[*off];
  if ((fb >> 6) == 0) {
    *val = (size_t)(fb & 0x3FU);
    *off += 1;
    return 0;
  }
  if ((fb >> 6) == 1) {
    if (*off + 2 > len) {
      return -1;
    }
    *val = ((size_t)(fb & 0x3FU) << 8) | (size_t)src[*off + 1];
    *off += 2;
    return 0;
  }
  if ((fb >> 6) == 2) {
    if (*off + 4 > len) {
      return -1;
    }
    *val = ((size_t)(fb & 0x3FU) << 24) | ((size_t)src[*off + 1] << 16) |
           ((size_t)src[*off + 2] << 8) | (size_t)src[*off + 3];
    *off += 4;
    return 0;
  }
  return -1;
}

static void mock_lsq_build_response(lsquic_stream_t *s) {
  char path[128];
  char ua[128];
  char masque[128];
  char pauth[128];
  const uint8_t *req_body = NULL;
  size_t req_body_len = 0;
  size_t off = 0;
  uint8_t resp[20480];
  size_t rpos = 0;
  uint8_t hblock[1024];
  size_t hpos = 0;
  const char *default_body = "LSQUIC-H3-OK";
  const uint8_t *out_body;
  size_t out_body_len;

  path[0] = '/';
  path[1] = '\0';
  ua[0] = '\0';
  masque[0] = '\0';
  pauth[0] = '\0';

  while (off < s->tx_len) {
    size_t ftype = 0;
    size_t flen = 0;
    if (mock_lsq_read_varint(s->tx_buf, s->tx_len, &off, &ftype) != 0 ||
        mock_lsq_read_varint(s->tx_buf, s->tx_len, &off, &flen) != 0 ||
        off + flen > s->tx_len) {
      break;
    }
    if (ftype == 0x01U && flen >= 2) {
      size_t hoff = off + 2;
      size_t hend = off + flen;
      while (hoff < hend) {
        size_t kl = 0;
        size_t vl = 0;
        const uint8_t *kp;
        const uint8_t *vp;
        if (s->tx_buf[hoff++] != 0x20U) {
          break;
        }
        if (mock_lsq_read_varint(s->tx_buf, hend, &hoff, &kl) != 0 ||
            hoff + kl > hend) {
          break;
        }
        kp = s->tx_buf + hoff;
        hoff += kl;
        if (mock_lsq_read_varint(s->tx_buf, hend, &hoff, &vl) != 0 ||
            hoff + vl > hend) {
          break;
        }
        vp = s->tx_buf + hoff;
        hoff += vl;
        if (kl == 5 && memcmp(kp, ":path", 5) == 0 && vl < sizeof(path)) {
          memcpy(path, vp, vl);
          path[vl] = '\0';
        } else if (kl == 10 &&
                   (memcmp(kp, "user-agent", 10) == 0 ||
                    memcmp(kp, "User-Agent", 10) == 0) &&
                   vl < sizeof(ua)) {
          memcpy(ua, vp, vl);
          ua[vl] = '\0';
        } else if (kl == 15 && memcmp(kp, "x-masque-target", 15) == 0 &&
                   vl < sizeof(masque)) {
          memcpy(masque, vp, vl);
          masque[vl] = '\0';
        } else if (kl == 19 && memcmp(kp, "proxy-authorization", 19) == 0 &&
                   vl < sizeof(pauth)) {
          memcpy(pauth, vp, vl);
          pauth[vl] = '\0';
        }
      }
    } else if (ftype == 0x00U) {
      req_body = s->tx_buf + off;
      req_body_len = flen;
    }
    off += flen;
  }

  if (strcmp(path, "/trunc-flen") == 0) {
    resp[0] = 0x01U;
    rpos = 1;
  } else if (strcmp(path, "/trunc-frame") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x10U;
    resp[2] = 0x00U;
    rpos = 3;
  } else if (strcmp(path, "/varint8") == 0) {
    resp[0] = 0xC0U;
    resp[1] = 0x00U;
    rpos = 2;
  } else if (strcmp(path, "/bad-qpack") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x01U;
    resp[2] = 0x00U;
    rpos = 3;
  } else if (strcmp(path, "/bad-hdr-marker") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x03U;
    resp[2] = 0x00U;
    resp[3] = 0x00U;
    resp[4] = 0xFFU;
    rpos = 5;
  } else if (strcmp(path, "/trunc-kl") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x03U;
    resp[2] = 0x00U;
    resp[3] = 0x00U;
    resp[4] = 0x20U;
    rpos = 5;
  } else if (strcmp(path, "/trunc-hdr") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x04U;
    resp[2] = 0x00U;
    resp[3] = 0x00U;
    resp[4] = 0x20U;
    resp[5] = 0x0AU;
    rpos = 6;
  } else if (strcmp(path, "/trunc-vl") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x05U;
    resp[2] = 0x00U;
    resp[3] = 0x00U;
    resp[4] = 0x20U;
    resp[5] = 0x01U;
    resp[6] = 'k';
    rpos = 7;
  } else if (strcmp(path, "/trunc-val") == 0) {
    resp[0] = 0x01U;
    resp[1] = 0x06U;
    resp[2] = 0x00U;
    resp[3] = 0x00U;
    resp[4] = 0x20U;
    resp[5] = 0x01U;
    resp[6] = 'k';
    resp[7] = 0x0AU;
    rpos = 8;
  } else if (strcmp(path, "/trunc-varint2") == 0) {
    resp[0] = 0x40U;
    rpos = 1;
  } else if (strcmp(path, "/trunc-varint4") == 0) {
    resp[0] = 0x80U;
    resp[1] = 0x00U;
    rpos = 2;
  } else {
    if (strcmp(path, "/grease4") == 0) {
      resp[rpos++] = 0x80U;
      resp[rpos++] = 0x00U;
      resp[rpos++] = 0x00U;
      resp[rpos++] = 0x21U;
      resp[rpos++] = 0x00U;
    }
    hblock[hpos++] = 0x00U;
    hblock[hpos++] = 0x00U;
    mock_lsq_write_hdr(hblock, &hpos, ":status", "200");
    mock_lsq_write_hdr(hblock, &hpos, "content-type", "text/plain");
    mock_lsq_write_hdr(hblock, &hpos, "set-cookie", "lsq_sid=abc; Path=/");
    mock_lsq_write_hdr(hblock, &hpos, "Set-Cookie", "lsq_plain=xyz");
    mock_lsq_write_hdr(hblock, &hpos, "set-cookie", "=empty_name");
    mock_lsq_write_hdr(hblock, &hpos, "set-cookie", "no_equals_cookie");
    if (ua[0] != '\0') {
      mock_lsq_write_hdr(hblock, &hpos, "x-echo-ua", ua);
    }
    if (masque[0] != '\0') {
      mock_lsq_write_hdr(hblock, &hpos, "x-echo-masque", masque);
    }
    if (pauth[0] != '\0') {
      mock_lsq_write_hdr(hblock, &hpos, "x-echo-proxy-auth", pauth);
    }

    mock_lsq_write_varint(resp, &rpos, 0x01U);
    mock_lsq_write_varint(resp, &rpos, hpos);
    memcpy(resp + rpos, hblock, hpos);
    rpos += hpos;

    mock_lsq_write_varint(resp, &rpos, 0x21U);
    mock_lsq_write_varint(resp, &rpos, 2);
    resp[rpos++] = 0xAAU;
    resp[rpos++] = 0xBBU;

    if (strcmp(path, "/empty") != 0) {
      out_body = (req_body && req_body_len > 0) ? req_body
                                                : (const uint8_t *)default_body;
      out_body_len =
          (req_body && req_body_len > 0) ? req_body_len : strlen(default_body);
      mock_lsq_write_varint(resp, &rpos, 0x00U);
      mock_lsq_write_varint(resp, &rpos, out_body_len);
      memcpy(resp + rpos, out_body, out_body_len);
      rpos += out_body_len;
    } else {
      mock_lsq_write_varint(resp, &rpos, 0x00U);
      mock_lsq_write_varint(resp, &rpos, 0);
    }
  }

  s->rx_buf = (uint8_t *)malloc(rpos);
  if (s->rx_buf) {
    memcpy(s->rx_buf, resp, rpos);
    s->rx_len = rpos;
  }
}

static void lsquic_engine_process_conns(lsquic_engine_t *eng) {
  lsquic_conn_t *c;
  lsquic_stream_t *st;
  if (!eng || !eng->active_conn || !eng->stream_if) {
    return;
  }
  c = eng->active_conn;
  while (c->pending_streams > 0) {
    lsquic_stream_t *ns = (lsquic_stream_t *)calloc(1, sizeof(*ns));
    c->pending_streams--;
    if (!ns) {
      continue;
    }
    ns->conn = c;
    ns->next = c->streams;
    c->streams = ns;
    if (eng->stream_if->on_new_stream) {
      ns->st_ctx = eng->stream_if->on_new_stream(eng->stream_if_ctx, ns);
      if (!ns->st_ctx) {
        ns->want_write = 1;
        ns->want_read = 1;
      }
    }
  }

  for (st = c->streams; st; st = st->next) {
    if (!st->closed && st->want_write && eng->stream_if->on_write) {
      eng->stream_if->on_write(st, st->st_ctx);
    }
    if (!st->closed && st->write_fin && !st->rx_buf) {
      mock_lsq_build_response(st);
    }
    if (!st->closed && st->want_read && eng->stream_if->on_read) {
      eng->stream_if->on_read(st, st->st_ctx);
    }
    if (!st->closed && !st->st_ctx && eng->stream_if->on_close) {
      eng->stream_if->on_close(st, NULL);
      st->closed = 1;
    }
  }
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_LSQUIC) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_LSQUIC_H */
