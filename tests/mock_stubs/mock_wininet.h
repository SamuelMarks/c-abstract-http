/**
 * @file mock_wininet.h
 * @brief Internal test-only WinInet C-library stubs for OOM and fault
 * injection.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_WININET_H
#define C_ABSTRACT_HTTP_TEST_MOCK_WININET_H

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "../../src/wincrypt_compat.h"
#if !defined(_WIN32)
#include "mock_wincrypt.h"
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern int g_mock_wininet_open_fail;
extern int g_mock_wininet_connect_fail;
extern int g_mock_wininet_open_request_fail;
extern int g_mock_wininet_send_request_fail;
extern int g_mock_wininet_send_request_ex_fail;
extern int g_mock_wininet_write_file_fail;
extern int g_mock_wininet_end_request_fail;
extern int g_mock_wininet_query_info_fail;
extern int g_mock_wininet_read_file_fail;
extern int g_mock_wininet_set_option_fail;
extern int g_mock_wininet_crack_url_fail;
extern int g_mock_wininet_add_headers_fail;
extern int g_mock_wininet_context_init_fail;
extern int g_mock_wininet_response_init_fail;
extern int g_mock_wininet_status_code;
extern int g_mock_wininet_cookie_count;
extern int g_mock_wininet_read_chunks;
extern int g_mock_wininet_read_chunk_alloc_fail;
extern int g_mock_wininet_body_realloc_fail;
extern int g_mock_wininet_res_alloc_fail;
extern int g_mock_wininet_close_fail;
extern int g_mock_wininet_cookie_set_fail;

#if !defined(_WIN32)
typedef void *HINTERNET;
#ifndef CAH_WIN32_BASIC_TYPES_DEFINED
#define CAH_WIN32_BASIC_TYPES_DEFINED 1
typedef unsigned long DWORD;
typedef int BOOL;
#endif
typedef size_t DWORD_PTR;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef DWORD *LPDWORD;
typedef wchar_t WCHAR;
typedef unsigned short INTERNET_PORT;

#ifndef CALLBACK
#define CALLBACK
#endif

typedef void(CALLBACK *INTERNET_STATUS_CALLBACK)(
    HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
    LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

typedef struct {
  DWORD_PTR dwResult;
  DWORD dwError;
} INTERNET_ASYNC_RESULT;

#define INTERNET_OPEN_TYPE_PRECONFIG 0
#define INTERNET_SERVICE_HTTP 3
#define INTERNET_FLAG_ASYNC 0x10000000
#define INTERNET_FLAG_RELOAD 0x80000000
#define INTERNET_FLAG_NO_CACHE_WRITE 0x04000000
#define INTERNET_FLAG_SECURE 0x00800000
#define INTERNET_FLAG_IGNORE_CERT_CN_INVALID 0x00001000
#define INTERNET_FLAG_IGNORE_CERT_DATE_INVALID 0x00002000
#define INTERNET_FLAG_NO_AUTO_REDIRECT 0x00200000
#define SECURITY_FLAG_IGNORE_UNKNOWN_CA 0x00000100
#define INTERNET_OPTION_CONNECT_TIMEOUT 2
#define INTERNET_OPTION_SEND_TIMEOUT 5
#define INTERNET_OPTION_RECEIVE_TIMEOUT 6
#define INTERNET_OPTION_SECURITY_FLAGS 31
#define INTERNET_OPTION_USER_AGENT 41
#define INTERNET_OPTION_PROXY_USERNAME 43
#define INTERNET_OPTION_PROXY_PASSWORD 44
#define INTERNET_OPTION_CLIENT_CERT_CONTEXT 84
#define INTERNET_OPTION_SERVER_CERT_CHAIN_CONTEXT 105
#define INTERNET_OPTION_ENABLE_HTTP_PROTOCOL 148
#define HTTP_PROTOCOL_FLAG_HTTP2 0x2
#define INTERNET_STATUS_REQUEST_COMPLETE 100
#define INTERNET_INVALID_STATUS_CALLBACK ((INTERNET_STATUS_CALLBACK)(-1L))
#define ERROR_IO_PENDING 997
#define HTTP_ADDREQ_FLAG_ADD 0x20000000
#define HTTP_ADDREQ_FLAG_REPLACE 0x80000000
#define HTTP_QUERY_STATUS_CODE 19
#define HTTP_QUERY_FLAG_NUMBER 0x20000000
#define HTTP_QUERY_SET_COOKIE 43
#define ERROR_INSUFFICIENT_BUFFER 122
#define INTERNET_SCHEME_HTTP 1
#define INTERNET_SCHEME_HTTPS 2

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
static INTERNET_STATUS_CALLBACK s_mock_wininet_status_cb = NULL;
static DWORD_PTR s_mock_wininet_async_ctx = 0;

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
  if (g_mock_wininet_open_fail == 1) {
    return NULL;
  }
  if (dwFlags & INTERNET_FLAG_ASYNC) {
    return (HINTERNET)(size_t)301;
  }
  return (HINTERNET)(size_t)201;
}

static INTERNET_STATUS_CALLBACK
InternetSetStatusCallbackW(HINTERNET hInternet,
                           INTERNET_STATUS_CALLBACK lpfnInternetCallback) {
  INTERNET_STATUS_CALLBACK prev = s_mock_wininet_status_cb;
  (void)hInternet;
  if (g_mock_wininet_open_fail == 2 && lpfnInternetCallback != NULL) {
    return INTERNET_INVALID_STATUS_CALLBACK;
  }
  s_mock_wininet_status_cb = lpfnInternetCallback;
  return prev;
}

static BOOL InternetCloseHandle(HINTERNET hInternet) {
  if (hInternet == (HINTERNET)(size_t)301) {
    s_mock_wininet_status_cb = NULL;
    s_mock_wininet_async_ctx = 0;
  }
  if (g_mock_wininet_close_fail < 0) {
    return 0;
  }
  if (g_mock_wininet_close_fail > 0) {
    if (--g_mock_wininet_close_fail == 0) {
      return 0;
    }
    return 1;
  }
  return 1;
}

static BOOL InternetSetOption(HINTERNET hInternet, DWORD dwOption,
                              LPVOID lpBuffer, DWORD dwBufferLength) {
  if (hInternet || dwOption || lpBuffer || dwBufferLength) {
  }
  if (g_mock_wininet_set_option_fail) {
    return 0;
  }
  return 1;
}

#if !defined(_WIN32)
static BOOL InternetQueryOption(HINTERNET hInternet, DWORD dwOption,
                                LPVOID lpBuffer, LPDWORD lpdwBufferLength) {
  static const unsigned char k_bad_der[] = {0x30, 0x00};
  if (hInternet) {
  }
  if (dwOption == INTERNET_OPTION_SERVER_CERT_CHAIN_CONTEXT && lpBuffer &&
      lpdwBufferLength &&
      *lpdwBufferLength >= sizeof(CAH_CERT_CHAIN_CONTEXT *)) {
    CAH_CERT_CONTEXT *leaf;
    CAH_CERT_CHAIN_CONTEXT *chain = NULL;
    int saved_cm = s_mock_wincrypt_chain_mode;
    if (s_mock_wincrypt_query_cert_mode == 1) {
      return 0;
    }
    if (s_mock_wincrypt_query_cert_mode == 3) {
      chain = (CAH_CERT_CHAIN_CONTEXT *)calloc(1, sizeof(*chain));
      if (!chain) {
        return 0;
      }
      *(CAH_CERT_CHAIN_CONTEXT **)lpBuffer = chain;
      *lpdwBufferLength = (DWORD)sizeof(CAH_CERT_CHAIN_CONTEXT *);
      return 1;
    }
    leaf = CertCreateCertificateContext(
        CAH_X509_PKCS7_ENCODING,
        s_mock_wincrypt_query_cert_mode == 2 ? k_bad_der
                                             : k_cah_mock_server_cert_der,
        s_mock_wincrypt_query_cert_mode == 2
            ? (DWORD)sizeof(k_bad_der)
            : (DWORD)sizeof(k_cah_mock_server_cert_der));
    if (!leaf) {
      return 0;
    }
    s_mock_wincrypt_chain_mode = 0;
    if (!CertGetCertificateChain(NULL, leaf, NULL, NULL, NULL, 0, NULL,
                                 &chain)) {
      s_mock_wincrypt_chain_mode = saved_cm;
      CertFreeCertificateContext(leaf);
      return 0;
    }
    s_mock_wincrypt_chain_mode = saved_cm;
    CertFreeCertificateContext(leaf);
    *(CAH_CERT_CHAIN_CONTEXT **)lpBuffer = chain;
    *lpdwBufferLength = (DWORD)sizeof(CAH_CERT_CHAIN_CONTEXT *);
    return 1;
  }
  return 0;
}
#endif

static BOOL InternetSetOptionA(HINTERNET hInternet, DWORD dwOption,
                               LPVOID lpBuffer, DWORD dwBufferLength) {
  (void)hInternet;
  (void)dwOption;
  (void)lpBuffer;
  (void)dwBufferLength;
  if (g_mock_wininet_set_option_fail == 1) {
    return 0;
  }
  if (g_mock_wininet_set_option_fail == 2 &&
      dwOption == INTERNET_OPTION_PROXY_PASSWORD) {
    return 0;
  }
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
  if (g_mock_wininet_crack_url_fail) {
    return 0;
  }
  p = wcsstr(lpszUrl, L"://");
  if (!p) {
    return 0;
  }
  if (p - lpszUrl == 5) {
    if (wcsncmp(lpszUrl, L"https", 5) == 0) {
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
    unsigned int port_val;
    port_val = 0;
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

static HINTERNET InternetConnectW(HINTERNET hInternet,
                                  const wchar_t *lpszServerName,
                                  INTERNET_PORT nServerPort,
                                  const wchar_t *lpszUsername,
                                  const wchar_t *lpszPassword, DWORD dwService,
                                  DWORD dwFlags, DWORD_PTR dwContext) {
  (void)lpszServerName;
  (void)nServerPort;
  (void)lpszUsername;
  (void)lpszPassword;
  (void)dwService;
  (void)dwFlags;
  (void)dwContext;
  if (g_mock_wininet_connect_fail) {
    return NULL;
  }
  if (hInternet == (HINTERNET)(size_t)301) {
    return (HINTERNET)(size_t)302;
  }
  return (HINTERNET)(size_t)202;
}

static HINTERNET HttpOpenRequestW(HINTERNET hConnect, const wchar_t *lpszVerb,
                                  const wchar_t *lpszObjectName,
                                  const wchar_t *lpszVersion,
                                  const wchar_t *lpszReferrer,
                                  const wchar_t **lplpszAcceptTypes,
                                  DWORD dwFlags, DWORD_PTR dwContext) {
  (void)lpszVerb;
  (void)lpszObjectName;
  (void)lpszVersion;
  (void)lpszReferrer;
  (void)lplpszAcceptTypes;
  (void)dwFlags;
  if (g_mock_wininet_open_request_fail) {
    return NULL;
  }
  if (hConnect == (HINTERNET)(size_t)302) {
    s_mock_wininet_async_ctx = dwContext;
    return (HINTERNET)(size_t)303;
  }
  return (HINTERNET)(size_t)203;
}

static BOOL HttpAddRequestHeadersW(HINTERNET hRequest,
                                   const wchar_t *lpszHeaders,
                                   DWORD dwHeadersLength, DWORD dwModifiers) {
  (void)hRequest;
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)dwModifiers;
  if (g_mock_wininet_add_headers_fail) {
    return 0;
  }
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
  if (g_mock_wininet_send_request_ex_fail) {
    return 0;
  }
  return 1;
}

static BOOL InternetWriteFile(HINTERNET hFile, LPCVOID lpBuffer,
                              DWORD dwNumberOfBytesToWrite,
                              LPDWORD lpdwNumberOfBytesWritten) {
  (void)hFile;
  (void)lpBuffer;
  if (g_mock_wininet_write_file_fail) {
    return 0;
  }
  *lpdwNumberOfBytesWritten = dwNumberOfBytesToWrite;
  return 1;
}

static BOOL HttpEndRequestW(HINTERNET hRequest, INTERNET_BUFFERSW *lpBuffersOut,
                            DWORD dwFlags, DWORD dwContext) {
  (void)hRequest;
  (void)lpBuffersOut;
  (void)dwFlags;
  (void)dwContext;
  if (g_mock_wininet_end_request_fail) {
    return 0;
  }
  return 1;
}

static BOOL HttpSendRequestW(HINTERNET hRequest, const wchar_t *lpszHeaders,
                             DWORD dwHeadersLength, LPVOID lpOptional,
                             DWORD dwOptionalLength) {
  (void)lpszHeaders;
  (void)dwHeadersLength;
  (void)lpOptional;
  (void)dwOptionalLength;
  if (hRequest == (HINTERNET)(size_t)303 && s_mock_wininet_status_cb &&
      s_mock_wininet_async_ctx != 0) {
    INTERNET_ASYNC_RESULT ar;
    if (g_mock_wininet_send_request_fail == 2) {
      return 1;
    }
    if (g_mock_wininet_send_request_fail == 3) {
      SetLastError(12002);
      return 0;
    }
    ar.dwResult = g_mock_wininet_send_request_fail ? 0 : 1;
    ar.dwError = g_mock_wininet_send_request_fail ? 12002 : 0;
    s_mock_wininet_status_cb(hRequest, 0, 0x99999999U, NULL, 0);
    s_mock_wininet_status_cb(hRequest, s_mock_wininet_async_ctx, 0x99999999U,
                             NULL, 0);
    SetLastError(ERROR_IO_PENDING);
    s_mock_wininet_status_cb(hRequest, s_mock_wininet_async_ctx,
                             INTERNET_STATUS_REQUEST_COMPLETE, &ar, sizeof(ar));
    SetLastError(ERROR_IO_PENDING);
    return 0;
  }
  if (g_mock_wininet_send_request_fail) {
    return 0;
  }
  return 1;
}

static BOOL HttpQueryInfoW(HINTERNET hRequest, DWORD dwInfoLevel,
                           LPVOID lpBuffer, LPDWORD lpdwBufferLength,
                           LPDWORD lpdwIndex) {
  (void)hRequest;
  (void)lpdwIndex;
  if (g_mock_wininet_query_info_fail) {
    return 0;
  }
  if ((dwInfoLevel & 0xFFFF) == HTTP_QUERY_STATUS_CODE) {
    DWORD code =
        g_mock_wininet_status_code ? (DWORD)g_mock_wininet_status_code : 200;
    memcpy(lpBuffer, &code, sizeof(DWORD));
    *lpdwBufferLength = sizeof(DWORD);
    return 1;
  }
  if (g_mock_wininet_cookie_count > 0) {
    wchar_t utf_cookie[8];
    const wchar_t cookie_val1[] = L"mock_cookie=456; path=/";
    const wchar_t cookie_val2[] = L"mock_cookie2=789";
    const wchar_t *cookie_val;
    utf_cookie[0] = L'u';
    utf_cookie[1] = L'=';
    utf_cookie[2] = (wchar_t)0x00E9;
    utf_cookie[3] = (wchar_t)0x20AC;
    utf_cookie[4] = L'\0';
    cookie_val =
        (g_mock_wininet_cookie_count == 99)
            ? utf_cookie
            : ((g_mock_wininet_cookie_count == 1) ? cookie_val2 : cookie_val1);
    {
      DWORD needed_bytes = (DWORD)((wcslen(cookie_val) + 1) * sizeof(wchar_t));
      if (!lpBuffer) {
        *lpdwBufferLength = needed_bytes;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return 0;
      }
      memcpy(lpBuffer, cookie_val, needed_bytes);
      *lpdwBufferLength = needed_bytes;
      if (g_mock_wininet_cookie_count == 99) {
        g_mock_wininet_cookie_count = 0;
      } else {
        g_mock_wininet_cookie_count--;
      }
      SetLastError(0);
      return 1;
    }
  }
  SetLastError(0);
  return 0;
}

static BOOL InternetReadFile(HINTERNET hFile, LPVOID lpBuffer,
                             DWORD dwNumberOfBytesToRead,
                             LPDWORD lpdwNumberOfBytesRead) {
  (void)hFile;
  if (g_mock_wininet_read_file_fail) {
    return 0;
  }
  if (g_mock_wininet_read_chunks > 0) {
    g_mock_wininet_read_chunks--;
    memset(lpBuffer, 'I', 10);
    *lpdwNumberOfBytesRead = 10;
    return 1;
  }
  (void)lpBuffer;
  (void)dwNumberOfBytesToRead;
  *lpdwNumberOfBytesRead = 0;
  return 1;
}
#endif /* !defined(_WIN32) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_WININET_H */
