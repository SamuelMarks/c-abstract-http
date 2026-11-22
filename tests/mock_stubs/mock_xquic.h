/**
 * @file mock_xquic.h
 * @brief Internal test-only Alibaba xquic C-library loopback stubs for HTTP/3
 * framing and fault-injection tests.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_XQUIC_H
#define C_ABSTRACT_HTTP_TEST_MOCK_XQUIC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <basetsd.h>
typedef SSIZE_T xqc_ssize_t;
#else
#include <sys/types.h>
typedef ssize_t xqc_ssize_t;
#endif
#include <c_abstract_http/http_types.h>
/* clang-format on */

extern int g_mock_alloc_fail;
extern int g_mock_xquic_config_init_fail;
extern int g_mock_xquic_engine_present;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_XQUIC)

typedef struct xqc_engine_s {
  int dummy;
} xqc_engine_t;

typedef struct xqc_conn_settings_s {
  uint64_t idle_time_out;
} xqc_conn_settings_t;

typedef struct xqc_conn_ssl_config_s {
  int cert_verify_flag;
} xqc_conn_ssl_config_t;

typedef struct xqc_connection_s {
  xqc_engine_t *engine;
  char host[256];
  int port;
  int closed;
} xqc_connection_t;

typedef struct xqc_stream_s {
  xqc_connection_t *conn;
  uint8_t *tx_buf;
  size_t tx_len;
  uint8_t rx_buf[20480];
  size_t rx_len;
  size_t rx_off;
  int fin_sent;
  int rx_built;
} xqc_stream_t;

static xqc_engine_t *xqc_engine_create_client(int fail_flag) {
  if (fail_flag) {
    return NULL;
  }
  return (xqc_engine_t *)calloc(1, sizeof(xqc_engine_t));
}

static void xqc_engine_destroy(xqc_engine_t *engine) { free(engine); }

static xqc_connection_t *
xqc_connect(xqc_engine_t *engine, const xqc_conn_settings_t *conn_settings,
            const unsigned char *token, unsigned int token_len,
            const char *server_host, int no_crypto_flag,
            const xqc_conn_ssl_config_t *conn_ssl_config, const void *peer_addr,
            size_t peer_addrlen, const char *alpn) {
  xqc_connection_t *conn;
  (void)conn_settings;
  (void)token;
  (void)token_len;
  (void)no_crypto_flag;
  (void)conn_ssl_config;
  (void)peer_addr;
  (void)alpn;
  if (!engine || (int)peer_addrlen == 59999 ||
      (server_host && strcmp(server_host, "conn-fail.invalid") == 0)) {
    return NULL;
  }
  conn = (xqc_connection_t *)calloc(1, sizeof(xqc_connection_t));
  if (!conn) {
    return NULL;
  }
  conn->engine = engine;
  conn->port = (int)peer_addrlen;
  if (server_host) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strncpy_s(conn->host, sizeof(conn->host), server_host,
              sizeof(conn->host) - 1);
#else
    strncpy(conn->host, server_host, sizeof(conn->host) - 1);
#endif
    conn->host[sizeof(conn->host) - 1] = '\0';
  }
  return conn;
}

static int xqc_conn_close(xqc_engine_t *engine, xqc_connection_t *conn) {
  (void)engine;
  if (conn) {
    free(conn);
  }
  return 0;
}

static xqc_stream_t *xqc_stream_create(xqc_engine_t *engine,
                                       xqc_connection_t *conn,
                                       void *user_data) {
  xqc_stream_t *st;
  (void)engine;
  (void)user_data;
  if (!conn || conn->port == 59998) {
    return NULL;
  }
  st = (xqc_stream_t *)calloc(1, sizeof(xqc_stream_t));
  if (!st) {
    return NULL;
  }
  st->conn = conn;
  return st;
}

static void xqc_stream_close(xqc_stream_t *stream) {
  if (stream) {
    free(stream->tx_buf);
    free(stream);
  }
}

static xqc_ssize_t xqc_stream_send(xqc_stream_t *stream,
                                   unsigned char *send_data,
                                   size_t send_data_size, uint8_t fin) {
  uint8_t *nbuf;
  if (!stream || (stream->conn && stream->conn->port == 59997)) {
    return -1;
  }
  if (send_data_size > 0) {
    nbuf = (uint8_t *)realloc(stream->tx_buf, stream->tx_len + send_data_size);
    if (!nbuf) {
      return -1;
    }
    memcpy(nbuf + stream->tx_len, send_data, send_data_size);
    stream->tx_buf = nbuf;
    stream->tx_len += send_data_size;
  }
  if (fin) {
    stream->fin_sent = 1;
  }
  return (xqc_ssize_t)send_data_size;
}

static void mock_xqc_write_varint(uint8_t *dst, size_t *pos, size_t val) {
  if (val < 64) {
    dst[(*pos)++] = (uint8_t)val;
  } else if (val < 16384) {
    dst[(*pos)++] = (uint8_t)(0x40 | ((val >> 8) & 0x3F));
    dst[(*pos)++] = (uint8_t)(val & 0xFF);
  } else {
    dst[(*pos)++] = (uint8_t)(0x80 | ((val >> 24) & 0x3F));
    dst[(*pos)++] = (uint8_t)((val >> 16) & 0xFF);
    dst[(*pos)++] = (uint8_t)((val >> 8) & 0xFF);
    dst[(*pos)++] = (uint8_t)(val & 0xFF);
  }
}

static void mock_xqc_write_hdr(uint8_t *dst, size_t *pos, const char *k,
                               const char *v) {
  size_t kl = strlen(k);
  size_t vl = strlen(v);
  dst[(*pos)++] = 0x20;
  mock_xqc_write_varint(dst, pos, kl);
  memcpy(dst + *pos, k, kl);
  *pos += kl;
  mock_xqc_write_varint(dst, pos, vl);
  memcpy(dst + *pos, v, vl);
  *pos += vl;
}

static int mock_xqc_read_varint(const uint8_t *src, size_t len, size_t *off,
                                size_t *val) {
  uint8_t fb;
  if (*off >= len) {
    return -1;
  }
  fb = src[*off];
  if ((fb >> 6) == 0) {
    *val = (size_t)(fb & 0x3F);
    *off += 1;
    return 0;
  }
  if ((fb >> 6) == 1) {
    if (*off + 2 > len) {
      return -1;
    }
    *val = ((size_t)(fb & 0x3F) << 8) | (size_t)src[*off + 1];
    *off += 2;
    return 0;
  }
  if ((fb >> 6) == 2) {
    if (*off + 4 > len) {
      return -1;
    }
    *val = ((size_t)(fb & 0x3F) << 24) | ((size_t)src[*off + 1] << 16) |
           ((size_t)src[*off + 2] << 8) | (size_t)src[*off + 3];
    *off += 4;
    return 0;
  }
  return -1;
}

static void mock_xqc_build_response(xqc_stream_t *stream) {
  char path[128];
  char ua[128];
  char masque[128];
  char pauth[128];
  const uint8_t *req_body = NULL;
  size_t req_body_len = 0;
  size_t off = 0;
  static uint8_t resp[20480];
  size_t rpos = 0;
  uint8_t hblock[1024];
  size_t hpos = 0;
  const char *default_body = "XQUIC-H3-OK";
  const uint8_t *out_body;
  size_t out_body_len;

  path[0] = '/';
  path[1] = '\0';
  ua[0] = '\0';
  masque[0] = '\0';
  pauth[0] = '\0';

  while (off < stream->tx_len) {
    size_t ftype = 0;
    size_t flen = 0;
    if (mock_xqc_read_varint(stream->tx_buf, stream->tx_len, &off, &ftype) !=
            0 ||
        mock_xqc_read_varint(stream->tx_buf, stream->tx_len, &off, &flen) !=
            0 ||
        off + flen > stream->tx_len) {
      break;
    }
    if (ftype == 0x01 && flen >= 2) {
      size_t hoff = off + 2;
      size_t hend = off + flen;
      while (hoff < hend) {
        size_t kl = 0;
        size_t vl = 0;
        const uint8_t *kp;
        const uint8_t *vp;
        if (stream->tx_buf[hoff++] != 0x20) {
          break;
        }
        if (mock_xqc_read_varint(stream->tx_buf, hend, &hoff, &kl) != 0 ||
            hoff + kl > hend) {
          break;
        }
        kp = stream->tx_buf + hoff;
        hoff += kl;
        if (mock_xqc_read_varint(stream->tx_buf, hend, &hoff, &vl) != 0 ||
            hoff + vl > hend) {
          break;
        }
        vp = stream->tx_buf + hoff;
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
    } else if (ftype == 0x00) {
      req_body = stream->tx_buf + off;
      req_body_len = flen;
    }
    off += flen;
  }

  if (strcmp(path, "/net-close") == 0) {
    stream->rx_built = -1;
    return;
  }
  if (strcmp(path, "/trunc-flen") == 0) {
    resp[rpos++] = 0x01;
  } else if (strcmp(path, "/trunc-kl") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x03;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x20;
  } else if (strcmp(path, "/trunc-vl") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x05;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x20;
    resp[rpos++] = 0x01;
    resp[rpos++] = 'k';
  } else if (strcmp(path, "/trunc-frame") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x10;
    resp[rpos++] = 0x00;
  } else if (strcmp(path, "/varint8") == 0) {
    resp[rpos++] = 0xC0;
    resp[rpos++] = 0x00;
  } else if (strcmp(path, "/bad-qpack") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x00;
  } else if (strcmp(path, "/bad-hdr-marker") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x03;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0xFF;
  } else if (strcmp(path, "/trunc-hdr") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x04;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x20;
    resp[rpos++] = 0x0A;
  } else if (strcmp(path, "/trunc-val") == 0) {
    resp[rpos++] = 0x01;
    resp[rpos++] = 0x06;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x20;
    resp[rpos++] = 0x01;
    resp[rpos++] = 'k';
    resp[rpos++] = 0x0A;
  } else if (strcmp(path, "/trunc-varint2") == 0) {
    resp[rpos++] = 0x40;
  } else if (strcmp(path, "/trunc-varint4") == 0) {
    resp[rpos++] = 0x80;
    resp[rpos++] = 0x00;
  } else {
    if (strcmp(path, "/grease4") == 0) {
      resp[rpos++] = 0x80;
      resp[rpos++] = 0x00;
      resp[rpos++] = 0x00;
      resp[rpos++] = 0x21;
      resp[rpos++] = 0x00;
    }
    hblock[hpos++] = 0x00;
    hblock[hpos++] = 0x00;
    mock_xqc_write_hdr(hblock, &hpos, ":status", "200");
    mock_xqc_write_hdr(hblock, &hpos, "content-type", "text/plain");
    mock_xqc_write_hdr(hblock, &hpos, "set-cookie", "xqc_sid=123; Path=/");
    mock_xqc_write_hdr(hblock, &hpos, "Set-Cookie", "xqc_plain=456");
    mock_xqc_write_hdr(hblock, &hpos, "set-cookie", "=empty");
    mock_xqc_write_hdr(hblock, &hpos, "set-cookie", "no_equals_cookie");
    if (ua[0] != '\0') {
      mock_xqc_write_hdr(hblock, &hpos, "x-echo-ua", ua);
    }
    if (masque[0] != '\0') {
      mock_xqc_write_hdr(hblock, &hpos, "x-echo-masque", masque);
    }
    if (pauth[0] != '\0') {
      mock_xqc_write_hdr(hblock, &hpos, "x-echo-proxy-auth", pauth);
    }

    mock_xqc_write_varint(resp, &rpos, 0x01);
    mock_xqc_write_varint(resp, &rpos, hpos);
    memcpy(resp + rpos, hblock, hpos);
    rpos += hpos;

    mock_xqc_write_varint(resp, &rpos, 0x21);
    mock_xqc_write_varint(resp, &rpos, 2);
    resp[rpos++] = 0x11;
    resp[rpos++] = 0x22;

    if (strcmp(path, "/empty") != 0) {
      out_body = (req_body && req_body_len > 0) ? req_body
                                                : (const uint8_t *)default_body;
      out_body_len =
          (req_body && req_body_len > 0) ? req_body_len : strlen(default_body);
      mock_xqc_write_varint(resp, &rpos, 0x00);
      mock_xqc_write_varint(resp, &rpos, out_body_len);
      memcpy(resp + rpos, out_body, out_body_len);
      rpos += out_body_len;
    } else {
      mock_xqc_write_varint(resp, &rpos, 0x00);
      mock_xqc_write_varint(resp, &rpos, 0);
    }
  }

  if (rpos > 0 && rpos <= sizeof(stream->rx_buf)) {
    memcpy(stream->rx_buf, resp, rpos);
    stream->rx_len = rpos;
  }
  stream->rx_built = 1;
}

static xqc_ssize_t xqc_stream_recv(xqc_stream_t *stream,
                                   unsigned char *recv_buf,
                                   size_t recv_buf_size, uint8_t *fin) {
  size_t avail;
  size_t to_copy;
  if (!stream || (stream->conn && stream->conn->port == 59996)) {
    return -1;
  }
  if (!stream->rx_built) {
    mock_xqc_build_response(stream);
  }
  if (stream->rx_built < 0) {
    return -1;
  }
  avail = stream->rx_len - stream->rx_off;
  if (avail == 0) {
    *fin = 1;
    return 0;
  }
  to_copy = avail < recv_buf_size ? avail : recv_buf_size;
  if (to_copy > 8 && stream->rx_off == 0) {
    /* Return partial read first to exercise multi-read loop */
    to_copy = 8;
  }
  if (to_copy > 0) {
    memcpy(recv_buf, stream->rx_buf + stream->rx_off, to_copy);
    stream->rx_off += to_copy;
  }
  /* Deliver FIN on a subsequent 0-byte read when exact buffer consumed */
  *fin = 0;
  return (xqc_ssize_t)to_copy;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_XQUIC) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_XQUIC_H */
