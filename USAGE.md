# Usage

This guide covers CMake integration, client lifecycle, synchronous and concurrent HTTP requests, error percolation, OAuth 2.0 grants, streaming, WebSockets, Server-Sent Events (SSE), multipart uploads, and custom event-loop hooks in `c-abstract-http`.

All examples conform strictly to ISO C89 (ANSI C / C90) and percolate `enum c_abstract_http_error` without discarding `NO_DISCARD` return values.

---

## 1. Choosing a Backend & Compiling

By default, CMake selects the native OS transport (`WinHTTP` on modern Windows, `WinINet` on MSVC < 2010, `CFNetwork`/`NSURLSession` on macOS/iOS, Emscripten Fetch on WebAssembly, raw sockets on MS-DOS, and `libcurl` on Linux/BSD/POSIX):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

You can override the transport backend or enable single-header amalgamation (`INTERFACE` library) at configure time:

```bash
# HTTP/3 via Microsoft msh3 (msquic)
cmake -S . -B build -DC_ABSTRACT_HTTP_USE_MSH3=ON

# Evented TCP via libuv
cmake -S . -B build -DC_ABSTRACT_HTTP_USE_LIBUV=ON

# STB-style single-header mode
cmake -S . -B build -DC_ABSTRACT_HTTP_HEADER_ONLY=ON
```

---

## 2. Initializing & Cleaning Up the Client

Initialize global transport state once at process startup (`transport_global_init`), initialize `struct HttpClient` (`http_client_init`), and bind the compiled transport backend (`transport_factory_init_client`). Tear down in reverse order:

```c
/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

int main(void) {
  struct HttpClient client;
  enum c_abstract_http_error rc;
  enum c_abstract_http_error clean_rc;

  rc = transport_global_init();
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    fprintf(stderr, "transport_global_init failed: %d\n", (int)rc);
    return 1;
  }

  rc = http_client_init(&client);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    fprintf(stderr, "http_client_init failed: %d\n", (int)rc);
    clean_rc = transport_global_cleanup();
    return clean_rc != C_ABSTRACT_HTTP_SUCCESS ? (int)clean_rc : (int)rc;
  }

  rc = transport_factory_init_client(&client);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    fprintf(stderr, "transport_factory_init_client failed: %d\n", (int)rc);
    clean_rc = http_client_free(&client);
    if (clean_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return (int)clean_rc;
    }
    clean_rc = transport_global_cleanup();
    return clean_rc != C_ABSTRACT_HTTP_SUCCESS ? (int)clean_rc : (int)rc;
  }

  client.config.timeout_ms = 5000;
  client.config.follow_redirects = 1;

  rc = transport_factory_cleanup_client(&client);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return (int)rc;
  }
  rc = http_client_free(&client);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return (int)rc;
  }
  rc = transport_global_cleanup();
  return (int)rc;
}
```

---

## 3. Sending Synchronous HTTP Requests

Note that `http_request_free()` frees `req.url` and `req.body` when non-`NULL`. Always allocate `req.url` (for example with `c_abstract_http_strdup`) or reset borrowed pointers to `NULL` before calling `http_request_free(&req)`:

```c
/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <c_abstract_http/c_abstract_http.h>
#include <c_abstract_http/win_compat_sym.h>
/* clang-format on */

enum c_abstract_http_error fetch_json(struct HttpClient *client) {
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const char *content_type = NULL;
  enum c_abstract_http_error rc;
  enum c_abstract_http_error free_rc;

  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  req.method = HTTP_GET;
  rc = c_abstract_http_strdup("https://api.example.com/v1/status", &req.url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  rc = http_headers_add(&req.headers, "Accept", "application/json");
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  rc = client->send(client->transport, &req, &res);
  if (rc == C_ABSTRACT_HTTP_SUCCESS && res != NULL) {
    printf("Status: %d (%" SIZE_T_FMT " bytes)\n", res->status_code,
           res->body_len);
    if (http_headers_get(&res->headers, "Content-Type", &content_type) ==
        C_ABSTRACT_HTTP_SUCCESS) {
      printf("Content-Type: %s\n", content_type);
    }
    free_rc = http_response_free(res);
    free(res);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      rc = free_rc;
    }
  }

  free_rc = http_request_free(&req);
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS && rc == C_ABSTRACT_HTTP_SUCCESS) {
    rc = free_rc;
  }
  return rc;
}
```

---

## 4. Error Handling

Every API call returns `enum c_abstract_http_error` (`0` is `C_ABSTRACT_HTTP_SUCCESS`):

```c
/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

enum c_abstract_http_error log_transport_result(enum c_abstract_http_error rc) {
  switch (rc) {
  case C_ABSTRACT_HTTP_SUCCESS:
    return C_ABSTRACT_HTTP_SUCCESS;
  case C_ABSTRACT_HTTP_ERR_INVAL:
    fprintf(stderr, "Invalid argument (%d)\n", (int)rc);
    return rc;
  case C_ABSTRACT_HTTP_ERR_NOMEM:
    fprintf(stderr, "Out of memory (%d)\n", (int)rc);
    return rc;
  case C_ABSTRACT_HTTP_ERR_TIMEOUT:
    fprintf(stderr, "Operation timed out (%d)\n", (int)rc);
    return rc;
  case C_ABSTRACT_HTTP_ERR_ACCES:
    fprintf(stderr, "Permission or TLS verification denied (%d)\n", (int)rc);
    return rc;
  default:
    fprintf(stderr, "Transport error (%d)\n", (int)rc);
    return rc;
  }
}
```

---

## 5. OAuth 2.0 Helpers & Localhost Loopback Intercept

`http_types.h` provides RFC-compliant OAuth 2.0 request builders (Client Credentials, Password, Authorization Code + PKCE, Refresh Token, Device Code, JWT Bearer, Token Introspection, and Token Revocation) plus a synchronous loopback callback listener (`http_oauth2_localhost_intercept`).

### Client Credentials Grant

```c
/* clang-format off */
#include <stdlib.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

enum c_abstract_http_error request_client_token(struct HttpClient *client) {
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;
  enum c_abstract_http_error free_rc;

  rc = http_request_init_oauth2_client_credentials_grant(
      &req, "https://auth.example.com/oauth/token", "my_client_id",
      "my_client_secret", "read:items");
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = client->send(client->transport, &req, &res);
  if (rc == C_ABSTRACT_HTTP_SUCCESS && res != NULL) {
    free_rc = http_response_free(res);
    free(res);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
      rc = free_rc;
    }
  }

  free_rc = http_request_free(&req);
  return rc != C_ABSTRACT_HTTP_SUCCESS ? rc : free_rc;
}
```

### Localhost OAuth 2.0 Redirect Intercept

```c
/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

enum c_abstract_http_error intercept_oauth_callback(void) {
  char *code = NULL;
  char *state = NULL;
  char *err = NULL;
  char *err_desc = NULL;
  const char *reply =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Connection: close\r\n\r\n"
      "<html><body>Authentication complete. You may close this tab.</body></html>";
  enum c_abstract_http_error rc;

  rc = http_oauth2_localhost_intercept(8080, reply, &code, &state, &err,
                                       &err_desc);
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    if (code != NULL) {
      printf("Authorization code received: %s\n", code);
    } else if (err != NULL) {
      fprintf(stderr, "OAuth error: %s (%s)\n", err,
              err_desc ? err_desc : "no description");
    }
  }

  free(code);
  free(state);
  free(err);
  free(err_desc);
  return rc;
}
```

---

## 6. Streaming Downloads & Concurrent Multi-Requests

Register `req.on_chunk` (`http_on_chunk_fn`) to stream large responses without buffering the entire payload in memory, or dispatch batches via `http_client_send_multi`:

```c
/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/c_abstract_http.h>
#include <c_abstract_http/win_compat_sym.h>
/* clang-format on */

static enum c_abstract_http_error on_body_chunk(void *user_data,
                                                const void *chunk,
                                                size_t len) {
  size_t *total = (size_t *)user_data;
  if (chunk) {
    *total += len;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error on_multi_progress(size_t current_bytes,
                                                    size_t total_bytes,
                                                    void *user_data) {
  if (user_data) {
  }
  printf("Progress: %" SIZE_T_FMT " / %" SIZE_T_FMT "\n", current_bytes,
         total_bytes);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error setup_streaming_Callbacks(struct HttpRequest *req,
                                                     size_t *counter,
                                                     http_multi_progress_cb *out_cb) {
  if (!req || !counter || !out_cb) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  req->on_chunk = on_body_chunk;
  req->on_chunk_user_data = counter;
  *out_cb = on_multi_progress;
  return C_ABSTRACT_HTTP_SUCCESS;
}
```

---

## 7. Raw Sockets Fallback (`http_raw.c`)

For minimal POSIX environments or retro targets such as MS-DOS (OpenWatcom), `c-abstract-http` includes a plain HTTP/1.1 TCP socket transport (`http_raw.c`). Note that `http_raw.c` performs unencrypted HTTP/1.1 over TCP sockets and does not perform TLS negotiation:

```bash
# Force raw sockets transport on Linux, macOS, BSD, or Windows
cmake -S . -B build -DC_ABSTRACT_HTTP_USE_RAW_SOCKETS=ON

# MS-DOS cross-compilation (automatically selects http_raw.c)
cmake -S . -B build -DCMAKE_SYSTEM_NAME=DOS
```

---

## 8. WebSockets & Server-Sent Events (SSE)

Both `C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS` and `C_ABSTRACT_HTTP_ENABLE_SSE` default to `ON` in CMake.

### WebSockets (`http_ws.h`)

```c
/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/c_abstract_http.h>
#include <c_abstract_http/win_compat_sym.h>
/* clang-format on */

static enum c_abstract_http_error
on_ws_msg(const struct c_abstract_http_ws_event *ev, void *user_data) {
  if (user_data) {
  }
  printf("Received WS frame opcode=%d (%" SIZE_T_FMT " bytes)\n",
         (int)ev->opcode, ev->payload_len);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error on_ws_err(int error_code, void *user_data) {
  if (user_data) {
  }
  fprintf(stderr, "WS error: %d\n", error_code);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error on_ws_closed(int status_code,
                                               void *user_data) {
  if (user_data) {
  }
  printf("WS closed with status %d\n", status_code);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error run_ws_session(struct HttpClient *client) {
  struct HttpRequest req;
  struct c_abstract_http_ws_config cfg;
  enum c_abstract_http_error rc;
  enum c_abstract_http_error free_rc;

  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = c_abstract_http_strdup("https://echo.example.com/ws", &req.url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  cfg.custom_headers = NULL;
  cfg.subprotocols = "chat";
  cfg.masking_preference = 1;

  rc = c_abstract_http_ws_init(&req, &cfg);
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    rc = c_abstract_http_ws_sync_read_loop(client, &req, on_ws_msg, on_ws_err,
                                           on_ws_closed, NULL, NULL);
    free_rc = c_abstract_http_ws_free(&req);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS && rc == C_ABSTRACT_HTTP_SUCCESS) {
      rc = free_rc;
    }
  }

  free_rc = http_request_free(&req);
  return rc != C_ABSTRACT_HTTP_SUCCESS ? rc : free_rc;
}
```

### Server-Sent Events (`http_sse.h`)

```c
/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

static enum c_abstract_http_error
on_sse_ev(const struct c_abstract_http_sse_event *ev, void *user_data) {
  if (user_data) {
  }
  printf("Event [%s]: %.*s\n", ev->event ? ev->event : "message",
         (int)ev->data_len, ev->data ? ev->data : "");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error on_sse_err(int error_code, void *user_data) {
  if (user_data) {
  }
  fprintf(stderr, "SSE error: %d\n", error_code);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error on_sse_closed(void *user_data) {
  if (user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error run_sse_stream(struct HttpClient *client) {
  struct HttpRequest req;
  struct c_abstract_http_sse_config cfg;
  enum c_abstract_http_error rc;
  enum c_abstract_http_error free_rc;

  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = c_abstract_http_strdup("https://sse.example.com/events", &req.url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  cfg.last_event_id = NULL;
  cfg.retry_timeout_ms = 3000;

  rc = c_abstract_http_sse_init(&req, &cfg);
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    rc = c_abstract_http_sse_sync_read_loop(client, &req, on_sse_ev, on_sse_err,
                                            on_sse_closed, NULL, NULL);
    free_rc = c_abstract_http_sse_free(&req);
    if (free_rc != C_ABSTRACT_HTTP_SUCCESS && rc == C_ABSTRACT_HTTP_SUCCESS) {
      rc = free_rc;
    }
  }

  free_rc = http_request_free(&req);
  return rc != C_ABSTRACT_HTTP_SUCCESS ? rc : free_rc;
}
```

---

## 9. Multipart Form-Data Uploads

Use `http_request_add_part`, optional `http_request_add_part_header_last`, and `http_request_flatten_parts` to construct an RFC 7578 `multipart/form-data` payload in `req.body` and set the boundary `Content-Type` header automatically:

```c
/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

enum c_abstract_http_error upload_multipart(struct HttpClient *client) {
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  const char *meta_json = "{\"title\":\"report\"}";
  const char *csv_bytes = "id,score\n1,99\n";
  enum c_abstract_http_error rc;
  enum c_abstract_http_error free_rc;

  rc = http_request_init(&req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  req.method = HTTP_POST;
  rc = c_abstract_http_strdup("https://api.example.com/upload", &req.url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  rc = http_request_add_part(&req, "metadata", NULL, "application/json",
                             meta_json, strlen(meta_json));
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  rc = http_request_add_part(&req, "attachment", "scores.csv", "text/csv",
                             csv_bytes, strlen(csv_bytes));
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  rc = http_request_add_part_header_last(&req, "X-Part-Source", "cli");
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free_rc = http_request_free(&req);
    return free_rc != C_ABSTRACT_HTTP_SUCCESS ? free_rc : rc;
  }

  rc = http_request_flatten_parts(&req);
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    rc = client->send(client->transport, &req, &res);
    if (rc == C_ABSTRACT_HTTP_SUCCESS && res != NULL) {
      free_rc = http_response_free(res);
      free(res);
      if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
        rc = free_rc;
      }
    }
  }

  free_rc = http_request_free(&req);
  return rc != C_ABSTRACT_HTTP_SUCCESS ? rc : free_rc;
}
```

---

## 10. Advanced Concurrency (Thread Pool & External Event Loop)

### Cross-Platform Thread Pool (`thread_pool.h`)

Task callbacks match `abstract_http_thread_task_cb` and return `enum c_abstract_http_error`:

```c
/* clang-format off */
#include <stdio.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

static enum c_abstract_http_error worker_task(void *arg) {
  const char *job_name = (const char *)arg;
  printf("Executing job: %s\n", job_name);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error run_pool_jobs(void) {
  struct AbstractHttpThreadPool *pool = NULL;
  char job1[] = "sync-catalog";
  char job2[] = "flush-metrics";
  enum c_abstract_http_error rc;
  enum c_abstract_http_error free_rc;

  rc = abstract_http_thread_pool_init(&pool, 4);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  rc = abstract_http_thread_pool_push(pool, worker_task, job1);
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    rc = abstract_http_thread_pool_push(pool, worker_task, job2);
  }

  free_rc = abstract_http_thread_pool_free(pool);
  return rc != C_ABSTRACT_HTTP_SUCCESS ? rc : free_rc;
}
```

### External Event Loop Proxy (`event_loop.h`)

All hook callbacks in `struct HttpLoopHooks` return `enum c_abstract_http_error`:

```c
/* clang-format off */
#include <string.h>
#include <c_abstract_http/c_abstract_http.h>
/* clang-format on */

static enum c_abstract_http_error custom_add_fd(void *ctx, int fd, int events,
                                                http_loop_cb cb,
                                                void *user_data) {
  if (ctx || fd || events || cb || user_data) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error custom_mod_fd(void *ctx, int fd, int events) {
  if (ctx || fd || events) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error custom_remove_fd(void *ctx, int fd) {
  if (ctx || fd) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error custom_add_timer(void *ctx, long timeout_ms,
                                                   http_timer_cb cb,
                                                   void *user_data,
                                                   int *out_timer_id) {
  if (ctx || timeout_ms || cb || user_data) {
  }
  if (out_timer_id) {
    *out_timer_id = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error custom_cancel_timer(void *ctx, int timer_id) {
  if (ctx || timer_id) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error custom_wakeup(void *ctx) {
  if (ctx) {
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error attach_external_loop(void *host_reactor) {
  struct ModalityEventLoop *loop = NULL;
  struct HttpLoopHooks hooks;
  enum c_abstract_http_error rc;

  memset(&hooks, 0, sizeof(hooks));
  hooks.external_context = host_reactor;
  hooks.add_fd = custom_add_fd;
  hooks.mod_fd = custom_mod_fd;
  hooks.remove_fd = custom_remove_fd;
  hooks.add_timer = custom_add_timer;
  hooks.cancel_timer = custom_cancel_timer;
  hooks.wakeup = custom_wakeup;

  rc = http_loop_init_external(&loop, &hooks);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return http_loop_free(loop);
}
```
