/**
 * @file http_raw.c
 * @brief Raw BSD/Winsock2 HTTP/1.1 transport backend implementation of the
 * Abstract Network Interface (ANI) with TLS hooks, chunked encoding, streaming,
 * HTTP proxy / CONNECT tunneling, redirects, and ModalityEventLoop
 * multiplexing.
 */

/* clang-format off */
#include <c_abstract_http/http_raw.h>
#include "c_abstract_http/log.h"
#include "crypto_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS) || defined(C_ABSTRACT_HTTP_USE_RAW_SOCKETS) || 1

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
/** @brief Exclude rarely-used APIs from Win32 headers */
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
/**
 * @brief Close a raw socket handle on Windows.
 * @param s Socket handle.
 * @return 0 on success or SOCKET_ERROR.
 */
#define RAW_CLOSESOCKET(s) closesocket(s)
/** @brief Platform-native raw socket descriptor type */
typedef SOCKET raw_socket_t;
/** @brief Invalid raw socket sentinel value */
#define RAW_INVALID_SOCKET INVALID_SOCKET
/**
 * @brief Add raw socket to fd_set on Windows.
 * @param s Socket handle.
 * @param set Target fd_set pointer.
 */
#define RAW_FD_SET(s, set) FD_SET(s, set)
/**
 * @brief Poll raw socket readiness via Winsock select.
 * @param n Highest fd + 1 (ignored on Winsock).
 * @param r Read fd_set.
 * @param w Write fd_set.
 * @param e Error fd_set.
 * @param tv Timeout pointer.
 * @return Number of ready sockets or error.
 */
#define RAW_SELECT(n, r, w, e, tv) select((n), (r), (w), (e), (tv))
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#if defined(C_ABSTRACT_HTTP_DOS_WATT32)
#include <tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
/**
 * @brief Close a raw socket descriptor on DOS Watt-32.
 * @param s Socket descriptor.
 * @return 0 on success or -1 on error.
 */
#define RAW_CLOSESOCKET(s) closesocket(s)
/** @brief Platform-native raw socket descriptor type on DOS Watt-32 */
typedef int raw_socket_t;
/** @brief Invalid raw socket sentinel value on DOS Watt-32 */
#define RAW_INVALID_SOCKET (-1)
/**
 * @brief Add raw socket to fd_set on DOS Watt-32.
 * @param s Socket descriptor.
 * @param set Target fd_set pointer.
 */
#define RAW_FD_SET(s, set) FD_SET((s), (set))
/**
 * @brief Poll raw socket readiness via Watt-32 select_s.
 * @param n Highest fd + 1.
 * @param r Read fd_set.
 * @param w Write fd_set.
 * @param e Error fd_set.
 * @param tv Timeout pointer.
 * @return Number of ready sockets or -1 on error.
 */
#define RAW_SELECT(n, r, w, e, tv) select_s((n), (r), (w), (e), (tv))
#else
/** @brief Sentinel indicating DOS build without Watt-32 TCP/IP stack */
#define CAH_DOS_NO_WATT32 1
/** @brief Platform-native raw socket descriptor type on DOS */
typedef int raw_socket_t;
/** @brief Invalid raw socket sentinel value on DOS */
#define RAW_INVALID_SOCKET (-1)
#endif
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
/**
 * @brief Close a raw socket descriptor on POSIX.
 * @param s Socket file descriptor.
 * @return 0 on success or -1 on error.
 */
#define RAW_CLOSESOCKET(s) close(s)
/** @brief Platform-native raw socket descriptor type */
typedef int raw_socket_t;
/** @brief Invalid raw socket sentinel value */
#define RAW_INVALID_SOCKET (-1)
/**
 * @brief Add raw socket to fd_set on POSIX.
 * @param s Socket file descriptor.
 * @param set Target fd_set pointer.
 */
#define RAW_FD_SET(s, set) FD_SET((size_t)(s), (set))
/**
 * @brief Poll raw socket readiness via POSIX select.
 * @param n Highest fd + 1.
 * @param r Read fd_set.
 * @param w Write fd_set.
 * @param e Error fd_set.
 * @param tv Timeout pointer.
 * @return Number of ready descriptors or -1 on error.
 */
#define RAW_SELECT(n, r, w, e, tv) select((n), (r), (w), (e), (tv))
#endif
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection flag for raw socket send failure */
extern int g_mock_raw_send_fail;
/** @brief Fault-injection flag for raw socket connect failure */
extern int g_mock_raw_connect_fail;
/** @brief Fault-injection flag for raw socket DNS lookup failure */
extern int g_mock_raw_gethostbyname_fail;
/** @brief Fault-injection mode for non-blocking socket setup failure */
extern int g_mock_raw_nonblocking_fail;
/** @brief Fault-injection mode for blocking socket restore failure */
extern int g_mock_raw_blocking_fail;
/** @brief Fault-injection mode for raw socket buffer reallocation failure */
extern int g_mock_raw_realloc_fail;
/** @brief Fault-injection flag for raw socket response init failure */
extern int g_mock_raw_response_init_fail;
/** @brief Fault-injection flag for raw socket response allocation failure */
extern int g_mock_raw_res_alloc_fail;
#endif

/** @brief Maximum automatic HTTP redirect hops */
#define RAW_MAX_REDIRECTS 10

/** @brief Initialize global raw socket state */
NO_DISCARD c_abstract_http_error_t http_raw_global_init(void) {
#if defined(_WIN32)
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#elif defined(C_ABSTRACT_HTTP_DOS_WATT32)
  if (sock_init() != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Clean up global raw socket state */
NO_DISCARD c_abstract_http_error_t http_raw_global_cleanup(void) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Internal raw socket transport context */
struct RawCtx {
  long timeout_ms;                  /**< Default timeout in milliseconds */
  struct HttpConfig config;         /**< Active HTTP configuration */
  int has_custom_tls;               /**< Non-zero if tls_hooks is populated */
  struct HttpRawTlsHooks tls_hooks; /**< Custom TLS session hook table */
};

NO_DISCARD c_abstract_http_error_t
http_raw_context_init(struct HttpTransportContext **ctx) {
  struct RawCtx *c;
  c_abstract_http_error_t rc;
  LOG_DEBUG("http_raw_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_raw_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  c = (struct RawCtx *)calloc(1, sizeof(struct RawCtx));
  if (!c) {
    LOG_DEBUG("http_raw_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&c->config);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_raw_context_init: Error http_config_init failed with %d",
              rc);
    free(c);
    return rc;
  }

  c->timeout_ms = 30000L;
  *ctx = (struct HttpTransportContext *)c;
  LOG_DEBUG("http_raw_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_raw_context_free(struct HttpTransportContext *ctx) {
  struct RawCtx *c = (struct RawCtx *)ctx;
  LOG_DEBUG("http_raw_context_free: Entering");
  if (c) {
    c_abstract_http_error_t rc = http_config_free(&c->config);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    free(c);
  }
  LOG_DEBUG("http_raw_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t http_raw_set_tls_hooks(
    struct HttpTransportContext *ctx, const struct HttpRawTlsHooks *hooks) {
  struct RawCtx *c = (struct RawCtx *)ctx;
  if (!c) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (!hooks) {
    c->has_custom_tls = 0;
    memset(&c->tls_hooks, 0, sizeof(c->tls_hooks));
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!hooks->open || !hooks->handshake || !hooks->read || !hooks->write ||
      !hooks->close) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  c->tls_hooks = *hooks;
  c->has_custom_tls = 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Replace a heap-allocated string field only if changed.
 * @param dst In/out heap string pointer.
 * @param src Optional replacement string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t raw_copy_opt_str(char **dst,
                                                           const char *src) {
  if (!src) {
    free(*dst);
    *dst = NULL;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (*dst && strcmp(*dst, src) == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  free(*dst);
  *dst = NULL;
  return c_abstract_http_strdup(src, dst);
}

NO_DISCARD c_abstract_http_error_t http_raw_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  struct RawCtx *c = (struct RawCtx *)ctx;
  c_abstract_http_error_t rc;

  if (!c || !config) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->version_mask != 0 &&
      (config->version_mask & ((unsigned int)HTTP_VERSION_1_0 |
                               (unsigned int)HTTP_VERSION_1_1)) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  if (config->proxy_url) {
    if (strncmp(config->proxy_url, "http://", 7) != 0) {
      if (strstr(config->proxy_url, "://") != NULL) {
        return C_ABSTRACT_HTTP_ERR_NOTSUP;
      }
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
  }
  if ((config->client_cert_path && !config->client_key_path) ||
      (!config->client_cert_path && config->client_key_path) ||
      (config->ca_buffer_len > 0 && !config->ca_buffer)) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = raw_copy_opt_str(&c->config.user_agent, config->user_agent);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.proxy_url, config->proxy_url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.proxy_username, config->proxy_username);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.proxy_password, config->proxy_password);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.ca_path, config->ca_path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.client_cert_path, config->client_cert_path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.client_key_path, config->client_key_path);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = raw_copy_opt_str(&c->config.cert_pin_sha256, config->cert_pin_sha256);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  free(c->config.ca_buffer);
  c->config.ca_buffer = NULL;
  c->config.ca_buffer_len = 0;
  if (config->ca_buffer && config->ca_buffer_len > 0) {
    c->config.ca_buffer = (unsigned char *)malloc(config->ca_buffer_len);
    if (!c->config.ca_buffer) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(c->config.ca_buffer, config->ca_buffer, config->ca_buffer_len);
    c->config.ca_buffer_len = config->ca_buffer_len;
  }

  if (config->timeout_ms > 0) {
    c->timeout_ms = config->timeout_ms;
  }
  c->config.timeout_ms = config->timeout_ms;
  c->config.connect_timeout_ms = config->connect_timeout_ms;
  c->config.read_timeout_ms = config->read_timeout_ms;
  c->config.write_timeout_ms = config->write_timeout_ms;
  c->config.verify_peer = config->verify_peer;
  c->config.verify_host = config->verify_host;
  c->config.follow_redirects = config->follow_redirects;
  c->config.cookie_jar = config->cookie_jar;
  c->config.version_mask = config->version_mask;
  c->config.tls_version_mask = config->tls_version_mask;
  c->config.http3_fallback = config->http3_fallback;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse URL into host, port, path, and HTTPS flag.
 * @param url Input URL string.
 * @param host Output pointer to allocated host string.
 * @param port Output pointer to port number.
 * @param path Output pointer to allocated path string.
 * @param out_https Output pointer set to 1 for https://, 0 for http://.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
NO_DISCARD static c_abstract_http_error_t parse_url(const char *url,
                                                    char **host, int *port,
                                                    char **path,
                                                    int *out_https) {
  const char *p;
  const char *port_start;
  const char *path_start;
  size_t host_len;

  if (!url)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (strncmp(url, "http://", 7) == 0) {
    p = url + 7;
    *port = 80;
    *out_https = 0;
  } else if (strncmp(url, "https://", 8) == 0) {
    p = url + 8;
    *port = 443;
    *out_https = 1;
  } else {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  port_start = strchr(p, ':');
  path_start = strchr(p, '/');

  if (path_start && port_start && port_start > path_start) {
    port_start = NULL;
  }

  if (port_start) {
    long parsed_port;
    host_len = (size_t)(port_start - p);
    parsed_port = strtol(port_start + 1, NULL, 10);
    if (parsed_port <= 0L || parsed_port > 65535L) {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    *port = (int)parsed_port;
  } else if (path_start) {
    host_len = (size_t)(path_start - p);
  } else {
    host_len = strlen(p);
  }

  if (host_len == 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *host = (char *)malloc(host_len + 1);
  if (!*host)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memcpy(*host, p, host_len);
  (*host)[host_len] = '\0';

  if (path_start) {
    size_t path_len = strlen(path_start);
    *path = (char *)malloc(path_len + 1);
    if (!*path) {
      free(*host);
      *host = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(*path, path_start, path_len + 1);
  } else {
    *path = (char *)malloc(2);
    if (!*path) {
      free(*host);
      *host = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    strcpy_s(*path, 2, "/");
#else
    strcpy(*path, "/");
#endif
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

#if defined(CAH_DOS_NO_WATT32)
/** @brief Send an HTTP request over raw sockets on DOS without Watt-32 */
NO_DISCARD c_abstract_http_error_t
http_raw_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
              struct HttpResponse **res) {
  char *host = NULL;
  int port = 80;
  char *path = NULL;
  int is_https = 0;
  c_abstract_http_error_t rc;
  (void)ctx;
  if (!req || !res) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc = parse_url(req->url, &host, &port, &path, &is_https);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  free(host);
  free(path);
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
}

/** @brief Send multiple HTTP requests over raw sockets on DOS without Watt-32
 */
NO_DISCARD c_abstract_http_error_t http_raw_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *reqs, struct HttpFuture **future) {
  size_t i;
  (void)loop;
  if (!ctx || !reqs || !future) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  for (i = 0; i < reqs->count; ++i) {
    struct HttpResponse *res = NULL;
    c_abstract_http_error_t rc = http_raw_send(ctx, reqs->requests[i], &res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      future[i]->response = NULL;
      future[i]->error_code = rc;
      future[i]->is_ready = 1;
      return rc;
    }
    future[i]->response = res;
    future[i]->error_code = C_ABSTRACT_HTTP_SUCCESS;
    future[i]->is_ready = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
#else /* !defined(CAH_DOS_NO_WATT32) */

/**
 * @brief Convert a 16-bit integer from host to network byte order.
 * @param hostshort Value in host byte order.
 * @return Value in network byte order.
 */
static unsigned short math_htons(unsigned short hostshort) {
  unsigned short out;
  unsigned char *p = (unsigned char *)&out;
  p[0] = (unsigned char)(hostshort >> 8);
  p[1] = (unsigned char)(hostshort & 0xFF);
  return out;
}

/**
 * @brief Case-insensitive ASCII string equality check.
 * @param a First string.
 * @param b Second string.
 * @return 0 when equal ignoring ASCII case, non-zero otherwise.
 */
static int math_raw_strcasecmp(const char *a, const char *b) {
  unsigned char ca;
  unsigned char cb;
  while (*a && *b) {
    ca = (unsigned char)*a++;
    cb = (unsigned char)*b++;
    if (ca >= 'A' && ca <= 'Z') {
      ca = (unsigned char)(ca + ('a' - 'A'));
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (unsigned char)(cb + ('a' - 'A'));
    }
    if (ca != cb) {
      return (int)ca - (int)cb;
    }
  }
  return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/**
 * @brief Case-insensitive substring check for token presence in a header value.
 * @param haystack Header value string.
 * @param needle Lowercase ASCII token to locate.
 * @return 1 if @p needle occurs in @p haystack, 0 otherwise.
 */
static int math_raw_strcasestr_has(const char *haystack, const char *needle) {
  size_t nlen = strlen(needle);
  size_t i;
  for (; *haystack; ++haystack) {
    int ok = 1;
    for (i = 0; i < nlen; ++i) {
      unsigned char ch = (unsigned char)haystack[i];
      if (!ch) {
        return 0;
      }
      if (ch >= 'A' && ch <= 'Z') {
        ch = (unsigned char)(ch + ('a' - 'A'));
      }
      if (ch != (unsigned char)needle[i]) {
        ok = 0;
        break;
      }
    }
    if (ok) {
      return 1;
    }
  }
  return 0;
}

/**
 * @brief Configure socket to be non-blocking.
 * @param sock Socket descriptor.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
NO_DISCARD static c_abstract_http_error_t
make_socket_nonblocking(raw_socket_t sock) {
#if defined(_WIN32)
  u_long mode = 1;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_nonblocking_fail == 1 || g_mock_raw_nonblocking_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (ioctlsocket(sock, (long)FIONBIO, &mode) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(C_ABSTRACT_HTTP_DOS_WATT32)
  int mode = 1;
  if (ioctlsocket(sock, (long)FIONBIO, (char *)&mode) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  int flags;
  int set_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_nonblocking_fail == 1)
    flags = -1;
  else
#endif
    flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1)
    return C_ABSTRACT_HTTP_ERR_IO;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_nonblocking_fail == 2)
    set_rc = -1;
  else
#endif
    set_rc = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  if (set_rc == -1)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Configure socket to be blocking.
 * @param sock Socket descriptor.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code otherwise.
 */
NO_DISCARD static c_abstract_http_error_t
make_socket_blocking(raw_socket_t sock) {
#if defined(_WIN32)
  u_long mode = 0;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_blocking_fail == 1 || g_mock_raw_blocking_fail == 2)
    return C_ABSTRACT_HTTP_ERR_IO;
#endif
  if (ioctlsocket(sock, (long)FIONBIO, &mode) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(C_ABSTRACT_HTTP_DOS_WATT32)
  int mode = 0;
  if (ioctlsocket(sock, (long)FIONBIO, (char *)&mode) != 0)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  int flags;
  int set_rc;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_blocking_fail == 1)
    flags = -1;
  else
#endif
    flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1)
    return C_ABSTRACT_HTTP_ERR_IO;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_blocking_fail == 2)
    set_rc = -1;
  else
#endif
    set_rc = fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
  if (set_rc == -1)
    return C_ABSTRACT_HTTP_ERR_IO;
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Initialize an HttpResponse with optional fault injection.
 * @param r Target response structure.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_response_init_checked(struct HttpResponse *r) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_response_init_fail)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
#endif
  return http_response_init(r);
}

/**
 * @brief Add a parsed response header with optional fault injection.
 * @param headers Target headers collection.
 * @param key Header name.
 * @param value Header value.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_headers_add_checked(struct HttpHeaders *headers, const char *key,
                        const char *value) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_realloc_fail == 5)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
#endif
  return http_headers_add(headers, key, value);
}

/**
 * @brief Select active TLS hook table for a context.
 * @param c Raw transport context (may be NULL).
 * @param out_hooks Output pointer receiving TLS hook table or NULL.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_resolve_tls_hooks(const struct RawCtx *c,
                      const struct HttpRawTlsHooks **out_hooks) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_send_fail == 2) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  if (g_mock_raw_send_fail == 3) {
    *out_hooks = NULL;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif
  if (c && c->has_custom_tls) {
    *out_hooks = &c->tls_hooks;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_get_default_tls_hooks(out_hooks);
}

/**
 * @brief Compute effective timeout in milliseconds for connect/read/write.
 * @param c Raw context (may be NULL).
 * @param specific_ms Operation-specific timeout field.
 * @return Effective timeout in milliseconds (> 0).
 */
static long math_raw_timeout_ms(const struct RawCtx *c, long specific_ms) {
  if (specific_ms > 0) {
    return specific_ms;
  }
  if (c && c->timeout_ms > 0) {
    return c->timeout_ms;
  }
  return 30000L;
}

/**
 * @brief Poll a socket for readability or writability with a millisecond
 * timeout.
 * @param sock Socket handle.
 * @param want 1 for read readiness, 2 for write readiness.
 * @param timeout_ms Timeout in milliseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS on readiness, C_ABSTRACT_HTTP_ERR_IO on
 * error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_wait_socket(raw_socket_t sock, int want, long timeout_ms) {
  fd_set fdset;
  struct timeval tv;
  int res_sel;

  FD_ZERO(&fdset);
  RAW_FD_SET(sock, &fdset);
  tv.tv_sec = timeout_ms / 1000L;
  tv.tv_usec = (int)((timeout_ms % 1000L) * 1000L);

  if (want == 2) {
    res_sel = RAW_SELECT((int)(sock + 1), NULL, &fdset, NULL, &tv);
  } else {
    res_sel = RAW_SELECT((int)(sock + 1), &fdset, NULL, NULL, &tv);
  }
  if (res_sel <= 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Resolve IPv4 address for a target hostname and port.
 * @param host Target hostname or dotted-quad IPv4 string.
 * @param port Target TCP port.
 * @param out_addr Populated sockaddr_in output.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_IO.
 */
NO_DISCARD static c_abstract_http_error_t
raw_resolve_addr(const char *host, int port, struct sockaddr_in *out_addr) {
#if defined(__MSDOS__) || defined(__DOS__) || defined(DOS) ||                  \
    (defined(_MSC_VER) && _MSC_VER < 1400)
  struct hostent *he;
  memset(out_addr, 0, sizeof(*out_addr));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_gethostbyname_fail)
    he = NULL;
  else
#endif
    he = gethostbyname(host);
  if (!he) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  out_addr->sin_family = AF_INET;
  out_addr->sin_port = math_htons((unsigned short)port);
  memcpy(&out_addr->sin_addr, he->h_addr_list[0], (size_t)he->h_length);
  return C_ABSTRACT_HTTP_SUCCESS;
#else
  struct addrinfo hints;
  struct addrinfo *ai_res = NULL;
  int gai_rc;

  memset(out_addr, 0, sizeof(*out_addr));
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_gethostbyname_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  if (strcmp(host, "127.0.0.1") == 0) {
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = math_htons((unsigned short)port);
    out_addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  gai_rc = getaddrinfo(host, NULL, &hints, &ai_res);
  if (gai_rc != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  memcpy(out_addr, ai_res->ai_addr, sizeof(struct sockaddr_in));
  out_addr->sin_port = math_htons((unsigned short)port);
  freeaddrinfo(ai_res);
  return C_ABSTRACT_HTTP_SUCCESS;
#endif
}

/**
 * @brief Convert enum HttpMethod to wire token string.
 * @param m Method enum.
 * @param out_str Output pointer receiving static wire method string.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t raw_method_str(enum HttpMethod m,
                                                         const char **out_str) {
  if ((int)m < 0) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  switch ((int)m) {
  case HTTP_POST:
    *out_str = "POST";
    break;
  case HTTP_PUT:
    *out_str = "PUT";
    break;
  case HTTP_DELETE:
    *out_str = "DELETE";
    break;
  case HTTP_PATCH:
    *out_str = "PATCH";
    break;
  case HTTP_HEAD:
    *out_str = "HEAD";
    break;
  case HTTP_OPTIONS:
    *out_str = "OPTIONS";
    break;
  case HTTP_TRACE:
    *out_str = "TRACE";
    break;
  case HTTP_CONNECT:
    *out_str = "CONNECT";
    break;
  default:
    *out_str = "GET";
    break;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Build Basic Proxy-Authorization header line ("Proxy-Authorization:
Basic ...
").
 * @param user Proxy username.
 * @param pass Optional proxy password.
 * @param out_line Newly allocated header line.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
raw_build_proxy_auth_line(const char *user, const char *pass, char **out_line) {
  const char *pw = pass ? pass : "";
  size_t ulen = strlen(user);
  size_t plen = strlen(pw);
  char *creds;
  char *b64 = NULL;
  size_t b64_len = 0;
  char *line;
  size_t prefix_len = strlen("Proxy-Authorization: Basic ");
  c_abstract_http_error_t rc;

  creds = (char *)malloc(ulen + 1 + plen + 1);
  if (!creds) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(creds, user, ulen);
  creds[ulen] = ':';
  memcpy(creds + ulen + 1, pw, plen);
  creds[ulen + 1 + plen] = '\0';

  rc = base64_encode((const unsigned char *)creds, ulen + 1 + plen, &b64,
                     &b64_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(creds);
    return rc;
  }
  free(creds);

  line = (char *)malloc(prefix_len + b64_len + 3);
  if (!line) {
    free(b64);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(line, "Proxy-Authorization: Basic ", prefix_len);
  memcpy(line + prefix_len, b64, b64_len);
  memcpy(line + prefix_len + b64_len, "\r\n", 3);
  free(b64);
  *out_line = line;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Ensure growable char buffer has capacity for additional bytes.
 * @param buf In/out heap buffer pointer.
 * @param cap In/out capacity.
 * @param used Current used byte count.
 * @param add Additional bytes needed (excluding NUL).
 * @param fail_mode `g_mock_raw_realloc_fail` mode value to match (1 or 2).
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
raw_ensure_req_cap(char **buf, size_t *cap, size_t used, size_t add,
                   int fail_mode) {
  char *new_buf;
  size_t new_cap;
  (void)fail_mode;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_realloc_fail >= 10 && --g_mock_raw_realloc_fail == 9) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  if (used + add < *cap) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  new_cap = *cap * 2;
  if (new_cap <= used + add) {
    new_cap = used + add + 1;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_realloc_fail == fail_mode)
    new_buf = NULL;
  else
#endif
    new_buf = (char *)realloc(*buf, new_cap);
  if (!new_buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  *buf = new_buf;
  *cap = new_cap;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Serialize HTTP/1.x request start-line, headers, and static body.
 * @param c Raw transport context (may be NULL).
 * @param req Effective HTTP request.
 * @param host Origin host string.
 * @param path Origin path string.
 * @param use_proxy_abs_uri Non-zero when sending HTTP over an explicit HTTP
 * proxy.
 * @param out_buf Newly allocated serialized wire buffer.
 * @param out_len Output byte length of @p out_buf.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_build_wire_request(const struct RawCtx *c, const struct HttpRequest *req,
                       const char *host, const char *path,
                       int use_proxy_abs_uri, char **out_buf, size_t *out_len) {
  size_t req_cap = 1024;
  size_t req_len = 0;
  char *request_buf;
  const char *method_str = NULL;
  const char *target = use_proxy_abs_uri ? req->url : path;
  const char *ver_str = "HTTP/1.1";
  int has_ua = 0;
  int has_cl = 0;
  size_t i;
  size_t start_need;
  c_abstract_http_error_t rc;

  rc = raw_method_str(req->method, &method_str);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (c && (c->config.version_mask & (unsigned int)HTTP_VERSION_1_0) &&
      !(c->config.version_mask & (unsigned int)HTTP_VERSION_1_1)) {
    ver_str = "HTTP/1.0";
  }

  start_need = strlen(method_str) + strlen(target) + strlen(host) + 64;
  if (start_need >= req_cap) {
    req_cap = start_need * 2;
  }
  request_buf = (char *)malloc(req_cap);
  if (!request_buf) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(request_buf, req_cap,
            "%s %s %s\r\nHost: %s\r\nConnection: close\r\n", method_str, target,
            ver_str, host);
#else
  sprintf(request_buf, "%s %s %s\r\nHost: %s\r\nConnection: close\r\n",
          method_str, target, ver_str, host);
#endif
  req_len = strlen(request_buf);

  for (i = 0; i < req->headers.count; ++i) {
    size_t line_len = strlen(req->headers.headers[i].key) +
                      strlen(req->headers.headers[i].value) + 4;
    if (math_raw_strcasecmp(req->headers.headers[i].key, "User-Agent") == 0) {
      has_ua = 1;
    }
    if (math_raw_strcasecmp(req->headers.headers[i].key, "Content-Length") ==
        0) {
      has_cl = 1;
    }
    rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, line_len, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(request_buf);
      return rc;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(request_buf + req_len, req_cap - req_len, "%s: %s\r\n",
              req->headers.headers[i].key, req->headers.headers[i].value);
#else
    sprintf(request_buf + req_len, "%s: %s\r\n", req->headers.headers[i].key,
            req->headers.headers[i].value);
#endif
    req_len += strlen(request_buf + req_len);
  }

  if (!has_ua && c && c->config.user_agent) {
    size_t ua_len = strlen(c->config.user_agent) + 14;
    rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, ua_len, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(request_buf);
      return rc;
    }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(request_buf + req_len, req_cap - req_len, "User-Agent: %s\r\n",
              c->config.user_agent);
#else
    sprintf(request_buf + req_len, "User-Agent: %s\r\n", c->config.user_agent);
#endif
    req_len += strlen(request_buf + req_len);
  }

  if (use_proxy_abs_uri && c && c->config.proxy_username) {
    char *pauth = NULL;
    size_t palen;
    rc = raw_build_proxy_auth_line(c->config.proxy_username,
                                   c->config.proxy_password, &pauth);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(request_buf);
      return rc;
    }
    palen = strlen(pauth);
    rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, palen, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(pauth);
      free(request_buf);
      return rc;
    }
    memcpy(request_buf + req_len, pauth, palen + 1);
    req_len += palen;
    free(pauth);
  }

  if (req->read_chunk) {
    char shdr[80];
    int use_chunked = (req->expected_body_len == 0 && !has_cl);
    if (req->expected_body_len > 0 && !has_cl) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(shdr, sizeof(shdr), "Content-Length: %lu\r\n\r\n",
                (unsigned long)req->expected_body_len);
#else
      sprintf(shdr, "Content-Length: %lu\r\n\r\n",
              (unsigned long)req->expected_body_len);
#endif
    } else if (use_chunked) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(shdr, sizeof(shdr), "Transfer-Encoding: chunked\r\n\r\n");
#else
      strcpy(shdr, "Transfer-Encoding: chunked\r\n\r\n");
#endif
    } else {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(shdr, sizeof(shdr), "\r\n");
#else
      strcpy(shdr, "\r\n");
#endif
    }
    rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, strlen(shdr), 2);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(request_buf);
      return rc;
    }
    memcpy(request_buf + req_len, shdr, strlen(shdr));
    req_len += strlen(shdr);

    for (;;) {
      char chunk[4096];
      size_t nread = 0;
      rc = req->read_chunk(req->read_chunk_user_data, chunk, sizeof(chunk),
                           &nread);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(request_buf);
        return rc;
      }
      if (nread == 0) {
        if (use_chunked) {
          rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, 5, 2);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            free(request_buf);
            return rc;
          }
          memcpy(request_buf + req_len, "0\r\n\r\n", 5);
          req_len += 5;
        }
        break;
      }
      if (use_chunked) {
        char chdr[32];
        size_t chlen;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
        sprintf_s(chdr, sizeof(chdr), "%lx\r\n", (unsigned long)nread);
#else
        sprintf(chdr, "%lx\r\n", (unsigned long)nread);
#endif
        chlen = strlen(chdr);
        rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len,
                                chlen + nread + 2, 2);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(request_buf);
          return rc;
        }
        memcpy(request_buf + req_len, chdr, chlen);
        req_len += chlen;
        memcpy(request_buf + req_len, chunk, nread);
        req_len += nread;
        memcpy(request_buf + req_len, "\r\n", 2);
        req_len += 2;
      } else {
        rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, nread, 2);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          free(request_buf);
          return rc;
        }
        memcpy(request_buf + req_len, chunk, nread);
        req_len += nread;
      }
    }
  } else if (req->body && req->body_len > 0) {
    char len_buf[64];
    if (!has_cl) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      sprintf_s(len_buf, sizeof(len_buf), "Content-Length: %lu\r\n\r\n",
                (unsigned long)req->body_len);
#else
      sprintf(len_buf, "Content-Length: %lu\r\n\r\n",
              (unsigned long)req->body_len);
#endif
    } else {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
      strcpy_s(len_buf, sizeof(len_buf), "\r\n");
#else
      strcpy(len_buf, "\r\n");
#endif
    }
    rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len,
                            strlen(len_buf) + req->body_len, 2);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(request_buf);
      return rc;
    }
    memcpy(request_buf + req_len, len_buf, strlen(len_buf));
    req_len += strlen(len_buf);
    memcpy(request_buf + req_len, req->body, req->body_len);
    req_len += req->body_len;
  } else {
    rc = raw_ensure_req_cap(&request_buf, &req_cap, req_len, 2, 2);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(request_buf);
      return rc;
    }
    memcpy(request_buf + req_len, "\r\n", 3);
    req_len += 2;
  }

  *out_buf = request_buf;
  *out_len = req_len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Write all bytes over either an active TLS session or raw TCP socket.
 * @param sock Connected socket descriptor.
 * @param tls_hooks Active TLS hooks (NULL for plaintext TCP).
 * @param tls_sess Active TLS session (NULL for plaintext TCP).
 * @param buf Source byte buffer.
 * @param len Byte count to write.
 * @param timeout_ms Write poll timeout in milliseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_io_write_all(raw_socket_t sock, const struct HttpRawTlsHooks *tls_hooks,
                 void *tls_sess, const char *buf, size_t len, long timeout_ms) {
  const char *p = buf;
  size_t rem = len;
  c_abstract_http_error_t rc;

  while (rem > 0) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_raw_send_fail == 1) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#endif
    if (tls_hooks && tls_sess) {
      size_t nw = 0;
      int want = 0;
      rc = tls_hooks->write(tls_sess, p, rem, &nw, &want);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      if (want != 0) {
        rc = raw_wait_socket(sock, want, timeout_ms);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return rc;
        }
        continue;
      }
      if (nw == 0) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      p += nw;
      rem -= nw;
    } else {
      long s_rc;
#if defined(_WIN32)
      s_rc = (long)send(sock, p, (int)rem, 0);
#else
      s_rc = (long)send(sock, p, rem, 0);
#endif
      if (s_rc <= 0) {
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      p += (size_t)s_rc;
      rem -= (size_t)s_rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Parse hexadecimal digit value or -1 if invalid.
 * @param c Input character.
 * @return 0..15 or -1.
 */
static int math_raw_hex_val(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

/**
 * @brief Check whether a buffered HTTP/1.x response has reached complete
 * framing.
 * @param buf Accumulated raw wire response buffer (NUL-terminated).
 * @param len Byte length of @p buf.
 * @param req_method Originating HTTP request method.
 * @return 1 if message framing is complete, 0 if more bytes are needed.
 */
static int math_raw_response_complete(const char *buf, size_t len,
                                      enum HttpMethod req_method) {
  const char *hdr_end;
  size_t hdr_len;
  const char *line;
  int status_code = 200;
  int is_chunked = 0;
  long content_len = -1L;

  hdr_end = strstr(buf, "\r\n\r\n");
  if (!hdr_end) {
    return 0;
  }
  hdr_len = (size_t)(hdr_end - buf) + 4;
  if (strncmp(buf, "HTTP/1.", 7) == 0) {
    status_code = atoi(buf + 9);
  }
  if (req_method == HTTP_HEAD || status_code == 204 || status_code == 304) {
    return 1;
  }

  line = buf;
  while (line < hdr_end && !(line[0] == '\r' && line[1] == '\n')) {
    line++;
  }
  while (line < hdr_end) {
    const char *ls = line + 2;
    const char *le = ls;
    const char *colon;
    while (le < hdr_end + 2 && !(le[0] == '\r' && le[1] == '\n')) {
      le++;
    }
    colon = (const char *)memchr(ls, ':', (size_t)(le - ls));
    if (colon) {
      size_t klen = (size_t)(colon - ls);
      const char *vs = colon + 1;
      while (vs < le && (*vs == ' ' || *vs == '\t')) {
        vs++;
      }
      if (klen == 17 && strncmp(ls, "Transfer-Encoding", 17) == 0) {
        is_chunked = math_raw_strcasestr_has(vs, "chunked");
      } else if (klen == 17) {
        char ktmp[18];
        memcpy(ktmp, ls, 17);
        ktmp[17] = '\0';
        if (math_raw_strcasecmp(ktmp, "transfer-encoding") == 0) {
          is_chunked = math_raw_strcasestr_has(vs, "chunked");
        }
      } else if (klen == 14) {
        char ktmp[15];
        memcpy(ktmp, ls, 14);
        ktmp[14] = '\0';
        if (math_raw_strcasecmp(ktmp, "content-length") == 0) {
          content_len = atol(vs);
        }
      }
    }
    line = le;
  }

  if (is_chunked) {
    size_t off = hdr_len;
    while (off < len) {
      size_t csz = 0;
      int saw_hex = 0;
      while (off < len && buf[off] != '\r' && buf[off] != ';') {
        int hv = math_raw_hex_val(buf[off]);
        if (hv < 0) {
          return 1;
        }
        csz = (csz << 4) + (size_t)hv;
        saw_hex = 1;
        off++;
      }
      if (!saw_hex) {
        return 1;
      }
      while (off < len && buf[off] != '\r') {
        off++;
      }
      if (off + 2 > len || buf[off] != '\r' || buf[off + 1] != '\n') {
        return 0;
      }
      off += 2;
      if (csz == 0) {
        if (off + 2 <= len && buf[off] == '\r' && buf[off + 1] == '\n') {
          return 1;
        }
        if (strstr(buf + off, "\r\n\r\n") != NULL) {
          return 1;
        }
        return 0;
      }
      if (off + csz + 2 > len) {
        return 0;
      }
      off += csz + 2;
    }
    return 0;
  }

  if (content_len >= 0) {
    return (len - hdr_len) >= (size_t)content_len ? 1 : 0;
  }
  return 0;
}

/**
 * @brief Decode an RFC 9112 chunked HTTP response body into `res` or
 * `on_chunk`.
 * @param src Encoded chunked payload starting immediately after CRLFCRLF.
 * @param src_len Byte length of @p src.
 * @param req Originating request (for optional `on_chunk`).
 * @param res Output response structure.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_decode_chunked_body(const char *src, size_t src_len,
                        const struct HttpRequest *req,
                        struct HttpResponse *res) {
  size_t off = 0;
  char *out = NULL;
  size_t out_len = 0;
  size_t out_cap = 0;
  c_abstract_http_error_t rc;

  while (off < src_len) {
    size_t chunk_sz = 0;
    int saw_digit = 0;

    while (off < src_len && src[off] != '\r' && src[off] != ';') {
      int hv = math_raw_hex_val(src[off]);
      if (hv < 0) {
        free(out);
        return C_ABSTRACT_HTTP_ERR_IO;
      }
      chunk_sz = (chunk_sz << 4) + (size_t)hv;
      saw_digit = 1;
      off++;
    }
    if (!saw_digit) {
      free(out);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    while (off < src_len && src[off] != '\r') {
      off++;
    }
    if (off + 2 > src_len || src[off] != '\r' || src[off + 1] != '\n') {
      free(out);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    off += 2;

    if (chunk_sz == 0) {
      if (out) {
        out[out_len] = '\0';
        res->body = out;
        res->body_len = out_len;
      }
      return C_ABSTRACT_HTTP_SUCCESS;
    }

    if (off + chunk_sz + 2 > src_len || src[off + chunk_sz] != '\r' ||
        src[off + chunk_sz + 1] != '\n') {
      free(out);
      return C_ABSTRACT_HTTP_ERR_IO;
    }

    if (req->on_chunk) {
      rc = req->on_chunk(req->on_chunk_user_data, src + off, chunk_sz);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(out);
        return rc;
      }
    } else {
      if (out_len + chunk_sz + 1 > out_cap) {
        char *nb;
        size_t ncap = out_cap == 0 ? (chunk_sz + 64) : (out_cap * 2);
        if (ncap < out_len + chunk_sz + 1) {
          ncap = out_len + chunk_sz + 1;
        }
        nb = (char *)realloc(out, ncap);
        if (!nb) {
          free(out);
          return C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        out = nb;
        out_cap = ncap;
      }
      memcpy(out + out_len, src + off, chunk_sz);
      out_len += chunk_sz;
    }
    off += chunk_sz + 2;
  }

  free(out);
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Parse raw wire HTTP/1.x response buffer into `struct HttpResponse`.
 * @param body Mutable raw wire buffer (`body[body_len] == '\0'`).
 * @param body_len Total received wire bytes.
 * @param req Originating request (for `on_chunk` delivery).
 * @param res Initialized target response structure.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_parse_wire_response(char *body, size_t body_len,
                        const struct HttpRequest *req,
                        struct HttpResponse *res) {
  char *header_end;
  c_abstract_http_error_t rc;

  if (body_len == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  body[body_len] = '\0';
  header_end = strstr(body, "\r\n\r\n");
  if (header_end) {
    char *p_nl;
    size_t header_len = (size_t)(header_end - body) + 4;
    size_t actual_body_len = body_len - header_len;
    int is_chunked = 0;

    if (strncmp(body, "HTTP/1.", 7) == 0) {
      res->status_code = atoi(body + 9);
    } else {
      res->status_code = 200;
    }

    p_nl = strstr(body, "\r\n");
    if (p_nl < header_end) {
      p_nl += 2;
      while (p_nl < header_end) {
        char *colon;
        char *next_nl = strstr(p_nl, "\r\n");
        colon = strchr(p_nl, ':');
        if (colon && colon < next_nl) {
          char *vstart = colon + 1;
          *colon = '\0';
          *next_nl = '\0';
          while (*vstart == ' ' || *vstart == '\t') {
            vstart++;
          }
          if (math_raw_strcasecmp(p_nl, "Transfer-Encoding") == 0 &&
              math_raw_strcasestr_has(vstart, "chunked")) {
            is_chunked = 1;
          }
          rc = raw_headers_add_checked(&res->headers, p_nl, vstart);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            return rc;
          }
        }
        p_nl = next_nl + 2;
      }
    }

    if (is_chunked) {
      return raw_decode_chunked_body(body + header_len, actual_body_len, req,
                                     res);
    }
    if (actual_body_len > 0) {
      if (req->on_chunk) {
        return req->on_chunk(req->on_chunk_user_data, body + header_len,
                             actual_body_len);
      } else {
        char *real_body;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        if (g_mock_raw_realloc_fail == 6)
          real_body = NULL;
        else
#endif
          real_body = (char *)malloc(actual_body_len + 1);
        if (real_body) {
          memcpy(real_body, body + header_len, actual_body_len);
          real_body[actual_body_len] = '\0';
          res->body = real_body;
          res->body_len = actual_body_len;
        }
      }
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  res->status_code = 200;
  if (req->on_chunk) {
    return req->on_chunk(req->on_chunk_user_data, body, body_len);
  }
  {
    char *real_body;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (g_mock_raw_realloc_fail == 7)
      real_body = NULL;
    else
#endif
      real_body = (char *)malloc(body_len + 1);
    if (real_body) {
      memcpy(real_body, body, body_len);
      real_body[body_len] = '\0';
      res->body = real_body;
      res->body_len = body_len;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Perform HTTP CONNECT proxy tunnel handshake on a connected TCP socket.
 * @param c Raw transport context.
 * @param sock Connected proxy socket.
 * @param host Origin hostname.
 * @param port Origin TCP port.
 * @return C_ABSTRACT_HTTP_SUCCESS on HTTP 200 tunnel establishment, or error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_proxy_connect_tunnel(const struct RawCtx *c, raw_socket_t sock,
                         const char *host, int port) {
  char *pauth = NULL;
  char req_line[512];
  char resp_buf[1024];
  size_t rlen = 0;
  long w_to = math_raw_timeout_ms(c, c ? c->config.write_timeout_ms : 0);
  long r_to = math_raw_timeout_ms(c, c ? c->config.read_timeout_ms : 0);
  c_abstract_http_error_t rc;

  if (c && c->config.proxy_username) {
    rc = raw_build_proxy_auth_line(c->config.proxy_username,
                                   c->config.proxy_password, &pauth);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(req_line, sizeof(req_line),
            "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n%s\r\n", host, port, host,
            port, pauth ? pauth : "");
#else
  sprintf(req_line, "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n%s\r\n", host,
          port, host, port, pauth ? pauth : "");
#endif
  free(pauth);

  rc = raw_io_write_all(sock, NULL, NULL, req_line, strlen(req_line), w_to);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  while (rlen + 1 < sizeof(resp_buf)) {
    int nr;
    rc = raw_wait_socket(sock, 1, r_to);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
#if defined(_WIN32)
    nr =
        (int)recv(sock, resp_buf + rlen, (int)(sizeof(resp_buf) - 1 - rlen), 0);
#else
    nr = (int)recv(sock, resp_buf + rlen, sizeof(resp_buf) - 1 - rlen, 0);
#endif
    if (nr <= 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    rlen += (size_t)nr;
    resp_buf[rlen] = '\0';
    if (strstr(resp_buf, "\r\n\r\n") != NULL) {
      if (strncmp(resp_buf, "HTTP/1.", 7) == 0 && atoi(resp_buf + 9) == 200) {
        return C_ABSTRACT_HTTP_SUCCESS;
      }
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

/**
 * @brief Close optional TLS session and underlying socket, preserving primary
 * error.
 * @param sock Socket handle (may be RAW_INVALID_SOCKET).
 * @param tls_hooks Optional TLS hook table.
 * @param tls_sess Optional TLS session pointer.
 * @param primary_rc Primary operation status to return if TLS close succeeds.
 * @return Cleanup error if TLS close failed, otherwise @p primary_rc.
 */
NO_DISCARD static c_abstract_http_error_t
raw_close_conn(raw_socket_t sock, const struct HttpRawTlsHooks *tls_hooks,
               void *tls_sess, c_abstract_http_error_t primary_rc) {
  c_abstract_http_error_t crc = C_ABSTRACT_HTTP_SUCCESS;
  if (tls_hooks && tls_sess) {
    crc = tls_hooks->close(tls_sess);
    if (crc != C_ABSTRACT_HTTP_SUCCESS) {
      if (sock != RAW_INVALID_SOCKET) {
        RAW_CLOSESOCKET(sock);
      }
      return (primary_rc != C_ABSTRACT_HTTP_SUCCESS) ? primary_rc : crc;
    }
  }
  if (sock != RAW_INVALID_SOCKET) {
    RAW_CLOSESOCKET(sock);
  }
  return primary_rc;
}

/**
 * @brief Execute one single-hop HTTP/1.x transaction (without following
 * redirects).
 * @param rctx Raw transport context (may be NULL).
 * @param req Effective HTTP request.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_send_single_hop(struct RawCtx *rctx, const struct HttpRequest *req,
                    struct HttpResponse **res) {
  raw_socket_t sock = RAW_INVALID_SOCKET;
  char *host = NULL;
  int port = 80;
  char *path = NULL;
  int is_https = 0;
  char *proxy_host = NULL;
  int proxy_port = 80;
  char *proxy_path = NULL;
  int proxy_https = 0;
  const char *dial_host;
  int dial_port;
  int use_proxy_abs_uri = 0;
  const struct HttpRawTlsHooks *tls_hooks = NULL;
  void *tls_sess = NULL;
  struct sockaddr_in addr;
  char *request_buf = NULL;
  size_t req_len = 0;
  char recv_buf[8192];
  c_abstract_http_error_t rc;
  c_abstract_http_error_t rc_send = C_ABSTRACT_HTTP_SUCCESS;
  size_t body_len = 0;
  char *body = NULL;
  size_t body_cap = 0;
  int conn_res = 0;
  long conn_to =
      math_raw_timeout_ms(rctx, rctx ? rctx->config.connect_timeout_ms : 0);
  long write_to =
      math_raw_timeout_ms(rctx, rctx ? rctx->config.write_timeout_ms : 0);
  long read_to =
      math_raw_timeout_ms(rctx, rctx ? rctx->config.read_timeout_ms : 0);

  rc = parse_url(req->url, &host, &port, &path, &is_https);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (is_https) {
    if (rctx && rctx->config.tls_version_mask != 0 &&
        (rctx->config.tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) ==
            0) {
      free(host);
      free(path);
      return C_ABSTRACT_HTTP_ERR_NOTSUP;
    }
    rc = raw_resolve_tls_hooks(rctx, &tls_hooks);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(host);
      free(path);
      return rc;
    }
    if (!tls_hooks) {
      free(host);
      free(path);
      return C_ABSTRACT_HTTP_ERR_NOTSUP;
    }
  }

  if (rctx && rctx->config.proxy_url) {
    rc = parse_url(rctx->config.proxy_url, &proxy_host, &proxy_port,
                   &proxy_path, &proxy_https);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(proxy_path);
      free(host);
      free(path);
      return rc;
    }
    free(proxy_path);
    dial_host = proxy_host;
    dial_port = proxy_port;
    use_proxy_abs_uri = !is_https;
  } else {
    dial_host = host;
    dial_port = port;
  }

  rc = raw_resolve_addr(dial_host, dial_port, &addr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(proxy_host);
    free(host);
    free(path);
    return rc;
  }

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == RAW_INVALID_SOCKET) {
    free(proxy_host);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  rc = make_socket_nonblocking(sock);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(proxy_host);
    free(host);
    free(path);
    RAW_CLOSESOCKET(sock);
    return rc;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_connect_fail == 1) {
    RAW_CLOSESOCKET(sock);
    free(proxy_host);
    free(host);
    free(path);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  conn_res = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_connect_fail == 2) {
    conn_res = -1;
#if defined(_WIN32)
    WSASetLastError(WSAECONNREFUSED);
#else
    errno = ECONNREFUSED;
#endif
  } else if (g_mock_raw_connect_fail == 3) {
    conn_res = 0;
  }
#endif
  if (conn_res < 0) {
#if defined(_WIN32)
    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
    if (errno != EINPROGRESS)
#endif
    {
      RAW_CLOSESOCKET(sock);
      free(proxy_host);
      free(host);
      free(path);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }

  rc = raw_wait_socket(sock, 2, conn_to);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    RAW_CLOSESOCKET(sock);
    free(proxy_host);
    free(host);
    free(path);
    return rc;
  }

  rc = make_socket_blocking(sock);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    RAW_CLOSESOCKET(sock);
    free(proxy_host);
    free(host);
    free(path);
    return rc;
  }

  if (is_https && proxy_host) {
    rc = raw_proxy_connect_tunnel(rctx, sock, host, port);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      RAW_CLOSESOCKET(sock);
      free(proxy_host);
      free(host);
      free(path);
      return rc;
    }
  }
  free(proxy_host);
  proxy_host = NULL;

  if (is_https) {
    rc = tls_hooks->open(tls_hooks->provider_data, (int)sock, host,
                         rctx ? &rctx->config : NULL, &tls_sess);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      RAW_CLOSESOCKET(sock);
      free(host);
      free(path);
      return rc;
    }
    for (;;) {
      int want = 0;
      rc = tls_hooks->handshake(tls_sess, &want);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(host);
        free(path);
        return raw_close_conn(sock, tls_hooks, tls_sess, rc);
      }
      if (want == 0) {
        break;
      }
      rc = raw_wait_socket(sock, want, conn_to);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free(host);
        free(path);
        return raw_close_conn(sock, tls_hooks, tls_sess, rc);
      }
    }
  }

  rc = raw_build_wire_request(rctx, req, host, path, use_proxy_abs_uri,
                              &request_buf, &req_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(host);
    free(path);
    return raw_close_conn(sock, tls_hooks, tls_sess, rc);
  }
  free(host);
  free(path);

  rc_send = raw_io_write_all(sock, tls_hooks, tls_sess, request_buf, req_len,
                             write_to);
  if (rc_send != C_ABSTRACT_HTTP_SUCCESS) {
    free(request_buf);
    return raw_close_conn(sock, tls_hooks, tls_sess, rc_send);
  }
  free(request_buf);

  *res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_res_alloc_fail) {
    free(*res);
    *res = NULL;
  }
#endif
  if (!*res) {
    return raw_close_conn(sock, tls_hooks, tls_sess, C_ABSTRACT_HTTP_ERR_NOMEM);
  }

  rc = raw_response_init_checked(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    return raw_close_conn(sock, tls_hooks, tls_sess, rc);
  }

  body_cap = 8192;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_realloc_fail == 4)
    body = NULL;
  else
#endif
    body = (char *)malloc(body_cap);
  if (!body) {
    c_abstract_http_error_t rch = http_response_free(*res);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      return raw_close_conn(sock, tls_hooks, tls_sess, rch);
    }
    free(*res);
    *res = NULL;
    return raw_close_conn(sock, tls_hooks, tls_sess, C_ABSTRACT_HTTP_ERR_NOMEM);
  }

  for (;;) {
    size_t nr_bytes = 0;

    if (tls_hooks && tls_sess) {
      int want = 0;
      rc = tls_hooks->read(tls_sess, recv_buf, sizeof(recv_buf), &nr_bytes,
                           &want);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        c_abstract_http_error_t rch;
        free(body);
        rch = http_response_free(*res);
        if (rch != C_ABSTRACT_HTTP_SUCCESS) {
          free(*res);
          *res = NULL;
          return raw_close_conn(sock, tls_hooks, tls_sess, rch);
        }
        free(*res);
        *res = NULL;
        return raw_close_conn(sock, tls_hooks, tls_sess, rc);
      }
      if (want != 0) {
        rc = raw_wait_socket(sock, want, read_to);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          c_abstract_http_error_t rch;
          free(body);
          rch = http_response_free(*res);
          if (rch != C_ABSTRACT_HTTP_SUCCESS) {
            free(*res);
            *res = NULL;
            return raw_close_conn(sock, tls_hooks, tls_sess, rch);
          }
          free(*res);
          *res = NULL;
          return raw_close_conn(sock, tls_hooks, tls_sess, rc);
        }
        continue;
      }
      if (nr_bytes == 0) {
        break;
      }
    } else {
      int r_rc;
      rc = raw_wait_socket(sock, 1, read_to);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        c_abstract_http_error_t rch;
        free(body);
        rch = http_response_free(*res);
        if (rch != C_ABSTRACT_HTTP_SUCCESS) {
          free(*res);
          *res = NULL;
          return raw_close_conn(sock, tls_hooks, tls_sess, rch);
        }
        free(*res);
        *res = NULL;
        return raw_close_conn(sock, tls_hooks, tls_sess, rc);
      }
      r_rc = (int)recv(sock, recv_buf, sizeof(recv_buf), 0);
      if (r_rc < 0) {
#if defined(_WIN32)
        int werr = WSAGetLastError();
        if ((werr == WSAECONNRESET || werr == WSAECONNABORTED) &&
            body_len > 0 && body && strstr(body, "\r\n\r\n") != NULL) {
          break;
        }
#endif
        rc_send = C_ABSTRACT_HTTP_ERR_IO;
        break;
      } else if (r_rc == 0) {
        break;
      }
      nr_bytes = (size_t)r_rc;
    }

    if (body_len + nr_bytes + 1 > body_cap
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
        || g_mock_raw_realloc_fail == 3
#endif
    ) {
      char *new_body;
      body_cap = (body_len + nr_bytes + 1) * 2;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (g_mock_raw_realloc_fail == 3)
        new_body = NULL;
      else
#endif
        new_body = (char *)realloc(body, body_cap);
      if (!new_body) {
        rc_send = C_ABSTRACT_HTTP_ERR_NOMEM;
        break;
      }
      body = new_body;
    }
    memcpy(body + body_len, recv_buf, nr_bytes);
    body_len += nr_bytes;
    body[body_len] = '\0';

    if (math_raw_response_complete(body, body_len, req->method)) {
      break;
    }
  }

  rc = raw_close_conn(sock, tls_hooks, tls_sess, rc_send);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t rch = http_response_free(*res);
    free(body);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      return rch;
    }
    free(*res);
    *res = NULL;
    return rc;
  }

  rc = raw_parse_wire_response(body, body_len, req, *res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t rch;
    free(body);
    rch = http_response_free(*res);
    if (rch != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      return rch;
    }
    free(*res);
    *res = NULL;
    return rc;
  }
  free(body);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Flatten multipart request parts into a temporary request copy when
 * needed.
 * @param req Input request.
 * @param flat_req Scratch request structure.
 * @param out_flattened Set to 1 when @p flat_req owns allocated resources.
 * @param out_eff Effective request pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_prepare_effective_req(const struct HttpRequest *req,
                          struct HttpRequest *flat_req, int *out_flattened,
                          const struct HttpRequest **out_eff) {
  size_t i;
  c_abstract_http_error_t rc;

  *out_flattened = 0;
  *out_eff = req;
  if (req->parts.count == 0 || req->body || req->read_chunk) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!req->parts.parts) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  memset(flat_req, 0, sizeof(*flat_req));
  flat_req->method = req->method;
  flat_req->url = req->url;
  flat_req->parts = req->parts;
  flat_req->on_chunk = req->on_chunk;
  flat_req->on_chunk_user_data = req->on_chunk_user_data;

  rc = http_headers_init(&flat_req->headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  for (i = 0; i < req->headers.count; ++i) {
    rc = http_headers_add(&flat_req->headers, req->headers.headers[i].key,
                          req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t frc = http_headers_free(&flat_req->headers);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        return frc;
      }
      return rc;
    }
  }
  rc = http_request_flatten_parts(flat_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t frc;
    free(flat_req->body);
    flat_req->body = NULL;
    frc = http_headers_free(&flat_req->headers);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return frc;
    }
    return rc;
  }
  *out_flattened = 1;
  *out_eff = flat_req;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up flattened temporary request resources if active.
 * @param flat_req Scratch request structure.
 * @param flattened Non-zero when @p flat_req owns allocated memory.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_cleanup_effective_req(struct HttpRequest *flat_req, int flattened) {
  if (!flattened) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  free(flat_req->body);
  flat_req->body = NULL;
  return http_headers_free(&flat_req->headers);
}

/**
 * @brief Resolve an HTTP Location header against the current request URL.
 * @param cur_url Current request URL.
 * @param location Redirect Location header value.
 * @param out_url Newly allocated resolved URL string.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_resolve_redirect_url(const char *cur_url, const char *location,
                         char **out_url) {
  char *host = NULL;
  int port = 80;
  char *path = NULL;
  int is_https = 0;
  size_t need;
  char *buf;
  c_abstract_http_error_t rc;

  if (strncmp(location, "http://", 7) == 0 ||
      strncmp(location, "https://", 8) == 0) {
    return c_abstract_http_strdup(location, out_url);
  }
  rc = parse_url(cur_url, &host, &port, &path, &is_https);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  free(path);

  need = strlen(host) + strlen(location) + 32;
  buf = (char *)malloc(need);
  if (!buf) {
    free(host);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(buf, need, "%s://%s:%d%s%s", is_https ? "https" : "http", host,
            port, location[0] == '/' ? "" : "/", location);
#else
  sprintf(buf, "%s://%s:%d%s%s", is_https ? "https" : "http", host, port,
          location[0] == '/' ? "" : "/", location);
#endif
  free(host);
  *out_url = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Check whether a response triggers an HTTP redirect hop.
 * @param rctx Raw context.
 * @param res Received response.
 * @param out_loc Output Location header value pointer when redirecting.
 * @return 1 if redirect should be followed, 0 otherwise.
 */
static int math_raw_should_redirect(const struct RawCtx *rctx,
                                    const struct HttpResponse *res,
                                    const char **out_loc) {
  size_t i;
  const char *loc = NULL;
  if (!rctx || !rctx->config.follow_redirects || !res) {
    return 0;
  }
  if (res->status_code != 301 && res->status_code != 302 &&
      res->status_code != 303 && res->status_code != 307 &&
      res->status_code != 308) {
    return 0;
  }
  for (i = 0; i < res->headers.count; ++i) {
    if (res->headers.headers[i].key &&
        math_raw_strcasecmp(res->headers.headers[i].key, "Location") == 0) {
      loc = res->headers.headers[i].value;
      break;
    }
  }
  if (!loc || loc[0] == '\0') {
    return 0;
  }
  *out_loc = loc;
  return 1;
}

/**
 * @brief Release redirect and flattened request buffers at end of
 * `http_raw_send`.
 * @param redir_url Heap redirect URL or NULL.
 * @param flat_req Flattened scratch request.
 * @param flattened Non-zero when `flat_req` owns heap resources.
 * @param res In/out response pointer (freed if cleanup fails).
 * @param primary_rc Primary status to return when cleanup succeeds.
 * @return Final `c_abstract_http_error_t`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_finish_send(char *redir_url, struct HttpRequest *flat_req, int flattened,
                struct HttpResponse **res, c_abstract_http_error_t primary_rc) {
  c_abstract_http_error_t cl_rc;
  free(redir_url);
  cl_rc = raw_cleanup_effective_req(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (*res) {
      c_abstract_http_error_t frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        return frc;
      }
      free(*res);
      *res = NULL;
    }
    return primary_rc != C_ABSTRACT_HTTP_SUCCESS ? primary_rc : cl_rc;
  }
  return primary_rc;
}

/** @brief Send an HTTP request over raw sockets */
NO_DISCARD c_abstract_http_error_t
http_raw_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
              struct HttpResponse **res) {
  struct RawCtx *rctx = (struct RawCtx *)ctx;
  struct HttpRequest flat_req;
  const struct HttpRequest *eff_req = NULL;
  struct HttpRequest hop_req;
  char *redir_url = NULL;
  int flattened = 0;
  int hop;
  c_abstract_http_error_t rc;

  LOG_DEBUG("http_raw_send: Entering");
  if (!req || !res) {
    LOG_DEBUG("http_raw_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = raw_prepare_effective_req(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  hop_req = *eff_req;
  for (hop = 0; hop <= RAW_MAX_REDIRECTS; ++hop) {
    const char *loc = NULL;
    char *next_url = NULL;
    c_abstract_http_error_t frc;
    int status;

    rc = raw_send_single_hop(rctx, &hop_req, res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_finish_send(redir_url, &flat_req, flattened, res, rc);
    }
    if (!math_raw_should_redirect(rctx, *res, &loc)) {
      break;
    }
    if (hop == RAW_MAX_REDIRECTS) {
      frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        return raw_finish_send(redir_url, &flat_req, flattened, res, frc);
      }
      free(*res);
      *res = NULL;
      return raw_finish_send(redir_url, &flat_req, flattened, res,
                             C_ABSTRACT_HTTP_ERR_LOOP);
    }

    status = (*res)->status_code;
    rc = raw_resolve_redirect_url(hop_req.url, loc, &next_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      frc = http_response_free(*res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(*res);
        *res = NULL;
        return raw_finish_send(redir_url, &flat_req, flattened, res, frc);
      }
      free(*res);
      *res = NULL;
      return raw_finish_send(redir_url, &flat_req, flattened, res, rc);
    }
    frc = http_response_free(*res);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*res);
      *res = NULL;
      free(next_url);
      return raw_finish_send(redir_url, &flat_req, flattened, res, frc);
    }
    free(*res);
    *res = NULL;

    free(redir_url);
    redir_url = next_url;
    hop_req.url = redir_url;
    if (status == 303 ||
        ((status == 301 || status == 302) && hop_req.method == HTTP_POST)) {
      hop_req.method = HTTP_GET;
      hop_req.body = NULL;
      hop_req.body_len = 0;
      hop_req.read_chunk = NULL;
    }
  }

  return raw_finish_send(redir_url, &flat_req, flattened, res,
                         C_ABSTRACT_HTTP_SUCCESS);
}

/** @brief Non-blocking state-machine phases for `http_raw_send_multi` */
enum RawSmPhase {
  RAW_SM_CONNECTING = 0,
  RAW_SM_TLS_HANDSHAKE = 1,
  RAW_SM_WRITE_REQ = 2,
  RAW_SM_READ_RESP = 3,
  RAW_SM_DONE = 4
};

struct RawMultiBatch;

/** @brief Per-request state-machine slot registered on `ModalityEventLoop` */
struct RawMultiSlot {
  /** @brief Owning batch coordinator */
  struct RawMultiBatch *batch;
  /** @brief Originating request */
  const struct HttpRequest *orig_req;
  /** @brief Flattened scratch request if multipart */
  struct HttpRequest flat_req;
  /** @brief Effective request for current hop */
  struct HttpRequest hop_req;
  /** @brief Non-zero when flat_req owns memory */
  int flattened;
  /** @brief Redirect URL heap string if redirected */
  char *redir_url;
  /** @brief Redirect hop counter */
  int hop_count;
  /** @brief Target future slot */
  struct HttpFuture *future;
  /** @brief Non-blocking socket descriptor */
  raw_socket_t sock;
  /** @brief Non-zero when registered in `loop` */
  int fd_registered;
  /** @brief Registered timeout timer ID (0 if none) */
  int timer_id;
  /** @brief Current state-machine phase */
  enum RawSmPhase phase;
  /** @brief Non-zero if HTTPS */
  int is_https;
  /** @brief Active TLS hooks */
  const struct HttpRawTlsHooks *tls_hooks;
  /** @brief Active TLS session */
  void *tls_sess;
  /** @brief Outbound wire buffer (headers + body/chunks) */
  char *tx_buf;
  /** @brief Total outbound byte length */
  size_t tx_len;
  /** @brief Sent byte offset */
  size_t tx_off;
  /** @brief Inbound raw response buffer */
  char *rx_buf;
  /** @brief Received byte length */
  size_t rx_len;
  /** @brief Inbound capacity */
  size_t rx_cap;
};

/** @brief Coordinator for concurrent `http_raw_send_multi` state machines */
struct RawMultiBatch {
  /** @brief Transport context */
  struct RawCtx *rctx;
  /** @brief Event loop driving non-blocking sockets */
  struct ModalityEventLoop *loop;
  /** @brief Non-zero when caller supplied an external loop */
  int caller_loop;
  /** @brief Number of still-in-flight slots */
  size_t remaining;
  /** @brief First encountered error across slots */
  c_abstract_http_error_t first_err;
};

NO_DISCARD static c_abstract_http_error_t
raw_sm_start_hop(struct RawMultiSlot *slot);

/**
 * @brief Free socket, TLS session, and I/O buffers on a state-machine slot.
 * @param slot Target state-machine slot.
 * @param prev_err Error code to return if TLS close succeeds.
 * @return First error encountered or `prev_err`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_release_slot_buffers(struct RawMultiSlot *slot,
                            c_abstract_http_error_t prev_err) {
  c_abstract_http_error_t crc = C_ABSTRACT_HTTP_SUCCESS;
  if (slot->tls_hooks && slot->tls_sess) {
    crc = slot->tls_hooks->close(slot->tls_sess);
    if (crc != C_ABSTRACT_HTTP_SUCCESS) {
      slot->tls_sess = NULL;
      if (slot->sock != RAW_INVALID_SOCKET) {
        RAW_CLOSESOCKET(slot->sock);
        slot->sock = RAW_INVALID_SOCKET;
      }
      free(slot->tx_buf);
      slot->tx_buf = NULL;
      slot->tx_len = 0;
      slot->tx_off = 0;
      free(slot->rx_buf);
      slot->rx_buf = NULL;
      slot->rx_len = 0;
      slot->rx_cap = 0;
      return prev_err != C_ABSTRACT_HTTP_SUCCESS ? prev_err : crc;
    }
    slot->tls_sess = NULL;
  }
  if (slot->sock != RAW_INVALID_SOCKET) {
    RAW_CLOSESOCKET(slot->sock);
    slot->sock = RAW_INVALID_SOCKET;
  }
  free(slot->tx_buf);
  slot->tx_buf = NULL;
  slot->tx_len = 0;
  slot->tx_off = 0;
  free(slot->rx_buf);
  slot->rx_buf = NULL;
  slot->rx_len = 0;
  slot->rx_cap = 0;
  return prev_err;
}

/**
 * @brief Tear down socket, TLS session, and loop registrations for a slot.
 * @param slot Target state-machine slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_teardown_io(struct RawMultiSlot *slot) {
  if (slot->timer_id > 0) {
    int tid = slot->timer_id;
    c_abstract_http_error_t trc;
    slot->timer_id = 0;
    trc = http_loop_cancel_timer(slot->batch->loop, tid);
    if (trc != C_ABSTRACT_HTTP_SUCCESS) {
      if (slot->fd_registered && slot->sock != RAW_INVALID_SOCKET) {
        c_abstract_http_error_t frc;
        slot->fd_registered = 0;
        frc = http_loop_remove_fd(slot->batch->loop, (int)slot->sock);
        if (frc != C_ABSTRACT_HTTP_SUCCESS) {
          return raw_sm_release_slot_buffers(slot, frc);
        }
      }
      return raw_sm_release_slot_buffers(slot, trc);
    }
  }
  if (slot->fd_registered && slot->sock != RAW_INVALID_SOCKET) {
    c_abstract_http_error_t frc;
    slot->fd_registered = 0;
    frc = http_loop_remove_fd(slot->batch->loop, (int)slot->sock);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_sm_release_slot_buffers(slot, frc);
    }
  }
  return raw_sm_release_slot_buffers(slot, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Settle future fields, decrement batch counter, and return status.
 * @param slot Target slot.
 * @param res Completed response (NULL on failure).
 * @param status Final error code for this slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop stop error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_settle_slot_state(struct RawMultiSlot *slot, struct HttpResponse *res,
                         c_abstract_http_error_t status) {
  slot->phase = RAW_SM_DONE;
  slot->future->response = res;
  slot->future->error_code = status;
  slot->future->is_ready = 1;
  if (status != C_ABSTRACT_HTTP_SUCCESS &&
      slot->batch->first_err == C_ABSTRACT_HTTP_SUCCESS) {
    slot->batch->first_err = status;
  }
  if (slot->batch->remaining > 0) {
    slot->batch->remaining--;
    if (slot->batch->remaining == 0 && slot->batch->caller_loop) {
      return http_loop_stop(slot->batch->loop);
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Complete a state-machine slot with a final response or error.
 * @param slot Target slot.
 * @param res Completed response (NULL on failure).
 * @param status Final error code for this slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop stop error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_finish_slot(struct RawMultiSlot *slot, struct HttpResponse *res,
                   c_abstract_http_error_t status) {
  c_abstract_http_error_t td_rc = raw_sm_teardown_io(slot);
  if (td_rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t eff_err =
        (status != C_ABSTRACT_HTTP_SUCCESS) ? status : td_rc;
    c_abstract_http_error_t st_rc =
        raw_sm_settle_slot_state(slot, res, eff_err);
    if (st_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return st_rc;
    }
    return td_rc;
  }
  return raw_sm_settle_slot_state(slot, res, status);
}

/**
 * @brief Timeout callback for an in-flight non-blocking raw socket slot.
 * @param loop Active event loop.
 * @param timer_id Fired timer ID.
 * @param user_data Slot pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop stop error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_timer_cb(struct ModalityEventLoop *loop, int timer_id, void *user_data) {
  struct RawMultiSlot *slot = (struct RawMultiSlot *)user_data;
  (void)loop;
  (void)timer_id;
  slot->timer_id = 0;
  return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_TIMEOUT);
}

/**
 * @brief Finalize received raw bytes in `slot->rx_buf` into response or
 * redirect.
 * @param slot Completed read slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_complete_response(struct RawMultiSlot *slot) {
  struct HttpResponse *res;
  const char *loc = NULL;
  c_abstract_http_error_t rc;

  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  if (!res) {
    return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_NOMEM);
  }
  rc = raw_response_init_checked(res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(res);
    return raw_sm_finish_slot(slot, NULL, rc);
  }

  rc = raw_parse_wire_response(slot->rx_buf, slot->rx_len, &slot->hop_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t frc = http_response_free(res);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      free(res);
      return raw_sm_finish_slot(slot, NULL, frc);
    }
    free(res);
    return raw_sm_finish_slot(slot, NULL, rc);
  }

  if (math_raw_should_redirect(slot->batch->rctx, res, &loc)) {
    char *next_url = NULL;
    c_abstract_http_error_t frc;
    int status = res->status_code;
    if (slot->hop_count >= RAW_MAX_REDIRECTS) {
      frc = http_response_free(res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(res);
        return raw_sm_finish_slot(slot, NULL, frc);
      }
      free(res);
      return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_LOOP);
    }
    rc = raw_resolve_redirect_url(slot->hop_req.url, loc, &next_url);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      frc = http_response_free(res);
      if (frc != C_ABSTRACT_HTTP_SUCCESS) {
        free(res);
        return raw_sm_finish_slot(slot, NULL, frc);
      }
      free(res);
      return raw_sm_finish_slot(slot, NULL, rc);
    }
    frc = http_response_free(res);
    if (frc != C_ABSTRACT_HTTP_SUCCESS) {
      free(res);
      free(next_url);
      return raw_sm_finish_slot(slot, NULL, frc);
    }
    free(res);
    rc = raw_sm_teardown_io(slot);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(next_url);
      return raw_sm_finish_slot(slot, NULL, rc);
    }
    free(slot->redir_url);
    slot->redir_url = next_url;
    slot->hop_req.url = slot->redir_url;
    slot->hop_count++;
    if (status == 303 || ((status == 301 || status == 302) &&
                          slot->hop_req.method == HTTP_POST)) {
      slot->hop_req.method = HTTP_GET;
      slot->hop_req.body = NULL;
      slot->hop_req.body_len = 0;
      slot->hop_req.read_chunk = NULL;
    }
    return raw_sm_start_hop(slot);
  }

  return raw_sm_finish_slot(slot, res, C_ABSTRACT_HTTP_SUCCESS);
}

/**
 * @brief Socket readiness callback driving the non-blocking HTTP/1.x state
 * machine.
 * @param loop Active event loop.
 * @param fd Ready socket descriptor.
 * @param events Ready event mask.
 * @param user_data Slot pointer (`struct RawMultiSlot *`).
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_io_cb(struct ModalityEventLoop *loop, int fd, int events,
             void *user_data) {
  struct RawMultiSlot *slot = (struct RawMultiSlot *)user_data;
  c_abstract_http_error_t rc;
  (void)fd;

  if ((events & HTTP_LOOP_ERROR) &&
      !(slot->phase == RAW_SM_READ_RESP && (events & HTTP_LOOP_READ))) {
    return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
  }

  if (slot->phase == RAW_SM_CONNECTING) {
    int so_err = 0;
#if defined(_WIN32)
    int optlen = (int)sizeof(so_err);
    if (getsockopt(slot->sock, SOL_SOCKET, SO_ERROR, (char *)&so_err,
                   &optlen) != 0 ||
        so_err != 0) {
      return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
    }
#else
    socklen_t optlen = (socklen_t)sizeof(so_err);
    if (getsockopt(slot->sock, SOL_SOCKET, SO_ERROR, &so_err, &optlen) != 0 ||
        so_err != 0) {
      return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
    }
#endif
    if (slot->is_https) {
      slot->phase = RAW_SM_TLS_HANDSHAKE;
    } else {
      slot->phase = RAW_SM_WRITE_REQ;
    }
  }

  if (slot->phase == RAW_SM_TLS_HANDSHAKE) {
    int want = 0;
    rc = slot->tls_hooks->handshake(slot->tls_sess, &want);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_sm_finish_slot(slot, NULL, rc);
    }
    if (want != 0) {
      return http_loop_mod_fd(loop, (int)slot->sock,
                              want == 1 ? HTTP_LOOP_READ : HTTP_LOOP_WRITE);
    }
    slot->phase = RAW_SM_WRITE_REQ;
    rc = http_loop_mod_fd(loop, (int)slot->sock, HTTP_LOOP_WRITE);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_sm_finish_slot(slot, NULL, rc);
    }
  }

  if (slot->phase == RAW_SM_WRITE_REQ) {
    while (slot->tx_off < slot->tx_len) {
      if (slot->tls_hooks && slot->tls_sess) {
        size_t nw = 0;
        int want = 0;
        rc = slot->tls_hooks->write(slot->tls_sess, slot->tx_buf + slot->tx_off,
                                    slot->tx_len - slot->tx_off, &nw, &want);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          return raw_sm_finish_slot(slot, NULL, rc);
        }
        if (want != 0) {
          return http_loop_mod_fd(loop, (int)slot->sock,
                                  want == 1 ? HTTP_LOOP_READ : HTTP_LOOP_WRITE);
        }
        if (nw == 0) {
          return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
        }
        slot->tx_off += nw;
      } else {
        long s_rc;
        size_t rem = slot->tx_len - slot->tx_off;
#if defined(_WIN32)
        s_rc = (long)send(slot->sock, slot->tx_buf + slot->tx_off, (int)rem, 0);
        if (s_rc < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
          return C_ABSTRACT_HTTP_SUCCESS;
        }
#else
        s_rc = (long)send(slot->sock, slot->tx_buf + slot->tx_off, rem, 0);
        if (s_rc < 0 && (errno == EAGAIN
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
                         || errno == EWOULDBLOCK
#endif
                         )) {
          return C_ABSTRACT_HTTP_SUCCESS;
        }
#endif
        if (s_rc <= 0) {
          return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
        }
        slot->tx_off += (size_t)s_rc;
      }
    }
    slot->phase = RAW_SM_READ_RESP;
    rc = http_loop_mod_fd(loop, (int)slot->sock, HTTP_LOOP_READ);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_sm_finish_slot(slot, NULL, rc);
    }
  }

  for (;;) {
    char rbuf[4096];
    size_t nr = 0;
    if (slot->tls_hooks && slot->tls_sess) {
      int want = 0;
      rc =
          slot->tls_hooks->read(slot->tls_sess, rbuf, sizeof(rbuf), &nr, &want);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return raw_sm_finish_slot(slot, NULL, rc);
      }
      if (want != 0) {
        return http_loop_mod_fd(loop, (int)slot->sock,
                                want == 1 ? HTTP_LOOP_READ : HTTP_LOOP_WRITE);
      }
      if (nr == 0) {
        return raw_sm_complete_response(slot);
      }
    } else {
      int r_rc = (int)recv(slot->sock, rbuf, sizeof(rbuf), 0);
      if (r_rc < 0) {
#if defined(_WIN32)
        int werr = WSAGetLastError();
        if (werr == WSAEWOULDBLOCK) {
          return C_ABSTRACT_HTTP_SUCCESS;
        }
        if ((werr == WSAECONNRESET || werr == WSAECONNABORTED) &&
            slot->rx_len > 0 && strstr(slot->rx_buf, "\r\n\r\n") != NULL) {
          return raw_sm_complete_response(slot);
        }
#else
        if (errno == EAGAIN
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
            || errno == EWOULDBLOCK
#endif
        ) {
          return C_ABSTRACT_HTTP_SUCCESS;
        }
#endif
        return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
      }
      if (r_rc == 0) {
        return raw_sm_complete_response(slot);
      }
      nr = (size_t)r_rc;
    }

    if (slot->rx_len + nr + 1 > slot->rx_cap) {
      char *nb;
      size_t ncap = slot->rx_cap == 0 ? 8192 : (slot->rx_len + nr + 1) * 2;
      nb = (char *)realloc(slot->rx_buf, ncap);
      if (!nb) {
        return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_NOMEM);
      }
      slot->rx_buf = nb;
      slot->rx_cap = ncap;
    }
    memcpy(slot->rx_buf + slot->rx_len, rbuf, nr);
    slot->rx_len += nr;
    slot->rx_buf[slot->rx_len] = '\0';

    if (math_raw_response_complete(slot->rx_buf, slot->rx_len,
                                   slot->hop_req.method)) {
      return raw_sm_complete_response(slot);
    }
  }
}

/**
 * @brief Start or restart a non-blocking HTTP hop for a slot on `batch->loop`.
 * @param slot Target state-machine slot.
 * @return C_ABSTRACT_HTTP_SUCCESS or loop error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_sm_start_hop(struct RawMultiSlot *slot) {
  struct RawCtx *rctx = slot->batch->rctx;
  char *host = NULL;
  int port = 80;
  char *path = NULL;
  char *proxy_host = NULL;
  int proxy_port = 80;
  char *proxy_path = NULL;
  int proxy_https = 0;
  const char *dial_host;
  int dial_port;
  int use_proxy_abs_uri = 0;
  struct sockaddr_in addr;
  int conn_res;
  long to_ms = math_raw_timeout_ms(rctx, rctx ? rctx->config.timeout_ms : 0);
  c_abstract_http_error_t rc;

  rc = parse_url(slot->hop_req.url, &host, &port, &path, &slot->is_https);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return raw_sm_finish_slot(slot, NULL, rc);
  }

  if (slot->is_https) {
    rc = raw_resolve_tls_hooks(rctx, &slot->tls_hooks);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(host);
      free(path);
      return raw_sm_finish_slot(slot, NULL, rc);
    }
    if (!slot->tls_hooks) {
      free(host);
      free(path);
      return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_NOTSUP);
    }
  } else {
    slot->tls_hooks = NULL;
  }

  if (rctx && rctx->config.proxy_url && !slot->is_https) {
    rc = parse_url(rctx->config.proxy_url, &proxy_host, &proxy_port,
                   &proxy_path, &proxy_https);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(proxy_path);
      free(host);
      free(path);
      return raw_sm_finish_slot(slot, NULL, rc);
    }
    free(proxy_path);
    dial_host = proxy_host;
    dial_port = proxy_port;
    use_proxy_abs_uri = 1;
  } else {
    dial_host = host;
    dial_port = port;
  }

  rc = raw_resolve_addr(dial_host, dial_port, &addr);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(proxy_host);
    free(host);
    free(path);
    return raw_sm_finish_slot(slot, NULL, rc);
  }
  free(proxy_host);

  rc = raw_build_wire_request(rctx, &slot->hop_req, host, path,
                              use_proxy_abs_uri, &slot->tx_buf, &slot->tx_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(path);
    free(host);
    return raw_sm_finish_slot(slot, NULL, rc);
  }
  free(path);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_connect_fail == 2) {
    slot->sock = RAW_INVALID_SOCKET;
  } else
#endif
  {
    slot->sock = socket(AF_INET, SOCK_STREAM, 0);
  }
  if (slot->sock == RAW_INVALID_SOCKET) {
    free(host);
    return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
  }
  rc = make_socket_nonblocking(slot->sock);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(host);
    return raw_sm_finish_slot(slot, NULL, rc);
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_raw_connect_fail == 1) {
#if !defined(_WIN32)
    errno = ECONNREFUSED;
#endif
    conn_res = -1;
  } else
#endif
  {
    conn_res = connect(slot->sock, (struct sockaddr *)&addr, sizeof(addr));
  }
  if (conn_res < 0) {
#if defined(_WIN32)
    if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
    if (errno != EINPROGRESS)
#endif
    {
      free(host);
      return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_IO);
    }
  }

  if (slot->is_https) {
    rc = slot->tls_hooks->open(slot->tls_hooks->provider_data, (int)slot->sock,
                               host, rctx ? &rctx->config : NULL,
                               &slot->tls_sess);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(host);
      return raw_sm_finish_slot(slot, NULL, rc);
    }
  }
  free(host);

  slot->phase = RAW_SM_CONNECTING;
  rc = http_loop_add_fd(slot->batch->loop, (int)slot->sock, HTTP_LOOP_WRITE,
                        raw_sm_io_cb, slot);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return raw_sm_finish_slot(slot, NULL, rc);
  }
  slot->fd_registered = 1;

  rc = http_loop_add_timer(slot->batch->loop, to_ms, raw_sm_timer_cb, slot,
                           &slot->timer_id);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return raw_sm_finish_slot(slot, NULL, rc);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Arm one slot in `http_raw_send_multi`.
 * @param slot Initialized slot entry.
 * @return C_ABSTRACT_HTTP_SUCCESS or slot error code.
 */
NO_DISCARD static c_abstract_http_error_t
raw_arm_multi_slot(struct RawMultiSlot *slot) {
  const struct HttpRequest *eff = NULL;
  c_abstract_http_error_t rc;

  if (!slot->orig_req || !slot->future) {
    if (slot->future) {
      return raw_sm_finish_slot(slot, NULL, C_ABSTRACT_HTTP_ERR_INVAL);
    }
    slot->batch->remaining--;
    slot->batch->first_err = C_ABSTRACT_HTTP_ERR_INVAL;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  rc = raw_prepare_effective_req(slot->orig_req, &slot->flat_req,
                                 &slot->flattened, &eff);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return raw_sm_finish_slot(slot, NULL, rc);
  }
  slot->hop_req = *eff;
  return raw_sm_start_hop(slot);
}

/**
 * @brief Tear down all slots and event loop at end of `http_raw_send_multi`.
 * @param slots Slot array to release.
 * @param count Number of slots.
 * @param owned_loop Internally owned loop or NULL.
 * @param caller_loop Caller-provided loop or NULL.
 * @param primary_rc Primary batch status.
 * @return Final `c_abstract_http_error_t`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_finish_multi_cleanup(struct RawMultiSlot *slots, size_t count,
                         struct ModalityEventLoop *owned_loop,
                         struct ModalityEventLoop *caller_loop,
                         c_abstract_http_error_t primary_rc) {
  size_t i;
  c_abstract_http_error_t cleanup_rc;

  for (i = 0; i < count; ++i) {
    c_abstract_http_error_t td_rc = raw_sm_teardown_io(&slots[i]);
    if (td_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slots[i].redir_url);
      free(slots);
      if (owned_loop) {
        c_abstract_http_error_t lrc = http_loop_free(owned_loop);
        if (lrc != C_ABSTRACT_HTTP_SUCCESS) {
          return lrc;
        }
      }
      return td_rc;
    }
    cleanup_rc =
        raw_cleanup_effective_req(&slots[i].flat_req, slots[i].flattened);
    if (cleanup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(slots[i].redir_url);
      free(slots);
      if (owned_loop) {
        c_abstract_http_error_t lrc = http_loop_free(owned_loop);
        if (lrc != C_ABSTRACT_HTTP_SUCCESS) {
          return lrc;
        }
      }
      return cleanup_rc;
    }
    free(slots[i].redir_url);
  }
  free(slots);

  if (owned_loop) {
    cleanup_rc = http_loop_free(owned_loop);
    if (cleanup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_rc;
    }
  } else if (caller_loop) {
    cleanup_rc = abstract_http_event_loop_test_unstop(caller_loop);
    if (cleanup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_rc;
    }
    cleanup_rc = http_loop_wakeup(caller_loop);
    if (cleanup_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return cleanup_rc;
    }
  }
  return primary_rc;
}

/** @brief Multiplex multiple HTTP requests non-blockingly on ModalityEventLoop
 */
NO_DISCARD c_abstract_http_error_t http_raw_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *reqs, struct HttpFuture **future) {
  struct RawCtx *rctx = (struct RawCtx *)ctx;
  struct ModalityEventLoop *owned_loop = NULL;
  struct ModalityEventLoop *active_loop;
  struct RawMultiBatch batch;
  struct RawMultiSlot *slots;
  size_t i;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

  LOG_DEBUG("http_raw_send_multi: Entering");
  if (!ctx || !reqs || !future) {
    LOG_DEBUG("http_raw_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (reqs->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  if (loop) {
    active_loop = loop;
  } else {
    rc = http_loop_init(&owned_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    active_loop = owned_loop;
  }

  slots = (struct RawMultiSlot *)calloc(reqs->count, sizeof(*slots));
  if (!slots) {
    if (owned_loop) {
      c_abstract_http_error_t lrc = http_loop_free(owned_loop);
      if (lrc != C_ABSTRACT_HTTP_SUCCESS) {
        return lrc;
      }
    }
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  memset(&batch, 0, sizeof(batch));
  batch.rctx = rctx;
  batch.loop = active_loop;
  batch.caller_loop = (loop != NULL) ? 1 : 0;
  batch.remaining = reqs->count;
  batch.first_err = C_ABSTRACT_HTTP_SUCCESS;

  for (i = 0; i < reqs->count; ++i) {
    slots[i].batch = &batch;
    slots[i].orig_req = reqs->requests[i];
    slots[i].future = future[i];
    slots[i].sock = RAW_INVALID_SOCKET;
    rc = raw_arm_multi_slot(&slots[i]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_finish_multi_cleanup(
          slots, reqs->count, owned_loop, loop,
          batch.first_err != C_ABSTRACT_HTTP_SUCCESS ? batch.first_err : rc);
    }
  }

  if (batch.remaining > 0) {
    rc = http_loop_run(active_loop);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_finish_multi_cleanup(
          slots, reqs->count, owned_loop, loop,
          batch.first_err != C_ABSTRACT_HTTP_SUCCESS ? batch.first_err : rc);
    }
  }

  LOG_DEBUG("http_raw_send_multi: Exiting");
  return raw_finish_multi_cleanup(slots, reqs->count, owned_loop, loop,
                                  batch.first_err);
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief State for mock TLS hooks inside `c_abstract_http_test_raw_edges` */
struct RawEdgeTlsState {
  int mode;       /**< Selected TLS simulation mode */
  int call_count; /**< Invocation counter */
};

/**
 * @brief Mock TLS open hook for raw state-machine edge tests.
 * @param pdata Provider state pointer.
 * @param fd Socket descriptor.
 * @param host Target hostname.
 * @param cfg HTTP configuration.
 * @param out_sess Output session pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_tls_open(void *pdata, int fd, const char *host,
                  const struct HttpConfig *cfg, void **out_sess) {
  struct RawEdgeTlsState *st = (struct RawEdgeTlsState *)pdata;
  (void)fd;
  (void)host;
  (void)cfg;
  if (st->mode == 1) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  *out_sess = st;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock TLS handshake hook for raw state-machine edge tests.
 * @param sess Session pointer.
 * @param out_want Output readiness direction (`0`, `1`, or `2`).
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_tls_handshake(void *sess, int *out_want) {
  struct RawEdgeTlsState *st = (struct RawEdgeTlsState *)sess;
  st->call_count++;
  *out_want = 0;
  if (st->mode == 2) {
    return C_ABSTRACT_HTTP_ERR_ACCES;
  }
  if (st->mode == 3) {
    *out_want = 1;
  } else if (st->mode == 4) {
    *out_want = 2;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock TLS write hook for raw state-machine edge tests.
 * @param sess Session pointer.
 * @param buf Buffer to write.
 * @param len Byte count.
 * @param out_written Output bytes written.
 * @param out_want Output readiness direction.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_tls_write(void *sess, const void *buf, size_t len, size_t *out_written,
                   int *out_want) {
  struct RawEdgeTlsState *st = (struct RawEdgeTlsState *)sess;
  (void)buf;
  *out_written = 0;
  *out_want = 0;
  if (st->mode == 5) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (st->mode == 6) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->mode == 7) {
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->mode == 8) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_written = len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock TLS read hook for raw state-machine edge tests.
 * @param sess Session pointer.
 * @param buf Destination buffer.
 * @param cap Buffer capacity.
 * @param out_read Output bytes read.
 * @param out_want Output readiness direction.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_tls_read(void *sess, void *buf, size_t cap, size_t *out_read,
                  int *out_want) {
  struct RawEdgeTlsState *st = (struct RawEdgeTlsState *)sess;
  const char resp[] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
  *out_read = 0;
  *out_want = 0;
  if (st->mode == 9) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (st->mode == 10) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->mode == 11) {
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->mode == 12) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (st->mode == 13 && cap >= 4096) {
    st->call_count++;
    if (st->call_count <= 3) {
      memset(buf, 'A', 4000);
      *out_read = 4000;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  memcpy(buf, resp, sizeof(resp) - 1);
  *out_read = sizeof(resp) - 1;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Mock TLS close hook for raw state-machine edge tests.
 * @param sess Session pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
NO_DISCARD static c_abstract_http_error_t raw_edge_tls_close(void *sess) {
  struct RawEdgeTlsState *st = (struct RawEdgeTlsState *)sess;
  if (st && st->mode == 14) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Streaming upload callback for raw wire builder edge tests.
 * @param ud Pointer to `int *` counter.
 * @param buf Destination buffer.
 * @param cap Capacity.
 * @param out_read Output bytes produced.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_read_chunk_cb(void *ud, void *buf, size_t cap, size_t *out_read) {
  int *cnt = (int *)ud;
  (void)cap;
  (*cnt)++;
  if (*cnt == -9) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (*cnt == 1) {
    memcpy(buf, "chunk", 5);
    *out_read = 5;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_read = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free heap char buffer pointer and percolate primary error code.
 * @param ptr Address of heap char pointer to free and zero.
 * @param rc Status code to percolate.
 * @return `rc`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_free_ret(char **ptr, c_abstract_http_error_t rc) {
  free(*ptr);
  *ptr = NULL;
  return rc;
}

/**
 * @brief Free heap void buffer pointer and percolate primary error code.
 * @param ptr Address of heap void pointer to free and zero.
 * @param rc Status code to percolate.
 * @return `rc`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_free_void(void **ptr, c_abstract_http_error_t rc) {
  free(*ptr);
  *ptr = NULL;
  return rc;
}

/**
 * @brief Reset integer fault-injection flag and percolate status code.
 * @param flag Pointer to integer flag to zero.
 * @param rc Status code to percolate.
 * @return `rc`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_reset_ret(int *flag, c_abstract_http_error_t rc) {
  *flag = 0;
  return rc;
}

/**
 * @brief Close optional POSIX descriptors, free loop, and percolate status.
 * @param loop Optional event loop to destroy.
 * @param p0 First descriptor (`-1` if unused).
 * @param p1 Second descriptor (`-1` if unused).
 * @param primary_rc Primary status code.
 * @return Loop cleanup error or `primary_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_close_fds_loop(struct ModalityEventLoop *loop, int p0, int p1,
                        c_abstract_http_error_t primary_rc) {
#if defined(_WIN32)
  (void)p0;
  (void)p1;
#else
  if (p0 >= 0) {
    close(p0);
  }
  if (p1 >= 0) {
    close(p1);
  }
#endif
  if (loop) {
    c_abstract_http_error_t lrc = http_loop_free(loop);
    if (lrc != C_ABSTRACT_HTTP_SUCCESS) {
      return lrc;
    }
  }
  return primary_rc;
}

/**
 * @brief Clean up slot, future response, pipes, and loop at end of an edge
 * step.
 * @param slot State-machine slot.
 * @param fut Target future.
 * @param loop Event loop to destroy.
 * @param p0 Optional pipe read fd (`-1` if unused/owned by slot).
 * @param p1 Optional pipe write fd (`-1` if unused).
 * @param primary_rc Status from the tested step.
 * @return Cleanup error or `primary_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
raw_edge_finish_step(struct RawMultiSlot *slot, struct HttpFuture *fut,
                     struct ModalityEventLoop *loop, int p0, int p1,
                     c_abstract_http_error_t primary_rc) {
  c_abstract_http_error_t crc;
  free(slot->redir_url);
  slot->redir_url = NULL;
  if (fut && fut->response) {
    c_abstract_http_error_t rfr = http_response_free(fut->response);
    free(fut->response);
    fut->response = NULL;
    if (rfr != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_edge_close_fds_loop(loop, p0, p1, rfr);
    }
  }
  crc = raw_sm_teardown_io(slot);
  if (crc != C_ABSTRACT_HTTP_SUCCESS) {
    return raw_edge_close_fds_loop(loop, p0, p1, crc);
  }
  return raw_edge_close_fds_loop(loop, p0, p1, primary_rc);
}

/**
 * @brief Exercise internal raw wire-builder, chunked-decoder, and state-machine
 * edge paths for 100% branch and line coverage.
 * @param step Sub-case selector (`0..59`).
 * @return Status of selected sub-case.
 */
NO_DISCARD c_abstract_http_error_t c_abstract_http_test_raw_edges(int step);

NO_DISCARD c_abstract_http_error_t c_abstract_http_test_raw_edges(int step) {
  struct RawEdgeTlsState tls_st;
  struct HttpRawTlsHooks hooks;
  struct RawCtx rctx;
  struct ModalityEventLoop *loop = NULL;
  struct RawMultiBatch batch;
  struct RawMultiSlot slot;
  struct HttpFuture fut;
  struct HttpRequest req;
  struct HttpResponse res;
  struct HttpResponse *res_ptr = NULL;
  char *out_buf = NULL;
  size_t out_len = 0;
  char s_https_url[] = "https://127.0.0.1:1/";
  char s_http_url[] = "http://127.0.0.1:1/";
  char s_up_url[] = "http://example.com/upload";
  char s_bad_url[] = "://bad";
  char s_start_url[] = "http://127.0.0.1:1/start";
  char s_k0[] = "X-Custom";
  char s_v0[] = "v";
  char s_k1[] = "Content-Length";
  char s_v1[] = "5";
  char s_ua[] = "custom-ua";
  char s_pu[] = "pu";
  char s_pp[] = "pp";
  char s_body[] = "12345";
  c_abstract_http_error_t rc;

  memset(&tls_st, 0, sizeof(tls_st));
  memset(&hooks, 0, sizeof(hooks));
  hooks.provider_data = &tls_st;
  hooks.open = raw_edge_tls_open;
  hooks.handshake = raw_edge_tls_handshake;
  hooks.write = raw_edge_tls_write;
  hooks.read = raw_edge_tls_read;
  hooks.close = raw_edge_tls_close;

  memset(&rctx, 0, sizeof(rctx));
  rctx.has_custom_tls = 1;
  rctx.tls_hooks = hooks;
  rctx.config.follow_redirects = 1;

  if (step == 0) {
    char big_chunk[260];
    const char tr_resp[] = "HTTP/1.1 200 OK\r\ntransfer-encoding: "
                           "chunked\r\n\r\n5\r\nHello\r\n0\r\nX-T: 1\r\n\r\n";
    const char tr_inc[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nX-T: 1\r\n";
    const char no_hdr[] = "HTTP/1.1 200 OK\r\n\r\n";
    const char short_te[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chu\r\n\r\n";
    out_len = (size_t)(math_raw_strcasestr_has("chu", "chunked") +
                       math_raw_response_complete(
                           short_te, sizeof(short_te) - 1, HTTP_GET) +
                       math_raw_response_complete(no_hdr, sizeof(no_hdr) - 1,
                                                  HTTP_GET) +
                       math_raw_response_complete(tr_inc, sizeof(tr_inc) - 1,
                                                  HTTP_GET) +
                       math_raw_response_complete(tr_resp, sizeof(tr_resp) - 1,
                                                  HTTP_GET));
    memset(&req, 0, sizeof(req));
    memset(&res, 0, sizeof(res));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(big_chunk, sizeof(big_chunk), "5\r\nHello\r\n96\r\n");
#else
    sprintf(big_chunk, "5\r\nHello\r\n96\r\n");
#endif
    out_len = strlen(big_chunk);
    memset(big_chunk + out_len, 'Z', 150);
    memcpy(big_chunk + out_len + 150, "\r\n0\r\n\r\n", 7);
    return raw_edge_free_void(
        &res.body,
        raw_decode_chunked_body(big_chunk, out_len + 157, &req, &res));
  }

  if (step == 1) {
    memset(&req, 0, sizeof(req));
    req.method = (enum HttpMethod)(-1);
    req.url = s_http_url;
    return raw_build_wire_request(&rctx, &req, "example.com", "/", 0, &out_buf,
                                  &out_len);
  }

  if ((step >= 2 && step <= 6) || step == 53) {
    int cnt = (step == 53) ? -10 : 0;
    struct HttpHeader hdrs[2];
    memset(&req, 0, sizeof(req));
    req.method = (step == 6) ? HTTP_GET : HTTP_POST;
    req.url = s_up_url;
    hdrs[0].key = s_k0;
    hdrs[0].value = s_v0;
    hdrs[1].key = s_k1;
    hdrs[1].value = s_v1;
    req.headers.headers = hdrs;
    req.headers.count = (step == 4 || step == 5) ? 2 : 1;
    rctx.config.user_agent = s_ua;
    rctx.config.proxy_username = s_pu;
    rctx.config.proxy_password = s_pp;
    if (step == 5) {
      req.body = s_body;
      req.body_len = 5;
    } else if (step != 6) {
      req.read_chunk = raw_edge_read_chunk_cb;
      req.read_chunk_user_data = &cnt;
      req.expected_body_len = (step == 3) ? 5 : 0;
    }
    return raw_edge_free_ret(
        &out_buf, raw_build_wire_request(&rctx, &req, "example.com", "/upload",
                                         1, &out_buf, &out_len));
  }

  if (step == 7 || step == 34) {
    tls_st.mode = (step == 7) ? 8 : 6;
    return raw_io_write_all(0, &hooks, &tls_st, "GET", 3, 1);
  }

  if (step == 8 || step == 35) {
    return raw_edge_free_ret(
        &out_buf,
        raw_resolve_redirect_url((step == 8) ? "://bad" : "http://ex.com/a",
                                 "/rel", &out_buf));
  }

  if (step == 36) {
#if !defined(_WIN32)
    int sv[2] = {-1, -1};
    out_len = (size_t)socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    return raw_edge_close_fds_loop(
        NULL, sv[0], sv[1],
        raw_proxy_connect_tunnel(&rctx, sv[0], "ex.com", 443));
#else
    return C_ABSTRACT_HTTP_SUCCESS;
#endif
  }

  if (step == 37 || step == 38) {
    memset(&req, 0, sizeof(req));
    req.url = s_https_url;
    rctx.has_custom_tls = 0;
    g_mock_raw_send_fail = (step == 37) ? 2 : 3;
    return raw_edge_reset_ret(&g_mock_raw_send_fail,
                              raw_send_single_hop(&rctx, &req, &res_ptr));
  }

  if (step == 39) {
    struct HttpRequest freq;
    c_abstract_http_error_t fin_rc;
    memset(&freq, 0, sizeof(freq));
    res_ptr = (struct HttpResponse *)calloc(1, sizeof(*res_ptr));
    fin_rc = raw_finish_send(NULL, &freq, 1, &res_ptr, C_ABSTRACT_HTTP_SUCCESS);
    if (fin_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(res_ptr);
      return fin_rc;
    }
    free(res_ptr);
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  rc = http_loop_init(&loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (step == 40) {
    struct RawMultiSlot *ms = (struct RawMultiSlot *)calloc(1, sizeof(*ms));
    if (!ms) {
      return raw_edge_close_fds_loop(loop, -1, -1, C_ABSTRACT_HTTP_ERR_NOMEM);
    }
    memset(&batch, 0, sizeof(batch));
    batch.loop = loop;
    ms[0].batch = &batch;
    ms[0].sock = RAW_INVALID_SOCKET;
    ms[0].timer_id = 9999;
    return raw_finish_multi_cleanup(ms, 1, loop, NULL, C_ABSTRACT_HTTP_SUCCESS);
  }

  memset(&batch, 0, sizeof(batch));
  memset(&slot, 0, sizeof(slot));
  memset(&fut, 0, sizeof(fut));
  batch.rctx = &rctx;
  batch.loop = loop;
  batch.remaining = 1;
  batch.caller_loop = 1;
  slot.batch = &batch;
  slot.future = &fut;
  slot.sock = RAW_INVALID_SOCKET;

  if (step == 59) {
    slot.timer_id = 9999;
    return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                C_ABSTRACT_HTTP_SUCCESS);
  }

  if (step >= 9 && step <= 12) {
    int pipe_fds[2] = {-1, -1};
    int saved_fd = -1;
#if !defined(_WIN32)
    if (pipe(pipe_fds) == 0) {
      rc = http_loop_add_fd(loop, pipe_fds[0], HTTP_LOOP_READ, raw_sm_io_cb,
                            &slot);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return raw_edge_finish_step(&slot, &fut, loop, pipe_fds[0], pipe_fds[1],
                                    rc);
      }
      slot.sock = pipe_fds[0];
      slot.fd_registered = 1;
      pipe_fds[0] = -1;
    }
#endif
    rc = http_loop_add_timer(loop, 500, raw_sm_timer_cb, &slot, &slot.timer_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_edge_finish_step(&slot, &fut, loop, pipe_fds[0], pipe_fds[1],
                                  rc);
    }
    if (step == 9 || step == 10) {
      if (step == 9) {
        tls_st.mode = 14;
        slot.tls_hooks = &hooks;
        slot.tls_sess = &tls_st;
      }
      return raw_edge_finish_step(
          &slot, &fut, loop, pipe_fds[0], pipe_fds[1],
          raw_sm_finish_slot(&slot, NULL, C_ABSTRACT_HTTP_SUCCESS));
    }
    saved_fd = (int)slot.sock;
    slot.timer_id = (step == 11) ? 0 : 9999;
    return raw_edge_finish_step(
        &slot, &fut, loop, saved_fd, pipe_fds[1],
        (step == 11) ? raw_sm_finish_slot(&slot, NULL, C_ABSTRACT_HTTP_SUCCESS)
                     : raw_sm_teardown_io(&slot));
  }

  if (step >= 13 && step <= 17) {
    char ok_redir[] = "HTTP/1.1 302 Found\r\nLocation: /next\r\n\r\n";
    char bad_wire[] =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZZ\r\n";
    slot.hop_req.url = (step == 16) ? s_bad_url : s_start_url;
    slot.hop_req.method = HTTP_POST;
    if (step == 13) {
      slot.rx_buf = (char *)malloc(sizeof(bad_wire));
      if (slot.rx_buf) {
        memcpy(slot.rx_buf, bad_wire, sizeof(bad_wire));
        slot.rx_len = sizeof(bad_wire) - 1;
      }
    } else {
      slot.rx_buf = (char *)malloc(sizeof(ok_redir));
      if (slot.rx_buf) {
        memcpy(slot.rx_buf, ok_redir, sizeof(ok_redir));
        slot.rx_len = sizeof(ok_redir) - 1;
      }
      if (step == 14) {
        slot.hop_count = RAW_MAX_REDIRECTS;
      } else if (step == 17) {
        slot.timer_id = 9999;
      }
    }
    return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                raw_sm_complete_response(&slot));
  }

  if (step >= 18 && step <= 30) {
    slot.tls_hooks = &hooks;
    slot.tls_sess = &tls_st;
    slot.hop_req.url = s_https_url;
    slot.hop_req.method = HTTP_GET;
    slot.tx_buf = (char *)malloc(8);
    if (slot.tx_buf) {
      memcpy(slot.tx_buf, "GET /", 6);
      slot.tx_len = 5;
    }
    if (step == 18) {
      slot.phase = RAW_SM_CONNECTING;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_sm_io_cb(loop, -1, HTTP_LOOP_ERROR, &slot));
    }
    if (step >= 19 && step <= 22) {
      slot.phase = RAW_SM_TLS_HANDSHAKE;
      tls_st.mode = (step == 19) ? 2 : (step == 20) ? 3 : (step == 21) ? 4 : 0;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_sm_io_cb(loop, -1, HTTP_LOOP_WRITE, &slot));
    }
    if (step >= 23 && step <= 26) {
      slot.phase = RAW_SM_WRITE_REQ;
      tls_st.mode = (step == 23) ? 5 : (step == 24) ? 6 : (step == 25) ? 7 : 8;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_sm_io_cb(loop, -1, HTTP_LOOP_WRITE, &slot));
    }
    slot.phase = RAW_SM_READ_RESP;
    tls_st.mode = (step == 27)   ? 9
                  : (step == 28) ? 10
                  : (step == 29) ? 11
                  : (step == 30) ? 13
                                 : 12;
    return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                raw_sm_io_cb(loop, -1, HTTP_LOOP_READ, &slot));
  }

  if ((step >= 41 && step <= 48) || (step >= 54 && step <= 57)) {
    struct HttpPart pt;
    memset(&req, 0, sizeof(req));
    memset(&pt, 0, sizeof(pt));
    req.url = (step <= 43 || step == 56) ? s_https_url : s_http_url;
    if (step == 57) {
      pt.name = s_pu;
      pt.data = s_body;
      pt.data_len = 5;
      req.parts.parts = &pt;
      req.parts.count = 1;
    }
    slot.orig_req = &req;
    slot.hop_req = req;
    if (step == 41 || step == 42) {
      rctx.has_custom_tls = 0;
      g_mock_raw_send_fail = (step == 41) ? 2 : 3;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_edge_reset_ret(&g_mock_raw_send_fail, raw_sm_start_hop(&slot)));
    }
    if (step == 43 || step == 56) {
      tls_st.mode = (step == 43) ? 1 : 0;
      return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                  raw_sm_start_hop(&slot));
    }
    if (step == 44) {
      rctx.config.proxy_url = s_bad_url;
      return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                  raw_sm_start_hop(&slot));
    }
    if (step == 45) {
      g_mock_raw_nonblocking_fail = 1;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_edge_reset_ret(&g_mock_raw_nonblocking_fail,
                             raw_sm_start_hop(&slot)));
    }
    if (step == 46 || step == 54) {
      g_mock_raw_connect_fail = (step == 46) ? 1 : 2;
      return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                  raw_edge_reset_ret(&g_mock_raw_connect_fail,
                                                     raw_sm_start_hop(&slot)));
    }
    if (step == 55) {
      return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                  raw_sm_start_hop(&slot));
    }
    if (step == 47) {
      slot.future = NULL;
      return raw_edge_finish_step(&slot, NULL, loop, -1, -1,
                                  raw_arm_multi_slot(&slot));
    }
    if (step == 57) {
      return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                  raw_arm_multi_slot(&slot));
    }
    slot.orig_req = NULL;
    return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                raw_arm_multi_slot(&slot));
  }

  if ((step >= 49 && step <= 52) || step == 58) {
#if !defined(_WIN32)
    int sv[2] = {-1, -1};
    char fill[8192];
    out_len = (size_t)socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    rc = make_socket_nonblocking(sv[0]);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return raw_edge_finish_step(&slot, &fut, loop, sv[0], sv[1], rc);
    }
    slot.sock = sv[0];
    slot.hop_req.url = s_http_url;
    slot.hop_req.method = HTTP_GET;
    slot.tx_buf = (char *)malloc(8);
    if (slot.tx_buf) {
      memcpy(slot.tx_buf, "GET /", 6);
      slot.tx_len = 5;
    }
    if (step == 49) {
      slot.phase = RAW_SM_WRITE_REQ;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, sv[1],
          raw_sm_io_cb(loop, sv[0], HTTP_LOOP_WRITE, &slot));
    }
    if (step == 58) {
      memset(fill, 'F', sizeof(fill));
      while (send(sv[0], fill, sizeof(fill), 0) > 0) {
      }
      slot.phase = RAW_SM_WRITE_REQ;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, sv[1],
          raw_sm_io_cb(loop, sv[0], HTTP_LOOP_WRITE, &slot));
    }
    if (step == 50) {
      close(sv[0]);
      close(sv[1]);
      slot.sock = -1;
      slot.phase = RAW_SM_WRITE_REQ;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_sm_io_cb(loop, -1, HTTP_LOOP_WRITE, &slot));
    }
    if (step == 51) {
      close(sv[0]);
      close(sv[1]);
      slot.sock = -1;
      slot.phase = RAW_SM_READ_RESP;
      return raw_edge_finish_step(
          &slot, &fut, loop, -1, -1,
          raw_sm_io_cb(loop, -1, HTTP_LOOP_READ, &slot));
    }
    close(sv[1]);
    slot.phase = RAW_SM_READ_RESP;
    return raw_edge_finish_step(
        &slot, &fut, loop, -1, -1,
        raw_sm_io_cb(loop, sv[0], HTTP_LOOP_READ, &slot));
#else
    return raw_edge_finish_step(&slot, &fut, loop, -1, -1,
                                C_ABSTRACT_HTTP_SUCCESS);
#endif
  }

  {
    int pipe_fds[2] = {-1, -1};
#if !defined(_WIN32)
    if (pipe(pipe_fds) == 0) {
      rc = http_loop_add_fd(loop, pipe_fds[0], HTTP_LOOP_WRITE, raw_sm_io_cb,
                            &slot);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return raw_edge_finish_step(&slot, &fut, loop, pipe_fds[0], pipe_fds[1],
                                    rc);
      }
      slot.sock = pipe_fds[0];
      slot.fd_registered = 1;
      pipe_fds[0] = -1;
    }
#endif
    slot.tls_hooks = &hooks;
    slot.tls_sess = &tls_st;
    slot.hop_req.url = s_https_url;
    slot.hop_req.method = HTTP_GET;
    slot.tx_buf = (char *)malloc(8);
    if (slot.tx_buf) {
      memcpy(slot.tx_buf, "GET /", 6);
      slot.tx_len = 5;
    }
    slot.phase = (step == 31) ? RAW_SM_TLS_HANDSHAKE : RAW_SM_READ_RESP;
    tls_st.mode = (step == 32) ? 12 : 0;
    return raw_edge_finish_step(
        &slot, &fut, loop, pipe_fds[0], pipe_fds[1],
        raw_sm_io_cb(loop, (int)slot.sock, HTTP_LOOP_WRITE, &slot));
  }
}
#endif

#endif /* !defined(CAH_DOS_NO_WATT32) */
#endif
