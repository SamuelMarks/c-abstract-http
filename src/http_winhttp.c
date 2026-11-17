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

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
#include <c_abstract_http/win_compat_sym.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <winerror.h>
#include <winhttp.h>
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#include "mock_stubs/mock_winhttp.h"
#endif
/* clang-format on */

/**
 * @brief Convert an ASCII string into a wide-character buffer.
 * @param s Source NUL-terminated ASCII string.
 * @param ws Destination wide-character buffer.
 * @param buf_cap Capacity of @p ws in wchar_t elements.
 * @param out_len Receives written wide-character count.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
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

/**
 * @brief Convert a wide-character string into an 8-bit ASCII buffer.
 * @param ws Source NUL-terminated wide string.
 * @param s Destination char buffer.
 * @param buf_cap Capacity of @p s in bytes.
 * @param out_len Receives written character count.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
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

/**
 * @brief Map an HttpMethod enum to a wide-character HTTP verb string.
 * @param method Input HTTP method enum.
 * @param out Receives wide literal string pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
static enum c_abstract_http_error method_to_wide(enum HttpMethod method,
                                                 const wchar_t **out) {
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

/**
 * @brief Close and clear a WinHTTP HINTERNET handle if non-NULL.
 * @param h In/out handle pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
static enum c_abstract_http_error safe_close_handle(HINTERNET *h) {
  if (*h) {
    if (!WinHttpCloseHandle(*h)) {
      *h = NULL;
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    *h = NULL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
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
  size_t total_wide_chars = 0;
  wchar_t *buf;
  wchar_t *p;

  *out = NULL;
  if (headers->count == 0) {
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
    enum c_abstract_http_error conv_rc;
    conv_rc = ascii_to_wide(headers->headers[i].key, p,
                            total_wide_chars - (size_t)(p - buf), &written);
    if (conv_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf);
      return conv_rc;
    }
    p += written;
    p[0] = L':';
    p[1] = L' ';
    p += 2;
    conv_rc = ascii_to_wide(headers->headers[i].value, p,
                            total_wide_chars - (size_t)(p - buf), &written);
    if (conv_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf);
      return conv_rc;
    }
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

/* Create and initialize a new WinHTTP transport context. */
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

/* Free the WinHTTP transport context and release associated resources. */
enum c_abstract_http_error
http_winhttp_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_winhttp_context_free: Entering");
  if (ctx) {
    enum c_abstract_http_error rch;
    WinHttpCloseHandle(ctx->hSession);
    ctx->hSession = NULL;
    rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    free(ctx);
  }
  LOG_DEBUG("http_winhttp_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration settings to an active WinHTTP context. */
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
    {
      enum c_abstract_http_error conv_rc =
          ascii_to_wide(config->proxy_url, wProxy, cap, &wProxyLen);
      if (conv_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wProxy);
        return conv_rc;
      }
    }

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
      enum c_abstract_http_error conv_rc;
      wUser = (wchar_t *)malloc(ucap * sizeof(wchar_t));
      if (!wUser) {
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      wPass = (wchar_t *)malloc(pcap * sizeof(wchar_t));
      if (!wPass) {
        free(wUser);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      conv_rc = ascii_to_wide(config->proxy_username, wUser, ucap, &wUserLen);
      if (conv_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wUser);
        free(wPass);
        return conv_rc;
      }
      conv_rc = ascii_to_wide(config->proxy_password, wPass, pcap, &wPassLen);
      if (conv_rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wUser);
        free(wPass);
        return conv_rc;
      }
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

/* Send an HTTP request synchronously using WinHTTP. */
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
    rc = ascii_to_wide(req->url, wUrl, cap, &wLen);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(wUrl);
      return rc;
    }
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

  rc = method_to_wide(req->method, &wmethod);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    goto cleanup;
  }

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
      rc = ascii_to_wide(cbuf, wcbuf, 4096, &written);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup;
      }
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
      rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                           sizeof(chunkBuf), &out_read);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
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
    free(readBuf);
    readBuf = NULL;
  }
#endif
  if (!readBuf) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  for (;;) {
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

    if (req->on_chunk) {
      rc = req->on_chunk(req->on_chunk_user_data, readBuf, dwDownloaded);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto cleanup;
      }
    } else {
      char *new_ptr = (char *)realloc(totalBody, totalSize + dwDownloaded + 1);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_winhttp_total_body_realloc_fail) {
        free(new_ptr);
        new_ptr = NULL;
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

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_res_alloc_fail) {
    free(*res);
    *res = NULL;
  }
#endif
  if (!*res) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_response_init_fail) {
    free(*res);
    *res = NULL;
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }
#endif
  rc = http_response_init(*res);
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
      WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE,
                          WINHTTP_HEADER_NAME_BY_INDEX, pwszCookie, &cbCookie,
                          &dwIndex);
      {
        char cbuf[4096];
        size_t cwritten = 0;
        rc = wide_to_ascii(pwszCookie, cbuf, sizeof(cbuf), &cwritten);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(pwszCookie);
          goto cleanup;
        }
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
          if (g_mock_winhttp_cookie_set_fail) {
            LOG_DEBUG("http_winhttp_send: mock http_cookie_jar_set failure");
          } else
#endif
          {
            enum c_abstract_http_error cookie_rc =
                http_cookie_jar_set(ctx->cookie_jar, name, val);
            if (cookie_rc != C_ABSTRACT_HTTP_SUCCESS) {
              LOG_DEBUG("http_winhttp_send: http_cookie_jar_set failed with %d",
                        (int)cookie_rc);
              free(pwszCookie);
              if (*res) {
                enum c_abstract_http_error fr_rc = http_response_free(*res);
                free(*res);
                *res = NULL;
                if (fr_rc != C_ABSTRACT_HTTP_SUCCESS) {
                  return fr_rc;
                }
              }
              rc = cookie_rc;
              free(wUrl);
              free(wHeaders);
              free(hostName);
              free(urlPath);
              free(readBuf);
              free(totalBody);
              return cookie_rc;
            }
          }
        }
      }
      free(pwszCookie);
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

cleanup: {
  enum c_abstract_http_error close_req_rc;
  enum c_abstract_http_error close_conn_rc;
  close_req_rc = safe_close_handle(&hRequest);
  if (close_req_rc != C_ABSTRACT_HTTP_SUCCESS) {
    close_conn_rc = safe_close_handle(&hConnect);
    if (close_conn_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(wUrl);
      free(wHeaders);
      free(hostName);
      free(urlPath);
      free(readBuf);
      free(totalBody);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      return close_conn_rc;
    }
    free(wUrl);
    free(wHeaders);
    free(hostName);
    free(urlPath);
    free(readBuf);
    free(totalBody);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    return close_req_rc;
  }
  close_conn_rc = safe_close_handle(&hConnect);
  if (close_conn_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(wUrl);
    free(wHeaders);
    free(hostName);
    free(urlPath);
    free(readBuf);
    free(totalBody);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    return close_conn_rc;
  }
}
  free(wUrl);
  free(wHeaders);
  free(hostName);
  free(urlPath);
  free(readBuf);
  free(totalBody);

  return rc;
}

/** @brief Internal struct WinHttpAsyncWorkerCtx */
struct WinHttpAsyncWorkerCtx {
  struct HttpTransportContext *ctx; /**< Transport context */
  struct ModalityEventLoop *loop;   /**< Event loop */
  const struct HttpRequest *req;    /**< HTTP request */
  struct HttpFuture *future;        /**< Output future */
};

/**
 * @brief Execute one asynchronous WinHTTP request and wake the event loop.
 * @param worker_ctx Heap-allocated worker context (freed before return).
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
winhttp_async_worker_impl(struct WinHttpAsyncWorkerCtx *worker_ctx) {
  struct HttpResponse *res = NULL;
  enum c_abstract_http_error rc;

  rc = http_winhttp_send(worker_ctx->ctx, worker_ctx->req, &res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    worker_ctx->future->response = res;
    worker_ctx->future->error_code = rc;
    worker_ctx->future->is_ready = 1;
    if (worker_ctx->loop) {
      enum c_abstract_http_error rc_wake = http_loop_wakeup(worker_ctx->loop);
      if (rc_wake != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_loop_wakeup failed: %d", (int)rc_wake);
        free(worker_ctx);
        return rc_wake;
      }
    }
    free(worker_ctx);
    return rc;
  }

  worker_ctx->future->response = res;
  worker_ctx->future->error_code = rc;
  worker_ctx->future->is_ready = 1;

  if (worker_ctx->loop) {
    enum c_abstract_http_error rc_wake = http_loop_wakeup(worker_ctx->loop);
    if (rc_wake != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("http_loop_wakeup failed: %d", (int)rc_wake);
      free(worker_ctx);
      return rc_wake;
    }
  }
  free(worker_ctx);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Win32 CreateThread entry point for asynchronous WinHTTP requests.
 * @param lpParam Pointer to struct WinHttpAsyncWorkerCtx.
 * @return Thread exit code 0.
 */
static DWORD WINAPI winhttp_async_worker(LPVOID lpParam) {
  struct WinHttpAsyncWorkerCtx *worker_ctx =
      (struct WinHttpAsyncWorkerCtx *)lpParam;
  enum c_abstract_http_error rc = winhttp_async_worker_impl(worker_ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return (DWORD)((int)rc * 0);
  }
  return (DWORD)C_ABSTRACT_HTTP_SUCCESS;
}

/* Send multiple HTTP requests asynchronously via WinHTTP and worker pool. */
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
