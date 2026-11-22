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
  return u;
}

static void fetchFreeURL(struct url *u) { free(u); }

static FILE *fetchReqHTTP(struct url *u, const char *method, const char *flags,
                          const char *content_type, const char *body) {
  FILE *f;
  (void)u;
  (void)method;
  (void)flags;
  (void)content_type;
  (void)body;

  if (g_mock_fetch_req_fail) {
    fetchLastErrCode =
        g_mock_fetch_err_code ? g_mock_fetch_err_code : FETCH_DOWN;
    return NULL;
  }
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
  {
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
