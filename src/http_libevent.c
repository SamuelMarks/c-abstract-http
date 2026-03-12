/**
 * @file http_libevent.c
 * @brief Implementation of the libevent backend using evhttp.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_libevent.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/str.h>

#ifdef C_ABSTRACT_HTTP_USE_LIBEVENT
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#endif

/* clang-format on */

struct HttpTransportContext {
  struct HttpConfig config;
};

static int libevent_global_init_count = 0;

int http_libevent_global_init(void) {
  libevent_global_init_count++;
#ifdef _WIN32
  if (libevent_global_init_count == 1) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
  }
#endif
  return 0;
}

void http_libevent_global_cleanup(void) {
  if (libevent_global_init_count > 0) {
    libevent_global_init_count--;
#ifdef _WIN32
    if (libevent_global_init_count == 0) {
      WSACleanup();
    }
#endif
  }
}

int http_libevent_context_init(struct HttpTransportContext **ctx) {
  if (!ctx) {
    return EINVAL;
  }
  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    return ENOMEM;
  }
  memset(*ctx, 0, sizeof(struct HttpTransportContext));
  return 0;
}

void http_libevent_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    free(ctx);
  }
}

int http_libevent_config_apply(struct HttpTransportContext *ctx,
                               const struct HttpConfig *config) {
  if (!ctx || !config) {
    return EINVAL;
  }
  ctx->config = *config;
  return 0;
}

#ifdef C_ABSTRACT_HTTP_USE_LIBEVENT
static int get_method_cmd(enum HttpMethod method) {
  switch (method) {
  case HTTP_GET:
    return EVHTTP_REQ_GET;
  case HTTP_POST:
    return EVHTTP_REQ_POST;
  case HTTP_PUT:
    return EVHTTP_REQ_PUT;
  case HTTP_DELETE:
    return EVHTTP_REQ_DELETE;
  case HTTP_PATCH:
    return EVHTTP_REQ_PATCH;
  case HTTP_HEAD:
    return EVHTTP_REQ_HEAD;
  case HTTP_OPTIONS:
    return EVHTTP_REQ_OPTIONS;
  case HTTP_TRACE:
    return EVHTTP_REQ_TRACE;
  case HTTP_CONNECT:
    return EVHTTP_REQ_CONNECT;
  default:
    return EVHTTP_REQ_GET;
  }
}
#endif

#ifdef C_ABSTRACT_HTTP_USE_LIBEVENT

struct libevent_state {
  struct event_base *base;
  struct evhttp_connection *conn;
  const struct HttpRequest *req;
  struct HttpResponse **res;
  int error_code;
  int done;
};

static void http_request_done(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state = (struct libevent_state *)arg;
  state->done = 1;

  if (!req_ev) {
    /* Request failed */
    state->error_code = ECONNREFUSED;
    return;
  }

  if (state->error_code != 0) {
    /* Aborted */
    return;
  }

  *state->res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  memset(*state->res, 0, sizeof(struct HttpResponse));
  (*state->res)->status_code = evhttp_request_get_response_code(req_ev);

  {
    struct evbuffer *evb = evhttp_request_get_input_buffer(req_ev);
    size_t len = evbuffer_get_length(evb);
    if (len > 0) {
      char *body_char = (char *)malloc(len + 1);
      evbuffer_remove(evb, body_char, len);
      body_char[len] = '\0';
      (*state->res)->body = body_char;
      (*state->res)->body_len = len;
    }
  }
}

static void http_chunked_cb(struct evhttp_request *req_ev, void *arg) {
  struct libevent_state *state = (struct libevent_state *)arg;
  struct evbuffer *evb = evhttp_request_get_input_buffer(req_ev);
  size_t len = evbuffer_get_length(evb);

  if (len > 0 && state->req->on_chunk) {
    char *buf = (char *)malloc(len);
    evbuffer_remove(evb, buf, len);
    {
      int rc = state->req->on_chunk(state->req->on_chunk_user_data, buf, len);
      if (rc != 0) {
        state->error_code = rc;
        /* we can't abort nicely in evhttp callback without closing conn,
           but evhttp_connection_free is unsafe here. We just set error. */
        event_base_loopbreak(state->base);
      }
    }
    free(buf);
  }
}

#endif /* C_ABSTRACT_HTTP_USE_LIBEVENT */

int http_libevent_send(struct HttpTransportContext *ctx,
                       const struct HttpRequest *req,
                       struct HttpResponse **res) {
  if (!ctx || !req || !res) {
    return EINVAL;
  }

#ifndef C_ABSTRACT_HTTP_USE_LIBEVENT
  return ENOSYS;
#else
  {
    struct libevent_state state;
    struct evhttp_request *req_ev;
    char host[256] = {0};
    int port = 80;
    const char *p;
    char *path = "/";
    struct evkeyvalq *output_headers;
    int rc = 0;

    memset(&state, 0, sizeof(state));
    state.req = req;
    state.res = res;
    state.error_code = 0;

    /* Parse URL: very basic parsing for test */
    if (strncmp(req->url, "http://", 7) == 0) {
      p = req->url + 7;
    } else {
      p = req->url;
    }

    {
      const char *colon = strchr(p, ':');
      const char *slash = strchr(p, '/');

      if (colon && (!slash || colon < slash)) {
        size_t len = (size_t)(colon - p);
        if (len < sizeof(host)) {
          memcpy(host, p, len);
          host[len] = '\0';
        }
        port = atoi(colon + 1);
        if (slash)
          path = (char *)slash;
      } else if (slash) {
        size_t len = (size_t)(slash - p);
        if (len < sizeof(host)) {
          memcpy(host, p, len);
          host[len] = '\0';
        }
        path = (char *)slash;
      } else {
        size_t len = strlen(p);
        if (len < sizeof(host)) {
          memcpy(host, p, len);
          host[len] = '\0';
        }
      }
    }

    state.base = event_base_new();
    if (!state.base)
      return ENOMEM;

    state.conn = evhttp_connection_base_new(state.base, NULL, host, port);
    if (!state.conn) {
      event_base_free(state.base);
      return ENOMEM;
    }

    if (ctx->config.timeout_ms > 0) {
      evhttp_connection_set_timeout(state.conn, ctx->config.timeout_ms / 1000);
    }

    req_ev = evhttp_request_new(http_request_done, &state);
    if (!req_ev) {
      evhttp_connection_free(state.conn);
      event_base_free(state.base);
      return ENOMEM;
    }

    if (req->on_chunk) {
      evhttp_request_set_chunked_cb(req_ev, http_chunked_cb);
    }

    output_headers = evhttp_request_get_output_headers(req_ev);
    evhttp_add_header(output_headers, "Host", host);
    evhttp_add_header(output_headers, "Connection", "close");

    if (req->headers.count > 0) {
      size_t i;
      for (i = 0; i < req->headers.count; i++) {
        evhttp_add_header(output_headers, req->headers.headers[i].key,
                          req->headers.headers[i].value);
      }
    }

    if (req->body && req->body_len > 0) {
      struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
      evbuffer_add(evb, req->body, req->body_len);
    } else if (req->read_chunk) {
      struct evbuffer *evb = evhttp_request_get_output_buffer(req_ev);
      char buf[4096];
      size_t read_bytes = 0;
      while (1) {
        int r = req->read_chunk(req->read_chunk_user_data, buf, sizeof(buf),
                                &read_bytes);
        if (r != 0 || read_bytes == 0)
          break;
        evbuffer_add(evb, buf, read_bytes);
      }
    }

    rc = evhttp_make_request(state.conn, req_ev, get_method_cmd(req->method),
                             path);
    if (rc != 0) {
      evhttp_connection_free(state.conn);
      event_base_free(state.base);
      return ECONNREFUSED;
    }

    event_base_dispatch(state.base);

    if (state.error_code != 0) {
      rc = state.error_code;
    } else if (!*res) {
      rc = ECONNREFUSED;
    }

    evhttp_connection_free(state.conn);
    event_base_free(state.base);

    return rc;
  }
#endif
}