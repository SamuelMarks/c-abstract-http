/**
 * @file mock_nghttp3.h
 * @brief Internal test-only nghttp3 + ngtcp2 C-library loopback stubs for
 * HTTP/3 QPACK framing, QUIC stream multiplexing, and fault-injection tests.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_NGHTTP3_H
#define C_ABSTRACT_HTTP_TEST_MOCK_NGHTTP3_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <c_abstract_http/http_types.h>
#include <c89stringutils_string_extras.h>
/* clang-format on */

extern int g_mock_alloc_fail;
extern int g_mock_nghttp3_global_init_fail;
extern int g_mock_nghttp3_context_init_fail;
extern int g_mock_nghttp3_config_init_fail;
extern int g_mock_nghttp3_conn_new_fail;
extern int g_mock_nghttp3_response_alloc_fail;
extern int g_mock_nghttp3_response_init_fail;
extern int g_mock_nghttp3_send_fail;
extern int g_mock_nghttp3_loop_wakeup_fail;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3)

#define NGHTTP3_NV_FLAG_NONE 0
#define NGHTTP3_DATA_FLAG_NONE 0
#define NGHTTP3_DATA_FLAG_EOF 1
#define NGHTTP3_ERR_CALLBACK_FAILURE (-902)

typedef long nghttp3_ssize;

typedef struct nghttp3_vec {
  uint8_t *base;
  size_t len;
} nghttp3_vec;

typedef struct nghttp3_nv {
  const uint8_t *name;
  const uint8_t *value;
  size_t namelen;
  size_t valuelen;
  uint8_t flags;
} nghttp3_nv;

typedef struct nghttp3_rcbuf {
  const uint8_t *base;
  size_t len;
} nghttp3_rcbuf;

struct nghttp3_conn;
typedef struct nghttp3_conn nghttp3_conn;

typedef nghttp3_ssize (*nghttp3_read_data_callback)(
    nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec, size_t veccnt,
    uint32_t *pflags, void *conn_user_data, void *stream_user_data);

typedef struct nghttp3_data_reader {
  nghttp3_read_data_callback read_data;
} nghttp3_data_reader;

typedef struct nghttp3_callbacks {
  int (*recv_header)(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                     nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                     void *conn_user_data, void *stream_user_data);
  int (*recv_data)(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                   size_t datalen, void *conn_user_data,
                   void *stream_user_data);
  int (*end_stream)(nghttp3_conn *conn, int64_t stream_id, void *conn_user_data,
                    void *stream_user_data);
} nghttp3_callbacks;

typedef struct nghttp3_settings {
  uint64_t max_field_section_size;
  size_t qpack_max_dtable_capacity;
} nghttp3_settings;

typedef struct ngtcp2_settings {
  uint64_t handshake_timeout;
} ngtcp2_settings;

typedef struct ngtcp2_transport_params {
  uint64_t max_idle_timeout;
  uint64_t initial_max_streams_bidi;
} ngtcp2_transport_params;

typedef struct ngtcp2_conn {
  int64_t next_bidi_id;
  int64_t next_uni_id;
  unsigned short port;
  char host[128];
} ngtcp2_conn;

struct mock_nghttp3_stream {
  int64_t stream_id;
  void *stream_user_data;
  char method[16];
  char path[128];
  char ua[128];
  char masque[128];
  char pauth[128];
  char clen[32];
  uint8_t *req_body;
  size_t req_body_len;
  int active;
};

struct nghttp3_conn {
  nghttp3_callbacks cbs;
  nghttp3_settings settings;
  void *user_data;
  struct mock_nghttp3_stream streams[32];
  size_t stream_count;
};

static nghttp3_vec nghttp3_rcbuf_get_buf(const nghttp3_rcbuf *rcbuf) {
  nghttp3_vec v;
  v.base = (uint8_t *)(size_t)rcbuf->base;
  v.len = rcbuf->len;
  return v;
}

static void nghttp3_settings_default(nghttp3_settings *settings) {
  if (settings) {
    memset(settings, 0, sizeof(*settings));
    settings->max_field_section_size = 65536U;
  }
}

static void ngtcp2_settings_default(ngtcp2_settings *settings) {
  if (settings) {
    memset(settings, 0, sizeof(*settings));
  }
}

static void ngtcp2_transport_params_default(ngtcp2_transport_params *params) {
  if (params) {
    memset(params, 0, sizeof(*params));
    params->initial_max_streams_bidi = 100U;
  }
}

static int ngtcp2_conn_client_new(ngtcp2_conn **pconn, const char *host,
                                  unsigned short port,
                                  const ngtcp2_settings *settings,
                                  const ngtcp2_transport_params *params) {
  ngtcp2_conn *c;
  (void)settings;
  (void)params;
  if (!pconn || g_mock_nghttp3_conn_new_fail == 1 || port == 59999 ||
      (host && strcmp(host, "cnx-fail.invalid") == 0)) {
    return -1;
  }
  c = (ngtcp2_conn *)calloc(1, sizeof(*c));
  if (!c) {
    return -1;
  }
  c->next_bidi_id = 0;
  c->next_uni_id = 2;
  c->port = port;
  if (host) {
    size_t hlen = strlen(host);
    if (hlen >= sizeof(c->host)) {
      hlen = sizeof(c->host) - 1;
    }
    memcpy(c->host, host, hlen);
    c->host[hlen] = '\0';
  }
  *pconn = c;
  return 0;
}

static void ngtcp2_conn_del(ngtcp2_conn *conn) { free(conn); }

static int ngtcp2_conn_open_bidi_stream(ngtcp2_conn *conn, int64_t *pstream_id,
                                        void *stream_user_data) {
  (void)stream_user_data;
  if (!conn || !pstream_id || conn->port == 59998) {
    return -1;
  }
  *pstream_id = conn->next_bidi_id;
  conn->next_bidi_id += 4;
  return 0;
}

static int ngtcp2_conn_open_uni_stream(ngtcp2_conn *conn, int64_t *pstream_id,
                                       void *stream_user_data) {
  (void)stream_user_data;
  if (!conn || !pstream_id || conn->port == 59996) {
    return -1;
  }
  *pstream_id = conn->next_uni_id;
  conn->next_uni_id += 4;
  return 0;
}

static int nghttp3_conn_client_new(nghttp3_conn **pconn,
                                   const nghttp3_callbacks *callbacks,
                                   const nghttp3_settings *settings,
                                   const void *mem, void *user_data) {
  nghttp3_conn *c;
  (void)mem;
  if (!pconn || !callbacks || g_mock_nghttp3_conn_new_fail == 2) {
    return -1;
  }
  c = (nghttp3_conn *)calloc(1, sizeof(*c));
  if (!c) {
    return -1;
  }
  c->cbs = *callbacks;
  if (settings) {
    c->settings = *settings;
  }
  c->user_data = user_data;
  *pconn = c;
  return 0;
}

static void nghttp3_conn_del(nghttp3_conn *conn) {
  size_t i;
  if (!conn) {
    return;
  }
  for (i = 0; i < conn->stream_count; ++i) {
    free(conn->streams[i].req_body);
  }
  free(conn);
}

static int nghttp3_conn_bind_control_stream(nghttp3_conn *conn,
                                            int64_t stream_id) {
  (void)stream_id;
  if (!conn || g_mock_nghttp3_conn_new_fail == 3) {
    return -1;
  }
  return 0;
}

static int nghttp3_conn_bind_qpack_streams(nghttp3_conn *conn,
                                           int64_t qenc_stream_id,
                                           int64_t qdec_stream_id) {
  (void)qenc_stream_id;
  (void)qdec_stream_id;
  if (!conn || g_mock_nghttp3_conn_new_fail == 4) {
    return -1;
  }
  return 0;
}

static int nghttp3_conn_submit_request(nghttp3_conn *conn, int64_t stream_id,
                                       const nghttp3_nv *nva, size_t nvlen,
                                       const nghttp3_data_reader *dr,
                                       void *stream_user_data) {
  struct mock_nghttp3_stream *st;
  size_t i;
  if (!conn || g_mock_nghttp3_send_fail == 1 || conn->stream_count >= 32) {
    return -1;
  }
  st = &conn->streams[conn->stream_count++];
  memset(st, 0, sizeof(*st));
  st->stream_id = stream_id;
  st->stream_user_data = stream_user_data;
  st->path[0] = '/';
  st->path[1] = '\0';
  st->active = 1;

  for (i = 0; i < nvlen; ++i) {
    if (nva[i].namelen == 7 && memcmp(nva[i].name, ":method", 7) == 0 &&
        nva[i].valuelen < sizeof(st->method)) {
      memcpy(st->method, nva[i].value, nva[i].valuelen);
      st->method[nva[i].valuelen] = '\0';
    } else if (nva[i].namelen == 5 && memcmp(nva[i].name, ":path", 5) == 0 &&
               nva[i].valuelen < sizeof(st->path)) {
      memcpy(st->path, nva[i].value, nva[i].valuelen);
      st->path[nva[i].valuelen] = '\0';
    } else if (nva[i].namelen == 10 &&
               (memcmp(nva[i].name, "user-agent", 10) == 0 ||
                memcmp(nva[i].name, "User-Agent", 10) == 0) &&
               nva[i].valuelen < sizeof(st->ua)) {
      memcpy(st->ua, nva[i].value, nva[i].valuelen);
      st->ua[nva[i].valuelen] = '\0';
    } else if (nva[i].namelen == 14 &&
               (memcmp(nva[i].name, "content-length", 14) == 0 ||
                memcmp(nva[i].name, "Content-Length", 14) == 0) &&
               nva[i].valuelen < sizeof(st->clen)) {
      memcpy(st->clen, nva[i].value, nva[i].valuelen);
      st->clen[nva[i].valuelen] = '\0';
    } else if (nva[i].namelen == 15 &&
               memcmp(nva[i].name, "x-masque-target", 15) == 0 &&
               nva[i].valuelen < sizeof(st->masque)) {
      memcpy(st->masque, nva[i].value, nva[i].valuelen);
      st->masque[nva[i].valuelen] = '\0';
    } else if (nva[i].namelen == 19 &&
               memcmp(nva[i].name, "proxy-authorization", 19) == 0 &&
               nva[i].valuelen < sizeof(st->pauth)) {
      memcpy(st->pauth, nva[i].value, nva[i].valuelen);
      st->pauth[nva[i].valuelen] = '\0';
    }
  }

  if (dr && dr->read_data) {
    for (;;) {
      nghttp3_vec vec;
      uint32_t flags = 0;
      nghttp3_ssize rv;
      memset(&vec, 0, sizeof(vec));
      rv = dr->read_data(conn, stream_id, &vec, 1, &flags, conn->user_data,
                         stream_user_data);
      if (rv < 0) {
        return (int)rv;
      }
      if (rv > 0 && vec.len > 0 && vec.base) {
        uint8_t *nb =
            (uint8_t *)realloc(st->req_body, st->req_body_len + vec.len);
        if (!nb) {
          return -1;
        }
        memcpy(nb + st->req_body_len, vec.base, vec.len);
        st->req_body = nb;
        st->req_body_len += vec.len;
      }
      if (flags & NGHTTP3_DATA_FLAG_EOF) {
        break;
      }
    }
  }
  return 0;
}

static int mock_ngh3_emit_hdr(nghttp3_conn *conn,
                              struct mock_nghttp3_stream *st, const char *k,
                              const char *v) {
  nghttp3_rcbuf nk;
  nghttp3_rcbuf nv;
  if (!conn->cbs.recv_header) {
    return 0;
  }
  nk.base = (const uint8_t *)k;
  nk.len = strlen(k);
  nv.base = (const uint8_t *)v;
  nv.len = strlen(v);
  return conn->cbs.recv_header(conn, st->stream_id, 0, &nk, &nv, 0,
                               conn->user_data, st->stream_user_data);
}

static int mock_nghttp3_drive_loopback(nghttp3_conn *conn, ngtcp2_conn *qconn) {
  size_t i;
  const char *default_body = "NGHTTP3-H3-OK";
  if (!conn || !qconn || g_mock_nghttp3_send_fail == 2 ||
      qconn->port == 59997) {
    return -1;
  }
  for (i = 0; i < conn->stream_count; ++i) {
    struct mock_nghttp3_stream *st = &conn->streams[i];
    const uint8_t *out_body;
    size_t out_len;
    if (!st->active) {
      continue;
    }
    st->active = 0;
    if (strcmp(st->path, "/net-err") == 0) {
      return -1;
    }
    if (strcmp(st->path, "/redir-301") == 0) {
      if (mock_ngh3_emit_hdr(conn, st, ":status", "301") != 0 ||
          mock_ngh3_emit_hdr(conn, st, "location", "/final") != 0) {
        return -1;
      }
    } else if (strcmp(st->path, "/redir-307") == 0) {
      if (mock_ngh3_emit_hdr(conn, st, ":status", "307") != 0 ||
          mock_ngh3_emit_hdr(conn, st, "location", "/final") != 0) {
        return -1;
      }
    } else if (strcmp(st->path, "/redir-loop") == 0) {
      if (mock_ngh3_emit_hdr(conn, st, ":status", "302") != 0 ||
          mock_ngh3_emit_hdr(conn, st, "location", "/redir-loop") != 0) {
        return -1;
      }
    } else if (mock_ngh3_emit_hdr(conn, st, ":status", "200") != 0) {
      return -1;
    }
    {
      char sid_buf[32];
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(sid_buf, sizeof(sid_buf), "%ld", (long)st->stream_id);
#else
      c89stringutils_snprintf(sid_buf, sizeof(sid_buf), "%ld",
                              (long)st->stream_id);
#endif
      if (mock_ngh3_emit_hdr(conn, st, "x-echo-stream-id", sid_buf) != 0) {
        return -1;
      }
    }
    if (st->method[0] != '\0' &&
        mock_ngh3_emit_hdr(conn, st, "x-echo-method", st->method) != 0) {
      return -1;
    }
    if (st->clen[0] != '\0' &&
        mock_ngh3_emit_hdr(conn, st, "x-echo-clen", st->clen) != 0) {
      return -1;
    }
    if (mock_ngh3_emit_hdr(conn, st, "content-type", "text/plain") != 0) {
      return -1;
    }
    if (mock_ngh3_emit_hdr(conn, st, "set-cookie", "ng3_sid=abc; Path=/") !=
        0) {
      return -1;
    }
    if (mock_ngh3_emit_hdr(conn, st, "Set-Cookie", "ng3_plain=xyz") != 0) {
      return -1;
    }
    if (mock_ngh3_emit_hdr(conn, st, "set-cookie", "=empty_name") != 0) {
      return -1;
    }
    if (mock_ngh3_emit_hdr(conn, st, "set-cookie", "no_equals_cookie") != 0) {
      return -1;
    }
    if (st->ua[0] != '\0' &&
        mock_ngh3_emit_hdr(conn, st, "x-echo-ua", st->ua) != 0) {
      return -1;
    }
    if (st->masque[0] != '\0' &&
        mock_ngh3_emit_hdr(conn, st, "x-echo-masque", st->masque) != 0) {
      return -1;
    }
    if (st->pauth[0] != '\0' &&
        mock_ngh3_emit_hdr(conn, st, "x-echo-proxy-auth", st->pauth) != 0) {
      return -1;
    }

    if (strcmp(st->path, "/empty") != 0 && conn->cbs.recv_data) {
      out_body = (st->req_body && st->req_body_len > 0)
                     ? st->req_body
                     : (const uint8_t *)default_body;
      out_len = (st->req_body && st->req_body_len > 0) ? st->req_body_len
                                                       : strlen(default_body);
      if (out_len > 4) {
        if (conn->cbs.recv_data(conn, st->stream_id, out_body, 4,
                                conn->user_data, st->stream_user_data) != 0) {
          return -1;
        }
        if (conn->cbs.recv_data(conn, st->stream_id, out_body + 4, out_len - 4,
                                conn->user_data, st->stream_user_data) != 0) {
          return -1;
        }
      } else if (conn->cbs.recv_data(conn, st->stream_id, out_body, out_len,
                                     conn->user_data,
                                     st->stream_user_data) != 0) {
        return -1;
      }
    } else if (conn->cbs.recv_data) {
      if (conn->cbs.recv_data(conn, st->stream_id, (const uint8_t *)"", 0,
                              conn->user_data, st->stream_user_data) != 0) {
        return -1;
      }
    }

    if (conn->cbs.end_stream) {
      if (conn->cbs.end_stream(conn, st->stream_id, conn->user_data,
                               st->stream_user_data) != 0) {
        return -1;
      }
    }
  }
  return 0;
}

#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_NGHTTP3) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_NGHTTP3_H */
