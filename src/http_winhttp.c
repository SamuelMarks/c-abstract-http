
/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)

#include "win_compat_sym.h"
#include <windef.h>
#include <winnt.h>
#include <winbase.h>
#include <winerror.h>
#include <winhttp.h>


#endif

#include <c_abstract_http/event_loop.h>
#include <cfs/cfs.h>
#include <c_abstract_http/http_winhttp.h>
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

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#pragma comment(lib, "winhttp.lib")
#endif
#endif

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief hSession (variable) of struct HttpTransportContext */
  HINTERNET hSession;
  /** @brief security_flags (variable) of struct HttpTransportContext */
  DWORD security_flags;
  /** @brief disable_redirects (variable) of struct HttpTransportContext */
  int disable_redirects;
  struct HttpCookieJar *cookie_jar;
  struct HttpConfig config;
};
#else
struct HttpTransportContext {
  /** @brief Documented */
  int dummy;
};
#endif

/* ... (Helpers like method_to_wide, safe_close_handle omitted for brevity if
   unchanged logic is same, but providing full file content below for
   correctness) ... */

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
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
  case HTTP_PATCH:
    *out = L"PATCH";
    return C_ABSTRACT_HTTP_SUCCESS;
  default:
    *out = L"GET";
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

static void safe_close_handle(HINTERNET *h) {
  if (h && *h) {
    WinHttpCloseHandle(*h);
    *h = NULL;
  }
}

static int headers_to_wide_block(const struct HttpHeaders *headers,
                                 wchar_t **out) {
  size_t i;
  size_t total_wide_chars = 0;
  wchar_t *buf;
  wchar_t *p;

  *out = NULL;
  if (!headers || headers->count == 0)
    return C_ABSTRACT_HTTP_SUCCESS;

  for (i = 0; i < headers->count; ++i) {
    total_wide_chars += strlen(headers->headers[i].key);
    total_wide_chars += strlen(headers->headers[i].value);
    total_wide_chars += 4;
  }
  total_wide_chars += 1;

  buf = (wchar_t *)calloc(total_wide_chars, sizeof(wchar_t));
  if (!buf)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = buf;
  for (i = 0; i < headers->count; ++i) {
    size_t written = 0;
    if (ascii_to_wide(headers->headers[i].key, p, total_wide_chars - (p - buf),
                      &written) != 0) {
      free(buf);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    p += written;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    wcscpy_s(p, total_wide_chars - (p - buf), L": ");
#else
    wcscpy(p, L": ");
#endif
    p += 2;
    if (ascii_to_wide(headers->headers[i].value, p,
                      total_wide_chars - (p - buf), &written) != 0) {
      free(buf);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    p += written;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    wcscpy_s(p, total_wide_chars - (p - buf), L"\r\n");
#else
    wcscpy(p, L"\r\n");
#endif
    p += 2;
  }
  *out = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif /* _WIN32 */

enum c_abstract_http_error http_winhttp_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error http_winhttp_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_winhttp_context_init(struct HttpTransportContext **ctx) {
#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
  HINTERNET hSession;
  int rc;

  LOG_DEBUG("http_winhttp_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_winhttp_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  hSession = WinHttpOpen(L"c_cdd/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) {
    LOG_DEBUG("http_winhttp_context_init: Error EIO");
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
  if (rc != 0) {
    LOG_DEBUG(
        "http_winhttp_context_init: Error http_config_init failed with %d", rc);
    free(*ctx);
    *ctx = NULL;
    WinHttpCloseHandle(hSession);
    return rc;
  }

  (*ctx)->hSession = hSession;
  (*ctx)->security_flags = 0;
  (*ctx)->cookie_jar = NULL;

  LOG_DEBUG("http_winhttp_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  (void)ctx;
#ifdef ENOTSUP
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#else
  return C_ABSTRACT_HTTP_ERR_INVAL;
#endif
#endif
}

void http_winhttp_context_free(struct HttpTransportContext *ctx) {
#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
  LOG_DEBUG("http_winhttp_context_free: Entering");
  if (ctx) {
    if (ctx->hSession)
      WinHttpCloseHandle(ctx->hSession);
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
  LOG_DEBUG("http_winhttp_context_free: Exiting");
#else
  (void)ctx;
#endif
}

enum c_abstract_http_error
http_winhttp_config_apply(struct HttpTransportContext *ctx,
                          const struct HttpConfig *config) {
#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
  DWORD dwResolveTimeout, dwConnectTimeout, dwSendTimeout, dwReceiveTimeout;
  LOG_DEBUG("http_winhttp_config_apply: Entering");
  if (!ctx || !ctx->hSession || !config) {
    LOG_DEBUG("http_winhttp_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  dwResolveTimeout = (config->connect_timeout_ms > 0)
                         ? (DWORD)config->connect_timeout_ms
                         : (DWORD)config->timeout_ms;
  dwConnectTimeout = (config->connect_timeout_ms > 0)
                         ? (DWORD)config->connect_timeout_ms
                         : (DWORD)config->timeout_ms;
  dwSendTimeout = (config->write_timeout_ms > 0)
                      ? (DWORD)config->write_timeout_ms
                      : (DWORD)config->timeout_ms;
  dwReceiveTimeout = (config->read_timeout_ms > 0)
                         ? (DWORD)config->read_timeout_ms
                         : (DWORD)config->timeout_ms;

  if (!WinHttpSetTimeouts(ctx->hSession, dwResolveTimeout, dwConnectTimeout,
                          dwSendTimeout, dwReceiveTimeout)) {
    LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetTimeouts failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (config->proxy_url) {
    WINHTTP_PROXY_INFO proxyInfo;
    wchar_t *wProxy = NULL;
    size_t wLen = 0;
    size_t bufSize = strlen(config->proxy_url) + 1;
    wProxy = (wchar_t *)calloc(bufSize, sizeof(wchar_t));
    if (!wProxy) {
      LOG_DEBUG("http_winhttp_config_apply: Error ENOMEM");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    if (ascii_to_wide(config->proxy_url, wProxy, bufSize, &wLen) != 0) {
      LOG_DEBUG("http_winhttp_config_apply: Error ascii_to_wide failed");
      free(wProxy);
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    proxyInfo.lpszProxy = wProxy;
    proxyInfo.lpszProxyBypass = NULL;
    if (!WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY, &proxyInfo,
                          sizeof(proxyInfo))) {
      LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetOption failed");
      free(wProxy);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    free(wProxy);

    if (config->proxy_username && config->proxy_password) {
      size_t uLen = 0, pLen = 0;
      size_t uBufSize = strlen(config->proxy_username) + 1;
      size_t pBufSize = strlen(config->proxy_password) + 1;
      wchar_t *wUser = (wchar_t *)calloc(uBufSize, sizeof(wchar_t));
      wchar_t *wPass = (wchar_t *)calloc(pBufSize, sizeof(wchar_t));

      if (!wUser || !wPass) {
        LOG_DEBUG(
            "http_winhttp_config_apply: Error ENOMEM (proxy credentials)");
        if (wUser)
          free(wUser);
        if (wPass)
          free(wPass);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }

      if (ascii_to_wide(config->proxy_username, wUser, uBufSize, &uLen) == 0 &&
          ascii_to_wide(config->proxy_password, wPass, pBufSize, &pLen) == 0) {

        if (!WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_USERNAME,
                              wUser, (DWORD)(uLen * sizeof(wchar_t)))) {
          LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetOption proxy "
                    "user failed");
        }
        if (!WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_PASSWORD,
                              wPass, (DWORD)(pLen * sizeof(wchar_t)))) {
          LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetOption proxy "
                    "pass failed");
        }
      } else {
        LOG_DEBUG("http_winhttp_config_apply: Error ascii_to_wide credentials "
                  "failed");
      }

      free(wUser);
      free(wPass);
    }
  } else {
    WINHTTP_PROXY_INFO proxyInfo;
    proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    proxyInfo.lpszProxy = NULL;
    proxyInfo.lpszProxyBypass = NULL;
    if (!WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY, &proxyInfo,
                          sizeof(proxyInfo))) {
      LOG_DEBUG("http_winhttp_config_apply: Error WinHttpSetOption clear proxy "
                "failed");
    }
  }

  ctx->security_flags = 0;
  if (!config->verify_peer) {
    ctx->security_flags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    ctx->security_flags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
  }
  if (!config->verify_host) {
    ctx->security_flags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
  }

  ctx->disable_redirects = !config->follow_redirects;
  ctx->cookie_jar = config->cookie_jar;

  return C_ABSTRACT_HTTP_SUCCESS;
#else
  (void)ctx;
  (void)config;
#ifdef ENOTSUP
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#else
  return C_ABSTRACT_HTTP_ERR_INVAL;
#endif
#endif
}

#define CLEANUP_AND_RET(err)                                                   \
  do {                                                                         \
    rc = (err);                                                                \
    goto cleanup;                                                              \
  } while (0)

enum c_abstract_http_error http_winhttp_send(struct HttpTransportContext *ctx,
                                             const struct HttpRequest *req,
                                             struct HttpResponse **res) {
#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
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
  int rc = 0;

  LOG_DEBUG("http_winhttp_send: Entering");
  if (!ctx || !ctx->hSession || !req || !res) {
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
    if (ascii_to_wide(req->url, wUrl, cap, &wLen) != 0) {
      LOG_DEBUG("http_winhttp_send: Error ascii_to_wide (wUrl) failed");
      free(wUrl);
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
  }

  memset(&urlComp, 0, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);
  hostName = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  urlPath = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!hostName || !urlPath) {
    LOG_DEBUG("http_winhttp_send: Error ENOMEM (hostName or urlPath)");
    free(wUrl);
    if (hostName)
      free(hostName);
    if (urlPath)
      free(urlPath);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  urlComp.lpszHostName = hostName;
  urlComp.dwHostNameLength = (DWORD)wLen + 1;
  urlComp.lpszUrlPath = urlPath;
  urlComp.dwUrlPathLength = (DWORD)wLen + 1;

  if (!WinHttpCrackUrl(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    LOG_DEBUG("http_winhttp_send: Error WinHttpCrackUrl failed");
    CLEANUP_AND_RET(EINVAL);
  }

  hConnect = WinHttpConnect(ctx->hSession, urlComp.lpszHostName,
                            (INTERNET_PORT)urlComp.nPort, 0);
  if (!hConnect)
    CLEANUP_AND_RET(EIO);

  method_to_wide(req->method, &wmethod);
  hRequest = WinHttpOpenRequest(
      hConnect, wmethod, urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
  if (!hRequest)
    CLEANUP_AND_RET(EIO);

  if (ctx->security_flags != 0) {
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                     &ctx->security_flags, sizeof(ctx->security_flags));
  }

  if (ctx->disable_redirects) {
    DWORD dwDisableFeature = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE,
                     &dwDisableFeature, sizeof(dwDisableFeature));
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
        WinHttpAddRequestHeaders(hRequest, wcbuf, (DWORD)-1L,
                                 WINHTTP_ADDREQ_FLAG_ADD);
      }
    }
  }

  if (req->headers.count > 0) {
    if (headers_to_wide_block(&req->headers, &wHeaders) != 0)
      CLEANUP_AND_RET(ENOMEM);
    if (wHeaders) {
      WinHttpAddRequestHeaders(hRequest, wHeaders, (DWORD)-1L,
                               WINHTTP_ADDREQ_FLAG_ADD);
    }
  }

  if (req->read_chunk) {
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0,
                            (DWORD)req->expected_body_len, 0)) {
      CLEANUP_AND_RET(EIO);
    }

    while (1) {
      char chunkBuf[8192];
      size_t out_read = 0;
      int cb_rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                                  sizeof(chunkBuf), &out_read);
      if (cb_rc != 0) {
        CLEANUP_AND_RET(cb_rc);
      }
      if (out_read == 0)
        break; /* EOF */

      {
        DWORD dwWritten = 0;
        if (!WinHttpWriteData(hRequest, chunkBuf, (DWORD)out_read,
                              &dwWritten)) {
          CLEANUP_AND_RET(EIO);
        }
      }
    }
  } else {
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            req->body ? req->body : WINHTTP_NO_REQUEST_DATA,
                            (DWORD)req->body_len, (DWORD)req->body_len, 0)) {
      CLEANUP_AND_RET(EIO);
    }
  }

  if (!WinHttpReceiveResponse(hRequest, NULL))
    CLEANUP_AND_RET(EIO);

  if (!WinHttpQueryHeaders(
          hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
          WINHTTP_NO_HEADER_INDEX)) {
    CLEANUP_AND_RET(EIO);
  }

  /* Read Body Cycle */
  readBuf = (char *)malloc(8192);
  if (!readBuf)
    CLEANUP_AND_RET(ENOMEM);

  do {
    dwSize = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
      CLEANUP_AND_RET(EIO);
    if (dwSize == 0)
      break;
    if (dwSize > 8192)
      dwSize = 8192;

    if (!WinHttpReadData(hRequest, (LPVOID)readBuf, dwSize, &dwDownloaded))
      CLEANUP_AND_RET(EIO);

    if (dwDownloaded > 0) {
      if (req->on_chunk) {
        int cb_rc =
            req->on_chunk(req->on_chunk_user_data, readBuf, dwDownloaded);
        if (cb_rc != 0) {
          CLEANUP_AND_RET(cb_rc);
        }
      } else {
        char *new_ptr =
            (char *)realloc(totalBody, totalSize + dwDownloaded + 1);
        if (!new_ptr)
          CLEANUP_AND_RET(ENOMEM);
        totalBody = new_ptr;
        memcpy(totalBody + totalSize, readBuf, dwDownloaded);
        totalSize += dwDownloaded;
        totalBody[totalSize] = '\0'; /* Null terminate for text safety */
      }
    }
  } while (dwSize > 0);

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!*res)
    CLEANUP_AND_RET(ENOMEM);
  if (http_response_init(*res) != 0) {
    free(*res);
    *res = NULL;
    CLEANUP_AND_RET(ENOMEM);
  }

  /* Extract Cookies and map back to jar */
  if (ctx->cookie_jar) {
    DWORD dwIndex = 0;
    DWORD cbCookie = 0;

    /* First call to get size of header. */
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
                        &cbCookie, &dwIndex);

    while (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      wchar_t *pwszCookie = (wchar_t *)malloc(cbCookie);
      if (pwszCookie) {
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_SET_COOKIE,
                                WINHTTP_HEADER_NAME_BY_INDEX, pwszCookie,
                                &cbCookie, &dwIndex)) {
          /* Convert back to ascii to parse simply */
          char cbuf[4096];
          size_t cwritten = 0;
          if (wide_to_ascii(pwszCookie, cbuf, sizeof(cbuf), &cwritten) == 0) {
            /* Basic parse for "name=value" until ; */
            char *eq = strchr(cbuf, '=');
            if (eq) {
              const char *name = cbuf;
              const char *val = eq + 1;
              char *semi = strchr(val, ';');
              *eq = '\0';
              if (semi)
                *semi = '\0';
              http_cookie_jar_set(ctx->cookie_jar, name, val);
            }
          }
        }
        free(pwszCookie);
      }
      /* Ready for next loop iteration */
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
  if (wUrl)
    free(wUrl);
  if (wHeaders)
    free(wHeaders);
  if (hostName)
    free(hostName);
  if (urlPath)
    free(urlPath);
  if (readBuf)
    free(readBuf);
  if (totalBody)
    free(totalBody);

  return rc;
#else
  (void)ctx;
  (void)req;
  (void)res;
#ifdef ENOTSUP
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#else
  return C_ABSTRACT_HTTP_ERR_INVAL;
#endif
#endif
}

#if defined(_WIN32) && (!defined(_MSC_VER) || _MSC_VER >= 1600)
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
  int rc;

  rc = http_winhttp_send(worker_ctx->ctx, worker_ctx->req, &res);

  worker_ctx->future->response = res;
  worker_ctx->future->error_code = rc;
  worker_ctx->future->is_ready = 1;

  http_loop_wakeup(worker_ctx->loop);
  free(worker_ctx);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_winhttp_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  LOG_DEBUG("http_winhttp_send_multi: Entering");
  if (!ctx || !loop || !multi || !futures) {
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
#else
enum c_abstract_http_error http_winhttp_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
#ifdef ENOTSUP
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#else
  return C_ABSTRACT_HTTP_ERR_INVAL;
#endif
}
#endif
