
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
#include "str.h"
/* clang-format on */

static int ascii_to_wide(const char *s, wchar_t *ws, size_t buf_cap,
                         size_t *out_len) {
  cfs_size_t written = 0;
  if (cfs_mb_to_wide(s, ws, (cfs_size_t)buf_cap, &written) != 0 || written == 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *out_len = (size_t)(written - 1);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int wide_to_ascii(const wchar_t *ws, char *s, size_t buf_cap,
                         size_t *out_len) {
  cfs_size_t written = 0;
  if (cfs_wide_to_mb(ws, s, (cfs_size_t)buf_cap, &written) != 0 || written == 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *out_len = (size_t)(written - 1);
  return C_ABSTRACT_HTTP_SUCCESS;
}

#define CHECK_EINVAL(x)                                                        \
  do {                                                                         \
    if (!(x))                                                                  \
      return C_ABSTRACT_HTTP_ERR_INVAL;                                        \
  } while (0)

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief hInternet (variable) of struct HttpTransportContext */
  HINTERNET hInternet;
  DWORD security_flags;
  /** @brief proxy_username (variable) of struct HttpTransportContext */
  /** @brief proxy_password (variable) of struct HttpTransportContext */
  char *proxy_username;
  char *proxy_password;
  struct HttpCookieJar *cookie_jar;
  struct HttpConfig config;
};

/* --- Internal Helpers --- */

#ifdef _WIN32
static void safe_close_handle(HINTERNET *h) {
  if (h && *h) {
    InternetCloseHandle(*h);
    *h = NULL;
  }
}

static int method_to_wide(enum HttpMethod method, const wchar_t **out) {
  switch (method) {
  case HTTP_GET:
    *out = L"GET";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_POST:
    *out = L"POST";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_PUT:
    *out = L"PUT";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_DELETE:
    *out = L"DELETE";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_HEAD:
    *out = L"HEAD";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_PATCH:
    *out = L"PATCH";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_OPTIONS:
    *out = L"OPTIONS";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_TRACE:
    *out = L"TRACE";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_QUERY:
    *out = L"QUERY";
    return C_ABSTRACT_HTTP_SUCCESS;
  case HTTP_CONNECT:
    *out = L"CONNECT";
    return C_ABSTRACT_HTTP_SUCCESS;
  default:
    *out = L"GET";
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

static int headers_to_wide_block(const struct HttpHeaders *headers,
                                 wchar_t **out) {
  size_t i;
  size_t total_wchars = 0;
  wchar_t *buf, *p;

  *out = NULL;
  if (!headers || headers->count == 0)
    return C_ABSTRACT_HTTP_SUCCESS;

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
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = buf;
  for (i = 0; i < headers->count; ++i) {
    size_t written = 0;
    /* Key */
    if (ascii_to_wide(headers->headers[i].key, p, total_wchars - (p - buf),
                      &written) != 0) {
      free(buf);
      return C_ABSTRACT_HTTP_ERR_IO;
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
      return C_ABSTRACT_HTTP_ERR_IO;
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
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif /* _WIN32 */

/* --- Public API Implementation --- */

enum c_abstract_http_error http_wininet_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_wininet_global_cleanup(void) {}

enum c_abstract_http_error
http_wininet_context_init(struct HttpTransportContext **ctx) {
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
  (*ctx)->config.proxy_username = NULL;
  (*ctx)->config.proxy_password = NULL;
  (*ctx)->config.cookie_jar = NULL;

  LOG_DEBUG("http_wininet_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

void http_wininet_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_wininet_context_free: Entering");
#ifdef _WIN32
  if (ctx) {
    if (ctx->hInternet)
      InternetCloseHandle(ctx->hInternet);
    if (ctx->config.proxy_username)
      free((void *)ctx->config.proxy_username);
    if (ctx->config.proxy_password)
      free((void *)ctx->config.proxy_password);
    if (ctx->config.user_agent)
      free((void *)ctx->config.user_agent);
    if (ctx->config.proxy_url)
      free((void *)ctx->config.proxy_url);
    free(ctx);
  }
#endif
  LOG_DEBUG("http_wininet_context_free: Exiting");
}

enum c_abstract_http_error
http_wininet_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
#ifdef _WIN32
  DWORD timeout_connect, timeout_send, timeout_recv;
#endif
  LOG_DEBUG("http_wininet_config_apply: Entering");
#ifdef _WIN32
  if (!ctx || !ctx->hInternet || !config) {
    LOG_DEBUG("http_wininet_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
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
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_SEND_TIMEOUT,
                         &timeout_send, sizeof(timeout_send))) {
    LOG_DEBUG("http_wininet_config_apply: Error InternetSetOption send failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (!InternetSetOption(ctx->hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                         &timeout_recv, sizeof(timeout_recv))) {
    LOG_DEBUG("http_wininet_config_apply: Error InternetSetOption recv failed");
    return C_ABSTRACT_HTTP_ERR_IO;
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

  ctx->config.cookie_jar = config->cookie_jar;
  if (ctx->config.proxy_username)
    free((void *)ctx->config.proxy_username);
  if (ctx->config.proxy_password)
    free((void *)ctx->config.proxy_password);
  if (ctx->config.user_agent)
    free((void *)ctx->config.user_agent);
  if (ctx->config.proxy_url)
    free((void *)ctx->config.proxy_url);
  ctx->config = *config;
  if (config->proxy_username) {
    char *tmp = NULL;
    CDD_STRDUP(config->proxy_username, &tmp);
    ctx->config.proxy_username = tmp;
  }
  if (config->proxy_password) {
    char *tmp = NULL;
    CDD_STRDUP(config->proxy_password, &tmp);
    ctx->config.proxy_password = tmp;
  }
  if (config->user_agent) {
    char *tmp = NULL;
    CDD_STRDUP(config->user_agent, &tmp);
    ctx->config.user_agent = tmp;
  }
  if (config->proxy_url) {
    char *tmp = NULL;
    CDD_STRDUP(config->proxy_url, &tmp);
    ctx->config.proxy_url = tmp;
  }

  LOG_DEBUG("http_wininet_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

enum c_abstract_http_error http_wininet_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
#ifdef _WIN32
  HINTERNET hConnect = NULL;
  HINTERNET hRequest = NULL;
  URL_COMPONENTSW urlComp;
  wchar_t *wUrl = NULL;
  wchar_t *wHost = NULL;
  wchar_t *wPath = NULL;
  wchar_t *wHeaders = NULL;
  size_t wLen;
  const wchar_t *wmethod = NULL;
  DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
  DWORD dwStatusCode = 0;
  DWORD dwSize = sizeof(dwStatusCode);

  /* Body read logic */
  char *bodyBuf = NULL;
  char *readChunk = NULL;
  size_t bodySize = 0;
  DWORD bytesRead = 0;
  int rc = 0;
#endif

  LOG_DEBUG("http_wininet_send: Entering");

#ifdef _WIN32
  if (!ctx || !ctx->hInternet || !req || !res) {
    LOG_DEBUG("http_wininet_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (req->parts.count > 0 && !req->body) {
    /* Request has parts but not flattened. Caller error strictly,
       as we cannot modify const struct here. */
    /* See http_curl.c implementation comment for architectural reasoning */
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
    if (ascii_to_wide(req->url, wUrl, cap, &wLen) != 0) {
      LOG_DEBUG("http_wininet_send: Error ascii_to_wide wUrl failed");
      free(wUrl);
      return C_ABSTRACT_HTTP_ERR_INVAL;
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

  /* Apply Proxy Credentials if using a proxy and configured */
  if (ctx->config.proxy_username && ctx->config.proxy_password) {
    if (!InternetSetOptionA(hConnect, INTERNET_OPTION_PROXY_USERNAME,
                            ctx->config.proxy_username,
                            (DWORD)strlen(ctx->config.proxy_username))) {
      LOG_DEBUG(
          "http_wininet_send: Error InternetSetOptionA proxy username failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
      goto cleanup;
    }
    if (!InternetSetOptionA(hConnect, INTERNET_OPTION_PROXY_PASSWORD,
                            ctx->config.proxy_password,
                            (DWORD)strlen(ctx->config.proxy_password))) {
      LOG_DEBUG(
          "http_wininet_send: Error InternetSetOptionA proxy password failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
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
    rc = C_ABSTRACT_HTTP_ERR_IO;
    goto cleanup;
  }

  /* Set cookies from jar before headers */
  if (ctx->config.cookie_jar && ctx->config.cookie_jar->count > 0) {
    size_t i;
    for (i = 0; i < ctx->config.cookie_jar->count; ++i) {
      char cbuf[4096];
      wchar_t wcbuf[4096];
      size_t written = 0;
      /* format: Cookie: name=value */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(cbuf, sizeof(cbuf), "Cookie: %s=%s\r\n",
                ctx->config.cookie_jar->cookies[i].name,
                ctx->config.cookie_jar->cookies[i].value);
#else
      sprintf(cbuf, "Cookie: %s=%s\r\n",
              ctx->config.cookie_jar->cookies[i].name,
              ctx->config.cookie_jar->cookies[i].value);
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
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      goto cleanup;
    }
    /* Add headers (replacing existing if needed or adding) */
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
    /* body is binary safe void*, no string conversion */
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
  if (ctx->config.cookie_jar) {
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
              const char *name = cbuf;
              const char *val = eq + 1;
              char *semi = strchr(val, ';');
              *eq = '\0';
              if (semi)
                *semi = '\0';

              rc = http_cookie_jar_set(ctx->config.cookie_jar, name, val);
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
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  while (1) {
    if (!InternetReadFile(hRequest, readChunk, 4096, &bytesRead)) {
      LOG_DEBUG("http_wininet_send: Error InternetReadFile failed");
      rc = C_ABSTRACT_HTTP_ERR_IO;
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
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
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
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
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
