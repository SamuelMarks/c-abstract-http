/**
 * @file mock_picoquic.h
 * @brief Internal test-only picoquic C-library loopback stubs for HTTP/3
 * framing and fault-injection tests.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_PICOQUIC_H
#define C_ABSTRACT_HTTP_TEST_MOCK_PICOQUIC_H

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
extern int g_mock_picoquic_create_fail;
extern int g_mock_picoquic_config_init_fail;
extern int g_mock_picoquic_response_init_fail;
extern int g_mock_picoquic_quic_null_on_free;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_PICOQUIC)

typedef enum {
  picoquic_callback_stream_data = 0,
  picoquic_callback_stream_fin = 1,
  picoquic_callback_close = 2,
  picoquic_callback_application_close = 3
} picoquic_call_back_event_t;

struct st_picoquic_quic_t {
  int dummy;
};
typedef struct st_picoquic_quic_t picoquic_quic_t;

struct st_picoquic_cnx_t;
typedef struct st_picoquic_cnx_t picoquic_cnx_t;

typedef int (*picoquic_stream_data_cb_fn)(
    picoquic_cnx_t *cnx, uint64_t stream_id, uint8_t *bytes, size_t length,
    picoquic_call_back_event_t fin_or_event, void *callback_ctx,
    void *stream_ctx);

struct st_picoquic_cnx_t {
  picoquic_quic_t *quic;
  unsigned short port;
  picoquic_stream_data_cb_fn cb;
  void *cb_ctx;
  uint8_t *tx_buf;
  size_t tx_len;
  int fin_sent;
  int closed;
};

static picoquic_quic_t *
picoquic_create(uint32_t max_nb_connections, const char *cert_file_name,
                const char *key_file_name, const char *cert_root_file_name,
                const char *default_alpn, picoquic_stream_data_cb_fn default_cb,
                void *default_ctx, void *cnx_id_cb, void *cnx_id_cb_data,
                uint8_t reset_seed[16], uint64_t current_time,
                uint64_t *p_simulated_time, const char *ticket_file_name,
                const uint8_t *ticket_encryption_key,
                size_t ticket_encryption_key_length) {
  (void)max_nb_connections;
  (void)cert_file_name;
  (void)key_file_name;
  (void)cert_root_file_name;
  (void)default_alpn;
  (void)default_cb;
  (void)default_ctx;
  (void)cnx_id_cb;
  (void)cnx_id_cb_data;
  (void)reset_seed;
  (void)current_time;
  (void)p_simulated_time;
  (void)ticket_file_name;
  (void)ticket_encryption_key;
  (void)ticket_encryption_key_length;
  if (g_mock_picoquic_create_fail) {
    return NULL;
  }
  return (picoquic_quic_t *)calloc(1, sizeof(picoquic_quic_t));
}

static void picoquic_free(picoquic_quic_t *quic) { free(quic); }

static picoquic_cnx_t *picoquic_create_client_cnx(
    picoquic_quic_t *quic, struct sockaddr *addr, uint64_t start_time,
    uint32_t preferred_version, const char *sni, const char *alpn,
    picoquic_stream_data_cb_fn client_mode_callback, void *client_mode_ctx) {
  picoquic_cnx_t *cnx;
  struct sockaddr_in sin;
  unsigned short port = 443;
  (void)start_time;
  (void)preferred_version;
  (void)alpn;
  if (addr) {
    memcpy(&sin, addr, sizeof(sin));
    port = ntohs(sin.sin_port);
  }
  if (!quic || port == 59999 || (sni && strcmp(sni, "cnx-fail.invalid") == 0)) {
    return NULL;
  }
  cnx = (picoquic_cnx_t *)calloc(1, sizeof(picoquic_cnx_t));
  if (!cnx) {
    return NULL;
  }
  cnx->quic = quic;
  cnx->port = port;
  cnx->cb = client_mode_callback;
  cnx->cb_ctx = client_mode_ctx;
  return cnx;
}

static int picoquic_start_client_cnx(picoquic_cnx_t *cnx) {
  if (!cnx || cnx->port == 59998) {
    return -1;
  }
  return 0;
}

static void picoquic_delete_cnx(picoquic_cnx_t *cnx) {
  if (cnx) {
    free(cnx->tx_buf);
    free(cnx);
  }
}

static int picoquic_close(picoquic_cnx_t *cnx, uint64_t reason_code) {
  (void)reason_code;
  if (cnx) {
    cnx->closed = 1;
  }
  return 0;
}

static int picoquic_add_to_stream(picoquic_cnx_t *cnx, uint64_t stream_id,
                                  const uint8_t *data, size_t length,
                                  int set_fin) {
  uint8_t *nbuf;
  (void)stream_id;
  if (!cnx || cnx->port == 59997) {
    return -1;
  }
  if (length > 0) {
    nbuf = (uint8_t *)realloc(cnx->tx_buf, cnx->tx_len + length);
    if (!nbuf) {
      return -1;
    }
    memcpy(nbuf + cnx->tx_len, data, length);
    cnx->tx_buf = nbuf;
    cnx->tx_len += length;
  }
  if (set_fin) {
    cnx->fin_sent = 1;
  }
  return 0;
}

static void mock_pq_write_varint(uint8_t *dst, size_t *pos, size_t val) {
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

static void mock_pq_write_hdr(uint8_t *dst, size_t *pos, const char *k,
                              const char *v) {
  size_t kl = strlen(k);
  size_t vl = strlen(v);
  dst[(*pos)++] = 0x20;
  mock_pq_write_varint(dst, pos, kl);
  memcpy(dst + *pos, k, kl);
  *pos += kl;
  mock_pq_write_varint(dst, pos, vl);
  memcpy(dst + *pos, v, vl);
  *pos += vl;
}

static int mock_pq_read_varint(const uint8_t *src, size_t len, size_t *off,
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

static void picoquic_MockDriveLoopback(picoquic_cnx_t *cnx) {
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
  const char *default_body = "PICOQUIC-H3-OK";
  const uint8_t *out_body;
  size_t out_body_len;

  if (!cnx || !cnx->cb || !cnx->fin_sent) {
    return;
  }

  path[0] = '/';
  path[1] = '\0';
  ua[0] = '\0';
  masque[0] = '\0';
  pauth[0] = '\0';

  /* Parse sent HTTP/3 HEADERS and DATA frames */
  while (off < cnx->tx_len) {
    size_t ftype = 0;
    size_t flen = 0;
    if (mock_pq_read_varint(cnx->tx_buf, cnx->tx_len, &off, &ftype) != 0 ||
        mock_pq_read_varint(cnx->tx_buf, cnx->tx_len, &off, &flen) != 0 ||
        off + flen > cnx->tx_len) {
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
        if (cnx->tx_buf[hoff++] != 0x20) {
          break;
        }
        if (mock_pq_read_varint(cnx->tx_buf, hend, &hoff, &kl) != 0 ||
            hoff + kl > hend) {
          break;
        }
        kp = cnx->tx_buf + hoff;
        hoff += kl;
        if (mock_pq_read_varint(cnx->tx_buf, hend, &hoff, &vl) != 0 ||
            hoff + vl > hend) {
          break;
        }
        vp = cnx->tx_buf + hoff;
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
      req_body = cnx->tx_buf + off;
      req_body_len = flen;
    }
    off += flen;
  }

  if (strcmp(path, "/net-close") == 0) {
    cnx->cb(cnx, 0, NULL, 0, picoquic_callback_close, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/app-close") == 0) {
    cnx->cb(cnx, 0, NULL, 0, picoquic_callback_application_close, cnx->cb_ctx,
            NULL);
    return;
  }
  if (strcmp(path, "/trunc-flen") == 0) {
    resp[0] = 0x01;
    cnx->cb(cnx, 0, resp, 1, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-kl") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x03;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0x20;
    cnx->cb(cnx, 0, resp, 5, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-vl") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x05;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0x20;
    resp[5] = 0x01;
    resp[6] = 'k';
    cnx->cb(cnx, 0, resp, 7, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-frame") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x10;
    resp[2] = 0x00;
    cnx->cb(cnx, 0, resp, 3, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/varint8") == 0) {
    resp[0] = 0xC0;
    resp[1] = 0x00;
    cnx->cb(cnx, 0, resp, 2, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/bad-qpack") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x01;
    resp[2] = 0x00;
    cnx->cb(cnx, 0, resp, 3, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/bad-hdr-marker") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x03;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0xFF;
    cnx->cb(cnx, 0, resp, 5, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-hdr") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x04;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0x20;
    resp[5] = 0x0A;
    cnx->cb(cnx, 0, resp, 6, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-val") == 0) {
    resp[0] = 0x01;
    resp[1] = 0x06;
    resp[2] = 0x00;
    resp[3] = 0x00;
    resp[4] = 0x20;
    resp[5] = 0x01;
    resp[6] = 'k';
    resp[7] = 0x0A;
    cnx->cb(cnx, 0, resp, 8, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-varint2") == 0) {
    resp[0] = 0x40;
    cnx->cb(cnx, 0, resp, 1, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/trunc-varint4") == 0) {
    resp[0] = 0x80;
    resp[1] = 0x00;
    cnx->cb(cnx, 0, resp, 2, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
    return;
  }
  if (strcmp(path, "/grease4") == 0) {
    /* 4-byte varint frame type 0x80000021 with 0-byte payload + 204 response */
    resp[rpos++] = 0x80;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x00;
    resp[rpos++] = 0x21;
    resp[rpos++] = 0x00;
  }

  /* QPACK prefix */
  hblock[hpos++] = 0x00;
  hblock[hpos++] = 0x00;
  mock_pq_write_hdr(hblock, &hpos, ":status", "200");
  mock_pq_write_hdr(hblock, &hpos, "content-type", "text/plain");
  mock_pq_write_hdr(hblock, &hpos, "set-cookie", "pq_sid=abc; Path=/");
  mock_pq_write_hdr(hblock, &hpos, "Set-Cookie", "pq_plain=xyz");
  mock_pq_write_hdr(hblock, &hpos, "set-cookie", "=empty_name");
  mock_pq_write_hdr(hblock, &hpos, "set-cookie", "no_equals_cookie");
  if (ua[0] != '\0') {
    mock_pq_write_hdr(hblock, &hpos, "x-echo-ua", ua);
  }
  if (masque[0] != '\0') {
    mock_pq_write_hdr(hblock, &hpos, "x-echo-masque", masque);
  }
  if (pauth[0] != '\0') {
    mock_pq_write_hdr(hblock, &hpos, "x-echo-proxy-auth", pauth);
  }

  /* HEADERS frame (0x01) */
  mock_pq_write_varint(resp, &rpos, 0x01);
  mock_pq_write_varint(resp, &rpos, hpos);
  memcpy(resp + rpos, hblock, hpos);
  rpos += hpos;

  /* Unknown grease frame (0x21) to exercise RFC 9114 skip */
  mock_pq_write_varint(resp, &rpos, 0x21);
  mock_pq_write_varint(resp, &rpos, 2);
  resp[rpos++] = 0xAA;
  resp[rpos++] = 0xBB;

  if (strcmp(path, "/empty") != 0) {
    out_body = (req_body && req_body_len > 0) ? req_body
                                              : (const uint8_t *)default_body;
    out_body_len =
        (req_body && req_body_len > 0) ? req_body_len : strlen(default_body);
    mock_pq_write_varint(resp, &rpos, 0x00);
    mock_pq_write_varint(resp, &rpos, out_body_len);
    memcpy(resp + rpos, out_body, out_body_len);
    rpos += out_body_len;
  } else {
    /* 0-length DATA frame to exercise ftype == 0x00 && flen == 0 */
    mock_pq_write_varint(resp, &rpos, 0x00);
    mock_pq_write_varint(resp, &rpos, 0);
  }

  /* Deliver in chunks + separate 0-length FIN to exercise all stream_cb arms */
  if (strcmp(path, "/empty") == 0) {
    cnx->cb(cnx, 0, resp, rpos, picoquic_callback_stream_data, cnx->cb_ctx,
            NULL);
    cnx->cb(cnx, 0, resp, 0, picoquic_callback_stream_data, cnx->cb_ctx, NULL);
    cnx->cb(cnx, 0, NULL, 0, picoquic_callback_stream_fin, cnx->cb_ctx, NULL);
  } else if (rpos > 4) {
    cnx->cb(cnx, 0, resp, 4, picoquic_callback_stream_data, cnx->cb_ctx, NULL);
    if (!cnx->closed) {
      cnx->cb(cnx, 0, resp + 4, rpos - 4, picoquic_callback_stream_fin,
              cnx->cb_ctx, NULL);
    }
  } else {
    cnx->cb(cnx, 0, resp, rpos, picoquic_callback_stream_fin, cnx->cb_ctx,
            NULL);
  }
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_PICOQUIC) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_PICOQUIC_H */
