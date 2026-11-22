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

#ifdef _WIN32
#include <c_abstract_http/win_compat_sym.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <winerror.h>
#include <wininet.h>
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_wininet.h"
#endif
/* clang-format on */

#ifndef INTERNET_FLAG_ASYNC
/** @brief WinINet flag enabling asynchronous handle operations */
#define INTERNET_FLAG_ASYNC 0x10000000
#endif
#ifndef SECURITY_FLAG_IGNORE_UNKNOWN_CA
/** @brief WinINet security flag ignoring untrusted CA certificates */
#define SECURITY_FLAG_IGNORE_UNKNOWN_CA 0x00000100
#endif
#ifndef INTERNET_OPTION_SECURITY_FLAGS
/** @brief WinINet option ID for TLS security flags */
#define INTERNET_OPTION_SECURITY_FLAGS 31
#endif
#ifndef INTERNET_OPTION_USER_AGENT
/** @brief WinINet option ID for session User-Agent override */
#define INTERNET_OPTION_USER_AGENT 41
#endif
#ifndef INTERNET_OPTION_ENABLE_HTTP_PROTOCOL
/** @brief WinINet option ID for enabling HTTP/2 protocol negotiation */
#define INTERNET_OPTION_ENABLE_HTTP_PROTOCOL 148
#endif
#ifndef HTTP_PROTOCOL_FLAG_HTTP2
/** @brief WinINet protocol flag enabling HTTP/2 */
#define HTTP_PROTOCOL_FLAG_HTTP2 0x2
#endif
#ifndef INTERNET_STATUS_REQUEST_COMPLETE
/** @brief WinINet status code delivered when an async request completes */
#define INTERNET_STATUS_REQUEST_COMPLETE 100
#endif
#ifndef INTERNET_INVALID_STATUS_CALLBACK
/** @brief Sentinel returned by InternetSetStatusCallback on failure */
#define INTERNET_INVALID_STATUS_CALLBACK ((INTERNET_STATUS_CALLBACK)(-1L))
#endif
#ifndef ERROR_IO_PENDING
/** @brief Win32 error code indicating overlapped asynchronous I/O in flight */
#define ERROR_IO_PENDING 997
#endif

/**
 * @brief Convert an ASCII string into a wide-character buffer.
 * @param s Source NUL-terminated ASCII string.
 * @param ws Destination wide-character buffer.
 * @param buf_cap Capacity of @p ws in wchar_t elements.
 * @param out_len Receives written wide-character count.
 */
static void ascii_to_wide(const char *s, wchar_t *ws, size_t buf_cap,
                          size_t *out_len) {
  size_t i = 0;
  while (s[i] != '\0' && i + 1 < buf_cap) {
    ws[i] = (wchar_t)(unsigned char)s[i];
    i++;
  }
  ws[i] = L'\0';
  *out_len = i;
}

/**
 * @brief Convert a wide-character string into an 8-bit ASCII buffer.
 * @param ws Source NUL-terminated wide string.
 * @param s Destination char buffer.
 * @param buf_cap Capacity of @p s in bytes.
 * @param out_len Receives written character count.
 */
static void wide_to_ascii(const wchar_t *ws, char *s, size_t buf_cap,
                          size_t *out_len) {
  size_t i = 0;
  while (ws[i] != L'\0' && i + 1 < buf_cap) {
    s[i] = (char)(ws[i] & 0xFF);
    i++;
  }
  s[i] = '\0';
  *out_len = i;
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

/**
 * @brief Close and clear a WinINet HINTERNET handle if non-NULL.
 * @param h In/out handle pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error safe_close_handle(HINTERNET *h) {
  if (*h) {
    if (!InternetCloseHandle(*h)) {
      *h = NULL;
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    *h = NULL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Map an HttpMethod enum to a wide-character HTTP verb string.
 * @param method Input HTTP method enum.
 * @param out Receives wide literal string pointer.
 */
static void method_to_wide(enum HttpMethod method, const wchar_t **out) {
  switch ((int)method) {
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
}

/**
 * @brief Serialize HttpHeaders into a CRLF-delimited wide-character block.
 * @param headers Source headers collection.
 * @param out Receives newly allocated wide header block (or NULL if empty).
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
headers_to_wide_block(const struct HttpHeaders *headers, wchar_t **out) {
  size_t i;
  size_t total_wchars = 0;
  wchar_t *buf;
  wchar_t *p;

  *out = NULL;
  if (headers->count == 0) {
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

/* Create and initialize a new WinInet transport context. */
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

/* Free the WinInet transport context and release associated resources. */
enum c_abstract_http_error
http_wininet_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wininet_context_free: Entering");
  if (ctx) {
    enum c_abstract_http_error rch;
    InternetCloseHandle(ctx->hInternet);
    ctx->hInternet = NULL;
    free(ctx->proxy_username);
    ctx->proxy_username = NULL;
    free(ctx->proxy_password);
    ctx->proxy_password = NULL;
    rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    free(ctx);
  }
  LOG_DEBUG("http_wininet_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration settings to an active WinInet context. */
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

  if (config->tls_version_mask != 0 &&
      (config->tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  if ((config->version_mask & HTTP_VERSION_3) && !config->http3_fallback &&
      !(config->version_mask &
        (HTTP_VERSION_2 | HTTP_VERSION_1_1 | HTTP_VERSION_1_0))) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.version_mask = config->version_mask;
  ctx->config.tls_version_mask = config->tls_version_mask;
  ctx->config.http3_fallback = config->http3_fallback;

  ctx->security_flags = 0;
  if (!config->verify_peer) {
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

  if (config->user_agent &&
      strcmp(config->user_agent, "c-abstract-http/1.0") != 0 &&
      strcmp(config->user_agent, "c_abstract_http/WinInet") != 0) {
    free(ctx->config.user_agent);
    ctx->config.user_agent = NULL;
    {
      enum c_abstract_http_error urc =
          c_abstract_http_strdup(config->user_agent, &ctx->config.user_agent);
      if (urc != C_ABSTRACT_HTTP_SUCCESS) {
        return urc;
      }
    }
    InternetSetOptionA(ctx->hInternet, INTERNET_OPTION_USER_AGENT,
                       (LPVOID)ctx->config.user_agent,
                       (DWORD)strlen(ctx->config.user_agent));
  }

  if (config->version_mask != 0 &&
      config->version_mask != (unsigned int)HTTP_VERSION_DEFAULT &&
      (config->version_mask & HTTP_VERSION_2)) {
    DWORD h2 = HTTP_PROTOCOL_FLAG_HTTP2;
    InternetSetOption(ctx->hInternet, INTERNET_OPTION_ENABLE_HTTP_PROTOCOL, &h2,
                      sizeof(h2));
  }

  LOG_DEBUG("http_wininet_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Send an HTTP request synchronously using WinInet. */
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
      rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                           sizeof(chunkBuf), &out_read);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_wininet_send: Error read_chunk failed with %d",
                  (int)rc);
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
      if (!pwszCookie) {
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        goto cleanup;
      }
      HttpQueryInfoW(hRequest, HTTP_QUERY_SET_COOKIE, pwszCookie, &cbCookie,
                     &dwIndex);
      {
        char cbuf[4096];
        size_t cwritten = 0;
        wide_to_ascii(pwszCookie, cbuf, sizeof(cbuf), &cwritten);
        {
          char *eq = strchr(cbuf, '=');
          const char *name = cbuf;
          const char *val = eq + 1;
          char *semi = strchr(val, ';');
          *eq = '\0';
          if (semi) {
            *semi = '\0';
          }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
          if (g_mock_wininet_cookie_set_fail) {
            LOG_DEBUG("http_wininet_send: mock http_cookie_jar_set failure");
          } else
#endif
          {
            rc = http_cookie_jar_set(ctx->cookie_jar, name, val);
            if (rc != C_ABSTRACT_HTTP_SUCCESS) {
              LOG_DEBUG("http_wininet_send: http_cookie_jar_set failed with %d",
                        (int)rc);
              free(pwszCookie);
              goto cleanup;
            }
          }
        }
      }
      free(pwszCookie);
      cbCookie = 0;
      HttpQueryInfoW(hRequest, HTTP_QUERY_SET_COOKIE, NULL, &cbCookie,
                     &dwIndex);
    }
  }

  /* 8. Read Response Body */
  readChunk = (char *)malloc(4096);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_read_chunk_alloc_fail) {
    free(readChunk);
    readChunk = NULL;
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
      rc = req->on_chunk(req->on_chunk_user_data, readChunk, bytesRead);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_wininet_send: Error on_chunk failed %d", (int)rc);
        goto cleanup;
      }
    } else {
      char *new_buf = (char *)realloc(bodyBuf, bodySize + bytesRead + 1);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_wininet_body_realloc_fail) {
        free(new_buf);
        new_buf = NULL;
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
    free(*res);
    *res = NULL;
  }
#endif
  if (!*res) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_wininet_response_init_fail) {
    free(*res);
    *res = NULL;
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
#endif
  rc = http_response_init(*res);
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

cleanup: {
  enum c_abstract_http_error close_req_rc;
  enum c_abstract_http_error close_conn_rc;
  close_req_rc = safe_close_handle(&hRequest);
  if (close_req_rc != C_ABSTRACT_HTTP_SUCCESS) {
    close_conn_rc = safe_close_handle(&hConnect);
    if (close_conn_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(bodyBuf);
      free(readChunk);
      free(wUrl);
      free(wHost);
      free(wPath);
      free(wHeaders);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      return close_conn_rc;
    }
    free(bodyBuf);
    free(readChunk);
    free(wUrl);
    free(wHost);
    free(wPath);
    free(wHeaders);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    return close_req_rc;
  }
  close_conn_rc = safe_close_handle(&hConnect);
  if (close_conn_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(bodyBuf);
    free(readChunk);
    free(wUrl);
    free(wHost);
    free(wPath);
    free(wHeaders);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    return close_conn_rc;
  }
}
  free(bodyBuf);
  free(readChunk);
  free(wUrl);
  free(wHost);
  free(wPath);
  free(wHeaders);

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_wininet_send: Success");
  } else {
    LOG_DEBUG("http_wininet_send: Error returning %d", (int)rc);
  }
  return rc;
}

/** @brief Per-slot state for asynchronous `InternetSetStatusCallbackW` dispatch
 */
struct WinInetAsyncSlot {
  struct HttpTransportContext *ctx; /**< Transport context */
  struct ModalityEventLoop *loop;   /**< Optional event loop to wake */
  const struct HttpRequest *req;    /**< Originating HTTP request */
  struct HttpFuture *future;        /**< Output future */
  HINTERNET hAsyncRoot;             /**< Async WinINet root handle */
  HINTERNET hConnect;               /**< Connection handle */
  HINTERNET hRequest;               /**< Request handle */
  wchar_t *wHeaders;                /**< Wide request headers */
  volatile int finished;            /**< Non-zero once settled */
};

/**
 * @brief Read completed response from `slot->hRequest`, populate future, and
 * wake loop.
 * @param slot Active async slot (freed before returning).
 * @param send_err Terminal send error if request failed before reading.
 */
static void wininet_async_finish_slot(struct WinInetAsyncSlot *slot,
                                      enum c_abstract_http_error send_err) {
  struct HttpResponse *res = NULL;
  char *body = NULL;
  size_t body_len = 0;
  DWORD status_code = 0;
  DWORD sz = sizeof(status_code);
  enum c_abstract_http_error final_rc = send_err;

  if (!slot || slot->finished) {
    return;
  }
  slot->finished = 1;

  if (final_rc == C_ABSTRACT_HTTP_SUCCESS) {
    if (!HttpQueryInfoW(slot->hRequest,
                        HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &status_code, &sz, NULL)) {
      final_rc = C_ABSTRACT_HTTP_ERR_IO;
    } else {
      char chunk[4096];
      for (;;) {
        DWORD nread = 0;
        if (!InternetReadFile(slot->hRequest, chunk, sizeof(chunk), &nread)) {
          final_rc = C_ABSTRACT_HTTP_ERR_IO;
          break;
        }
        if (nread == 0) {
          break;
        }
        if (slot->req->on_chunk) {
          enum c_abstract_http_error crc =
              slot->req->on_chunk(slot->req->on_chunk_user_data, chunk, nread);
          if (crc != C_ABSTRACT_HTTP_SUCCESS) {
            final_rc = crc;
            break;
          }
        } else {
          char *nb = (char *)realloc(body, body_len + (size_t)nread + 1);
          if (!nb) {
            final_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
            break;
          }
          body = nb;
          memcpy(body + body_len, chunk, nread);
          body_len += (size_t)nread;
          body[body_len] = '\0';
        }
      }
    }
  }

  if (final_rc == C_ABSTRACT_HTTP_SUCCESS) {
    res = (struct HttpResponse *)calloc(1, sizeof(*res));
    if (!res) {
      final_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else {
      final_rc = http_response_init(res);
      if (final_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(res);
        res = NULL;
      } else {
        res->status_code = (int)status_code;
        res->body = body;
        res->body_len = body_len;
        body = NULL;
      }
    }
  }

  free(body);
  if (slot->hAsyncRoot) {
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
  }
  if (slot->hRequest) {
    InternetCloseHandle(slot->hRequest);
  }
  if (slot->hConnect) {
    InternetCloseHandle(slot->hConnect);
  }
  if (slot->hAsyncRoot) {
    InternetCloseHandle(slot->hAsyncRoot);
  }
  free(slot->wHeaders);

  slot->future->response = res;
  slot->future->error_code = final_rc;
  slot->future->is_ready = 1;
  if (slot->loop) {
    enum c_abstract_http_error wrc = http_loop_wakeup(slot->loop);
    if (wrc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("wininet_async_finish_slot: http_loop_wakeup returned %d",
                (int)wrc);
    }
  }
  free(slot);
}

/**
 * @brief `INTERNET_STATUS_CALLBACK` handling
 * `INTERNET_STATUS_REQUEST_COMPLETE`.
 * @param hInternet Handle generating notification.
 * @param dwContext Pointer to `struct WinInetAsyncSlot`.
 * @param dwInternetStatus WinINet status event.
 * @param lpvStatusInformation Pointer to `INTERNET_ASYNC_RESULT`.
 * @param dwStatusInformationLength Byte length of @p lpvStatusInformation.
 */
static void CALLBACK wininet_status_callback(HINTERNET hInternet,
                                             DWORD_PTR dwContext,
                                             DWORD dwInternetStatus,
                                             LPVOID lpvStatusInformation,
                                             DWORD dwStatusInformationLength) {
  struct WinInetAsyncSlot *slot = (struct WinInetAsyncSlot *)dwContext;
  (void)hInternet;
  (void)dwStatusInformationLength;
  if (!slot || slot->finished) {
    return;
  }
  if (dwInternetStatus == INTERNET_STATUS_REQUEST_COMPLETE) {
    INTERNET_ASYNC_RESULT *ar = (INTERNET_ASYNC_RESULT *)lpvStatusInformation;
    if (!ar || ar->dwResult == 0) {
      wininet_async_finish_slot(slot, C_ABSTRACT_HTTP_ERR_IO);
    } else {
      wininet_async_finish_slot(slot, C_ABSTRACT_HTTP_SUCCESS);
    }
  }
}

/**
 * @brief Submit one asynchronous WinINet request via
 * `InternetSetStatusCallbackW`.
 * @param ctx Transport context.
 * @param loop Optional `ModalityEventLoop` to wake on completion.
 * @param req HTTP request to dispatch.
 * @param future Output future.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error wininet_submit_async_slot(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpRequest *req, struct HttpFuture *future) {
  struct WinInetAsyncSlot *slot;
  URL_COMPONENTSW urlComp;
  wchar_t *wUrl = NULL;
  wchar_t *wHost = NULL;
  wchar_t *wPath = NULL;
  const wchar_t *wmethod = NULL;
  DWORD dwFlags =
      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | ctx->security_flags;
  size_t wLen = 0;
  size_t cap;
  enum c_abstract_http_error rc;

  if (!req || !req->url) {
    future->response = NULL;
    future->error_code = C_ABSTRACT_HTTP_ERR_INVAL;
    future->is_ready = 1;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  slot = (struct WinInetAsyncSlot *)calloc(1, sizeof(*slot));
  if (!slot) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  slot->ctx = ctx;
  slot->loop = loop;
  slot->req = req;
  slot->future = future;

  slot->hAsyncRoot =
      InternetOpenW(L"c_abstract_http/WinInet", INTERNET_OPEN_TYPE_PRECONFIG,
                    NULL, NULL, INTERNET_FLAG_ASYNC);
  if (!slot->hAsyncRoot) {
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (InternetSetStatusCallbackW(slot->hAsyncRoot, wininet_status_callback) ==
      INTERNET_INVALID_STATUS_CALLBACK) {
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  cap = strlen(req->url) + 1;
  wUrl = (wchar_t *)malloc(cap * sizeof(wchar_t));
  if (!wUrl) {
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  ascii_to_wide(req->url, wUrl, cap, &wLen);

  memset(&urlComp, 0, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);
  wHost = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  wPath = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!wHost || !wPath) {
    free(wUrl);
    free(wHost);
    free(wPath);
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  urlComp.lpszHostName = wHost;
  urlComp.dwHostNameLength = (DWORD)wLen + 1;
  urlComp.lpszUrlPath = wPath;
  urlComp.dwUrlPathLength = (DWORD)wLen + 1;

  if (!InternetCrackUrlW(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    free(wUrl);
    free(wHost);
    free(wPath);
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
    dwFlags |= INTERNET_FLAG_SECURE;
  }

  slot->hConnect = InternetConnectW(slot->hAsyncRoot, urlComp.lpszHostName,
                                    (INTERNET_PORT)urlComp.nPort, NULL, NULL,
                                    INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)slot);
  method_to_wide(req->method, &wmethod);
  if (!slot->hConnect) {
    free(wUrl);
    free(wHost);
    free(wPath);
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  slot->hRequest =
      HttpOpenRequestW(slot->hConnect, wmethod, urlComp.lpszUrlPath, NULL, NULL,
                       NULL, dwFlags, (DWORD_PTR)slot);
  free(wUrl);
  free(wHost);
  free(wPath);
  if (!slot->hRequest) {
    InternetCloseHandle(slot->hConnect);
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  rc = headers_to_wide_block(&req->headers, &slot->wHeaders);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    InternetCloseHandle(slot->hRequest);
    InternetCloseHandle(slot->hConnect);
    InternetSetStatusCallbackW(slot->hAsyncRoot, NULL);
    InternetCloseHandle(slot->hAsyncRoot);
    free(slot);
    return rc;
  }
  if (slot->wHeaders) {
    HttpAddRequestHeadersW(slot->hRequest, slot->wHeaders, (DWORD)-1L,
                           HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);
  }

  if (HttpSendRequestW(slot->hRequest, NULL, 0,
                       req->body ? (LPVOID)req->body : NULL,
                       (DWORD)req->body_len)) {
    wininet_async_finish_slot(slot, C_ABSTRACT_HTTP_SUCCESS);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (GetLastError() != ERROR_IO_PENDING) {
    wininet_async_finish_slot(slot, C_ABSTRACT_HTTP_ERR_IO);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Asynchronous multi-request dispatcher via InternetSetStatusCallbackW +
 * ModalityEventLoop. */
enum c_abstract_http_error http_wininet_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  if (!ctx || !multi || !futures) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  for (i = 0; i < multi->count; ++i) {
    enum c_abstract_http_error rc =
        wininet_submit_async_slot(ctx, loop, multi->requests[i], futures[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
