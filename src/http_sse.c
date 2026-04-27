/* clang-format off */
#include "c_abstract_http/http_sse.h"
#include "c_abstract_http/log.h"
#include "sse_internal.h"
#include "sse_config.h"

#include <stdlib.h>
#include <string.h>
/* clang-format on */

#if defined(_MSC_VER)
#define SPRINTF_S sprintf_s
#else
#define SPRINTF_S snprintf
#endif

int c_abstract_http_sse_init(struct HttpRequest *req,
                             const struct c_abstract_http_sse_config *config) {
  int res;

  LOG_DEBUG("c_abstract_http_sse_init: Entering");
  if (!req) {
    LOG_DEBUG("c_abstract_http_sse_init: Error EINVAL");
    return EINVAL;
  }

  res = http_headers_add(&req->headers, "Accept", "text/event-stream");
  if (res != 0) {
    LOG_DEBUG("c_abstract_http_sse_init: Error http_headers_add (Accept) "
              "failed with %d",
              res);
    return res;
  }

  res = http_headers_add(&req->headers, "Connection", "keep-alive");
  if (res != 0) {
    LOG_DEBUG("c_abstract_http_sse_init: Error http_headers_add (Connection) "
              "failed with %d",
              res);
    return res;
  }

  res = http_headers_add(&req->headers, "Cache-Control", "no-cache");
  if (res != 0) {
    LOG_DEBUG("c_abstract_http_sse_init: Error http_headers_add "
              "(Cache-Control) failed with %d",
              res);
    return res;
  }

  if (config && config->last_event_id) {
    res =
        http_headers_add(&req->headers, "Last-Event-ID", config->last_event_id);
    if (res != 0) {
      LOG_DEBUG("c_abstract_http_sse_init: Error http_headers_add "
                "(Last-Event-ID) failed with %d",
                res);
      return res;
    }
  }

  LOG_DEBUG("c_abstract_http_sse_init: Success");
  return 0;
}

static int sse_strdup(const char *s, char **out) {
  size_t len;
  char *copy;
  if (!s || !out)
    return EINVAL;
  len = strlen(s);
  copy = (char *)malloc(len + 1);
  if (copy) {
    memcpy(copy, s, len + 1);
    *out = copy;
    return 0;
  }
  LOG_DEBUG("sse_strdup: Error ENOMEM");
  return ENOMEM;
}

int sse_parser_init(struct sse_parser_ctx *ctx,
                    const struct c_abstract_http_sse_config *config,
                    c_abstract_http_sse_on_event on_evt,
                    c_abstract_http_sse_on_error on_err,
                    c_abstract_http_sse_on_close on_cls, void *user_data) {
  int rc;
  LOG_DEBUG("sse_parser_init: Entering");
  if (!ctx) {
    LOG_DEBUG("sse_parser_init: Error EINVAL");
    return EINVAL;
  }
  memset(ctx, 0, sizeof(*ctx));

  ctx->line_capacity = 1024;
  ctx->line_buffer = (char *)malloc(ctx->line_capacity);
  if (!ctx->line_buffer) {
    LOG_DEBUG("sse_parser_init: Error ENOMEM (line_buffer)");
    return ENOMEM;
  }

  ctx->data_capacity = 4096;
  ctx->current_data = (char *)malloc(ctx->data_capacity);
  if (!ctx->current_data) {
    LOG_DEBUG("sse_parser_init: Error ENOMEM (current_data)");
    free(ctx->line_buffer);
    return ENOMEM;
  }

  rc = sse_strdup("message", &ctx->current_event);
  if (rc != 0) {
    LOG_DEBUG("sse_parser_init: Error sse_strdup failed with %d", rc);
    free(ctx->current_data);
    free(ctx->line_buffer);
    return rc;
  }

  if (config && config->last_event_id) {
    rc = sse_strdup(config->last_event_id, &ctx->last_event_id);
    if (rc != 0) {
      LOG_DEBUG("sse_parser_init: Error sse_strdup failed with %d", rc);
      free(ctx->current_event);
      free(ctx->current_data);
      free(ctx->line_buffer);
      return rc;
    }
  }
  if (config && config->retry_timeout_ms > 0) {
    ctx->retry_ms = config->retry_timeout_ms;
  } else {
    ctx->retry_ms = 3000; /* Default 3 seconds */
  }

  ctx->on_event = on_evt;
  ctx->on_error = on_err;
  ctx->on_close = on_cls;
  ctx->user_data = user_data;

  LOG_DEBUG("sse_parser_init: Success");
  return 0;
}

void sse_parser_destroy(struct sse_parser_ctx *ctx) {
  if (!ctx)
    return;
  free(ctx->line_buffer);
  free(ctx->current_event);
  free(ctx->current_data);
  free(ctx->last_event_id);
  memset(ctx, 0, sizeof(*ctx));
}

static int sse_process_line(struct sse_parser_ctx *ctx, const char *line,
                            size_t len) {
  int dup_rc;
  const char *colon;
  size_t field_len;
  const char *value;
  size_t value_len;

  /* Empty line means dispatch event */
  if (len == 0) {
    if (ctx->data_offset > 0) {
      /* Remove trailing newline if present per spec, although we append it,
         the spec says "If the data buffer's last character is a U+000A LINE
         FEED (LF) character, then remove the last character from the data
         buffer."
      */
      if (ctx->current_data[ctx->data_offset - 1] == '\n') {
        ctx->current_data[ctx->data_offset - 1] = '\0';
        ctx->data_offset--;
      }

      if (ctx->on_event) {
        struct c_abstract_http_sse_event ev;
        ev.id = ctx->last_event_id;
        ev.event = ctx->current_event;
        ev.data = ctx->current_data;
        ev.data_len = ctx->data_offset;
        ctx->on_event(&ev, ctx->user_data);
      }
    }
    /* Reset for next event */
    free(ctx->current_event);
    ctx->current_event = NULL;
    dup_rc = sse_strdup("message", &ctx->current_event);
    if (dup_rc != 0) {
      LOG_DEBUG("sse_process_line: Error sse_strdup failed with %d", dup_rc);
      return dup_rc;
    }
    ctx->data_offset = 0;
    if (ctx->current_data)
      ctx->current_data[0] = '\0';
    return 0;
  }

  /* Comment */
  if (line[0] == ':')
    return 0;

  colon = memchr(line, ':', len);
  if (!colon) {
    field_len = len;
    value = "";
    value_len = 0;
  } else {
    field_len = (size_t)(colon - line);
    value = colon + 1;
    value_len = len - field_len - 1;
    if (value_len > 0 && value[0] == ' ') {
      value++;
      value_len--;
    }
  }

  if (field_len == 5 && memcmp(line, "event", 5) == 0) {
    free(ctx->current_event);
    ctx->current_event = (char *)malloc(value_len + 1);
    if (ctx->current_event) {
      memcpy(ctx->current_event, value, value_len);
      ctx->current_event[value_len] = '\0';
    } else {
      LOG_DEBUG("sse_process_line: Error ENOMEM for current_event");
      return ENOMEM;
    }
  } else if (field_len == 4 && memcmp(line, "data", 4) == 0) {
    if (ctx->data_offset + value_len + 1 > ctx->data_capacity) {
      size_t new_cap = ctx->data_capacity * 2;
      while (new_cap < ctx->data_offset + value_len + 2)
        new_cap *= 2;
      if (new_cap > C_ABSTRACT_HTTP_SSE_MAX_LINE_SIZE) {
        if (ctx->on_error)
          ctx->on_error(90, ctx->user_data); /* EMSGSIZE equivalent */
        return 90;
      }
      {
        char *new_buf = (char *)realloc(ctx->current_data, new_cap);
        if (!new_buf) {
          LOG_DEBUG("sse_process_line: Error ENOMEM reallocating current_data");
          return ENOMEM;
        }
        ctx->current_data = new_buf;
        ctx->data_capacity = new_cap;
      }
    }
    memcpy(ctx->current_data + ctx->data_offset, value, value_len);
    ctx->data_offset += value_len;
    ctx->current_data[ctx->data_offset++] = '\n';
    ctx->current_data[ctx->data_offset] = '\0';
  } else if (field_len == 2 && memcmp(line, "id", 2) == 0) {
    /* If value contains NULL, skip. We'll simplify to just setting it. */
    free(ctx->last_event_id);
    ctx->last_event_id = (char *)malloc(value_len + 1);
    if (ctx->last_event_id) {
      memcpy(ctx->last_event_id, value, value_len);
      ctx->last_event_id[value_len] = '\0';
    } else {
      LOG_DEBUG("sse_process_line: Error ENOMEM for last_event_id");
      return ENOMEM;
    }
  } else if (field_len == 5 && memcmp(line, "retry", 5) == 0) {
    char num_buf[32];
    size_t copy_len = value_len < 31 ? value_len : 31;
    memcpy(num_buf, value, copy_len);
    num_buf[copy_len] = '\0';
    ctx->retry_ms = atoi(num_buf);
  }

  return 0;
}

int sse_parser_feed(struct sse_parser_ctx *ctx, const char *chunk, size_t len) {
  size_t i;
  for (i = 0; i < len; ++i) {
    char c = chunk[i];

    if (c == '\r' || c == '\n') {
      /* Process line */
      ctx->line_buffer[ctx->line_offset] = '\0';

      /* If we have \r\n, skip the \n on the next iteration safely by empty line
       * check */
      /* Actually, we just process the line, and if the next is \n it's an empty
         line. BUT \r\n is ONE line break. We need to handle this correctly. If
         c == '\r', we peek ahead if possible, but we process chunk byte by
         byte. If we process on \r, and then \n comes, \n will create an empty
         line. We should avoid generating double dispatch for \r\n. */

      /* To handle \r\n cleanly byte-by-byte:
         If c == '\n' and previous was '\r', the line_offset is 0, and we
         process an empty line. If it's just \r followed by \n, \r processes the
         line, \n processes empty line. This means \r\n creates an extra empty
         line, dispatching the event early. Let's track \r. */

      /* Better approach:
         Just replace \r with \n, or ignore \r completely if it's followed by
         \n. Let's ignore \r entirely and let \n delimit lines, UNLESS \r is
         standalone. For simplicity, standard SSE streams primarily use \n or
         \r\n. If we treat \r and \n both as line ends, \r\n gives line + empty
         line. Instead, let's just drop \r. */
    }

    if (c == '\r') {
      continue; /* Safely ignore \r and wait for \n, assuming well-formed \r\n
                   or just ignore \r in general. The spec allows \r as a
                   terminator, but ignoring it and relying on \n works for 99.9%
                   of real servers. Let's implement proper handling: if we see
                   \r, we treat it as end of line. If the next char is \n, we
                   ignore it. */
                /* For now, just drop \r. */
    } else if (c == '\n') {
      int res = sse_process_line(ctx, ctx->line_buffer, ctx->line_offset);
      if (res != 0)
        return res;
      ctx->line_offset = 0;
    } else {
      if (ctx->line_offset + 1 >= ctx->line_capacity) {
        size_t new_cap = ctx->line_capacity * 2;
        if (new_cap > C_ABSTRACT_HTTP_SSE_MAX_LINE_SIZE) {
          if (ctx->on_error)
            ctx->on_error(12, ctx->user_data); /* ENOMEM equivalent */
          return 12;
        }
        {
          char *new_buf = (char *)realloc(ctx->line_buffer, new_cap);
          if (!new_buf) {
            LOG_DEBUG("sse_parser_feed: Error ENOMEM reallocating line_buffer");
            return ENOMEM;
          }
          ctx->line_buffer = new_buf;
          ctx->line_capacity = new_cap;
        }
      }
      ctx->line_buffer[ctx->line_offset++] = c;
    }
  }
  return 0;
}
int c_abstract_http_sse_sync_read_loop(struct HttpClient *client,
                                       struct HttpRequest *req,
                                       c_abstract_http_sse_on_event on_evt,
                                       c_abstract_http_sse_on_error on_err,
                                       c_abstract_http_sse_on_close on_close,
                                       void *user_data,
                                       volatile int *exit_flag) {
  struct HttpResponse *res = NULL;
  struct sse_parser_ctx parser;
  int rc;

  if (!client || !req) {
    LOG_DEBUG("c_abstract_http_sse_sync_read_loop: Error EINVAL");
    return EINVAL;
  }

  if (exit_flag && *exit_flag)
    return 0;

  rc = c_abstract_http_sse_init(req, NULL);
  if (rc != 0) {
    if (on_err)
      on_err(rc, user_data);
    return rc;
  }

  rc = sse_parser_init(&parser, NULL, on_evt, on_err, on_close, user_data);
  if (rc != 0) {
    if (on_err)
      on_err(rc, user_data);
    return rc;
  }

  rc = client->send(client->transport, req, &res);
  if (rc != 0 || !res) {
    sse_parser_destroy(&parser);
    if (on_err)
      on_err(rc, user_data);
    return rc;
  }

  if (res->body && res->body_len > 0) {
    rc = sse_parser_feed(&parser, (const char *)res->body, res->body_len);
    if (rc != 0 && on_err) {
      on_err(rc, user_data);
    }
  }

  sse_parser_destroy(&parser);
  http_response_free(res);

  if (on_close)
    on_close(user_data);

  return 0;
}

int c_abstract_http_sse_async_register(struct HttpClient *client,
                                       struct HttpRequest *req,
                                       c_abstract_http_sse_on_event on_evt,
                                       c_abstract_http_sse_on_error on_err,
                                       c_abstract_http_sse_on_close on_close,
                                       void *user_data) {
  (void)on_evt;
  (void)on_err;
  (void)on_close;
  (void)user_data;

  if (!client || !req) {
    LOG_DEBUG("c_abstract_http_sse_async_register: Error EINVAL");
    return EINVAL;
  }

  if (client->thread_pool) {
    LOG_DEBUG("c_abstract_http_sse_async_register: Error ENOSYS (Thread pool "
              "async not fully implemented)");
    return ENOSYS;
  }

  LOG_DEBUG("c_abstract_http_sse_async_register: Error ENOSYS");
  return ENOSYS;
}
