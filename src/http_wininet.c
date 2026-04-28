/**
 * @file http_wininet.c
 * @brief Implementation of the WinInet backend.
 *
 * Checks for multipart data (which should be flattened by the caller or
 * wrapper) prior to transmission.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#ifdef _WIN32

#include "win_compat_sym.h"
#include <windef.h>
#include <winnt.h>
#include <winbase.h>
#include <winerror.h>
#include <wininet.h>
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#pragma comment(lib, "wininet.lib")
#endif

#endif

#include <cfs/cfs.h>
#include <c_abstract_http/http_wininet.h>
#include "functions/parse/str.h"
/* clang-format on */

/**
 * @brief Helper to convert ASCII to wide string.
 * @param[in] s Source string.
 * @param[out] ws Destination wide string.
 * @param[in] buf_cap Capacity of destination buffer.
 * @param[out] out_len Pointer to store the number of characters written.
 * @return 0 on success, EINVAL on error.
 */
static int ascii_to_wide(const char *s, wchar_t *ws, size_t buf_cap,
                         size_t *out_len) {
  cfs_size_t written = 0;
  if (cfs_mb_to_wide(s, ws, (cfs_size_t)buf_cap, &written) != 0 || written == 0)
    return EINVAL;
  *out_len = (size_t)(written - 1);
  return 0;
}

/**
 * @brief Helper to convert wide string to ASCII.
 * @param[in] ws Source wide string.
 * @param[out] s Destination string.
 * @param[in] buf_cap Capacity of destination buffer.
 * @param[out] out_len Pointer to store the number of characters written.
 * @return 0 on success, EINVAL on error.
 */
static int wide_to_ascii(const wchar_t *ws, char *s, size_t buf_cap,
                         size_t *out_len) {
  cfs_size_t written = 0;
  if (cfs_wide_to_mb(ws, s, (cfs_size_t)buf_cap, &written) != 0 || written == 0)
    return EINVAL;
  *out_len = (size_t)(written - 1);
  return 0;
}

/** @brief CHECK_EINVAL definition */
#define CHECK_EINVAL(x)                                                        \
  do {                                                                         \
    if (!(x))                                                                  \
      return EINVAL;                                                           \
  } while (0)

/**
 * @brief Opaque context definition.
 * Holds the root internet session and cached security configuration.
 */
struct HttpTransportContext {
  HINTERNET hInternet;  /**< Root handle from InternetOpen */
  DWORD security_flags; /**< Flags to apply to requests (e.g. ignore cert) */
  char *proxy_username; /**< @brief Documented */
  char *proxy_password; /**< @brief Documented */
  struct HttpCookieJar *cookie_jar;
  struct HttpConfig config;
};

/* --- Internal Helpers --- */

#ifdef _WIN32
/**
 * @brief Safely close a WinInet handle and NULL it.
 */
static /**
        * @brief Executes the safe_close_handle operation.
        */
    void
    safe_close_handle(HINTERNET *h) {
  if (h && *h) {
    InternetCloseHandle(*h);
    *h = NULL;
  }
}

/**
 * @brief Convert HttpMethod enum to wide string verb.
 */
static /**
        * @brief Executes the method_to_wide operation.
        */
    int
    method_to_wide(enum HttpMethod method, const wchar_t **out) {
  switch (method) {
  case HTTP_GET:
    *out = L"GET";
    return 0;
  case HTTP_POST:
    *out = L"POST";
    return 0;
  case HTTP_PUT:
    *out = L"PUT";
    return 0;
  case HTTP_DELETE:
    *out = L"DELETE";
    return 0;
  case HTTP_HEAD:
    *out = L"HEAD";
    return 0;
  case HTTP_PATCH:
    *out = L"PATCH";
    return 0;
  case HTTP_OPTIONS:
    *out = L"OPTIONS";
    return 0;
  case HTTP_TRACE:
    *out = L"TRACE";
    return 0;
  case HTTP_QUERY:
    *out = L"QUERY";
    return 0;
  case HTTP_CONNECT:
    *out = L"CONNECT";
    return 0;
  default:
    *out = L"GET";
    return 0;
  }
}

/**
 * @brief Construct raw header string block from HttpHeaders.
 * Returns a malloc'd wide string (Key: Value\r\n...) or NULL.
 */
static /**
        * @brief Executes the headers_to_wide_block operation.
        */
    int
    headers_to_wide_block(const struct HttpHeaders *headers, wchar_t **out) {
  size_t i;
  size_t total_wchars = 0;
  wchar_t *buf, *p;

  *out = NULL;
  if (!headers || headers->count == 0)
    return 0;

  /* 1. Calculate Size */
  for (i = 0; i < headers->count; ++i) {
    /* Len = key + ": " + value + "\r\n" */
    /* Assume roughly 1 wchar per char. Detailed conversion follows. */
    total_wchars += strlen(headers->headers[i].key);
    total_wchars += strlen(headers->headers[i].value);
    total_wchars += 4; /* ": " and "\r\n" */
  }
  total_wchars += 1; /* Null term */

  buf = (wchar_t *)calloc(total_wchars, sizeof(wchar_t));
  if (!buf)
    return ENOMEM;

  p = buf;
  for (i = 0; i < headers->count; ++i) {
    size_t written = 0;
    /* Key */
    if (ascii_to_wide(headers->headers[i].key, p, total_wchars - (p - buf),
                      &written) != 0) {
      free(buf);
      return EIO;
    }
    p += written;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    wcscpy_s(p, total_wchars - (p - buf), L": ");
#else
    wcscpy(p, L": ");
#endif
    p += 2;

    /* Value */
    if (ascii_to_wide(headers->headers[i].value, p, total_wchars - (p - buf),
                      &written) != 0) {
      free(buf);
      return EIO;
    }
    p += written;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    wcscpy_s(p, total_wchars - (p - buf), L"\r\n");
#else
    wcscpy(p, L"\r\n");
#endif
    p += 2;
  }

  *out = buf;
  return 0;
}
#endif /* _WIN32 */

/* --- Public API Implementation --- */

/**
 * @brief Executes the http_wininet_global_init operation.
 */
int http_wininet_global_init(void) { return 0; }

/**
 * @brief Executes the http_wininet_global_cleanup operation.
 */
void http_wininet_global_cleanup(void) {}

/**
 * @brief Executes the http_wininet_context_init operation.
 */
int http_wininet_context_init(struct HttpTransportContext **ctx) {
#ifdef _WIN32
  HINTERNET hInternet;
  DWORD flags = 0;
  int rc;

  LOG_DEBUG("http_wininet_context_init: Entering");
  CHECK_EINVAL(ctx);

  /* INTERNET_OPEN_TYPE_PRECONFIG: Use ID/system settings */
  hInternet = InternetOpenW(L"c_cdd/WinInet", INTERNET_OPEN_TYPE_PRECONFIG,
                            NULL, NULL, flags);

  if (!hInternet) {
    LOG_DEBUG("http_wininet_context_init: Error InternetOpenW failed");
    return EIO;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_wininet_context_init: Error ENOMEM");
    InternetCloseHandle(hInternet);
    return ENOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != 0) {
    LOG_DEBUG(
        "http_wininet_context_init: Error http_config_init failed with %d", rc);
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
  return 0;
#endif
}

/**
 * @brief Executes the http_wininet_context_free operation.
 */
void http_wininet_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wininet_context_free: Entering");
#ifdef _WIN32
  if (ctx) {
    if (ctx->hInternet)
      InternetCloseHandle(ctx->hInternet);
    http_config_free(&ctx->config);
    free(ctx);
  }
#endif
  LOG_DEBUG("http_wininet_context_free: Exiting");
}

/**
 * @brief Executes the http_wininet_config_apply operation.
 */
int http_wininet_config_apply(struct HttpTransportContext *ctx,
                              const struct HttpConfig *config) {
  LOG_DEBUG("http_wininet_config_apply: Entering");
#ifdef _WIN32
  DWORD timeout_connect, timeout_send, timeout_recv;
  if (!ctx || !ctx->hInternet || !config) {
    LOG_DEBUG("http_wininet_config_apply: Error EINVAL");
    return EINVAL;
  }

  /* Timeouts: WinInet uses milliseconds */
  timeout_connect = (config->connect_timeout_ms > 0)
                        ? (DWORD)config->connect_timeout_ms
                        : (DWORD)config->timeout_ms;
  timeout_send = (config->write_timeout_ms > 0)
                     ? (DWORD)config->write_timeout_ms
                     : (DWORD)config->timeout_ms;
  timeout_recv = (config->read_timeout_ms > 0) ? (DWORD)config->read_timeout_ms
                                               : (DWORD)config->timeout_ms;

  /* Apply to logical handle (Connect, Send, Receive) */
  if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_CONNECT_TIMEOUT,
                         &timeout_connect, sizeof(timeout_connect))) {
    LOG_DEBUG(
        "http_wininet_config_apply: Error InternetSetOption connect failed");
    return EIO;
  }
  if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_SEND_TIMEOUT,
                         &timeout_send, sizeof(timeout_send))) {
    LOG_DEBUG("http_wininet_config_apply: Error InternetSetOption send failed");
    return EIO;
  }
  if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                         &timeout_recv, sizeof(timeout_recv))) {
    LOG_DEBUG("http_wininet_config_apply: Error InternetSetOption recv failed");
    return EIO;
  }

  /* Cache Security Flags for HttpOpenRequest */
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
  ctx->config = *config;

  LOG_DEBUG("http_wininet_config_apply: Success");
  return 0;
#endif
}

/**
 * @brief Executes the http_wininet_send operation.
 */
int http_wininet_send(struct HttpTransportContext *ctx,
                      const struct HttpRequest *req,
                      struct HttpResponse **res) {
  LOG_DEBUG("http_wininet_send: Entering");
#ifdef _WIN32
  HINTERNET hConnect = NULL;
  HINTERNET hRequest = NULL;
  URL_COMPONENTSW urlComp;
  wchar_t *wUrl = NULL;
  wchar_t *wHost = NULL;
  wchar_t *wPath = NULL;
  wchar_t *wHeaders = NULL;
  size_t wLen;
  int rc = 0;
  const wchar_t *wmethod = NULL;
  DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);

  /* Body read logic */
  char *bodyBuf = NULL;
  char *readChunk = NULL;
  size_t bodySize = 0;
  DWORD bytesRead = 0;

  if (!ctx || !ctx->hInternet || !req || !res) {
    LOG_DEBUG("http_wininet_send: Error EINVAL");
    return EINVAL;
  }

  if (req->parts.count > 0 && !req->body) {
    /* Request has parts but not flattened. Caller error strictly,
       as we cannot modify const struct here. */
    /* See http_curl.c implementation comment for architectural reasoning */
    LOG_DEBUG("http_wininet_send: Error EINVAL (multipart not flattened)");
    return EINVAL;
  }

  /* 1. Convert URL to Wide */
  {
    size_t cap = strlen(req->url) + 1;
    wUrl = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!wUrl) {
      LOG_DEBUG("http_wininet_send: Error ENOMEM for wUrl");
      return ENOMEM;
    }
    if (ascii_to_wide(req->url, wUrl, cap, &wLen) != 0) {
      LOG_DEBUG("http_wininet_send: Error ascii_to_wide wUrl failed");
      free(wUrl);
      return EINVAL;
    }
  }

  /* 2. Crack URL */
  memset(&urlComp, 0, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);

  /* Allocate buffers for parsing */
  wHost = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  wPath = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!wHost || !wPath) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating host/path buffers");
    rc = ENOMEM;
    goto cleanup;
  }

  urlComp.lpszHostName = wHost;
  urlComp.dwHostNameLength = (DWORD)wLen + 1;
  urlComp.lpszUrlPath = wPath;
  urlComp.dwUrlPathLength = (DWORD)wLen + 1;

  if (!InternetCrackUrlW(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    LOG_DEBUG("http_wininet_send: Error InternetCrackUrlW failed");
    rc = EINVAL;
    goto cleanup;
  }

  /* 3. Connect */
  hConnect = InternetConnectW(ctx->hInternet, urlComp.lpszHostName,
                              (INTERNET_PORT)urlComp.nPort, NULL, NULL,
                              INTERNET_SERVICE_HTTP, 0, 0);
  if (!hConnect) {
    LOG_DEBUG("http_wininet_send: Error InternetConnectW failed");
    rc = EIO;
    goto cleanup;
  }

  /* Apply Proxy Credentials if using a proxy and configured */
  if (ctx->proxy_username && ctx->proxy_password) {
    if (!InternetSetOptionA(hConnect, INTERNET_OPTION_PROXY_USERNAME,
                            ctx->proxy_username,
                            (DWORD)strlen(ctx->proxy_username))) {
      LOG_DEBUG(
          "http_wininet_send: Error InternetSetOptionA proxy username failed");
      rc = EIO;
      goto cleanup;
    }
    if (!InternetSetOptionA(hConnect, INTERNET_OPTION_PROXY_PASSWORD,
                            ctx->proxy_password,
                            (DWORD)strlen(ctx->proxy_password))) {
      LOG_DEBUG(
          "http_wininet_send: Error InternetSetOptionA proxy password failed");
      rc = EIO;
      goto cleanup;
    }
  }

  /* 4. Open Request */
  if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
    dwFlags |= INTERNET_FLAG_SECURE;
    dwFlags |= ctx->security_flags; /* Apply ignore-cert flags here */
  }

  method_to_wide(req->method, &wmethod);
  hRequest = HttpOpenRequestW(hConnect, wmethod, urlComp.lpszUrlPath, NULL,
                              NULL, NULL, dwFlags, 0);
  if (!hRequest) {
    LOG_DEBUG("http_wininet_send: Error HttpOpenRequestW failed");
    rc = EIO;
    goto cleanup;
  }

  /* Set cookies from jar before headers */
  if (ctx->cookie_jar && ctx->cookie_jar->count > 0) {
    size_t i;
    for (i = 0; i < ctx->cookie_jar->count; ++i) {
      char cbuf[4096];
      wchar_t wcbuf[4096];
      size_t written = 0;
      /* format: Cookie: name=value */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(cbuf, sizeof(cbuf), "Cookie: %s=%s\r\n",
                ctx->cookie_jar->cookies[i].name,
                ctx->cookie_jar->cookies[i].value);
#else
      sprintf(cbuf, "Cookie: %s=%s\r\n", ctx->cookie_jar->cookies[i].name,
              ctx->cookie_jar->cookies[i].value);
#endif

      if (ascii_to_wide(cbuf, wcbuf, 4096, &written) == 0) {
        if (!HttpAddRequestHeadersW(hRequest, wcbuf, (DWORD)-1L,
                                    HTTP_ADDREQ_FLAG_ADD)) {
          LOG_DEBUG("http_wininet_send: Error HttpAddRequestHeadersW (cookie) "
                    "failed");
        }
      }
    }
  }

  /* 5. Headers */
  if (req->headers.count > 0) {
    if (headers_to_wide_block(&req->headers, &wHeaders) != 0) {
      LOG_DEBUG("http_wininet_send: Error ENOMEM in headers_to_wide_block");
      rc = ENOMEM;
      goto cleanup;
    }
    /* Add headers (replacing existing if needed or adding) */
    if (!HttpAddRequestHeadersW(hRequest, wHeaders, (DWORD)-1L,
                                HTTP_ADDREQ_FLAG_ADD |
                                    HTTP_ADDREQ_FLAG_REPLACE)) {
      LOG_DEBUG("http_wininet_send: Error HttpAddRequestHeadersW failed");
      rc = EIO;
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
      rc = EIO;
      goto cleanup;
    }

    while (1) {
      char chunkBuf[8192];
      size_t out_read = 0;
      DWORD dwWritten = 0;
      int cb_rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                                  sizeof(chunkBuf), &out_read);
      if (cb_rc != 0) {
        LOG_DEBUG("http_wininet_send: Error read_chunk failed with %d", cb_rc);
        rc = cb_rc;
        goto cleanup;
      }
      if (out_read == 0)
        break; /* EOF */

      if (!InternetWriteFile(hRequest, chunkBuf, (DWORD)out_read, &dwWritten)) {
        LOG_DEBUG("http_wininet_send: Error InternetWriteFile failed");
        rc = EIO;
        goto cleanup;
      }
    }

    if (!HttpEndRequestW(hRequest, NULL, 0, 0)) {
      LOG_DEBUG("http_wininet_send: Error HttpEndRequestW failed");
      rc = EIO;
      goto cleanup;
    }
  } else {
    /* body is binary safe void*, no string conversion */
    if (!HttpSendRequestW(hRequest, NULL, 0, req->body, (DWORD)req->body_len)) {
      LOG_DEBUG("http_wininet_send: Error HttpSendRequestW failed");
      rc = EIO;
      goto cleanup;
    }
  }

  /* 7. Query Info (Status Code) */
  if (!HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                      &dwStatusCode, &dwSize, NULL)) {
    LOG_DEBUG("http_wininet_send: Error HttpQueryInfoW (status_code) failed");
    rc = EIO;
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
          if (wide_to_ascii(pwszCookie, cbuf, sizeof(cbuf), &cwritten) == 0) {
            char *eq = strchr(cbuf, '=');
            if (eq) {
              char *name = cbuf;
              char *val = eq + 1;
              char *semi = strchr(val, ';');
              *eq = '\0';
              if (semi)
                *semi = '\0';

              rc = http_cookie_jar_set(ctx->cookie_jar, name, val);
              if (rc != 0) {
                LOG_DEBUG("http_wininet_send: Error http_cookie_jar_set failed "
                          "with %d",
                          rc);
              }
            }
          } else {
            LOG_DEBUG("http_wininet_send: Error wide_to_ascii cookie failed");
          }
        }
        free(pwszCookie);
      } else {
        LOG_DEBUG("http_wininet_send: Error ENOMEM allocating pwszCookie");
        break;
      }
      cbCookie = 0;
      HttpQueryInfoW(hRequest, HTTP_QUERY_SET_COOKIE, NULL, &cbCookie,
                     &dwIndex);
    }
  }

  /* 8. Read Response Body */
  readChunk = (char *)malloc(4096);
  if (!readChunk) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating readChunk");
    rc = ENOMEM;
    goto cleanup;
  }

  while (1) {
    if (!InternetReadFile(hRequest, readChunk, 4096, &bytesRead)) {
      LOG_DEBUG("http_wininet_send: Error InternetReadFile failed");
      rc = EIO;
      goto cleanup;
    }
    if (bytesRead == 0)
      break;

    if (req->on_chunk) {
      int cb_rc = req->on_chunk(req->on_chunk_user_data, readChunk, bytesRead);
      if (cb_rc != 0) {
        LOG_DEBUG("http_wininet_send: Error on_chunk failed %d", cb_rc);
        rc = cb_rc;
        goto cleanup;
      }
    } else {
      /* Append */
      char *new_buf = (char *)realloc(bodyBuf, bodySize + bytesRead + 1);
      if (!new_buf) {
        LOG_DEBUG("http_wininet_send: Error ENOMEM reallocating body");
        rc = ENOMEM;
        goto cleanup;
      }
      bodyBuf = new_buf;
      memcpy(bodyBuf + bodySize, readChunk, bytesRead);
      bodySize += bytesRead;
      bodyBuf[bodySize] = '\0'; /* Ensure null-term for text usage */
    }
  }

  /* 9. Construct Response */
  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res) {
    LOG_DEBUG("http_wininet_send: Error ENOMEM allocating new_res");
    rc = ENOMEM;
    goto cleanup;
  }

  rc = http_response_init(*res);
  if (rc != 0) {
    LOG_DEBUG("http_wininet_send: Error http_response_init failed with %d", rc);
    free(*res);
    *res = NULL;
    goto cleanup;
  }
  (*res)->status_code = (int)dwStatusCode;
  (*res)->body = bodyBuf;
  (*res)->body_len = bodySize;
  bodyBuf = NULL; /* Transferred ownership */

cleanup:
  safe_close_handle(&hRequest);
  safe_close_handle(&hConnect);
  if (bodyBuf)
    free(bodyBuf);
  if (readChunk)
    free(readChunk);
  if (wUrl)
    free(wUrl);
  if (wHost)
    free(wHost);
  if (wPath)
    free(wPath);
  if (wHeaders)
    free(wHeaders);

  if (rc == 0) {
    LOG_DEBUG("http_wininet_send: Success");
  } else {
    LOG_DEBUG("http_wininet_send: Error returning %d", rc);
  }
  return rc;
#endif
}
