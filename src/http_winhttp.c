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
/** @brief Exclude rarely-used APIs from Win32 headers */
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

#ifndef WINHTTP_OPTION_USER_AGENT
/** @brief WinHTTP option ID for overriding session/request User-Agent */
#define WINHTTP_OPTION_USER_AGENT 11
#endif
#ifndef WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL
/** @brief WinHTTP option ID for enabling HTTP/2 and HTTP/3 protocols */
#define WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL 133
#endif
#ifndef WINHTTP_PROTOCOL_FLAG_HTTP2
/** @brief WinHTTP protocol bitmask flag for HTTP/2 */
#define WINHTTP_PROTOCOL_FLAG_HTTP2 0x1
#endif
#ifndef WINHTTP_PROTOCOL_FLAG_HTTP3
/** @brief WinHTTP protocol bitmask flag for HTTP/3 */
#define WINHTTP_PROTOCOL_FLAG_HTTP3 0x2
#endif
#ifndef WINHTTP_FLAG_ASYNC
/** @brief WinHTTP flag requesting non-blocking asynchronous session I/O */
#define WINHTTP_FLAG_ASYNC 0x10000000
#endif
#ifndef WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
/** @brief WinHTTP callback status when WinHttpSendRequest completes */
#define WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE 0x00020000
#endif
#ifndef WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
/** @brief WinHTTP callback status when response headers are ready */
#define WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE 0x00040000
#endif
#ifndef WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
/** @brief WinHTTP callback status when body byte count is available */
#define WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE 0x00080000
#endif
#ifndef WINHTTP_CALLBACK_STATUS_READ_COMPLETE
/** @brief WinHTTP callback status when WinHttpReadData completes */
#define WINHTTP_CALLBACK_STATUS_READ_COMPLETE 0x00100000
#endif
#ifndef WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE
/** @brief WinHTTP callback status when WinHttpWriteData completes */
#define WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE 0x00000020
#endif
#ifndef WINHTTP_CALLBACK_STATUS_REQUEST_ERROR
/** @brief WinHTTP callback status when an asynchronous operation fails */
#define WINHTTP_CALLBACK_STATUS_REQUEST_ERROR 0x00200000
#endif
#ifndef WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS
/** @brief Combined WinHTTP completion notification mask */
#define WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS                                  \
  (WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE |                              \
   WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE |                                 \
   WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE |                                    \
   WINHTTP_CALLBACK_STATUS_READ_COMPLETE |                                     \
   WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE |                                    \
   WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
#endif
#ifndef WINHTTP_INVALID_STATUS_CALLBACK
/** @brief Sentinel returned by WinHttpSetStatusCallback on failure */
#define WINHTTP_INVALID_STATUS_CALLBACK ((WINHTTP_STATUS_CALLBACK)(-1L))
#endif
#ifndef CALLBACK
#if defined(_WIN32)
/** @brief Win32 stdcall callback calling convention */
#define CALLBACK __stdcall
#else
/** @brief Calling convention macro fallback for non-Win32 analysis */
#define CALLBACK
#endif
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Countdown before `ascii_to_wide`/`wide_to_ascii`/`method_to_wide`
 * fail */
static int g_mock_winhttp_conv_fail = 0;
#endif

/**
 * @brief Convert a UTF-8 string into a UTF-16 wide-character buffer.
 * @param s Source NUL-terminated UTF-8 string.
 * @param ws Destination wide-character buffer.
 * @param buf_cap Capacity of @p ws in wchar_t elements.
 * @param out_len Receives written wide-character count.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
ascii_to_wide(const char *s, wchar_t *ws, size_t buf_cap, size_t *out_len) {
  size_t si = 0;
  size_t wi = 0;
  if (!s || !ws || buf_cap == 0 || !out_len) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_conv_fail > 0 && --g_mock_winhttp_conv_fail == 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#endif
  while (s[si] != '\0' && wi + 1 < buf_cap) {
    unsigned char b0 = (unsigned char)s[si];
    if (b0 < 0x80U) {
      ws[wi++] = (wchar_t)b0;
      si += 1;
    } else if ((b0 & 0xE0U) == 0xC0U &&
               ((unsigned char)s[si + 1] & 0xC0U) == 0x80U) {
      unsigned long cp = (((unsigned long)(b0 & 0x1FU)) << 6) |
                         (unsigned long)((unsigned char)s[si + 1] & 0x3FU);
      ws[wi++] = (wchar_t)cp;
      si += 2;
    } else if ((b0 & 0xF0U) == 0xE0U &&
               ((unsigned char)s[si + 1] & 0xC0U) == 0x80U &&
               ((unsigned char)s[si + 2] & 0xC0U) == 0x80U) {
      unsigned long cp =
          (((unsigned long)(b0 & 0x0FU)) << 12) |
          (((unsigned long)((unsigned char)s[si + 1] & 0x3FU)) << 6) |
          (unsigned long)((unsigned char)s[si + 2] & 0x3FU);
      ws[wi++] = (wchar_t)cp;
      si += 3;
    } else if ((b0 & 0xF8U) == 0xF0U &&
               ((unsigned char)s[si + 1] & 0xC0U) == 0x80U &&
               ((unsigned char)s[si + 2] & 0xC0U) == 0x80U &&
               ((unsigned char)s[si + 3] & 0xC0U) == 0x80U &&
               wi + 2 < buf_cap) {
      unsigned long cp =
          (((unsigned long)(b0 & 0x07U)) << 18) |
          (((unsigned long)((unsigned char)s[si + 1] & 0x3FU)) << 12) |
          (((unsigned long)((unsigned char)s[si + 2] & 0x3FU)) << 6) |
          (unsigned long)((unsigned char)s[si + 3] & 0x3FU);
      cp -= 0x10000UL;
      ws[wi++] = (wchar_t)(0xD800UL + ((cp >> 10) & 0x3FFUL));
      ws[wi++] = (wchar_t)(0xDC00UL + (cp & 0x3FFUL));
      si += 4;
    } else {
      ws[wi++] = (wchar_t)b0;
      si += 1;
    }
  }
  ws[wi] = L'\0';
  *out_len = wi;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Convert a UTF-16 wide-character string into a UTF-8 buffer.
 * @param ws Source NUL-terminated wide string.
 * @param s Destination char buffer.
 * @param buf_cap Capacity of @p s in bytes.
 * @param out_len Receives written byte count.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
wide_to_ascii(const wchar_t *ws, char *s, size_t buf_cap, size_t *out_len) {
  size_t wi = 0;
  size_t si = 0;
  if (!ws || !s || buf_cap == 0 || !out_len) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_conv_fail > 0 && --g_mock_winhttp_conv_fail == 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#endif
  while (ws[wi] != L'\0' && si + 4 < buf_cap) {
    unsigned long cp = (unsigned long)ws[wi] & 0xFFFFUL;
    if (cp < 0x80UL) {
      s[si++] = (char)cp;
    } else if (cp < 0x800UL) {
      s[si++] = (char)(0xC0UL | (cp >> 6));
      s[si++] = (char)(0x80UL | (cp & 0x3FUL));
    } else {
      s[si++] = (char)(0xE0UL | (cp >> 12));
      s[si++] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
      s[si++] = (char)(0x80UL | (cp & 0x3FUL));
    }
    wi++;
  }
  s[si] = '\0';
  *out_len = si;
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
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t method_to_wide(enum HttpMethod method,
                                                         const wchar_t **out) {
  if (!out) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_winhttp_conv_fail > 0 && --g_mock_winhttp_conv_fail == 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#endif
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
NO_DISCARD static c_abstract_http_error_t safe_close_handle(HINTERNET *h) {
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
NO_DISCARD static c_abstract_http_error_t
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
    c_abstract_http_error_t rc =
        ascii_to_wide(headers->headers[i].key, p,
                      total_wide_chars - (size_t)(p - buf), &written);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf);
      return rc;
    }
    p += written;
    p[0] = L':';
    p[1] = L' ';
    p += 2;
    rc = ascii_to_wide(headers->headers[i].value, p,
                       total_wide_chars - (size_t)(p - buf), &written);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(buf);
      return rc;
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
NO_DISCARD c_abstract_http_error_t http_winhttp_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Cleanup the global WinHTTP environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD c_abstract_http_error_t http_winhttp_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Create and initialize a new WinHTTP transport context. */
NO_DISCARD c_abstract_http_error_t
http_winhttp_context_init(struct HttpTransportContext **ctx) {
  HINTERNET hSession;
  c_abstract_http_error_t rc;

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
NO_DISCARD c_abstract_http_error_t
http_winhttp_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_winhttp_context_free: Entering");
  if (ctx) {
    c_abstract_http_error_t rch;
    WinHttpCloseHandle(ctx->hSession);
    ctx->hSession = NULL;
    rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(ctx);
      return rch;
    }
    free(ctx);
  }
  LOG_DEBUG("http_winhttp_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Apply configuration settings to an active WinHTTP context. */
NO_DISCARD c_abstract_http_error_t http_winhttp_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  LOG_DEBUG("http_winhttp_config_apply: Entering");
  if (!ctx) {
    LOG_DEBUG("http_winhttp_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!config) {
    LOG_DEBUG("http_winhttp_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;

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
      c_abstract_http_error_t rc =
          ascii_to_wide(config->proxy_url, wProxy, cap, &wProxyLen);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wProxy);
        return rc;
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
      c_abstract_http_error_t rc;
      wUser = (wchar_t *)malloc(ucap * sizeof(wchar_t));
      if (!wUser) {
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      wPass = (wchar_t *)malloc(pcap * sizeof(wchar_t));
      if (!wPass) {
        free(wUser);
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      rc = ascii_to_wide(config->proxy_username, wUser, ucap, &wUserLen);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wUser);
        free(wPass);
        return rc;
      }
      rc = ascii_to_wide(config->proxy_password, wPass, pcap, &wPassLen);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wUser);
        free(wPass);
        return rc;
      }
      WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_USERNAME, wUser,
                       (DWORD)wcslen(wUser));
      WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_PASSWORD, wPass,
                       (DWORD)wcslen(wPass));
      free(wUser);
      free(wPass);
    } else if (config->proxy_username) {
      size_t ucap = strlen(config->proxy_username) + 1;
      wchar_t *wUser = (wchar_t *)malloc(ucap * sizeof(wchar_t));
      if (wUser) {
        size_t wUserLen = 0;
        c_abstract_http_error_t rc =
            ascii_to_wide(config->proxy_username, wUser, ucap, &wUserLen);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(wUser);
          return rc;
        }
        WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY_USERNAME, wUser,
                         (DWORD)wUserLen);
        free(wUser);
      }
    }
  } else {
    WINHTTP_PROXY_INFO proxyInfo;
    memset(&proxyInfo, 0, sizeof(proxyInfo));
    proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_PROXY, &proxyInfo,
                     sizeof(proxyInfo));
  }

  /* Dynamic per-config user_agent override */
  if (config->user_agent &&
      strcmp(config->user_agent, "c-abstract-http/1.0") != 0 &&
      strcmp(config->user_agent, "c_abstract_http/1.0") != 0) {
    size_t acap = strlen(config->user_agent) + 1;
    wchar_t *wAgent = (wchar_t *)malloc(acap * sizeof(wchar_t));
    if (!wAgent) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    {
      size_t wAgentLen = 0;
      c_abstract_http_error_t rc =
          ascii_to_wide(config->user_agent, wAgent, acap, &wAgentLen);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(wAgent);
        return rc;
      }
      WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_USER_AGENT, wAgent,
                       (DWORD)(wAgentLen + 1));
    }
    free(wAgent);
  }

  /* HTTP/2 & HTTP/3 protocol flags + http3_fallback */
  ctx->config.version_mask = config->version_mask;
  ctx->config.http3_fallback = config->http3_fallback;
  if (config->version_mask != (unsigned int)HTTP_VERSION_DEFAULT) {
    DWORD proto_flags = 0;
    if (config->version_mask & HTTP_VERSION_2) {
      proto_flags |= WINHTTP_PROTOCOL_FLAG_HTTP2;
    }
    if (config->version_mask & HTTP_VERSION_3) {
      proto_flags |= WINHTTP_PROTOCOL_FLAG_HTTP3;
      if (config->http3_fallback) {
        proto_flags |= WINHTTP_PROTOCOL_FLAG_HTTP2;
      }
    }
    if (proto_flags != 0) {
      if (!WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
                            &proto_flags, sizeof(proto_flags))) {
        if ((config->version_mask & HTTP_VERSION_3) &&
            !config->http3_fallback &&
            !(config->version_mask & (HTTP_VERSION_2 | HTTP_VERSION_1_1))) {
          return C_ABSTRACT_HTTP_ERR_NOTSUP;
        }
        if (proto_flags & WINHTTP_PROTOCOL_FLAG_HTTP2) {
          DWORD h2_only = WINHTTP_PROTOCOL_FLAG_HTTP2;
          WinHttpSetOption(ctx->hSession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
                           &h2_only, sizeof(h2_only));
        }
      }
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
NO_DISCARD c_abstract_http_error_t
http_winhttp_send(struct HttpTransportContext *ctx,
                  const struct HttpRequest *req, struct HttpResponse **res) {
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
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

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
      if (!pwszCookie) {
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        goto cleanup;
      }
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
            c_abstract_http_error_t cookie_rc =
                http_cookie_jar_set(ctx->cookie_jar, name, val);
            if (cookie_rc != C_ABSTRACT_HTTP_SUCCESS) {
              LOG_DEBUG("http_winhttp_send: http_cookie_jar_set failed with %d",
                        (int)cookie_rc);
              free(pwszCookie);
              free(wUrl);
              free(wHeaders);
              free(hostName);
              free(urlPath);
              free(readBuf);
              free(totalBody);
              if (*res) {
                struct HttpResponse *tmp_r = *res;
                c_abstract_http_error_t fr_rc;
                *res = NULL;
                fr_rc = http_response_free(tmp_r);
                if (fr_rc != C_ABSTRACT_HTTP_SUCCESS) {
                  free(tmp_r);
                  return fr_rc;
                }
                free(tmp_r);
              }
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
  c_abstract_http_error_t close_req_rc;
  c_abstract_http_error_t close_conn_rc;
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

/** @brief Per-request state machine driven by `WINHTTP_STATUS_CALLBACK` */
struct WinHttpAsyncSlot {
  struct HttpTransportContext *ctx; /**< Transport context */
  struct ModalityEventLoop *loop;   /**< Optional event loop to wake */
  const struct HttpRequest *req;    /**< Originating HTTP request */
  struct HttpFuture *future;        /**< Output future */
  HINTERNET hAsyncSession;          /**< Per-slot async WinHTTP session */
  HINTERNET hConnect;               /**< Connection handle */
  HINTERNET hRequest;               /**< Request handle */
  wchar_t *wHeaders;                /**< Wide request headers */
  char read_buf[8192];              /**< Async chunk read buffer */
  char *body;                       /**< Accumulated response body */
  size_t body_len;                  /**< Accumulated body length */
  DWORD status_code;                /**< HTTP status code */
  volatile int finished;            /**< Non-zero once terminal callback ran */
};

/**
 * @brief Finalize an asynchronous WinHTTP slot, populate future, and wake loop.
 * @param slot Active async slot (freed once settled).
 * @param err Terminal error code (`C_ABSTRACT_HTTP_SUCCESS` on completion).
 * @return C_ABSTRACT_HTTP_SUCCESS or wakeup/init error code.
 */
NO_DISCARD static c_abstract_http_error_t
winhttp_async_complete(struct WinHttpAsyncSlot *slot,
                       c_abstract_http_error_t err) {
  struct HttpResponse *res = NULL;
  c_abstract_http_error_t final_rc = err;

  if (!slot || slot->finished) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  slot->finished = 1;

  if (final_rc == C_ABSTRACT_HTTP_SUCCESS) {
    res = (struct HttpResponse *)calloc(1, sizeof(*res));
    if (!res) {
      final_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else {
      c_abstract_http_error_t irc = http_response_init(res);
      if (irc != C_ABSTRACT_HTTP_SUCCESS) {
        free(res);
        res = NULL;
        if (slot->hRequest) {
          WinHttpSetStatusCallback(slot->hRequest, NULL, 0, 0);
          WinHttpCloseHandle(slot->hRequest);
        }
        if (slot->hConnect) {
          WinHttpCloseHandle(slot->hConnect);
        }
        if (slot->hAsyncSession) {
          WinHttpCloseHandle(slot->hAsyncSession);
        }
        free(slot->wHeaders);
        free(slot->body);
        slot->future->response = NULL;
        slot->future->error_code = irc;
        slot->future->is_ready = 1;
        if (slot->loop) {
          c_abstract_http_error_t wrc = http_loop_wakeup(slot->loop);
          if (wrc != C_ABSTRACT_HTTP_SUCCESS) {
            free(slot);
            return wrc;
          }
        }
        free(slot);
        return irc;
      }
      res->status_code = (int)slot->status_code;
      res->body = slot->body;
      res->body_len = slot->body_len;
      slot->body = NULL;
    }
  }

  if (slot->hRequest) {
    WinHttpSetStatusCallback(slot->hRequest, NULL, 0, 0);
    WinHttpCloseHandle(slot->hRequest);
    slot->hRequest = NULL;
  }
  if (slot->hConnect) {
    WinHttpCloseHandle(slot->hConnect);
    slot->hConnect = NULL;
  }
  if (slot->hAsyncSession) {
    WinHttpCloseHandle(slot->hAsyncSession);
    slot->hAsyncSession = NULL;
  }
  free(slot->wHeaders);
  slot->wHeaders = NULL;
  free(slot->body);
  slot->body = NULL;

  slot->future->response = res;
  slot->future->error_code = final_rc;
  slot->future->is_ready = 1;

  if (slot->loop) {
    c_abstract_http_error_t wrc = http_loop_wakeup(slot->loop);
    if (wrc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slot);
      return wrc;
    }
  }
  free(slot);
  return final_rc;
}

/**
 * @brief Dispatch an enum-returning WinHTTP helper inside a void Win32
 * callback.
 * @param expr Expression returning `c_abstract_http_error_t`.
 */
#define WINHTTP_VOID_CB_DISPATCH(expr)                                         \
  do {                                                                         \
    c_abstract_http_error_t _cb_rc = (expr);                                   \
    if (_cb_rc != C_ABSTRACT_HTTP_SUCCESS) {                                   \
      LOG_DEBUG("winhttp_status_callback: error %d", (int)_cb_rc);             \
    }                                                                          \
  } while (0)

/**
 * @brief Internal enum-returning WinHTTP async callback implementation.
 * @param hInternet Request handle.
 * @param slot Active async slot.
 * @param dwInternetStatus WinHTTP callback status code.
 * @param lpvStatusInformation Status-specific payload pointer.
 * @param dwStatusInformationLength Byte length of payload or bytes transferred.
 * @return C_ABSTRACT_HTTP_SUCCESS or slot error code.
 */
NO_DISCARD static c_abstract_http_error_t winhttp_status_callback_impl(
    HINTERNET hInternet, struct WinHttpAsyncSlot *slot, DWORD dwInternetStatus,
    LPVOID lpvStatusInformation, DWORD dwStatusInformationLength) {
  switch (dwInternetStatus) {
  case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
  case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE: {
    if (slot->req->read_chunk) {
      size_t out_read = 0;
      c_abstract_http_error_t rrc =
          slot->req->read_chunk(slot->req->read_chunk_user_data, slot->read_buf,
                                sizeof(slot->read_buf), &out_read);
      if (rrc != C_ABSTRACT_HTTP_SUCCESS) {
        return winhttp_async_complete(slot, rrc);
      }
      if (out_read > 0) {
        if (!WinHttpWriteData(hInternet, slot->read_buf, (DWORD)out_read,
                              NULL)) {
          return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
        }
        return C_ABSTRACT_HTTP_SUCCESS;
      }
    }
    if (!WinHttpReceiveResponse(hInternet, NULL)) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
    DWORD dwSize = sizeof(slot->status_code);
    if (!WinHttpQueryHeaders(
            hInternet, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &slot->status_code, &dwSize,
            WINHTTP_NO_HEADER_INDEX)) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
    }
    if (!WinHttpQueryDataAvailable(hInternet, NULL)) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
    DWORD avail = lpvStatusInformation ? *(DWORD *)lpvStatusInformation : 0;
    if (avail == 0) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_SUCCESS);
    }
    if (avail > sizeof(slot->read_buf)) {
      avail = (DWORD)sizeof(slot->read_buf);
    }
    if (!WinHttpReadData(hInternet, slot->read_buf, avail, NULL)) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
    if (dwStatusInformationLength == 0) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_SUCCESS);
    }
    if (slot->req->on_chunk) {
      c_abstract_http_error_t crc =
          slot->req->on_chunk(slot->req->on_chunk_user_data,
                              lpvStatusInformation, dwStatusInformationLength);
      if (crc != C_ABSTRACT_HTTP_SUCCESS) {
        return winhttp_async_complete(slot, crc);
      }
    } else {
      char *nb = (char *)realloc(
          slot->body, slot->body_len + (size_t)dwStatusInformationLength + 1);
      if (!nb) {
        return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_NOMEM);
      }
      slot->body = nb;
      memcpy(slot->body + slot->body_len, lpvStatusInformation,
             dwStatusInformationLength);
      slot->body_len += (size_t)dwStatusInformationLength;
      slot->body[slot->body_len] = '\0';
    }
    if (!WinHttpQueryDataAvailable(hInternet, NULL)) {
      return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
    return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
  }

  default:
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

/**
 * @brief Asynchronous `WINHTTP_STATUS_CALLBACK` driving non-blocking WinHTTP
 * I/O.
 * @param hInternet Request handle.
 * @param dwContext Pointer to `struct WinHttpAsyncSlot`.
 * @param dwInternetStatus WinHTTP callback status code.
 * @param lpvStatusInformation Status-specific payload pointer.
 * @param dwStatusInformationLength Byte length of payload or bytes transferred.
 */
static void CALLBACK winhttp_status_callback(HINTERNET hInternet,
                                             DWORD_PTR dwContext,
                                             DWORD dwInternetStatus,
                                             LPVOID lpvStatusInformation,
                                             DWORD dwStatusInformationLength) {
  struct WinHttpAsyncSlot *slot = (struct WinHttpAsyncSlot *)dwContext;
  if (!slot || slot->finished) {
    return;
  }
  WINHTTP_VOID_CB_DISPATCH(winhttp_status_callback_impl(
      hInternet, slot, dwInternetStatus, lpvStatusInformation,
      dwStatusInformationLength));
}

/**
 * @brief Submit one asynchronous WinHTTP request driven by
 * `WINHTTP_STATUS_CALLBACK`.
 * @param ctx Transport context.
 * @param loop Optional `ModalityEventLoop` to wake on completion.
 * @param req HTTP request to dispatch.
 * @param future Output future structure.
 * @return C_ABSTRACT_HTTP_SUCCESS on submission, or error code.
 */
NO_DISCARD static c_abstract_http_error_t winhttp_submit_async_slot(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpRequest *req, struct HttpFuture *future) {
  struct WinHttpAsyncSlot *slot;
  URL_COMPONENTS urlComp;
  wchar_t *wUrl = NULL;
  wchar_t *hostName = NULL;
  wchar_t *urlPath = NULL;
  const wchar_t *wmethod = NULL;
  size_t wLen = 0;
  size_t cap;
  c_abstract_http_error_t rc;

  if (!req || !req->url) {
    future->response = NULL;
    future->error_code = C_ABSTRACT_HTTP_ERR_INVAL;
    future->is_ready = 1;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  slot = (struct WinHttpAsyncSlot *)calloc(1, sizeof(*slot));
  if (!slot) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  slot->ctx = ctx;
  slot->loop = loop;
  slot->req = req;
  slot->future = future;

  slot->hAsyncSession = WinHttpOpen(
      L"c_abstract_http/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
  if (!slot->hAsyncSession) {
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (ctx->config.timeout_ms > 0 || ctx->config.connect_timeout_ms > 0 ||
      ctx->config.read_timeout_ms > 0 || ctx->config.write_timeout_ms > 0) {
    int cto = ctx->config.connect_timeout_ms > 0
                  ? (int)ctx->config.connect_timeout_ms
                  : (int)ctx->config.timeout_ms;
    int sto = ctx->config.write_timeout_ms > 0
                  ? (int)ctx->config.write_timeout_ms
                  : (int)ctx->config.timeout_ms;
    int rto = ctx->config.read_timeout_ms > 0 ? (int)ctx->config.read_timeout_ms
                                              : (int)ctx->config.timeout_ms;
    WinHttpSetTimeouts(slot->hAsyncSession, 0, cto, sto, rto);
  }
  if (ctx->config.version_mask != (unsigned int)HTTP_VERSION_DEFAULT) {
    DWORD proto_flags = 0;
    if (ctx->config.version_mask & HTTP_VERSION_2) {
      proto_flags |= WINHTTP_PROTOCOL_FLAG_HTTP2;
    }
    if (ctx->config.version_mask & HTTP_VERSION_3) {
      proto_flags |= WINHTTP_PROTOCOL_FLAG_HTTP3;
    }
    if (proto_flags != 0) {
      WinHttpSetOption(slot->hAsyncSession, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
                       &proto_flags, sizeof(proto_flags));
    }
  }

  cap = strlen(req->url) + 1;
  wUrl = (wchar_t *)malloc(cap * sizeof(wchar_t));
  if (!wUrl) {
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = ascii_to_wide(req->url, wUrl, cap, &wLen);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(wUrl);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return rc;
  }

  memset(&urlComp, 0, sizeof(urlComp));
  urlComp.dwStructSize = sizeof(urlComp);
  hostName = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  urlPath = (wchar_t *)calloc(wLen + 1, sizeof(wchar_t));
  if (!hostName || !urlPath) {
    free(wUrl);
    free(hostName);
    free(urlPath);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  urlComp.lpszHostName = hostName;
  urlComp.dwHostNameLength = (DWORD)wLen + 1;
  urlComp.lpszUrlPath = urlPath;
  urlComp.dwUrlPathLength = (DWORD)wLen + 1;

  if (!WinHttpCrackUrl(wUrl, (DWORD)wcslen(wUrl), 0, &urlComp)) {
    free(wUrl);
    free(hostName);
    free(urlPath);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  slot->hConnect = WinHttpConnect(slot->hAsyncSession, urlComp.lpszHostName,
                                  (INTERNET_PORT)urlComp.nPort, 0);
  rc = method_to_wide(req->method, &wmethod);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(wUrl);
    free(hostName);
    free(urlPath);
    if (slot->hConnect) {
      WinHttpCloseHandle(slot->hConnect);
    }
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return rc;
  }
  if (!slot->hConnect) {
    free(wUrl);
    free(hostName);
    free(urlPath);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  slot->hRequest = WinHttpOpenRequest(
      slot->hConnect, wmethod, urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES,
      (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
  free(wUrl);
  free(hostName);
  free(urlPath);
  if (!slot->hRequest) {
    WinHttpCloseHandle(slot->hConnect);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (ctx->security_flags != 0) {
    WinHttpSetOption(slot->hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                     &ctx->security_flags, sizeof(ctx->security_flags));
  }
  if (ctx->disable_redirects) {
    DWORD dwDisable = WINHTTP_DISABLE_REDIRECTS;
    WinHttpSetOption(slot->hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &dwDisable,
                     sizeof(dwDisable));
  }

  rc = headers_to_wide_block(&req->headers, &slot->wHeaders);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    WinHttpCloseHandle(slot->hRequest);
    WinHttpCloseHandle(slot->hConnect);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return rc;
  }
  if (slot->wHeaders) {
    WinHttpAddRequestHeaders(slot->hRequest, slot->wHeaders, (DWORD)-1L,
                             WINHTTP_ADDREQ_FLAG_ADD);
  }

  if (WinHttpSetStatusCallback(slot->hRequest, winhttp_status_callback,
                               WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS,
                               0) == WINHTTP_INVALID_STATUS_CALLBACK) {
    free(slot->wHeaders);
    WinHttpCloseHandle(slot->hRequest);
    WinHttpCloseHandle(slot->hConnect);
    WinHttpCloseHandle(slot->hAsyncSession);
    free(slot);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (!WinHttpSendRequest(
          slot->hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
          (!req->read_chunk && req->body) ? req->body : WINHTTP_NO_REQUEST_DATA,
          req->read_chunk ? 0 : (DWORD)req->body_len,
          req->read_chunk ? (DWORD)req->expected_body_len
                          : (DWORD)req->body_len,
          (DWORD_PTR)slot)) {
    return winhttp_async_complete(slot, C_ABSTRACT_HTTP_ERR_IO);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* Send multiple HTTP requests asynchronously via WINHTTP_STATUS_CALLBACK +
 * ModalityEventLoop. */
NO_DISCARD c_abstract_http_error_t http_winhttp_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  LOG_DEBUG("http_winhttp_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_winhttp_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; ++i) {
    c_abstract_http_error_t rc =
        winhttp_submit_async_slot(ctx, loop, multi->requests[i], futures[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  LOG_DEBUG("http_winhttp_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Exercise `winhttp_async_complete` and wide-conversion guards for unit
 * tests.
 * @param mode 0=NULL slot, 1=finished slot, 2=conversion NULL guards,
 * <0=arm `g_mock_winhttp_conv_fail = -mode` (`-999` resets to 0).
 * @return C_ABSTRACT_HTTP_SUCCESS or expected error code.
 */
NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_winhttp_slot_guard(int mode);

NO_DISCARD c_abstract_http_error_t
c_abstract_http_test_winhttp_slot_guard(int mode) {
  struct WinHttpAsyncSlot dummy;
  if (mode < 0) {
    g_mock_winhttp_conv_fail = (mode == -999) ? 0 : -mode;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (mode == 0) {
    return winhttp_async_complete(NULL, C_ABSTRACT_HTTP_SUCCESS);
  }
  if (mode >= 2 && mode <= 10) {
    wchar_t wbuf[8];
    char cbuf[8];
    size_t len = 0;
    if (mode == 2) {
      return ascii_to_wide(NULL, wbuf, 8, &len);
    }
    if (mode == 3) {
      return ascii_to_wide("a", NULL, 8, &len);
    }
    if (mode == 4) {
      return ascii_to_wide("a", wbuf, 0, &len);
    }
    if (mode == 5) {
      return ascii_to_wide("a", wbuf, 8, NULL);
    }
    if (mode == 6) {
      return wide_to_ascii(NULL, cbuf, 8, &len);
    }
    if (mode == 7) {
      return wide_to_ascii(L"a", NULL, 8, &len);
    }
    if (mode == 8) {
      return wide_to_ascii(L"a", cbuf, 0, &len);
    }
    if (mode == 9) {
      return wide_to_ascii(L"a", cbuf, 8, NULL);
    }
    return method_to_wide(HTTP_GET, NULL);
  }
  memset(&dummy, 0, sizeof(dummy));
  dummy.finished = 1;
  return winhttp_async_complete(&dummy, C_ABSTRACT_HTTP_SUCCESS);
}
#endif
