/**
 * @file mock_fetch.h
 * @brief Internal test-only libfetch C-library stubs for OOM and fault
 * injection.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_FETCH_H
#define C_ABSTRACT_HTTP_TEST_MOCK_FETCH_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include <unistd.h>
#endif
/* clang-format on */

extern int g_mock_fetch_global_init_fail;
extern int g_mock_fetch_context_init_fail;
extern int g_mock_fetch_config_init_fail;
extern int g_mock_fetch_parse_fail;
extern int g_mock_fetch_req_fail;
extern int g_mock_fetch_err_code;
extern int g_mock_fetch_empty_body;
extern int g_mock_fetch_response_init_fail;
extern int g_mock_fetch_response_alloc_fail;
extern int g_mock_fetch_upload_alloc_fail;
extern int g_mock_fetch_upload_realloc_fail;
extern int g_mock_fetch_body_realloc_fail;
extern int g_mock_fetch_loop_wakeup_fail;
extern int g_mock_fetch_stream_pipe;
extern int g_mock_fetch_env_fail;

#if !defined(C_ABSTRACT_HTTP_HAVE_REAL_FETCH)
/**
 * @brief Libfetch URL structure stub.
 */
struct url {
  /** @brief Scheme */
  char scheme[32];
  /** @brief User */
  char user[64];
  /** @brief Password */
  char pwd[64];
  /** @brief Host */
  char host[256];
  /** @brief Port */
  int port;
  /** @brief Document */
  char doc[1024];
  /** @brief Offset */
  size_t offset;
  /** @brief Length */
  size_t length;
};

#define FETCH_OK 0
#define FETCH_TIMEOUT 1
#define FETCH_DOWN 2
#define FETCH_NETWORK 3
#define FETCH_RESOLV 4
#define FETCH_MEMORY 5

static int fetchLastErrCode = 0;
static int fetchTimeout = 0;

static struct url *fetchParseURL(const char *url_str) {
  struct url *u;
  if (strcmp(url_str, "invalid://url") == 0) {
    return NULL;
  }
  if (g_mock_fetch_parse_fail) {
    return NULL;
  }
  u = (struct url *)calloc(1, sizeof(struct url));
  if (u) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strncpy_s(u->doc, sizeof(u->doc), url_str, sizeof(u->doc) - 1);
#else
    strncpy(u->doc, url_str, sizeof(u->doc) - 1);
#endif
    u->doc[sizeof(u->doc) - 1] = '\0';
  }
  return u;
}

static void fetchFreeURL(struct url *u) { free(u); }

#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
static int g_mock_fetch_open_pipe_w[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
#endif

static FILE *fetchReqHTTP(struct url *u, const char *method, const char *flags,
                          const char *content_type, const char *body) {
  FILE *f;
  (void)method;
  (void)flags;
  (void)content_type;
  (void)body;

  if (g_mock_fetch_req_fail) {
    fetchLastErrCode =
        g_mock_fetch_err_code ? g_mock_fetch_err_code : FETCH_DOWN;
    return NULL;
  }
#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
  if (g_mock_fetch_stream_pipe) {
    int pfd[2];
    const char *pmsg = "HTTP/1.1 200 OK\r\nX-Pipe: 1\r\n\r\nPipeStreamPart1";
    if (pipe(pfd) != 0) {
      fetchLastErrCode = FETCH_NETWORK;
      return NULL;
    }
    if (write(pfd[1], pmsg, strlen(pmsg)) < 0) {
      /* ignore mock pipe write status */
    }
    if (g_mock_fetch_stream_pipe == 2) {
      size_t pi;
      for (pi = 0; pi < 8; ++pi) {
        if (g_mock_fetch_open_pipe_w[pi] < 0) {
          g_mock_fetch_open_pipe_w[pi] = pfd[1];
          pfd[1] = -1;
          break;
        }
      }
      if (pfd[1] >= 0) {
        close(pfd[1]);
      }
    } else {
      close(pfd[1]);
    }
    f = fdopen(pfd[0], "rb");
    if (!f) {
      close(pfd[0]);
      fetchLastErrCode = FETCH_MEMORY;
      return NULL;
    }
    return f;
  }
#endif
  if (g_mock_fetch_empty_body) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    if (tmpfile_s(&f) != 0) {
      f = NULL;
    }
#else
    f = tmpfile();
#endif
    return f;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  if (tmpfile_s(&f) != 0) {
    f = NULL;
  }
#else
  f = tmpfile();
#endif
  if (u && strstr(u->doc, "/set-cookie") != NULL) {
    fputs("HTTP/1.1 201 Created\r\nSet-Cookie: fetch_sid=f88; "
          "Path=/\r\nX-Fetch: 1\r\n\r\nCreatedBody",
          f);
  } else if (u && strstr(u->doc, "/partial-http") != NULL) {
    fputs("HTTP/1.1 200 OK without delimiter", f);
  } else if (u && strstr(u->doc, "/204-empty") != NULL) {
    fputs("HTTP/1.1 204 No Content\r\n\r\n", f);
  } else if (u && strstr(u->doc, "/raw-body") != NULL) {
    fputs("PlainRawStreamBody", f);
  } else {
    static const char k_resp[] = {
        'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ', '2', '0', '0', ' ',
        'O', 'K', 13,  10,  13,  10,  'M', 'o', 'c', 'k', ' ', 'r', 'e',
        's', 'p', 'o', 'n', 's', 'e', ' ', 'b', 'o', 'd', 'y', ' ', 'f',
        'r', 'o', 'm', ' ', 'l', 'i', 'b', 'f', 'e', 't', 'c', 'h', 0};
    fputs(k_resp, f);
  }
  rewind(f);
  return f;
}
#endif /* !defined(C_ABSTRACT_HTTP_HAVE_REAL_FETCH) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_FETCH_H */
