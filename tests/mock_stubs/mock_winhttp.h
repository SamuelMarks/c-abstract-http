/**
 * @file mock_winhttp.h
 * @brief Internal test-only WinHTTP C-library stubs for OOM and fault
 * injection.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_WINHTTP_H
#define C_ABSTRACT_HTTP_TEST_MOCK_WINHTTP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
/* clang-format on */

extern int g_mock_winhttp_open_fail;
extern int g_mock_winhttp_connect_fail;
extern int g_mock_winhttp_open_request_fail;
extern int g_mock_winhttp_send_request_fail;
extern int g_mock_winhttp_write_data_fail;
extern int g_mock_winhttp_receive_response_fail;
extern int g_mock_winhttp_query_headers_fail;
extern int g_mock_winhttp_query_data_available_fail;
extern int g_mock_winhttp_read_data_fail;
extern int g_mock_winhttp_set_timeouts_fail;
extern int g_mock_winhttp_set_option_fail;
extern int g_mock_winhttp_crack_url_fail;
extern int g_mock_winhttp_add_request_headers_fail;
extern int g_mock_winhttp_queue_work_item_fail;
extern int g_mock_winhttp_context_init_fail;
extern int g_mock_winhttp_response_init_fail;
extern int g_mock_winhttp_status_code;
extern int g_mock_winhttp_cookie_count;
extern int g_mock_winhttp_read_chunks;
extern int g_mock_winhttp_total_body_realloc_fail;
extern int g_mock_winhttp_read_buf_alloc_fail;
extern int g_mock_winhttp_res_alloc_fail;
extern int g_mock_winhttp_close_fail;
extern int g_mock_winhttp_cookie_set_fail;

#if !defined(_WIN32) || (defined(_MSC_VER) && _MSC_VER < 1600)
typedef void *HINTERNET;
typedef unsigned long DWORD;
typedef size_t DWORD_PTR;
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
#ifndef CALLBACK
#define CALLBACK
#endif

typedef DWORD(WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef void(CALLBACK *WINHTTP_STATUS_CALLBACK)(
    HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
    LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

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
#define WINHTTP_FLAG_ASYNC 0x10000000
#define WINHTTP_OPTION_USER_AGENT 11
#define WINHTTP_OPTION_SECURITY_FLAGS 31
#define WINHTTP_OPTION_DISABLE_FEATURE 63
#define WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL 133
#define WINHTTP_PROTOCOL_FLAG_HTTP2 0x1
#define WINHTTP_PROTOCOL_FLAG_HTTP3 0x2
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

#define WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE 0x00020000
#define WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE 0x00040000
#define WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE 0x00080000
#define WINHTTP_CALLBACK_STATUS_READ_COMPLETE 0x00100000
#define WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE 0x00000020
#define WINHTTP_CALLBACK_STATUS_REQUEST_ERROR 0x00200000
#define WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS                                  \
  (WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE |                              \
   WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE |                                 \
   WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE |                                    \
   WINHTTP_CALLBACK_STATUS_READ_COMPLETE |                                     \
   WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE |                                    \
   WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
#define WINHTTP_INVALID_STATUS_CALLBACK ((WINHTTP_STATUS_CALLBACK)(-1L))

typedef struct {
  DWORD_PTR dwResult;
  DWORD dwError;
} WINHTTP_ASYNC_RESULT;

typedef struct {
  DWORD dwAccessType;
  wchar_t *lpszProxy;
  wchar_t *lpszProxyBypass;
} WINHTTP_PROXY_INFO;

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
static WINHTTP_STATUS_CALLBACK s_mock_winhttp_status_cb = NULL;
static DWORD_PTR s_mock_winhttp_async_ctx = 0;

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
  if (g_mock_winhttp_open_fail) {
    return NULL;
  }
  if (dwFlags & WINHTTP_FLAG_ASYNC) {
    return (HINTERNET)(size_t)201;
  }
  return (HINTERNET)(size_t)101;
}

static WINHTTP_STATUS_CALLBACK
WinHttpSetStatusCallback(HINTERNET hInternet,
                         WINHTTP_STATUS_CALLBACK lpfnInternetCallback,
                         DWORD dwNotificationFlags, DWORD_PTR dwReserved) {
  WINHTTP_STATUS_CALLBACK prev = s_mock_winhttp_status_cb;
  (void)hInternet;
  (void)dwNotificationFlags;
  (void)dwReserved;
  if (g_mock_winhttp_queue_work_item_fail && lpfnInternetCallback != NULL) {
    return WINHTTP_INVALID_STATUS_CALLBACK;
  }
  s_mock_winhttp_status_cb = lpfnInternetCallback;
  return prev;
}

static BOOL WinHttpCloseHandle(HINTERNET hInternet) {
  if (hInternet == (HINTERNET)(size_t)201) {
    s_mock_winhttp_status_cb = NULL;
    s_mock_winhttp_async_ctx = 0;
  }
  if (g_mock_winhttp_close_fail < 0) {
    return 0;
  }
  if (g_mock_winhttp_close_fail > 0) {
    if (--g_mock_winhttp_close_fail == 0) {
      return 0;
    }
    return 1;
  }
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
  if (g_mock_winhttp_set_timeouts_fail) {
    return 0;
  }
  return 1;
}

static BOOL WinHttpSetOption(HINTERNET hInternet, DWORD dwOption,
                             LPVOID lpBuffer, DWORD dwBufferLength) {
  (void)hInternet;
  (void)dwOption;
  (void)lpBuffer;
  (void)dwBufferLength;
  if (g_mock_winhttp_set_option_fail) {
    return 0;
  }
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
  if (g_mock_winhttp_crack_url_fail) {
    return 0;
  }
  p = wcsstr(pwszUrl, L"://");
  if (!p) {
    return 0;
  }
  if (p - pwszUrl == 5) {
    if (wcsncmp(pwszUrl, L"https", 5) == 0) {
      lpUrlComponents->nScheme = INTERNET_SCHEME_HTTPS;
      lpUrlComponents->nPort = 443;
    } else {
      lpUrlComponents->nScheme = INTERNET_SCHEME_HTTP;
      lpUrlComponents->nPort = 80;
    }
  } else {
    lpUrlComponents->nScheme = INTERNET_SCHEME_HTTP;
    lpUrlComponents->nPort = 80;
  }
  host_start = p + 3;
  p = host_start;
  while (*p) {
    if (*p == L':' || *p == L'/') {
      break;
    }
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
  memcpy(lpUrlComponents->lpszHostName, host_start, host_len * sizeof(wchar_t));
  lpUrlComponents->lpszHostName[host_len] = L'\0';
  lpUrlComponents->dwHostNameLength = (DWORD)host_len;

  if (*p == L'/') {
    path_start = p;
    path_len = wcslen(path_start);
    memcpy(lpUrlComponents->lpszUrlPath, path_start,
           path_len * sizeof(wchar_t));
    lpUrlComponents->lpszUrlPath[path_len] = L'\0';
    lpUrlComponents->dwUrlPathLength = (DWORD)path_len;
  } else {
    lpUrlComponents->lpszUrlPath[0] = L'/';
    lpUrlComponents->lpszUrlPath[1] = L'\0';
    lpUrlComponents->dwUrlPathLength = 1;
  }
  return 1;
}

static HINTERNET WinHttpConnect(HINTERNET hSession,
                                const wchar_t *pswzServerName,
                                INTERNET_PORT nServerPort, DWORD dwReserved) {
  (void)pswzServerName;
  (void)nServerPort;
  (void)dwReserved;
  if (g_mock_winhttp_connect_fail) {
    return NULL;
  }
  if (hSession == (HINTERNET)(size_t)201) {
    return (HINTERNET)(size_t)202;
  }
  return (HINTERNET)(size_t)102;
}

static HINTERNET WinHttpOpenRequest(HINTERNET hConnect, const wchar_t *pwszVerb,
                                    const wchar_t *pwszObjectName,
                                    const wchar_t *pwszVersion,
                                    const wchar_t *pwszReferrer,
                                    const wchar_t **ppwszAcceptTypes,
                                    DWORD dwFlags) {
  (void)pwszVerb;
  (void)pwszObjectName;
  (void)pwszVersion;
  (void)pwszReferrer;
  (void)ppwszAcceptTypes;
  (void)dwFlags;
  if (g_mock_winhttp_open_request_fail) {
    return NULL;
  }
  if (hConnect == (HINTERNET)(size_t)202) {
    return (HINTERNET)(size_t)203;
  }
  return (HINTERNET)(size_t)103;
}

static BOOL WinHttpAddRequestHeaders(HINTERNET hRequest,
                                     const wchar_t *lpszHeaders,
                                     DWORD dwHeadersLength, DWORD dwModifiers) {
  (void)hRequest;
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)dwModifiers;
  if (g_mock_winhttp_add_request_headers_fail) {
    return 0;
  }
  return 1;
}

static BOOL WinHttpSendRequest(HINTERNET hRequest, const wchar_t *lpszHeaders,
                               DWORD dwHeadersLength, LPVOID lpOptional,
                               DWORD dwOptionalLength, DWORD dwTotalLength,
                               DWORD_PTR dwContext) {
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)lpOptional;
  (void)dwOptionalLength;
  (void)dwTotalLength;
  if (g_mock_winhttp_send_request_fail == 2) {
    return 0;
  }
  if (g_mock_winhttp_send_request_fail) {
    if (hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb &&
        dwContext != 0) {
      WINHTTP_ASYNC_RESULT ar;
      ar.dwResult = 5;
      ar.dwError = 12002;
      s_mock_winhttp_async_ctx = dwContext;
      s_mock_winhttp_status_cb(hRequest, dwContext,
                               WINHTTP_CALLBACK_STATUS_REQUEST_ERROR, &ar,
                               sizeof(ar));
      return 1;
    }
    return 0;
  }
  if (hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb &&
      dwContext != 0) {
    s_mock_winhttp_async_ctx = dwContext;
    s_mock_winhttp_status_cb(hRequest, dwContext,
                             WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, NULL,
                             0);
  }
  return 1;
}

static BOOL WinHttpWriteData(HINTERNET hRequest, LPCVOID lpBuffer,
                             DWORD dwNumberOfBytesToWrite,
                             LPDWORD lpdwNumberOfBytesWritten) {
  (void)lpBuffer;
  if (g_mock_winhttp_write_data_fail) {
    return 0;
  }
  if (lpdwNumberOfBytesWritten) {
    *lpdwNumberOfBytesWritten = dwNumberOfBytesToWrite;
  }
  if (hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb &&
      !lpdwNumberOfBytesWritten) {
    DWORD written = dwNumberOfBytesToWrite;
    s_mock_winhttp_status_cb(hRequest, s_mock_winhttp_async_ctx,
                             WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE, &written,
                             sizeof(written));
  }
  return 1;
}

static BOOL WinHttpReceiveResponse(HINTERNET hRequest, LPVOID lpReserved) {
  (void)lpReserved;
  if (g_mock_winhttp_receive_response_fail) {
    return 0;
  }
  if (hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb) {
    s_mock_winhttp_status_cb(hRequest, s_mock_winhttp_async_ctx,
                             WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, NULL,
                             0);
  }
  return 1;
}

static BOOL WinHttpQueryHeaders(HINTERNET hRequest, DWORD dwInfoLevel,
                                const wchar_t *pwszName, LPVOID lpBuffer,
                                LPDWORD lpdwBufferLength, LPDWORD lpdwIndex) {
  (void)hRequest;
  (void)pwszName;
  (void)lpdwIndex;
  if (g_mock_winhttp_query_headers_fail) {
    return 0;
  }
  if ((dwInfoLevel & 0xFFFF) == WINHTTP_QUERY_STATUS_CODE) {
    DWORD code =
        g_mock_winhttp_status_code ? (DWORD)g_mock_winhttp_status_code : 200;
    memcpy(lpBuffer, &code, sizeof(DWORD));
    *lpdwBufferLength = sizeof(DWORD);
    return 1;
  }
  if (g_mock_winhttp_cookie_count > 0) {
    wchar_t utf_cookie[8];
    const wchar_t cookie_val1[] = L"mock_cookie=123; path=/";
    const wchar_t cookie_val2[] = L"mock_cookie2=456";
    const wchar_t *cookie_val;
    utf_cookie[0] = L'u';
    utf_cookie[1] = L'=';
    utf_cookie[2] = (wchar_t)0x00E9;
    utf_cookie[3] = (wchar_t)0x20AC;
    utf_cookie[4] = L'\0';
    cookie_val =
        (g_mock_winhttp_cookie_count == 99)
            ? utf_cookie
            : ((g_mock_winhttp_cookie_count == 1) ? cookie_val2 : cookie_val1);
    {
      DWORD needed_bytes = (DWORD)((wcslen(cookie_val) + 1) * sizeof(wchar_t));
      if (!lpBuffer) {
        *lpdwBufferLength = needed_bytes;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
      }
      memcpy(lpBuffer, cookie_val, needed_bytes);
      *lpdwBufferLength = needed_bytes;
      if (g_mock_winhttp_cookie_count == 99) {
        g_mock_winhttp_cookie_count = 0;
      } else {
        g_mock_winhttp_cookie_count--;
      }
      SetLastError(0);
      return 1;
    }
  }
  SetLastError(0);
  return 0;
}

static BOOL WinHttpQueryDataAvailable(HINTERNET hRequest,
                                      LPDWORD lpdwNumberOfBytesAvailable) {
  DWORD avail = 0;
  if (g_mock_winhttp_query_data_available_fail == 2) {
    g_mock_winhttp_query_data_available_fail = 1;
  } else if (g_mock_winhttp_query_data_available_fail) {
    return 0;
  }
  if (g_mock_winhttp_read_chunks > 0) {
    if (g_mock_winhttp_read_chunks == 3 && hRequest == (HINTERNET)(size_t)203 &&
        s_mock_winhttp_status_cb) {
      s_mock_winhttp_status_cb(hRequest, 0, 0x99999999U, NULL, 0);
      s_mock_winhttp_status_cb(hRequest, s_mock_winhttp_async_ctx, 0x99999999U,
                               NULL, 0);
    }
    avail = (g_mock_winhttp_read_chunks == 3) ? 9000 : 10;
    g_mock_winhttp_read_chunks--;
  }
  if (lpdwNumberOfBytesAvailable) {
    *lpdwNumberOfBytesAvailable = avail;
  } else if (hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb) {
    s_mock_winhttp_status_cb(hRequest, s_mock_winhttp_async_ctx,
                             WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, &avail,
                             sizeof(avail));
  }
  return 1;
}

static BOOL WinHttpReadData(HINTERNET hRequest, LPVOID lpBuffer,
                            DWORD dwNumberOfBytesToRead,
                            LPDWORD lpdwNumberOfBytesRead) {
  if (g_mock_winhttp_read_data_fail == 2 &&
      hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb) {
    s_mock_winhttp_status_cb(hRequest, s_mock_winhttp_async_ctx,
                             WINHTTP_CALLBACK_STATUS_READ_COMPLETE, lpBuffer,
                             0);
    return 1;
  }
  if (g_mock_winhttp_read_data_fail) {
    return 0;
  }
  memset(lpBuffer, 'W', dwNumberOfBytesToRead);
  if (lpdwNumberOfBytesRead) {
    *lpdwNumberOfBytesRead = dwNumberOfBytesToRead;
  } else if (hRequest == (HINTERNET)(size_t)203 && s_mock_winhttp_status_cb) {
    s_mock_winhttp_status_cb(hRequest, s_mock_winhttp_async_ctx,
                             WINHTTP_CALLBACK_STATUS_READ_COMPLETE, lpBuffer,
                             dwNumberOfBytesToRead);
  }
  return 1;
}
#endif /* !defined(_WIN32) || (defined(_MSC_VER) && _MSC_VER < 1600) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_WINHTTP_H */
