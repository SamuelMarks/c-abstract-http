/**
 * @file http_winhttp.c
 * @brief WinHTTP implementation of the Abstract Network Interface.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "c_abstract_http/log.h"
#include <c_abstract_http/event_loop.h>
#include <cfs/cfs.h>
#include <c_abstract_http/http_winhttp.h>
#include "str.h"

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
#include "win_compat_sym.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <winerror.h>
#include <winhttp.h>
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int *abstract_http_mock_get_g_mock_winhttp_open_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_open_request_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_send_request_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_write_data_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_receive_response_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_query_headers_fail(void);
extern int *
abstract_http_mock_get_g_mock_winhttp_query_data_available_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_read_data_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_set_timeouts_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_set_option_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_crack_url_fail(void);
extern int *
abstract_http_mock_get_g_mock_winhttp_add_request_headers_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_queue_work_item_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_status_code(void);
extern int *abstract_http_mock_get_g_mock_winhttp_cookie_count(void);
extern int *abstract_http_mock_get_g_mock_winhttp_read_chunks(void);
extern int *abstract_http_mock_get_g_mock_winhttp_total_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_read_buf_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_res_alloc_fail(void);

#define g_mock_winhttp_open_fail                                               \
  (*abstract_http_mock_get_g_mock_winhttp_open_fail())
#define g_mock_winhttp_connect_fail                                            \
  (*abstract_http_mock_get_g_mock_winhttp_connect_fail())
#define g_mock_winhttp_open_request_fail                                       \
  (*abstract_http_mock_get_g_mock_winhttp_open_request_fail())
#define g_mock_winhttp_send_request_fail                                       \
  (*abstract_http_mock_get_g_mock_winhttp_send_request_fail())
#define g_mock_winhttp_write_data_fail                                         \
  (*abstract_http_mock_get_g_mock_winhttp_write_data_fail())
#define g_mock_winhttp_receive_response_fail                                   \
  (*abstract_http_mock_get_g_mock_winhttp_receive_response_fail())
#define g_mock_winhttp_query_headers_fail                                      \
  (*abstract_http_mock_get_g_mock_winhttp_query_headers_fail())
#define g_mock_winhttp_query_data_available_fail                               \
  (*abstract_http_mock_get_g_mock_winhttp_query_data_available_fail())
#define g_mock_winhttp_read_data_fail                                          \
  (*abstract_http_mock_get_g_mock_winhttp_read_data_fail())
#define g_mock_winhttp_set_timeouts_fail                                       \
  (*abstract_http_mock_get_g_mock_winhttp_set_timeouts_fail())
#define g_mock_winhttp_set_option_fail                                         \
  (*abstract_http_mock_get_g_mock_winhttp_set_option_fail())
#define g_mock_winhttp_crack_url_fail                                          \
  (*abstract_http_mock_get_g_mock_winhttp_crack_url_fail())
#define g_mock_winhttp_add_request_headers_fail                                \
  (*abstract_http_mock_get_g_mock_winhttp_add_request_headers_fail())
#define g_mock_winhttp_queue_work_item_fail                                    \
  (*abstract_http_mock_get_g_mock_winhttp_queue_work_item_fail())
#define g_mock_winhttp_context_init_fail                                       \
  (*abstract_http_mock_get_g_mock_winhttp_context_init_fail())
#define g_mock_winhttp_response_init_fail                                      \
  (*abstract_http_mock_get_g_mock_winhttp_response_init_fail())
#define g_mock_winhttp_status_code                                             \
  (*abstract_http_mock_get_g_mock_winhttp_status_code())
#define g_mock_winhttp_cookie_count                                            \
  (*abstract_http_mock_get_g_mock_winhttp_cookie_count())
#define g_mock_winhttp_read_chunks                                             \
  (*abstract_http_mock_get_g_mock_winhttp_read_chunks())
#define g_mock_winhttp_total_body_realloc_fail                                 \
  (*abstract_http_mock_get_g_mock_winhttp_total_body_realloc_fail())
#define g_mock_winhttp_read_buf_alloc_fail                                     \
  (*abstract_http_mock_get_g_mock_winhttp_read_buf_alloc_fail())
#define g_mock_winhttp_res_alloc_fail                                          \
  (*abstract_http_mock_get_g_mock_winhttp_res_alloc_fail())
#endif

#if !defined(_WIN32) || (defined(_MSC_VER) && _MSC_VER < 1600)
typedef void *HINTERNET;
typedef unsigned long DWORD;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef DWORD *LPDWORD;
typedef void *PVOID;
typedef unsigned long ULONG;
typedef int BOOL;
typedef wchar_t WCHAR;
typedef unsigned short INTERNET_PORT;

#ifndef WINAPI
#define WINAPI
#endif

typedef DWORD(WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);

#ifndef WT_EXECUTEDEFAULT
#define WT_EXECUTEDEFAULT 0x00000000
#endif

#define WINHTTP_ACCESS_TYPE_NO_PROXY 1
#define WINHTTP_ACCESS_TYPE_DEFAULT_PROXY 0
#define WINHTTP_ACCESS_TYPE_NAMED_PROXY 3
#define WINHTTP_NO_PROXY_NAME NULL
#define WINHTTP_NO_PROXY_BYPASS NULL
#define WINHTTP_NO_REFERER NULL
#define WINHTTP_DEFAULT_ACCEPT_TYPES NULL
#define WINHTTP_FLAG_SECURE 0x00800000
#define WINHTTP_OPTION_SECURITY_FLAGS 31
#define WINHTTP_OPTION_DISABLE_FEATURE 63
#define WINHTTP_DISABLE_REDIRECTS 0x00000002
#define SECURITY_FLAG_IGNORE_UNKNOWN_CA 0x00000100
#define SECURITY_FLAG_IGNORE_CERT_DATE_INVALID 0x00002000
#define SECURITY_FLAG_IGNORE_CERT_CN_INVALID 0x00001000
#define WINHTTP_OPTION_PROXY 38
#define WINHTTP_OPTION_PROXY_USERNAME 39
#define WINHTTP_OPTION_PROXY_PASSWORD 40
#define WINHTTP_ADDREQ_FLAG_ADD 0x20000000
#define WINHTTP_NO_ADDITIONAL_HEADERS NULL
#define WINHTTP_NO_REQUEST_DATA NULL
#define WINHTTP_NO_OUTPUT_BUFFER NULL
#define WINHTTP_NO_HEADER_INDEX NULL
#define WINHTTP_HEADER_NAME_BY_INDEX NULL
#define WINHTTP_QUERY_STATUS_CODE 19
#define WINHTTP_QUERY_FLAG_NUMBER 0x20000000
#define WINHTTP_QUERY_SET_COOKIE 43
#define ERROR_INSUFFICIENT_BUFFER 122
#define INTERNET_SCHEME_HTTP 1
#define INTERNET_SCHEME_HTTPS 2

/** @brief Simulated WINHTTP_PROXY_INFO */
typedef struct {
  DWORD dwAccessType;
  wchar_t *lpszProxy;
  wchar_t *lpszProxyBypass;
} WINHTTP_PROXY_INFO;

/** @brief Simulated URL_COMPONENTS */
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
} URL_COMPONENTS;

static DWORD s_mock_winhttp_last_error = 0;

static DWORD GetLastError(void) { return s_mock_winhttp_last_error; }

static void SetLastError(DWORD dwErrCode) {
  s_mock_winhttp_last_error = dwErrCode;
}

static HINTERNET WinHttpOpen(const wchar_t *pszAgentW, DWORD dwAccessType,
                             const wchar_t *pszProxyW,
                             const wchar_t *pszProxyBypassW, DWORD dwFlags) {
  (void)pszAgentW;
  (void)dwAccessType;
  (void)pszProxyW;
  (void)pszProxyBypassW;
  (void)dwFlags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_open_fail) {
    return NULL;
  }
#endif
  return (HINTERNET)(size_t)101;
}

static BOOL WinHttpCloseHandle(HINTERNET hInternet) {
  (void)hInternet;
  return 1;
}

static BOOL WinHttpSetTimeouts(HINTERNET hInternet, int nResolveTimeout,
                               int nConnectTimeout, int nSendTimeout,
                               int nReceiveTimeout) {
  (void)hInternet;
  (void)nResolveTimeout;
  (void)nConnectTimeout;
  (void)nSendTimeout;
  (void)nReceiveTimeout;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_set_timeouts_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL WinHttpSetOption(HINTERNET hInternet, DWORD dwOption,
                             LPVOID lpBuffer, DWORD dwBufferLength) {
  (void)hInternet;
  (void)dwOption;
  (void)lpBuffer;
  (void)dwBufferLength;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_set_option_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL WinHttpCrackUrl(const wchar_t *pwszUrl, DWORD dwUrlLength,
                            DWORD dwFlags, URL_COMPONENTS *lpUrlComponents) {
  const wchar_t *p;
  const wchar_t *host_start;
  const wchar_t *host_end;
  const wchar_t *path_start;
  size_t host_len;
  size_t path_len;
  (void)dwUrlLength;
  (void)dwFlags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_crack_url_fail) {
    return 0;
  }
#endif
  p = wcsstr(pwszUrl, L"://");
  if (!p) {
    return 0;
  }
  if (p - pwszUrl == 5 && wcsncmp(pwszUrl, L"https", 5) == 0) {
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

static HINTERNET WinHttpConnect(HINTERNET hSession,
                                const wchar_t *pswzServerName,
                                INTERNET_PORT nServerPort, DWORD dwReserved) {
  (void)hSession;
  (void)pswzServerName;
  (void)nServerPort;
  (void)dwReserved;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_connect_fail) {
    return NULL;
  }
#endif
  return (HINTERNET)(size_t)102;
}

static HINTERNET WinHttpOpenRequest(HINTERNET hConnect, const wchar_t *pwszVerb,
                                    const wchar_t *pwszObjectName,
                                    const wchar_t *pwszVersion,
                                    const wchar_t *pwszReferrer,
                                    const wchar_t **ppwszAcceptTypes,
                                    DWORD dwFlags) {
  (void)hConnect;
  (void)pwszVerb;
  (void)pwszObjectName;
  (void)pwszVersion;
  (void)pwszReferrer;
  (void)ppwszAcceptTypes;
  (void)dwFlags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_open_request_fail) {
    return NULL;
  }
#endif
  return (HINTERNET)(size_t)103;
}

static BOOL WinHttpAddRequestHeaders(HINTERNET hRequest,
                                     const wchar_t *lpszHeaders,
                                     DWORD dwHeadersLength, DWORD dwModifiers) {
  (void)hRequest;
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)dwModifiers;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_add_request_headers_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL WinHttpSendRequest(HINTERNET hRequest, const wchar_t *lpszHeaders,
                               DWORD dwHeadersLength, LPVOID lpOptional,
                               DWORD dwOptionalLength, DWORD dwTotalLength,
                               DWORD dwContext) {
  (void)hRequest;
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)lpOptional;
  (void)dwOptionalLength;
  (void)dwTotalLength;
  (void)dwContext;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_send_request_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL WinHttpWriteData(HINTERNET hRequest, LPCVOID lpBuffer,
                             DWORD dwNumberOfBytesToWrite,
                             LPDWORD lpdwNumberOfBytesWritten) {
  (void)hRequest;
  (void)lpBuffer;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_write_data_fail) {
    return 0;
  }
#endif
  if (lpdwNumberOfBytesWritten) {
    *lpdwNumberOfBytesWritten = dwNumberOfBytesToWrite;
  }
  return 1;
}

static BOOL WinHttpReceiveResponse(HINTERNET hRequest, LPVOID lpReserved) {
  (void)hRequest;
  (void)lpReserved;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_receive_response_fail) {
    return 0;
  }
#endif
  return 1;
}

static BOOL WinHttpQueryHeaders(HINTERNET hRequest, DWORD dwInfoLevel,
                                const wchar_t *pwszName, LPVOID lpBuffer,
                                LPDWORD lpdwBufferLength, LPDWORD lpdwIndex) {
  (void)hRequest;
  (void)pwszName;
  (void)lpdwIndex;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_query_headers_fail) {
    return 0;
  }
#endif
  if ((dwInfoLevel & 0xFFFF) == WINHTTP_QUERY_STATUS_CODE) {
    if (lpBuffer) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      DWORD code =
          g_mock_winhttp_status_code ? (DWORD)g_mock_winhttp_status_code : 200;
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
  if (g_mock_winhttp_cookie_count > 0) {
    const wchar_t cookie_val[] = L"mock_cookie=123; path=/";
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
    g_mock_winhttp_cookie_count--;
    SetLastError(0);
    return 1;
  }
#endif
  SetLastError(0);
  return 0;
}

static BOOL WinHttpQueryDataAvailable(HINTERNET hRequest,
                                      LPDWORD lpdwNumberOfBytesAvailable) {
  (void)hRequest;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_query_data_available_fail) {
    return 0;
  }
  if (g_mock_winhttp_read_chunks > 0) {
    DWORD avail = (g_mock_winhttp_read_chunks == 3) ? 9000 : 10;
    g_mock_winhttp_read_chunks--;
    if (lpdwNumberOfBytesAvailable) {
      *lpdwNumberOfBytesAvailable = avail;
    }
    return 1;
  }
#endif
  if (lpdwNumberOfBytesAvailable) {
    *lpdwNumberOfBytesAvailable = 0;
  }
  return 1;
}

static BOOL WinHttpReadData(HINTERNET hRequest, LPVOID lpBuffer,
                            DWORD dwNumberOfBytesToRead,
                            LPDWORD lpdwNumberOfBytesRead) {
  (void)hRequest;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_read_data_fail) {
    return 0;
  }
#endif
  if (lpBuffer && dwNumberOfBytesToRead > 0) {
    memset(lpBuffer, 'W', dwNumberOfBytesToRead);
  }
  if (lpdwNumberOfBytesRead) {
    *lpdwNumberOfBytesRead = dwNumberOfBytesToRead;
  }
  return 1;
}

static BOOL QueueUserWorkItem(LPTHREAD_START_ROUTINE Function, PVOID Context,
                              ULONG Flags) {
  (void)Flags;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_queue_work_item_fail) {
    return 0;
  }
#endif
  if (Function) {
    Function(Context);
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
  /** @brief hSession handle */
  HINTERNET hSession;
  /** @brief security flags */
  DWORD security_flags;
  /** @brief disable redirects flag */
  int disable_redirects;
  /** @brief Cookie jar */
  struct HttpCookieJar *cookie_jar;
  /** @brief HTTP config copy */
  struct HttpConfig config;
};

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
  case HTTP_PATCH:
    *out = L"PATCH";
    break;
  default:
    *out = L"GET";
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static void safe_close_handle(HINTERNET *h) {
  if (h && *h) {
    WinHttpCloseHandle(*h);
    *h = NULL;
  }
}

static enum c_abstract_http_error
headers_to_wide_block(const struct HttpHeaders *headers, wchar_t **out) {
  size_t i;
  size_t total_wide_chars = 0;
  wchar_t *buf;
  wchar_t *p;

  *out = NULL;
  if (!headers || headers->count == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  for (i = 0; i < headers->count; ++i) {
    total_wide_chars += strlen(headers->headers[i].key);
    total_wide_chars += strlen(headers->headers[i].value);
    total_wide_chars += 4;
  }
  total_wide_chars += 1;

  buf = (wchar_t *)calloc(total_wide_chars, sizeof(wchar_t));
  if (!buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  p = buf;
  for (i = 0; i < headers->count; ++i) {
    size_t written = 0;
    ascii_to_wide(headers->headers[i].key, p,
                  total_wide_chars - (size_t)(p - buf), &written);
    p += written;
    p[0] = L':';
    p[1] = L' ';
    p += 2;
    ascii_to_wide(headers->headers[i].value, p,
                  total_wide_chars - (size_t)(p - buf), &written);
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
 * @brief Initialize the global WinHTTP environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_winhttp_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Cleanup the global WinHTTP environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_winhttp_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Create and initialize a new WinHTTP transport context.
 *
 * @param[out] ctx Double pointer to receive the allocated context.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_winhttp_context_init(struct HttpTransportContext **ctx) {
  HINTERNET hSession;
  enum c_abstract_http_error rc;

  LOG_DEBUG("http_winhttp_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_winhttp_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_context_init_fail) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  hSession = WinHttpOpen(L"c_abstract_http/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

  if (!hSession) {
    LOG_DEBUG("http_winhttp_context_init: Error WinHttpOpen failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_winhttp_context_init: Error ENOMEM");
    WinHttpCloseHandle(hSession);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_winhttp_context_init: Error http_config_init failed %d",
              (int)rc);
    WinHttpCloseHandle(hSession);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  (*ctx)->hSession = hSession;
  (*ctx)->security_flags = 0;
  (*ctx)->disable_redirects = 0;
  (*ctx)->cookie_jar = NULL;

  LOG_DEBUG("http_winhttp_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free the WinHTTP transport context and release associated resources.
 *
 * @param[in] ctx Pointer to the context to free. Satisfies NULL-safety.
 */
void http_winhttp_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_winhttp_context_free: Entering");
  if (ctx) {
    if (ctx->hSession) {
      WinHttpCloseHandle(ctx->hSession);
      ctx->hSession = NULL;
    }
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_winhttp_context_free: Exiting");
}

/**
 * @brief Apply configuration settings to an active WinHTTP context.
 *
 * @param[in,out] ctx The transport context.
 * @param[in] config Configuration options to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_winhttp_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
  LOG_DEBUG("http_winhttp_config_apply: Entering");
  if (!ctx) {
    LOG_DEBUG("http_winhttp_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!config) {
    LOG_DEBUG("http_winhttp_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (config->timeout_ms > 0 || config->connect_timeout_ms > 0 ||
      config->read_timeout_ms > 0 || config->write_timeout_ms > 0) {
    int resolve_timeout = 0;
    int connect_timeout = (config->connect_timeout_ms > 0)
                              ? (int)config->connect_timeout_ms
                              : (int)config->timeout_ms;
    int send_timeout = (config->write_timeout_ms > 0)
                           ? (int)config->write_timeout_ms
                           : (int)config->timeout_ms;
    int receive_timeout = (config->read_timeout_ms > 0)
                              ? (int)config->read_timeout_ms
                              : (int)config->timeout_ms;

    if (!WinHttpSetTimeouts(ctx->hSession, resolve_timeout, connect_timeout,
                            send_timeout, receive_timeout)) {
      LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetTimeouts failed");
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  if (config->proxy_url) {
    WINHTTP_PROXY_INFO proxyInfo;
    wchar_t *wProxy = NULL;
    size_t wProxyLen = 0;
    size_t cap = strlen(config->proxy_url) + 1;
    wProxy = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!wProxy) {
      LOG_DEBUG("http_winhttp_config_apply: Error ENOMEM (wProxy)");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    ascii_to_wide(config->proxy_url, wProxy, cap, &wProxyLen);

    memset(&proxyInfo, 0, sizeof(proxyInfo));
    proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    proxyInfo.lpszProxy = wProxy;
    proxyInfo.lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;

    if (!WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY, &proxyInfo,
                          sizeof(proxyInfo))) {
      LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetOption (PROXY) "
                "failed");
      free(wProxy);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    free(wProxy);

    if (config->proxy_username && config->proxy_password) {
      wchar_t *wUser = NULL;
      wchar_t *wPass = NULL;
      size_t wUserLen = 0;
      size_t wPassLen = 0;
      size_t ucap = strlen(config->proxy_username) + 1;
      size_t pcap = strlen(config->proxy_password) + 1;
      wUser = (wchar_t *)malloc(ucap * sizeof(wchar_t));
      if (!wUser) {
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      wPass = (wchar_t *)malloc(pcap * sizeof(wchar_t));
      if (!wPass) {
        free(wUser);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      ascii_to_wide(config->proxy_username, wUser, ucap, &wUserLen);
      ascii_to_wide(config->proxy_password, wPass, pcap, &wPassLen);
      WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_USERNAME, wUser,
                       (DWORD)wcslen(wUser));
      WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_PASSWORD, wPass,
                       (DWORD)wcslen(wPass));
      free(wUser);
      free(wPass);
    }
  } else {
    WINHTTP_PROXY_INFO proxyInfo;
    memset(&proxyInfo, 0, sizeof(proxyInfo));
    proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY, &proxyInfo,
                     sizeof(proxyInfo));
  }

  ctx->security_flags = 0;
  if (!config->verify_peer) {
    ctx->security_flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    ctx->security_flags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
  }
  if (!config->verify_host) {
    ctx->security_flags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
  }

  if (!config->follow_redirects) {
    ctx->disable_redirects = 1;
  } else {
    ctx->disable_redirects = 0;
  }

  ctx->cookie_jar = config->cookie_jar;

  LOG_DEBUG("http_winhttp_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send an HTTP request synchronously using WinHTTP.
 *
 * @param[in] ctx The transport context.
 * @param[in] req The HTTP request parameters.
 * @param[out] res Double pointer to receive the allocated HTTP response.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_winhttp_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
  HINTERNET hConnect = NULL, hRequest = NULL;
  URL_COMPONENTS urlComp;
  wchar_t *wUrl = NULL;
  wchar_t *wHeaders = NULL;
  wchar_t *hostName = NULL;
  wchar_t *urlPath = NULL;
  const wchar_t *wmethod = NULL;
  size_t wLen = 0;
  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);

  /* Body reading state */
  char *readBuf = NULL;
  char *totalBody = NULL;
  size_t totalSize = 0;
  DWORD dwDownloaded = 0;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_winhttp_send: Entering");
  if (!ctx || !ctx->hSession || !req || !res || !req->url) {
    LOG_DEBUG("http_winhttp_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  {
    size_t cap = strlen(req->url) + 1;
    wUrl = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!wUrl) {
      LOG_DEBUG("http_winhttp_send: Error ENOMEM (wUrl)");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    ascii_to_wide(req->url, wUrl, cap, &wLen);
  }

  memset(&urlComp, 0, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);
  hostName = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!hostName) {
    LOG_DEBUG("http_winhttp_send: Error ENOMEM (hostName)");
    free(wUrl);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  urlPath = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!urlPath) {
    LOG_DEBUG("http_winhttp_send: Error ENOMEM (urlPath)");
    free(wUrl);
    free(hostName);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  urlComp.lpszHostName = hostName;
  urlComp.dwHostNameLength = (DWORD)wLen + 1;
  urlComp.lpszUrlPath = urlPath;
  urlComp.dwUrlPathLength = (DWORD)wLen + 1;

  if (!WinHttpCrackUrl(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    LOG_DEBUG("http_winhttp_send: Error WinHttpCrackUrl failed");
    rc = C_ABSTRACT_HTTP_ERR_INVAL;
    goto cleanup;
  }

  hConnect = WinHttpConnect(ctx->hSession, urlComp.lpszHostName,
                            (INTERNET_PORT)urlComp.nPort, 0);
  if (!hConnect) {
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  method_to_wide(req->method, &wmethod);

  hRequest = WinHttpOpenRequest(
      hConnect, wmethod, urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
  if (!hRequest) {
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  if (ctx->security_flags != 0) {
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                     &ctx->security_flags, sizeof(ctx->security_flags));
  }

  if (ctx->disable_redirects) {
    DWORD dwDisable = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &dwDisable,
                     sizeof(dwDisable));
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
      WinHttpAddRequestHeaders(hRequest, wcbuf, (DWORD)-1L,
                               WINHTTP_ADDREQ_FLAG_ADD);
    }
  }

  rc = headers_to_wide_block(&req->headers, &wHeaders);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup;
  }
  if (wHeaders) {
    if (!WinHttpAddRequestHeaders(hRequest, wHeaders, (DWORD)-1L,
                                  WINHTTP_ADDREQ_FLAG_ADD)) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
  }

  if (req->read_chunk) {
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0,
                            (DWORD)req->expected_body_len, 0)) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    for (;;) {
      char chunkBuf[8192];
      size_t out_read = 0;
      int cb_rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                                  sizeof(chunkBuf), &out_read);
      if (cb_rc != 0) {
        rc = (enum c_abstract_http_error)cb_rc;
        goto cleanup;
      }
      if (out_read == 0) {
        break; /* EOF */
      }
      {
        DWORD dwWritten = 0;
        if (!WinHttpWriteData(hRequest, chunkBuf, (DWORD)out_read,
                              &dwWritten)) {
          rc = C_ABSTRACT_HTTP_ERR_IO;
          goto cleanup;
        }
      }
    }
  } else {
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            req->body ? req->body : WINHTTP_NO_REQUEST_DATA,
                            (DWORD)req->body_len, (DWORD)req->body_len, 0)) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
  }

  if (!WinHttpReceiveResponse(hRequest, NULL)) {
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  if (!WinHttpQueryHeaders(
          hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
          WINHTTP_NO_HEADER_INDEX)) {
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  /* Read Body Cycle */
  readBuf = (char *)malloc(8192);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_read_buf_alloc_fail) {
    if (readBuf) {
      free(readBuf);
      readBuf = NULL;
    }
  }
#endif
  if (!readBuf) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  do {
    dwSize = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
    if (dwSize == 0) {
      break;
    }
    if (dwSize > 8192) {
      dwSize = 8192;
    }

    if (!WinHttpReadData(hRequest, (LPVOID)readBuf, dwSize, &dwDownloaded)) {
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }

    if (dwDownloaded > 0) {
      if (req->on_chunk) {
        int cb_rc =
            req->on_chunk(req->on_chunk_user_data, readBuf, dwDownloaded);
        if (cb_rc != 0) {
          rc = (enum c_abstract_http_error)cb_rc;
          goto cleanup;
        }
      } else {
        char *new_ptr =
            (char *)realloc(totalBody, totalSize + dwDownloaded + 1);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_winhttp_total_body_realloc_fail) {
          if (new_ptr) {
            free(new_ptr);
            new_ptr = NULL;
          }
        }
#endif
        if (!new_ptr) {
          rc = C_ABSTRACT_HTTP_ERR_NOMEM;
          goto cleanup;
        }
        totalBody = new_ptr;
        memcpy(totalBody + totalSize, readBuf, dwDownloaded);
        totalSize += dwDownloaded;
        totalBody[totalSize] = '\0';
      }
    }
  } while (dwDownloaded > 0);

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_res_alloc_fail) {
    if (*res) {
      free(*res);
      *res = NULL;
    }
  }
#endif
  if (!*res) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
  rc = http_response_init(*res);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    goto cleanup;
  }

  /* Extract Cookies and map back to jar */
  if (ctx->cookie_jar) {
    DWORD dwIndex = 0;
    DWORD cbCookie = 0;

    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                        &cbCookie, &dwIndex);

    while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      wchar_t *pwszCookie = (wchar_t *)malloc(cbCookie);
      if (pwszCookie) {
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE,
                                WINHTTP_HEADER_NAME_BY_INDEX, pwszCookie,
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
      WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE,
                          WINHTTP_HEADER_NAME_BY_INDEX,
                          WINHTTP_NO_OUTPUT_BUFFER, &cbCookie, &dwIndex);
    }
  }

  (*res)->status_code = (int)dwStatusCode;
  (*res)->body = totalBody;
  (*res)->body_len = totalSize;
  totalBody = NULL;

cleanup:
  safe_close_handle(&hRequest);
  safe_close_handle(&hConnect);
  if (wUrl) {
    free(wUrl);
  }
  if (wHeaders) {
    free(wHeaders);
  }
  if (hostName) {
    free(hostName);
  }
  if (urlPath) {
    free(urlPath);
  }
  if (readBuf) {
    free(readBuf);
  }
  if (totalBody) {
    free(totalBody);
  }

  return rc;
}

/** @brief Internal struct WinHttpAsyncWorkerCtx */
struct WinHttpAsyncWorkerCtx {
  struct HttpTransportContext *ctx;
  struct ModalityEventLoop *loop;
  const struct HttpRequest *req;
  struct HttpFuture *future;
};

static DWORD WINAPI winhttp_async_worker(LPVOID lpParam) {
  struct WinHttpAsyncWorkerCtx *worker_ctx =
      (struct WinHttpAsyncWorkerCtx *)lpParam;
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  rc = http_winhttp_send(worker_ctx->ctx, worker_ctx->req, &res);

  worker_ctx->future->response = res;
  worker_ctx->future->error_code = rc;
  worker_ctx->future->is_ready = 1;

  if (worker_ctx->loop) {
    enum c_abstract_http_error rc_wake = http_loop_wakeup(worker_ctx->loop);
    if (rc_wake != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("http_loop_wakeup failed: %d", (int)rc_wake);
    }
  }
  free(worker_ctx);
  return (DWORD)C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Send multiple HTTP requests asynchronously via WinHTTP and worker
 * pool.
 *
 * @param[in] ctx The transport context.
 * @param[in] loop Event loop to signal upon completion.
 * @param[in] multi Multi-request description.
 * @param[out] futures Array of future handles.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_winhttp_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  LOG_DEBUG("http_winhttp_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_winhttp_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; ++i) {
    struct WinHttpAsyncWorkerCtx *wctx = (struct WinHttpAsyncWorkerCtx *)malloc(
        sizeof(struct WinHttpAsyncWorkerCtx));
    if (!wctx) {
      LOG_DEBUG("http_winhttp_send_multi: Error ENOMEM");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    wctx->ctx = ctx;
    wctx->loop = loop;
    wctx->req = multi->requests[i];
    wctx->future = futures[i];

    if (!QueueUserWorkItem(winhttp_async_worker, wctx, WT_EXECUTEDEFAULT)) {
      LOG_DEBUG("http_winhttp_send_multi: Error QueueUserWorkItem failed");
      free(wctx);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  LOG_DEBUG("http_winhttp_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
