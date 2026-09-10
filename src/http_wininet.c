/**
 * @file http_wininet.c
 * @brief WinInet implementation of the Abstract Network Interface.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "c_abstract_http/log.h"
#include <c_abstract_http/http_wininet.h>
#include <cfs/cfs.h>
#include "str.h"

#ifdef _WIN32
#include "win_compat_sym.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <winerror.h>
#include <wininet.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_wininet_open_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_open_request_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_send_request_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_send_request_ex_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_write_file_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_end_request_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_query_info_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_read_file_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_set_option_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_crack_url_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_add_headers_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_status_code(void);
extern int *abstract_http_mock_get_g_mock_wininet_cookie_count(void);
extern int *abstract_http_mock_get_g_mock_wininet_read_chunks(void);
extern int *abstract_http_mock_get_g_mock_wininet_read_chunk_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_res_alloc_fail(void);

#define g_mock_wininet_open_fail                                               \
  (*abstract_http_mock_get_g_mock_wininet_open_fail())
#define g_mock_wininet_connect_fail                                            \
  (*abstract_http_mock_get_g_mock_wininet_connect_fail())
#define g_mock_wininet_open_request_fail                                       \
  (*abstract_http_mock_get_g_mock_wininet_open_request_fail())
#define g_mock_wininet_send_request_fail                                       \
  (*abstract_http_mock_get_g_mock_wininet_send_request_fail())
#define g_mock_wininet_send_request_ex_fail                                    \
  (*abstract_http_mock_get_g_mock_wininet_send_request_ex_fail())
#define g_mock_wininet_write_file_fail                                         \
  (*abstract_http_mock_get_g_mock_wininet_write_file_fail())
#define g_mock_wininet_end_request_fail                                        \
  (*abstract_http_mock_get_g_mock_wininet_end_request_fail())
#define g_mock_wininet_query_info_fail                                         \
  (*abstract_http_mock_get_g_mock_wininet_query_info_fail())
#define g_mock_wininet_read_file_fail                                          \
  (*abstract_http_mock_get_g_mock_wininet_read_file_fail())
#define g_mock_wininet_set_option_fail                                         \
  (*abstract_http_mock_get_g_mock_wininet_set_option_fail())
#define g_mock_wininet_crack_url_fail                                          \
  (*abstract_http_mock_get_g_mock_wininet_crack_url_fail())
#define g_mock_wininet_add_headers_fail                                        \
  (*abstract_http_mock_get_g_mock_wininet_add_headers_fail())
#define g_mock_wininet_context_init_fail                                       \
  (*abstract_http_mock_get_g_mock_wininet_context_init_fail())
#define g_mock_wininet_response_init_fail                                      \
  (*abstract_http_mock_get_g_mock_wininet_response_init_fail())
#define g_mock_wininet_status_code                                             \
  (*abstract_http_mock_get_g_mock_wininet_status_code())
#define g_mock_wininet_cookie_count                                            \
  (*abstract_http_mock_get_g_mock_wininet_cookie_count())
#define g_mock_wininet_read_chunks                                             \
  (*abstract_http_mock_get_g_mock_wininet_read_chunks())
#define g_mock_wininet_read_chunk_alloc_fail                                   \
  (*abstract_http_mock_get_g_mock_wininet_read_chunk_alloc_fail())
#define g_mock_wininet_body_realloc_fail                                       \
  (*abstract_http_mock_get_g_mock_wininet_body_realloc_fail())
#define g_mock_wininet_res_alloc_fail                                          \
  (*abstract_http_mock_get_g_mock_wininet_res_alloc_fail())
#endif

#if !defined(_WIN32)
typedef void *HINTERNET;
typedef unsigned long DWORD;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef DWORD *LPDWORD;
typedef int BOOL;
typedef wchar_t WCHAR;
typedef unsigned short INTERNET_PORT;

#define INTERNET_OPEN_TYPE_PRECONFIG 0
#define INTERNET_SERVICE_HTTP 3
#define INTERNET_FLAG_RELOAD 0x80000000
#define INTERNET_FLAG_NO_CACHE_WRITE 0x04000000
#define INTERNET_FLAG_SECURE 0x00800000
#define INTERNET_FLAG_IGNORE_CERT_CN_INVALID 0x00001000
#define INTERNET_FLAG_IGNORE_CERT_DATE_INVALID 0x00002000
#define INTERNET_FLAG_NO_AUTO_REDIRECT 0x00200000
#define INTERNET_OPTION_CONNECT_TIMEOUT 2
#define INTERNET_OPTION_SEND_TIMEOUT 5
#define INTERNET_OPTION_RECEIVE_TIMEOUT 6
#define INTERNET_OPTION_PROXY_USERNAME 43
#define INTERNET_OPTION_PROXY_PASSWORD 44
#define HTTP_ADDREQ_FLAG_ADD 0x20000000
#define HTTP_ADDREQ_FLAG_REPLACE 0x80000000
#define HTTP_QUERY_STATUS_CODE 19
#define HTTP_QUERY_FLAG_NUMBER 0x20000000
#define HTTP_QUERY_SET_COOKIE 43
#define ERROR_INSUFFICIENT_BUFFER 122
#define INTERNET_SCHEME_HTTP 1
#define INTERNET_SCHEME_HTTPS 2

/** @brief Simulated URL_COMPONENTSW */
typedef struct {
  DWORD dwStructSize;
  wchar_t *lpszScheme;
  DWORD dwSchemeLength;
  int nScheme;
  wchar_t *lpszHostName;
  DWORD dwHostNameLength;
  INTERNET_PORT nPort;
  wchar_t *lpszUserName;
  DWORD dwUserNameLength;
  wchar_t *lpszPassword;
  DWORD dwPasswordLength;
  wchar_t *lpszUrlPath;
  DWORD dwUrlPathLength;
  wchar_t *lpszExtraInfo;
  DWORD dwExtraInfoLength;
} URL_COMPONENTSW;

/** @brief Simulated INTERNET_BUFFERSW */
typedef struct {
  DWORD dwStructSize;
  void *Next;
  const char *lpcszHeader;
  DWORD dwHeadersLength;
  DWORD dwHeadersTotal;
  void *lpvBuffer;
  DWORD dwBufferLength;
  DWORD dwBufferTotal;
  DWORD dwOffsetLow;
  DWORD dwOffsetHigh;
} INTERNET_BUFFERSW;

static DWORD s_mock_wininet_last_error = 0;

static DWORD GetLastError(void) { return s_mock_wininet_last_error; }

static void SetLastError(DWORD dwErrCode) {
  s_mock_wininet_last_error = dwErrCode;
}

static HINTERNET InternetOpenW(const wchar_t *lpszAgent, DWORD dwAccessType,
                               const wchar_t *lpszProxy,
                               const wchar_t *lpszProxyBypass, DWORD dwFlags) {
  (void)lpszAgent;
  (void)dwAccessType;
  (void)lpszProxy;
  (void)lpszProxyBypass;
  (void)dwFlags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_open_fail) {
    return NULL;
  }
#endif
  return (HINTERNET)(size_t)201;
}

static BOOL InternetCloseHandle(HINTERNET hInternet) {
  (void)hInternet;
  return 1;
}

static BOOL InternetSetOption(HINTERNET hInternet, DWORD dwOption,
                              LPVOID lpBuffer, DWORD dwBufferLength) {
  (void)hInternet;
  (void)dwOption;
  (void)lpBuffer;
  (void)dwBufferLength;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_set_option_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL InternetSetOptionA(HINTERNET hInternet, DWORD dwOption,
                               LPVOID lpBuffer, DWORD dwBufferLength) {
  (void)hInternet;
  (void)dwOption;
  (void)lpBuffer;
  (void)dwBufferLength;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_set_option_fail == 1) {
    return 0;
  }
  if (g_mock_wininet_set_option_fail == 2 &&
      dwOption == INTERNET_OPTION_PROXY_PASSWORD) {
    return 0;
  }
#endif
  return 1;
}

static BOOL InternetCrackUrlW(const wchar_t *lpszUrl, DWORD dwUrlLength,
                              DWORD dwFlags, URL_COMPONENTSW *lpUrlComponents) {
  const wchar_t *p;
  const wchar_t *host_start;
  const wchar_t *host_end;
  const wchar_t *path_start;
  size_t host_len;
  size_t path_len;
  (void)dwUrlLength;
  (void)dwFlags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_crack_url_fail) {
    return 0;
  }
#endif
  p = wcsstr(lpszUrl, L"://");
  if (!p) {
    return 0;
  }
  if (p - lpszUrl == 5 && wcsncmp(lpszUrl, L"https", 5) == 0) {
    lpUrlComponents->nScheme = INTERNET_SCHEME_HTTPS;
    lpUrlComponents->nPort = 443;
  } else {
    lpUrlComponents->nScheme = INTERNET_SCHEME_HTTP;
    lpUrlComponents->nPort = 80;
  }
  host_start = p + 3;
  p = host_start;
  while (*p && *p != L':' && *p != L'/') {
    p++;
  }
  host_end = p;
  if (*p == L':') {
    unsigned int port_val = 0;
    p++;
    while (*p >= L'0' && *p <= L'9') {
      port_val = port_val * 10 + (unsigned int)(*p - L'0');
      p++;
    }
    lpUrlComponents->nPort = (INTERNET_PORT)port_val;
  }
  host_len = (size_t)(host_end - host_start);
  if (lpUrlComponents->lpszHostName &&
      lpUrlComponents->dwHostNameLength > host_len) {
    memcpy(lpUrlComponents->lpszHostName, host_start,
           host_len * sizeof(wchar_t));
    lpUrlComponents->lpszHostName[host_len] = L'\0';
    lpUrlComponents->dwHostNameLength = (DWORD)host_len;
  }
  if (*p == L'/') {
    path_start = p;
    path_len = wcslen(path_start);
    if (lpUrlComponents->lpszUrlPath &&
        lpUrlComponents->dwUrlPathLength > path_len) {
      memcpy(lpUrlComponents->lpszUrlPath, path_start,
             path_len * sizeof(wchar_t));
      lpUrlComponents->lpszUrlPath[path_len] = L'\0';
      lpUrlComponents->dwUrlPathLength = (DWORD)path_len;
    }
  } else {
    if (lpUrlComponents->lpszUrlPath && lpUrlComponents->dwUrlPathLength >= 2) {
      lpUrlComponents->lpszUrlPath[0] = L'/';
      lpUrlComponents->lpszUrlPath[1] = L'\0';
      lpUrlComponents->dwUrlPathLength = 1;
    }
  }
  return 1;
}

static HINTERNET InternetConnectW(HINTERNET hInternet,
                                  const wchar_t *lpszServerName,
                                  INTERNET_PORT nServerPort,
                                  const wchar_t *lpszUsername,
                                  const wchar_t *lpszPassword, DWORD dwService,
                                  DWORD dwFlags, DWORD dwContext) {
  (void)hInternet;
  (void)lpszServerName;
  (void)nServerPort;
  (void)lpszUsername;
  (void)lpszPassword;
  (void)dwService;
  (void)dwFlags;
  (void)dwContext;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_connect_fail) {
    return NULL;
  }
#endif
  return (HINTERNET)(size_t)202;
}

static HINTERNET HttpOpenRequestW(HINTERNET hConnect, const wchar_t *lpszVerb,
                                  const wchar_t *lpszObjectName,
                                  const wchar_t *lpszVersion,
                                  const wchar_t *lpszReferrer,
                                  const wchar_t **lplpszAcceptTypes,
                                  DWORD dwFlags, DWORD dwContext) {
  (void)hConnect;
  (void)lpszVerb;
  (void)lpszObjectName;
  (void)lpszVersion;
  (void)lpszReferrer;
  (void)lplpszAcceptTypes;
  (void)dwFlags;
  (void)dwContext;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_open_request_fail) {
    return NULL;
  }
#endif
  return (HINTERNET)(size_t)203;
}

static BOOL HttpAddRequestHeadersW(HINTERNET hRequest,
                                   const wchar_t *lpszHeaders,
                                   DWORD dwHeadersLength, DWORD dwModifiers) {
  (void)hRequest;
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)dwModifiers;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_add_headers_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL HttpSendRequestExW(HINTERNET hRequest,
                               INTERNET_BUFFERSW *lpBuffersIn,
                               INTERNET_BUFFERSW *lpBuffersOut, DWORD dwFlags,
                               DWORD dwContext) {
  (void)hRequest;
  (void)lpBuffersIn;
  (void)lpBuffersOut;
  (void)dwFlags;
  (void)dwContext;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_send_request_ex_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL InternetWriteFile(HINTERNET hFile, LPCVOID lpBuffer,
                              DWORD dwNumberOfBytesToWrite,
                              LPDWORD lpdwNumberOfBytesWritten) {
  (void)hFile;
  (void)lpBuffer;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_write_file_fail) {
    return 0;
  }
#endif
  if (lpdwNumberOfBytesWritten) {
    *lpdwNumberOfBytesWritten = dwNumberOfBytesToWrite;
  }
  return 1;
}

static BOOL HttpEndRequestW(HINTERNET hRequest, INTERNET_BUFFERSW *lpBuffersOut,
                            DWORD dwFlags, DWORD dwContext) {
  (void)hRequest;
  (void)lpBuffersOut;
  (void)dwFlags;
  (void)dwContext;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_end_request_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL HttpSendRequestW(HINTERNET hRequest, const wchar_t *lpszHeaders,
                             DWORD dwHeadersLength, LPVOID lpOptional,
                             DWORD dwOptionalLength) {
  (void)hRequest;
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)lpOptional;
  (void)dwOptionalLength;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_send_request_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL HttpQueryInfoW(HINTERNET hRequest, DWORD dwInfoLevel,
                           LPVOID lpBuffer, LPDWORD lpdwBufferLength,
                           LPDWORD lpdwIndex) {
  (void)hRequest;
  (void)lpdwIndex;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_query_info_fail) {
    return 0;
  }
#endif
  if ((dwInfoLevel & 0xFFFF) == HTTP_QUERY_STATUS_CODE) {
    if (lpBuffer) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      DWORD code =
          g_mock_wininet_status_code ? (DWORD)g_mock_wininet_status_code : 200;
#else
      DWORD code = 200;
#endif
      memcpy(lpBuffer, &code, sizeof(DWORD));
    }
    if (lpdwBufferLength) {
      *lpdwBufferLength = sizeof(DWORD);
    }
    return 1;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_cookie_count > 0) {
    const wchar_t cookie_val[] = L"mock_cookie=456; path=/";
    DWORD needed_bytes = (DWORD)(sizeof(cookie_val));
    if (!lpBuffer) {
      if (lpdwBufferLength) {
        *lpdwBufferLength = needed_bytes;
      }
      SetLastError(ERROR_INSUFFICIENT_BUFFER);
      return 0;
    }
    memcpy(lpBuffer, cookie_val, needed_bytes);
    if (lpdwBufferLength) {
      *lpdwBufferLength = needed_bytes;
    }
    g_mock_wininet_cookie_count--;
    SetLastError(0);
    return 1;
  }
#endif
  SetLastError(0);
  return 0;
}

static BOOL InternetReadFile(HINTERNET hFile, LPVOID lpBuffer,
                             DWORD dwNumberOfBytesToRead,
                             LPDWORD lpdwNumberOfBytesRead) {
  (void)hFile;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_read_file_fail) {
    return 0;
  }
  if (g_mock_wininet_read_chunks > 0) {
    g_mock_wininet_read_chunks--;
    if (lpBuffer && dwNumberOfBytesToRead > 0) {
      memset(lpBuffer, 'I', 10);
    }
    if (lpdwNumberOfBytesRead) {
      *lpdwNumberOfBytesRead = 10;
    }
    return 1;
  }
#endif
  (void)lpBuffer;
  (void)dwNumberOfBytesToRead;
  if (lpdwNumberOfBytesRead) {
    *lpdwNumberOfBytesRead = 0;
  }
  return 1;
}
#endif

static enum c_abstract_http_error
ascii_to_wide(const char *s, wchar_t *ws, size_t buf_cap, size_t *out_len) {
  size_t i = 0;
  while (s[i] != '\0' && i + 1 < buf_cap) {
    ws[i] = (wchar_t)(unsigned char)s[i];
    i++;
  }
  ws[i] = L'\0';
  *out_len = i;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
wide_to_ascii(const wchar_t *ws, char *s, size_t buf_cap, size_t *out_len) {
  size_t i = 0;
  while (ws[i] != L'\0' && i + 1 < buf_cap) {
    s[i] = (char)(ws[i] & 0xFF);
    i++;
  }
  s[i] = '\0';
  *out_len = i;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief hInternet handle */
  HINTERNET hInternet;
  /** @brief security flags */
  DWORD security_flags;
  /** @brief proxy username */
  char *proxy_username;
  /** @brief proxy password */
  char *proxy_password;
  /** @brief Cookie jar */
  struct HttpCookieJar *cookie_jar;
  /** @brief HTTP configuration copy */
  struct HttpConfig config;
};

static void safe_close_handle(HINTERNET *h) {
  if (h && *h) {
    InternetCloseHandle(*h);
    *h = NULL;
  }
}

static enum c_abstract_http_error method_to_wide(enum HttpMethod method,
                                                 const wchar_t **out) {
  switch (method) {
  case HTTP_GET:
    *out = L"GET";
    break;
  case HTTP_POST:
    *out = L"POST";
    break;
  case HTTP_PUT:
    *out = L"PUT";
    break;
  case HTTP_DELETE:
    *out = L"DELETE";
    break;
  case HTTP_HEAD:
    *out = L"HEAD";
    break;
  case HTTP_PATCH:
    *out = L"PATCH";
    break;
  case HTTP_OPTIONS:
    *out = L"OPTIONS";
    break;
  case HTTP_TRACE:
    *out = L"TRACE";
    break;
  case HTTP_QUERY:
    *out = L"QUERY";
    break;
  case HTTP_CONNECT:
    *out = L"CONNECT";
    break;
  default:
    *out = L"GET";
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
headers_to_wide_block(const struct HttpHeaders *headers, wchar_t **out) {
  size_t i;
  size_t total_wchars = 0;
  wchar_t *buf;
  wchar_t *p;

  *out = NULL;
  if (!headers || headers->count == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  for (i = 0; i < headers->count; ++i) {
    total_wchars += strlen(headers->headers[i].key);
    total_wchars += strlen(headers->headers[i].value);
    total_wchars += 4;
  }
  total_wchars += 1;

  buf = (wchar_t *)calloc(total_wchars, sizeof(wchar_t));
  if (!buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  p = buf;
  for (i = 0; i < headers->count; ++i) {
    size_t written = 0;
    ascii_to_wide(headers->headers[i].key, p, total_wchars - (size_t)(p - buf),
                  &written);
    p += written;
    p[0] = L':';
    p[1] = L' ';
    p += 2;

    ascii_to_wide(headers->headers[i].value, p,
                  total_wchars - (size_t)(p - buf), &written);
    p += written;
    p[0] = L'\r';
    p[1] = L'\n';
    p += 2;
  }
  *p = L'\0';
  *out = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize the global WinInet environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_wininet_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Cleanup the global WinInet environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_wininet_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Create and initialize a new WinInet transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_wininet_context_init(struct HttpTransportContext **ctx) {
  HINTERNET hInternet;
  DWORD flags = 0;
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_wininet_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_wininet_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  hInternet = InternetOpenW(L"c_abstract_http/WinInet",
                            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, flags);

  if (!hInternet) {
    LOG_DEBUG("http_wininet_context_init: Error InternetOpenW failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_wininet_context_init: Error ENOMEM");
    InternetCloseHandle(hInternet);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_wininet_context_init: Error http_config_init failed with %d",
        (int)rc);
    InternetCloseHandle(hInternet);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->hInternet = hInternet;
  (*ctx)->security_flags = 0;
  (*ctx)->proxy_username = NULL;
  (*ctx)->proxy_password = NULL;
  (*ctx)->cookie_jar = NULL;

  LOG_DEBUG("http_wininet_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free the WinInet transport context and release associated resources.
 *
 * @param[in] ctx Pointer to the context to free. Satisfies NULL-safety.
 */
void http_wininet_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wininet_context_free: Entering");
  if (ctx) {
    if (ctx->hInternet) {
      InternetCloseHandle(ctx->hInternet);
      ctx->hInternet = NULL;
    }
    if (ctx->proxy_username) {
      free(ctx->proxy_username);
      ctx->proxy_username = NULL;
    }
    if (ctx->proxy_password) {
      free(ctx->proxy_password);
      ctx->proxy_password = NULL;
    }
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_wininet_context_free: Exiting");
}

/**
 * @brief Apply configuration settings to an active WinInet context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config Configuration options to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_wininet_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
  DWORD timeout_connect, timeout_send, timeout_recv;
  LOG_DEBUG("http_wininet_config_apply: Entering");
  if (!ctx) {
    LOG_DEBUG("http_wininet_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!config) {
    LOG_DEBUG("http_wininet_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  timeout_connect = (config->connect_timeout_ms > 0)
                        ? (DWORD)config->connect_timeout_ms
                        : (DWORD)config->timeout_ms;
  timeout_send = (config->write_timeout_ms > 0)
                     ? (DWORD)config->write_timeout_ms
                     : (DWORD)config->timeout_ms;
  timeout_recv = (config->read_timeout_ms > 0) ? (DWORD)config->read_timeout_ms
                                               : (DWORD)config->timeout_ms;

  if (timeout_connect > 0) {
    if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_CONNECT_TIMEOUT,
                           &timeout_connect, sizeof(timeout_connect))) {
      LOG_DEBUG(
          "http_wininet_config_apply: Error InternetSetOption connect failed");
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  if (timeout_send > 0) {
    if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_SEND_TIMEOUT,
                           &timeout_send, sizeof(timeout_send))) {
      LOG_DEBUG(
          "http_wininet_config_apply: Error InternetSetOption send failed");
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  if (timeout_recv > 0) {
    if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                           &timeout_recv, sizeof(timeout_recv))) {
      LOG_DEBUG(
          "http_wininet_config_apply: Error InternetSetOption recv failed");
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  ctx->security_flags = 0;
  if (!config->verify_peer) {
    ctx->security_flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
    ctx->security_flags |= INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
  }
  if (!config->verify_host) {
    ctx->security_flags |= INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
  }
  if (!config->follow_redirects) {
    ctx->security_flags |= INTERNET_FLAG_NO_AUTO_REDIRECT;
  }

  ctx->cookie_jar = config->cookie_jar;

  if (ctx->proxy_username) {
    free(ctx->proxy_username);
    ctx->proxy_username = NULL;
  }
  if (ctx->proxy_password) {
    free(ctx->proxy_password);
    ctx->proxy_password = NULL;
  }
  if (config->proxy_username) {
    char *tmp = NULL;
    enum c_abstract_http_error rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wininet_context_init_fail == 2) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_username, &tmp);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    ctx->proxy_username = tmp;
  }
  if (config->proxy_password) {
    char *tmp = NULL;
    enum c_abstract_http_error rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_wininet_context_init_fail == 3) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else
#endif
    {
      rc = c_abstract_http_strdup(config->proxy_password, &tmp);
    }
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    ctx->proxy_password = tmp;
  }

  LOG_DEBUG("http_wininet_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send an HTTP request synchronously using WinInet.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The HTTP request parameters.
 * @param[out] res Double pointer to receive the allocated HTTP response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_wininet_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
  HINTERNET hConnect = NULL;
  HINTERNET hRequest = NULL;
  URL_COMPONENTSW urlComp;
  wchar_t *wUrl = NULL;
  wchar_t *wHost = NULL;
  wchar_t *wPath = NULL;
  wchar_t *wHeaders = NULL;
  size_t wLen = 0;
  const wchar_t *wmethod = NULL;
  DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);

  /* Body read logic */
  char *bodyBuf = NULL;
  char *readChunk = NULL;
  size_t bodySize = 0;
  DWORD bytesRead = 0;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_wininet_send: Entering");

  if (!ctx || !ctx->hInternet || !req || !res || !req->url) {
    LOG_DEBUG("http_wininet_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (req->parts.count > 0 && !req->body) {
    LOG_DEBUG("http_wininet_send: Error EINVAL (multipart not flattened)");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  /* 1. Convert URL to Wide */
  {
    size_t cap = strlen(req->url) + 1;
    wUrl = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!wUrl) {
      LOG_DEBUG("http_wininet_send: Error ENOMEM for wUrl");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    ascii_to_wide(req->url, wUrl, cap, &wLen);
  }

  /* 2. Crack URL */
  memset(&urlComp, 0, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);

  wHost = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!wHost) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating host buffer");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
  wPath = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!wPath) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating path buffer");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  urlComp.lpszHostName = wHost;
  urlComp.dwHostNameLength = (DWORD)wLen + 1;
  urlComp.lpszUrlPath = wPath;
  urlComp.dwUrlPathLength = (DWORD)wLen + 1;

  if (!InternetCrackUrlW(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    LOG_DEBUG("http_wininet_send: Error InternetCrackUrlW failed");
    rc = C_ABSTRACT_HTTP_ERR_INVAL;
    goto cleanup;
  }

  /* 3. Connect */
  hConnect = InternetConnectW(ctx->hInternet, urlComp.lpszHostName,
                              (INTERNET_PORT)urlComp.nPort, NULL, NULL,
                              INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect) {
    LOG_DEBUG("http_wininet_send: Error InternetConnectW failed");
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  /* Apply Proxy Credentials if configured */
  if (ctx->proxy_username && ctx->proxy_password) {
    if (!InternetSetOptionA(hConnect, INTERNET_OPTION_PROXY_USERNAME,
                            ctx->proxy_username,
                            (DWORD)strlen(ctx->proxy_username))) {
      LOG_DEBUG(
          "http_wininet_send: Error InternetSetOptionA proxy username failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
    if (!InternetSetOptionA(hConnect, INTERNET_OPTION_PROXY_PASSWORD,
                            ctx->proxy_password,
                            (DWORD)strlen(ctx->proxy_password))) {
      LOG_DEBUG(
          "http_wininet_send: Error InternetSetOptionA proxy password failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
  }

  /* 4. Open Request */
  if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
    dwFlags |= INTERNET_FLAG_SECURE;
    dwFlags |= ctx->security_flags;
  }

  method_to_wide(req->method, &wmethod);

  hRequest = HttpOpenRequestW(hConnect, wmethod, urlComp.lpszUrlPath, NULL,
                              NULL, NULL, dwFlags, 0);
  if (!hRequest) {
    LOG_DEBUG("http_wininet_send: Error HttpOpenRequestW failed");
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  /* Set cookies from jar before headers */
  if (ctx->cookie_jar && ctx->cookie_jar->count > 0) {
    size_t i;
    for (i = 0; i < ctx->cookie_jar->count; ++i) {
      char cbuf[4096];
      wchar_t wcbuf[4096];
      size_t written = 0;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(cbuf, sizeof(cbuf), "Cookie: %s=%s\r\n",
                ctx->cookie_jar->cookies[i].name,
                ctx->cookie_jar->cookies[i].value);
#else
      sprintf(cbuf, "Cookie: %s=%s\r\n", ctx->cookie_jar->cookies[i].name,
              ctx->cookie_jar->cookies[i].value);
#endif

      ascii_to_wide(cbuf, wcbuf, 4096, &written);
      HttpAddRequestHeadersW(hRequest, wcbuf, (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD);
    }
  }

  /* 5. Headers */
  rc = headers_to_wide_block(&req->headers, &wHeaders);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wininet_send: Error in headers_to_wide_block");
    goto cleanup;
  }
  if (wHeaders) {
    if (!HttpAddRequestHeadersW(hRequest, wHeaders, (DWORD)-1L,
                                HTTP_ADDREQ_FLAG_ADD |
                                    HTTP_ADDREQ_FLAG_REPLACE)) {
      LOG_DEBUG("http_wininet_send: Error HttpAddRequestHeadersW failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
  }

  /* 6. Send */
  if (req->read_chunk) {
    INTERNET_BUFFERSW ib;
    memset(&ib, 0, sizeof(ib));
    ib.dwStructSize = sizeof(ib);
    ib.dwBufferTotal = (DWORD)req->expected_body_len;

    if (!HttpSendRequestExW(hRequest, &ib, NULL, 0, 0)) {
      LOG_DEBUG("http_wininet_send: Error HttpSendRequestExW failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    for (;;) {
      char chunkBuf[8192];
      size_t out_read = 0;
      DWORD dwWritten = 0;
      int cb_rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                                  sizeof(chunkBuf), &out_read);
      if (cb_rc != 0) {
        LOG_DEBUG("http_wininet_send: Error read_chunk failed with %d", cb_rc);
        rc = (enum c_abstract_http_error)cb_rc;
        goto cleanup;
      }
      if (out_read == 0) {
        break; /* EOF */
      }

      if (!InternetWriteFile(hRequest, chunkBuf, (DWORD)out_read, &dwWritten)) {
        LOG_DEBUG("http_wininet_send: Error InternetWriteFile failed");
        rc = C_ABSTRACT_HTTP_ERR_IO;
        goto cleanup;
      }
    }

    if (!HttpEndRequestW(hRequest, NULL, 0, 0)) {
      LOG_DEBUG("http_wininet_send: Error HttpEndRequestW failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
  } else {
    if (!HttpSendRequestW(hRequest, NULL, 0, req->body, (DWORD)req->body_len)) {
      LOG_DEBUG("http_wininet_send: Error HttpSendRequestW failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
  }

  /* 7. Query Info (Status Code) */
  if (!HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                      &dwStatusCode, &dwSize, NULL)) {
    LOG_DEBUG("http_wininet_send: Error HttpQueryInfoW (status_code) failed");
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  /* Extract Set-Cookie into Jar */
  if (ctx->cookie_jar) {
    DWORD dwIndex = 0;
    DWORD cbCookie = 0;

    HttpQueryInfoW(hRequest, HTTP_QUERY_SET_COOKIE, NULL, &cbCookie, &dwIndex);

    while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      wchar_t *pwszCookie = (wchar_t *)malloc(cbCookie);
      if (pwszCookie) {
        if (HttpQueryInfoW(hRequest, HTTP_QUERY_SET_COOKIE, pwszCookie,
                           &cbCookie, &dwIndex)) {
          char cbuf[4096];
          size_t cwritten = 0;
          wide_to_ascii(pwszCookie, cbuf, sizeof(cbuf), &cwritten);
          {
            char *eq = strchr(cbuf, '=');
            if (eq) {
              const char *name = cbuf;
              const char *val = eq + 1;
              char *semi = strchr(val, ';');
              *eq = '\0';
              if (semi) {
                *semi = '\0';
              }

              {
                enum c_abstract_http_error rc_cookie =
                    http_cookie_jar_set(ctx->cookie_jar, name, val);
                (void)rc_cookie;
              }
            }
          }
        }
        free(pwszCookie);
      }
      cbCookie = 0;
      HttpQueryInfoW(hRequest, HTTP_QUERY_SET_COOKIE, NULL, &cbCookie,
                     &dwIndex);
    }
  }

  /* 8. Read Response Body */
  readChunk = (char *)malloc(4096);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_read_chunk_alloc_fail) {
    if (readChunk) {
      free(readChunk);
      readChunk = NULL;
    }
  }
#endif
  if (!readChunk) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating readChunk");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  for (;;) {
    if (!InternetReadFile(hRequest, readChunk, 4096, &bytesRead)) {
      LOG_DEBUG("http_wininet_send: Error InternetReadFile failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
    if (bytesRead == 0) {
      break;
    }

    if (req->on_chunk) {
      int cb_rc = req->on_chunk(req->on_chunk_user_data, readChunk, bytesRead);
      if (cb_rc != 0) {
        LOG_DEBUG("http_wininet_send: Error on_chunk failed %d", cb_rc);
        rc = (enum c_abstract_http_error)cb_rc;
        goto cleanup;
      }
    } else {
      char *new_buf = (char *)realloc(bodyBuf, bodySize + bytesRead + 1);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_wininet_body_realloc_fail) {
        if (new_buf) {
          free(new_buf);
          new_buf = NULL;
        }
      }
#endif
      if (!new_buf) {
        LOG_DEBUG("http_wininet_send: Error ENOMEM reallocating body");
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        goto cleanup;
      }
      bodyBuf = new_buf;
      memcpy(bodyBuf + bodySize, readChunk, bytesRead);
      bodySize += bytesRead;
      bodyBuf[bodySize] = '\0';
    }
  }

  /* 9. Construct Response */
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_res_alloc_fail) {
    if (*res) {
      free(*res);
      *res = NULL;
    }
  }
#endif
  if (!*res) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  rc = http_response_init(*res);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wininet_send: Error http_response_init failed with %d",
              (int)rc);
    free(*res);
    *res = NULL;
    goto cleanup;
  }
  (*res)->status_code = (int)dwStatusCode;
  (*res)->body = bodyBuf;
  (*res)->body_len = bodySize;
  bodyBuf = NULL;

cleanup:
  safe_close_handle(&hRequest);
  safe_close_handle(&hConnect);
  if (bodyBuf) {
    free(bodyBuf);
  }
  if (readChunk) {
    free(readChunk);
  }
  if (wUrl) {
    free(wUrl);
  }
  if (wHost) {
    free(wHost);
  }
  if (wPath) {
    free(wPath);
  }
  if (wHeaders) {
    free(wHeaders);
  }

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wininet_send: Success");
  } else {
    LOG_DEBUG("http_wininet_send: Error returning %d", (int)rc);
  }
  return rc;
}
