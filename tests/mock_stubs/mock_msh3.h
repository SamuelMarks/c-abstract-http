/**
 * @file mock_msh3.h
 * @brief Internal test-only Microsoft MsH3 C-library loopback stubs for HTTP/3
 * framing, streaming, multiplexing, and fault-injection tests.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_MSH3_H
#define C_ABSTRACT_HTTP_TEST_MOCK_MSH3_H

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int g_mock_alloc_fail;
extern int g_mock_msh3_api_open_fail;
extern int g_mock_msh3_config_open_fail;
extern int g_mock_msh3_conn_open_fail;
extern int g_mock_msh3_request_open_fail;
extern int g_mock_msh3_send_fail;
extern int g_mock_msh3_shutdown_error;
extern int g_mock_msh3_header_alloc_fail;
extern int g_mock_msh3_body_realloc_fail;
extern int g_mock_msh3_getaddrinfo_fail;
extern int g_mock_msh3_getaddrinfo_null_result;
extern int g_mock_msh3_mutex_init_fail;
extern int g_mock_msh3_config_init_fail;
extern int g_mock_msh3_global_lock_fail;
extern int g_mock_msh3_res_alloc_fail;
extern int g_mock_msh3_res_init_fail;
extern int g_mock_msh3_cond_init_fail;
extern int g_mock_msh3_send_lock_fail;
extern int g_mock_msh3_cond_wait_fail;
extern int g_mock_msh3_header_add_fail;
extern int g_mock_msh3_cb_mutex_lock_fail;
extern int g_mock_msh3_parse_url_alloc_fail;
extern int g_mock_msh3_extra_events;
extern int g_mock_msh3_zero_addrlen;
extern int g_mock_msh3_cond_wait_loop;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_MSH3)

#ifndef MSH3_CALL
/** @brief MsH3 calling convention macro */
#define MSH3_CALL
#endif

/** @brief MsH3 status return code type */
typedef int MSH3_STATUS;

/** @brief MsH3 success status constant */
#define MSH3_STATUS_SUCCESS 0

/** @brief Mock MsH3 API instance */
typedef struct MSH3_API {
  int dummy;
} MSH3_API;

/** @brief Mock MsH3 configuration handle */
typedef struct MSH3_CONFIGURATION {
  int verify_disabled;
} MSH3_CONFIGURATION;

/** @brief Mock MsH3 settings structure */
typedef struct MSH3_SETTINGS {
  uint64_t IdleTimeoutMs;
} MSH3_SETTINGS;

/** @brief Mock MsH3 credential type enum */
typedef enum MSH3_CREDENTIAL_TYPE {
  MSH3_CREDENTIAL_TYPE_NONE = 0
} MSH3_CREDENTIAL_TYPE;

/** @brief Mock MsH3 credential flags */
#define MSH3_CREDENTIAL_FLAG_NONE 0
/** @brief Mock MsH3 client credential flag */
#define MSH3_CREDENTIAL_FLAG_CLIENT 1
/** @brief Mock MsH3 disable certificate validation flag */
#define MSH3_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION 2

/** @brief Mock MsH3 credential config structure */
typedef struct MSH3_CREDENTIAL_CONFIG {
  MSH3_CREDENTIAL_TYPE Type;
  int Flags;
} MSH3_CREDENTIAL_CONFIG;

/** @brief Mock MsH3 socket address storage */
typedef struct MSH3_ADDR {
  unsigned char raw[128];
} MSH3_ADDR;

/** @brief Mock MsH3 HTTP/3 header field */
typedef struct MSH3_HEADER {
  const char *Name;
  size_t NameLength;
  const char *Value;
  size_t ValueLength;
} MSH3_HEADER;

/** @brief Mock MsH3 request open flags */
typedef enum MSH3_REQUEST_FLAGS {
  MSH3_REQUEST_FLAG_NONE = 0
} MSH3_REQUEST_FLAGS;

/** @brief Mock MsH3 request send flags */
typedef enum MSH3_REQUEST_SEND_FLAGS {
  MSH3_REQUEST_SEND_FLAG_NONE = 0,
  MSH3_REQUEST_SEND_FLAG_FIN = 1
} MSH3_REQUEST_SEND_FLAGS;

/** @brief Mock MsH3 request event types */
typedef enum MSH3_REQUEST_EVENT_TYPE {
  MSH3_REQUEST_EVENT_SHUTDOWN_COMPLETE = 0,
  MSH3_REQUEST_EVENT_HEADER_RECEIVED = 1,
  MSH3_REQUEST_EVENT_DATA_RECEIVED = 2,
  MSH3_REQUEST_EVENT_PEER_SEND_ABORTED = 3
} MSH3_REQUEST_EVENT_TYPE;

/** @brief Mock MsH3 header-received event payload */
struct MSH3_HEADER_RECEIVED_PAYLOAD {
  const MSH3_HEADER *Header;
};

/** @brief Mock MsH3 data-received event payload */
struct MSH3_DATA_RECEIVED_PAYLOAD {
  const uint8_t *Data;
  uint32_t Length;
};

/** @brief Mock MsH3 shutdown-complete event payload */
struct MSH3_SHUTDOWN_COMPLETE_PAYLOAD {
  int ConnectionClosedRemotely;
  uint64_t ConnectionErrorCode;
};

/** @brief Mock MsH3 request callback event */
typedef struct MSH3_REQUEST_EVENT {
  MSH3_REQUEST_EVENT_TYPE Type;
  struct MSH3_HEADER_RECEIVED_PAYLOAD HEADER_RECEIVED;
  struct MSH3_DATA_RECEIVED_PAYLOAD DATA_RECEIVED;
  struct MSH3_SHUTDOWN_COMPLETE_PAYLOAD SHUTDOWN_COMPLETE;
} MSH3_REQUEST_EVENT;

/** @brief Mock MsH3 connection callback event */
typedef struct MSH3_CONNECTION_EVENT {
  int Type;
} MSH3_CONNECTION_EVENT;

struct MSH3_CONNECTION;
typedef struct MSH3_CONNECTION MSH3_CONNECTION;

struct MSH3_REQUEST;
typedef struct MSH3_REQUEST MSH3_REQUEST;

/** @brief Mock MsH3 connection event callback signature */
typedef MSH3_STATUS(MSH3_CALL *MSH3_CONNECTION_CALLBACK)(
    MSH3_CONNECTION *Connection, void *Context, MSH3_CONNECTION_EVENT *Event);

/** @brief Mock MsH3 request event callback signature */
typedef MSH3_STATUS(MSH3_CALL *MSH3_REQUEST_CALLBACK)(
    MSH3_REQUEST *Request, void *Context, MSH3_REQUEST_EVENT *Event);

/** @brief Mock MsH3 connection structure */
struct MSH3_CONNECTION {
  MSH3_API *api;
  MSH3_CONNECTION_CALLBACK callback;
  void *context;
  int started;
};

/** @brief Mock MsH3 request stream structure */
struct MSH3_REQUEST {
  MSH3_CONNECTION *conn;
  MSH3_REQUEST_CALLBACK callback;
  void *context;
  char method[16];
  char path[256];
  char user_agent[256];
  char content_type[256];
  char cookie[256];
  uint8_t *tx_buf;
  size_t tx_len;
};

static MSH3_API *MsH3ApiOpen(void) {
  if (g_mock_msh3_api_open_fail) {
    return NULL;
  }
  return (MSH3_API *)calloc(1, sizeof(MSH3_API));
}

static void MsH3ApiClose(MSH3_API *api) { free(api); }

static MSH3_CONFIGURATION *MsH3ConfigurationOpen(MSH3_API *api,
                                                 const MSH3_SETTINGS *settings,
                                                 uint32_t settings_len) {
  (void)api;
  (void)settings;
  (void)settings_len;
  if (g_mock_msh3_config_open_fail) {
    return NULL;
  }
  return (MSH3_CONFIGURATION *)calloc(1, sizeof(MSH3_CONFIGURATION));
}

static MSH3_STATUS
MsH3ConfigurationLoadCredential(MSH3_CONFIGURATION *cfg,
                                const MSH3_CREDENTIAL_CONFIG *cred) {
  if (!cfg || !cred || g_mock_msh3_extra_events) {
    return 1;
  }
  cfg->verify_disabled =
      (cred->Flags & MSH3_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION) ? 1 : 0;
  return MSH3_STATUS_SUCCESS;
}

static void MsH3ConfigurationClose(MSH3_CONFIGURATION *cfg) { free(cfg); }

static MSH3_CONNECTION *
MsH3ConnectionOpen(MSH3_API *api, MSH3_CONNECTION_CALLBACK cb, void *ctx) {
  MSH3_CONNECTION *conn;
  MSH3_CONNECTION_EVENT ev;
  if (g_mock_msh3_conn_open_fail) {
    return NULL;
  }
  conn = (MSH3_CONNECTION *)calloc(1, sizeof(MSH3_CONNECTION));
  if (!conn) {
    return NULL;
  }
  conn->api = api;
  conn->callback = cb;
  conn->context = ctx;
  if (cb) {
    memset(&ev, 0, sizeof(ev));
    cb(conn, ctx, &ev);
    if (g_mock_msh3_extra_events) {
      cb(NULL, ctx, &ev);
      cb(conn, NULL, &ev);
      cb(conn, ctx, NULL);
    }
  }
  return conn;
}

static void MsH3ConnectionStart(MSH3_CONNECTION *conn, MSH3_CONFIGURATION *cfg,
                                const char *server_name,
                                const MSH3_ADDR *addr) {
  (void)cfg;
  (void)server_name;
  (void)addr;
  if (conn) {
    conn->started = 1;
  }
}

static void MsH3ConnectionClose(MSH3_CONNECTION *conn) { free(conn); }

static MSH3_REQUEST *MsH3RequestOpen(MSH3_CONNECTION *conn,
                                     MSH3_REQUEST_CALLBACK cb, void *ctx,
                                     MSH3_REQUEST_FLAGS flags) {
  MSH3_REQUEST *req;
  (void)flags;
  if (!conn || g_mock_msh3_request_open_fail) {
    return NULL;
  }
  req = (MSH3_REQUEST *)calloc(1, sizeof(MSH3_REQUEST));
  if (!req) {
    return NULL;
  }
  req->conn = conn;
  req->callback = cb;
  req->context = ctx;
  return req;
}

static void MsH3RequestClose(MSH3_REQUEST *req) {
  if (req) {
    free(req->tx_buf);
    free(req);
  }
}

static void msh3_mock_copy_bounded(char *dst, size_t cap, const char *src,
                                   size_t len) {
  size_t n = len < (cap - 1) ? len : (cap - 1);
  if (n > 0 && src) {
    memcpy(dst, src, n);
  }
  dst[n] = '\0';
}

static int MsH3RequestSend(MSH3_REQUEST *req, MSH3_REQUEST_SEND_FLAGS flags,
                           const MSH3_HEADER *headers, size_t header_count,
                           const void *data, uint32_t data_len,
                           void *app_context) {
  size_t i;
  (void)app_context;

  if (!req || g_mock_msh3_send_fail == 1) {
    return 0;
  }
  if (g_mock_msh3_send_fail == 2 && !headers && data_len > 0) {
    return 0;
  }
  if (g_mock_msh3_send_fail == 3 && !headers && data_len == 0 &&
      (flags & MSH3_REQUEST_SEND_FLAG_FIN)) {
    return 0;
  }

  for (i = 0; headers && i < header_count; ++i) {
    const MSH3_HEADER *h = &headers[i];
    if (h->NameLength == 7 && memcmp(h->Name, ":method", 7) == 0) {
      msh3_mock_copy_bounded(req->method, sizeof(req->method), h->Value,
                             h->ValueLength);
    } else if (h->NameLength == 5 && memcmp(h->Name, ":path", 5) == 0) {
      msh3_mock_copy_bounded(req->path, sizeof(req->path), h->Value,
                             h->ValueLength);
    } else if (h->NameLength == 11 &&
               (memcmp(h->Name, "user-agent", 10) == 0 ||
                memcmp(h->Name, "User-Agent", 10) == 0)) {
      msh3_mock_copy_bounded(req->user_agent, sizeof(req->user_agent), h->Value,
                             h->ValueLength);
    } else if (h->NameLength == 10 &&
               (memcmp(h->Name, "user-agent", 10) == 0 ||
                memcmp(h->Name, "User-Agent", 10) == 0)) {
      msh3_mock_copy_bounded(req->user_agent, sizeof(req->user_agent), h->Value,
                             h->ValueLength);
    } else if (h->NameLength == 12 &&
               (memcmp(h->Name, "content-type", 12) == 0 ||
                memcmp(h->Name, "Content-Type", 12) == 0)) {
      msh3_mock_copy_bounded(req->content_type, sizeof(req->content_type),
                             h->Value, h->ValueLength);
    } else if (h->NameLength == 6 && (memcmp(h->Name, "cookie", 6) == 0 ||
                                      memcmp(h->Name, "Cookie", 6) == 0)) {
      msh3_mock_copy_bounded(req->cookie, sizeof(req->cookie), h->Value,
                             h->ValueLength);
    }
  }

  if (data && data_len > 0) {
    uint8_t *nb = (uint8_t *)realloc(req->tx_buf, req->tx_len + data_len);
    if (!nb) {
      return 0;
    }
    memcpy(nb + req->tx_len, data, data_len);
    req->tx_buf = nb;
    req->tx_len += data_len;
  }

  if ((flags & MSH3_REQUEST_SEND_FLAG_FIN) && req->callback) {
    MSH3_REQUEST_EVENT ev;
    MSH3_HEADER hdr;
    const char *status_val = "200";
    const uint8_t *resp_body = (const uint8_t *)"msh3-ok";
    uint32_t resp_len = 7;

    if (strncmp(req->path, "/notfound", 9) == 0) {
      status_val = "404";
    } else if (strcmp(req->method, "POST") == 0) {
      status_val = "201";
    }

    if (g_mock_msh3_extra_events) {
      memset(&ev, 0, sizeof(ev));
      req->callback(NULL, req->context, &ev);
      req->callback(req, NULL, &ev);
      req->callback(req, req->context, NULL);

      memset(&ev, 0, sizeof(ev));
      ev.Type = MSH3_REQUEST_EVENT_HEADER_RECEIVED;
      ev.HEADER_RECEIVED.Header = NULL;
      req->callback(req, req->context, &ev);

      memset(&ev, 0, sizeof(ev));
      ev.Type = MSH3_REQUEST_EVENT_DATA_RECEIVED;
      ev.DATA_RECEIVED.Data = (const uint8_t *)"";
      ev.DATA_RECEIVED.Length = 0;
      req->callback(req, req->context, &ev);

      memset(&ev, 0, sizeof(ev));
      ev.Type = MSH3_REQUEST_EVENT_PEER_SEND_ABORTED;
      req->callback(req, req->context, &ev);
    }

    memset(&ev, 0, sizeof(ev));
    hdr.Name = ":status";
    hdr.NameLength = 7;
    hdr.Value = status_val;
    hdr.ValueLength = strlen(status_val);
    ev.Type = MSH3_REQUEST_EVENT_HEADER_RECEIVED;
    ev.HEADER_RECEIVED.Header = &hdr;
    req->callback(req, req->context, &ev);

    memset(&ev, 0, sizeof(ev));
    hdr.Name = "content-type";
    hdr.NameLength = 12;
    hdr.Value = req->content_type[0] ? req->content_type : "text/plain";
    hdr.ValueLength = strlen(hdr.Value);
    ev.Type = MSH3_REQUEST_EVENT_HEADER_RECEIVED;
    ev.HEADER_RECEIVED.Header = &hdr;
    req->callback(req, req->context, &ev);

    if (req->user_agent[0] != '\0') {
      memset(&ev, 0, sizeof(ev));
      hdr.Name = "x-echo-ua";
      hdr.NameLength = 9;
      hdr.Value = req->user_agent;
      hdr.ValueLength = strlen(req->user_agent);
      ev.Type = MSH3_REQUEST_EVENT_HEADER_RECEIVED;
      ev.HEADER_RECEIVED.Header = &hdr;
      req->callback(req, req->context, &ev);
    }

    if (req->cookie[0] != '\0') {
      memset(&ev, 0, sizeof(ev));
      hdr.Name = "x-echo-cookie";
      hdr.NameLength = 13;
      hdr.Value = req->cookie;
      hdr.ValueLength = strlen(req->cookie);
      ev.Type = MSH3_REQUEST_EVENT_HEADER_RECEIVED;
      ev.HEADER_RECEIVED.Header = &hdr;
      req->callback(req, req->context, &ev);
    }

    if (strncmp(req->path, "/setcookie", 10) == 0) {
      memset(&ev, 0, sizeof(ev));
      hdr.Name = "set-cookie";
      hdr.NameLength = 10;
      hdr.Value = (strcmp(req->path, "/setcookie_bad") == 0) ? "=bad"
                                                             : "msh3=1; Path=/";
      hdr.ValueLength = strlen(hdr.Value);
      ev.Type = MSH3_REQUEST_EVENT_HEADER_RECEIVED;
      ev.HEADER_RECEIVED.Header = &hdr;
      req->callback(req, req->context, &ev);
    }

    if (req->tx_len > 0) {
      resp_body = req->tx_buf;
      resp_len = (uint32_t)req->tx_len;
    }

    memset(&ev, 0, sizeof(ev));
    ev.Type = MSH3_REQUEST_EVENT_DATA_RECEIVED;
    ev.DATA_RECEIVED.Data = resp_body;
    ev.DATA_RECEIVED.Length = resp_len;
    req->callback(req, req->context, &ev);

    memset(&ev, 0, sizeof(ev));
    ev.Type = MSH3_REQUEST_EVENT_SHUTDOWN_COMPLETE;
    if (g_mock_msh3_shutdown_error == 1) {
      ev.SHUTDOWN_COMPLETE.ConnectionClosedRemotely = 1;
      ev.SHUTDOWN_COMPLETE.ConnectionErrorCode = 0;
    } else if (g_mock_msh3_shutdown_error == 2) {
      ev.SHUTDOWN_COMPLETE.ConnectionClosedRemotely = 0;
      ev.SHUTDOWN_COMPLETE.ConnectionErrorCode = 1;
    } else {
      ev.SHUTDOWN_COMPLETE.ConnectionClosedRemotely = 0;
      ev.SHUTDOWN_COMPLETE.ConnectionErrorCode = 0;
    }
    req->callback(req, req->context, &ev);
  }

  return 1;
}

#endif /* !C_ABSTRACT_HTTP_HAVE_REAL_MSH3 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_MSH3_H */
