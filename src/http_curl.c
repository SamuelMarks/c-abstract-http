
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defines for C89 string functions if missing */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define strdup _strdup
#endif
#else
#endif

#include <curl/curl.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_curl.h>
#include "c_abstract_http/log.h"
#define ABSTRACT_HTTP_CURL_GLOBAL_INIT curl_global_init
#define ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE curl_multi_add_handle
#define ABSTRACT_HTTP_CURL_MULTI_REMOVE_HANDLE curl_multi_remove_handle
#define ABSTRACT_HTTP_CURL_EASY_PERFORM curl_easy_perform
#define ABSTRACT_HTTP_CURL_MULTI_SOCKET_ACTION curl_multi_socket_action
#define ABSTRACT_HTTP_MAP_CURL_ERROR map_curl_error
#define ABSTRACT_HTTP_HTTP_HEADERS_ADD http_headers_add
#define ABSTRACT_HTTP_HTTP_HEADERS_TO_STR http_headers_to_str
#include "str.h"
#define ABSTRACT_HTTP_HTTP_LOOP_REMOVE_FD http_loop_remove_fd
#define ABSTRACT_HTTP_HTTP_LOOP_ADD_FD http_loop_add_fd
#define ABSTRACT_HTTP_HTTP_LOOP_MOD_FD http_loop_mod_fd
#define ABSTRACT_HTTP_CURL_MULTI_CLEANUP curl_multi_cleanup
/* clang-format on */
#define ABSTRACT_HTTP_CURL_MULTI_INFO_READ curl_multi_info_read
#define ABSTRACT_HTTP_CURL_EASY_GETINFO curl_easy_getinfo
#define ABSTRACT_HTTP_CURL_EASY_SETOPT curl_easy_setopt
#define ABSTRACT_HTTP_CURL_MULTI_SETOPT curl_multi_setopt
#define ABSTRACT_HTTP_CURL_SLIST_APPEND curl_slist_append
#define ABSTRACT_HTTP_HTTP_HEADERS_INIT http_headers_init
#define ABSTRACT_HTTP_FORMAT_HEADER format_header
#define ABSTRACT_HTTP_SETUP_CURL_REQUEST setup_curl_request
#define ABSTRACT_HTTP_FINISH_CURL_REQUEST finish_curl_request
#define ABSTRACT_HTTP_CHECK_MULTI_INFO check_multi_info

#define ABSTRACT_HTTP_CURL_MULTI_ASSIGN curl_multi_assign
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
CURLcode g_mock_curl_perform_res = CURLE_OK;
int g_mock_curl_setopt_fail = 0;
int g_mock_curl_setopt_count = 0;
int g_mock_curl_init_fail = 0;

#undef ABSTRACT_HTTP_CURL_GLOBAL_INIT
#define ABSTRACT_HTTP_CURL_GLOBAL_INIT(flags)                                  \
  (g_mock_curl_init_fail == 3 ? CURLE_FAILED_INIT                              \
                              : (ABSTRACT_HTTP_CURL_GLOBAL_INIT)(flags))

#undef curl_easy_init
#define curl_easy_init()                                                       \
  (g_mock_curl_init_fail == 1 ? NULL : (curl_easy_init)())

#undef curl_multi_init
#define curl_multi_init()                                                      \
  (g_mock_curl_init_fail == 2 ? NULL : (curl_multi_init)())

#undef ABSTRACT_HTTP_CURL_EASY_PERFORM
#define ABSTRACT_HTTP_CURL_EASY_PERFORM(handle)                                \
  (g_mock_curl_perform_res != CURLE_OK                                         \
       ? g_mock_curl_perform_res                                               \
       : (ABSTRACT_HTTP_CURL_EASY_PERFORM)(handle))

#undef ABSTRACT_HTTP_CURL_EASY_SETOPT
#define ABSTRACT_HTTP_CURL_EASY_SETOPT(handle, option, param)                  \
  (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0                  \
       ? CURLE_OUT_OF_MEMORY                                                   \
       : (ABSTRACT_HTTP_CURL_EASY_SETOPT)(handle, option, param))

#undef ABSTRACT_HTTP_CURL_MULTI_SETOPT
#define ABSTRACT_HTTP_CURL_MULTI_SETOPT(handle, option, param)                 \
  (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0                  \
       ? CURLM_OUT_OF_MEMORY                                                   \
       : (ABSTRACT_HTTP_CURL_MULTI_SETOPT)(handle, option, param))

#undef ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE
#define ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE(multi, handle)                     \
  (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0                  \
       ? CURLM_OUT_OF_MEMORY                                                   \
       : (ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE)(multi, handle))

extern struct curl_slist *g_mock_curl_cookies;
#undef ABSTRACT_HTTP_CURL_EASY_GETINFO
#define ABSTRACT_HTTP_CURL_EASY_GETINFO(curl, info, param)                     \
  ((info) == CURLINFO_COOKIELIST && g_mock_curl_cookies                        \
       ? (*(struct curl_slist **)param = g_mock_curl_cookies, CURLE_OK)        \
       : (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0           \
              ? CURLE_OUT_OF_MEMORY                                            \
              : (ABSTRACT_HTTP_CURL_EASY_GETINFO)(curl, info, param)))

#undef curl_slist_free_all
#define curl_slist_free_all(list)                                              \
  do {                                                                         \
    if ((list) == g_mock_curl_cookies)                                         \
      g_mock_curl_cookies = NULL;                                              \
    (curl_slist_free_all)(list);                                               \
  } while (0)

#undef ABSTRACT_HTTP_CURL_SLIST_APPEND
#define ABSTRACT_HTTP_CURL_SLIST_APPEND(list, str)                             \
  (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0                  \
       ? NULL                                                                  \
       : (ABSTRACT_HTTP_CURL_SLIST_APPEND)(list, str))

#undef curl_easy_duphandle
#define curl_easy_duphandle(curl)                                              \
  (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0                  \
       ? NULL                                                                  \
       : (curl_easy_duphandle)(curl))

#endif

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief curl (variable) of struct HttpTransportContext */
  CURL *curl;
  /** @brief cookie_jar (variable) of struct HttpTransportContext */
  struct HttpCookieJar *cookie_jar;
  /** @brief multi (variable) of struct HttpTransportContext */
  CURLM *multi;
  /** @brief loop (variable) of struct HttpTransportContext */
  struct ModalityEventLoop *loop;
  /** @brief timer_id (variable) of struct HttpTransportContext */
  int timer_id;
};

/** @brief Internal struct CurlWriteContext */
struct CurlWriteContext {
  /** @brief MemoryStruct */
  struct MemoryStruct {
    /** @brief memory (variable) of struct CurlWriteContext::MemoryStruct */
    char *memory;
    /** @brief size (variable) of struct CurlWriteContext::MemoryStruct */
    size_t size;
  } chunk; /**< @brief chunk (variable) of struct CurlWriteContext */
  /** @brief req (variable) of struct CurlWriteContext */
  const struct HttpRequest *req;
  /** @brief user_aborted (variable) of struct CurlWriteContext */
  int user_aborted;
};

static size_t math_write_memory_callback(void *contents, size_t size,
                                         size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct CurlWriteContext *ctx = (struct CurlWriteContext *)userp;
  char *ptr;

  if (ctx->req && ctx->req->on_chunk) {
    int rc =
        ctx->req->on_chunk(ctx->req->on_chunk_user_data, contents, realsize);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      ctx->user_aborted = rc;
      return C_ABSTRACT_HTTP_SUCCESS; /* Returning less than realsize aborts the
                                         curl transfer */
    }
    return realsize;
  }

  /* Realloc to size + new_bytes + 1 (for null terminator) */
  ptr = (char *)realloc(ctx->chunk.memory, ctx->chunk.size + realsize + 1);
  if (!ptr) {
    /* Out of memory */
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  ctx->chunk.memory = ptr;
  memcpy(&(ctx->chunk.memory[ctx->chunk.size]), contents, realsize);
  ctx->chunk.size += realsize;
  ctx->chunk.memory[ctx->chunk.size] = 0; /* Null terminate for text safety */

  return realsize;
}

static size_t math_curl_read_callback(char *buffer, size_t size, size_t nitems,
                                      void *userdata) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  const struct HttpRequest *req = (const struct HttpRequest *)userdata;
  size_t max_bytes = size * nitems;
  size_t out_read = 0;

  rc = req->read_chunk(req->read_chunk_user_data, buffer, max_bytes, &out_read);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return CURL_READFUNC_ABORT;
  }

  return out_read;
}

static enum c_abstract_http_error ABSTRACT_HTTP_FORMAT_HEADER(const char *key,
                                                              const char *value,
                                                              char **_out_val) {
  size_t len = strlen(key) + 2 + strlen(value) + 1;
  char *buf = (char *)malloc(len);
  if (buf) {
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    sprintf_s(buf, len, "%s: %s", key, value);
#else
    sprintf(buf, "%s: %s", key, value);
#endif
  }
  {
    *_out_val = buf;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

static enum c_abstract_http_error ABSTRACT_HTTP_MAP_CURL_ERROR(CURLcode res) {
  switch (res) {
  case CURLE_UNSUPPORTED_PROTOCOL:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
    return EHOSTUNREACH;
  case CURLE_COULDNT_CONNECT:
    return ECONNREFUSED;
  case CURLE_OPERATION_TIMEDOUT:
    return C_ABSTRACT_HTTP_ERR_TIMEOUT;
  case CURLE_SSL_CONNECT_ERROR:
  case CURLE_PEER_FAILED_VERIFICATION:
    return EACCES;
  case CURLE_OUT_OF_MEMORY:
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  case CURLE_TOO_MANY_REDIRECTS:
    return ELOOP;
  case CURLE_SEND_ERROR:
  case CURLE_RECV_ERROR:
    return C_ABSTRACT_HTTP_ERR_IO;
  default:
    return C_ABSTRACT_HTTP_ERR_IO;
  }
}

static int g_curl_init_count = 0;
enum c_abstract_http_error http_ABSTRACT_HTTP_CURL_GLOBAL_INIT(void) {
  if (g_curl_init_count == 0) {
    if ((int)ABSTRACT_HTTP_CURL_GLOBAL_INIT(CURL_GLOBAL_ALL) != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  g_curl_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_curl_global_cleanup(void) {
  if (g_curl_init_count > 0) {
    g_curl_init_count--;
    if (g_curl_init_count == 0) {
      curl_global_cleanup();
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_curl_context_init(struct HttpTransportContext **const ctx) {
  LOG_DEBUG("http_curl_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_curl_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_curl_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  (*ctx)->curl = curl_easy_init();
  (*ctx)->multi = curl_multi_init();
  (*ctx)->loop = NULL;
  (*ctx)->timer_id = 0;
  (*ctx)->cookie_jar = NULL;

  if (!(*ctx)->curl || !(*ctx)->multi) {
    LOG_DEBUG("http_curl_context_init: Error curl init failed");
    if ((*ctx)->curl)
      curl_easy_cleanup((*ctx)->curl);
    if ((*ctx)->multi)
      ABSTRACT_HTTP_CURL_MULTI_CLEANUP((*ctx)->multi);
    free(*ctx);
    *ctx = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  LOG_DEBUG("http_curl_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_curl_context_free(struct HttpTransportContext *const ctx) {
  LOG_DEBUG("http_curl_context_free: Entering");
  if (ctx) {
    if (ctx->curl)
      curl_easy_cleanup(ctx->curl);
    if (ctx->multi)
      ABSTRACT_HTTP_CURL_MULTI_CLEANUP(ctx->multi);
    free(ctx);
  }
  LOG_DEBUG("http_curl_context_free: Exiting");
}

enum c_abstract_http_error
http_curl_config_apply(struct HttpTransportContext *ctx,
                       const struct HttpConfig *config) {
  long ssl_version_max = 0;
  LOG_DEBUG("http_curl_config_apply: Entering");
  if (!ctx || !ctx->curl || !config) {
    LOG_DEBUG("http_curl_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (config->version_mask & HTTP_VERSION_3) {
#if LIBCURL_VERSION_NUM >= 0x074200 /* 7.66.0 */
    if ((config->version_mask &
         (HTTP_VERSION_2 | HTTP_VERSION_1_1 | HTTP_VERSION_1_0)) ||
        config->http3_fallback) {
      if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                         CURL_HTTP_VERSION_3) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
    } else {
#if LIBCURL_VERSION_NUM >= 0x075000 /* 7.80.0 */
      if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                         CURL_HTTP_VERSION_3ONLY) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
#else
      if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                         CURL_HTTP_VERSION_3) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
#endif
    }
#else
    /* Fallback to default if libcurl is too old */
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       CURL_HTTP_VERSION_NONE) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  } else if (config->version_mask & HTTP_VERSION_2) {
#if LIBCURL_VERSION_NUM >= 0x072100 /* 7.33.0 */
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       CURL_HTTP_VERSION_2_0) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#else
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       CURL_HTTP_VERSION_NONE) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  } else if (config->version_mask & HTTP_VERSION_1_1) {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       CURL_HTTP_VERSION_1_1) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  } else if (config->version_mask & HTTP_VERSION_1_0) {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       CURL_HTTP_VERSION_1_0) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  } else {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       CURL_HTTP_VERSION_NONE) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (config->tls_version_mask != HTTP_TLS_VERSION_DEFAULT) {
    long ssl_version = CURL_SSLVERSION_DEFAULT;
#if LIBCURL_VERSION_NUM >= 0x073600
    ssl_version_max = CURL_SSLVERSION_MAX_DEFAULT;
#endif
    (void)ssl_version_max;

    if (config->tls_version_mask & HTTP_TLS_VERSION_1_0)
      ssl_version = CURL_SSLVERSION_TLSv1_0;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_1)
      ssl_version = CURL_SSLVERSION_TLSv1_1;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_2)
      ssl_version = CURL_SSLVERSION_TLSv1_2;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_3)
      ssl_version = CURL_SSLVERSION_TLSv1_3;

#if LIBCURL_VERSION_NUM >= 0x073600 /* 7.54.0 */
    ssl_version_max = CURL_SSLVERSION_MAX_DEFAULT;
    if (config->tls_version_mask & HTTP_TLS_VERSION_1_3)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_3;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_2)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_2;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_1)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_1;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_0)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_0;

    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_SSLVERSION,
                                       ssl_version | ssl_version_max) !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#else
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_SSLVERSION,
                                       ssl_version) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  }

  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_TIMEOUT_MS,
                                     config->timeout_ms) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_CONNECTTIMEOUT_MS,
                                     config->timeout_ms) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_SSL_VERIFYPEER,
                                     config->verify_peer ? 1L : 0L) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_SSL_VERIFYHOST,
                                     config->verify_host ? 2L : 0L) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_FOLLOWLOCATION,
                                     config->follow_redirects ? 1L : 0L) !=
      CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (config->user_agent) {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_USERAGENT,
                                       config->user_agent) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (config->proxy_url) {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_PROXY,
                                       config->proxy_url) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;

    if (config->proxy_username && config->proxy_password) {
      if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_PROXYUSERNAME,
                                         config->proxy_username) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
      if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_PROXYPASSWORD,
                                         config->proxy_password) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
    }
  } else {
    ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_PROXY, "");
  }

  if (config->cookie_jar) {
    ctx->cookie_jar = config->cookie_jar;
    /* Enable curl's cookie engine without reading a file */
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_COOKIEFILE, "") !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
    /* Instruct curl to write cookies to a dummy state (handled manually or via
     * curl's getinfo later) */
  } else {
    ctx->cookie_jar = NULL;
  }

  LOG_DEBUG("http_curl_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
ABSTRACT_HTTP_SETUP_CURL_REQUEST(CURL *curl, const struct HttpRequest *req,
                                 struct CurlWriteContext *write_ctx,
                                 struct curl_slist **out_headers) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  char *_ast_ABSTRACT_HTTP_FORMAT_HEADER_0;
  size_t i;
  void *payload = req->body;
  size_t payload_len = req->body_len;
  struct curl_slist *new_list;

  LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Entering");
  if (req->parts.count > 0 && !payload) {
    LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  write_ctx->chunk.memory = (char *)malloc(1);
  write_ctx->chunk.size = 0;
  if (!write_ctx->chunk.memory) {
    LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  write_ctx->chunk.memory[0] = '\0';
  write_ctx->req = req;
  write_ctx->user_aborted = 0;

  ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_URL, req->url);

  if (req->read_chunk) {
    switch (req->method) {
    case HTTP_PUT:
    case HTTP_POST:
    case HTTP_PATCH:
    case HTTP_QUERY:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_UPLOAD, 1L);
      if (req->method == HTTP_PUT) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      } else if (req->method == HTTP_POST) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "POST");
      } else if (req->method == HTTP_PATCH) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      } else if (req->method == HTTP_QUERY) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "QUERY");
      }
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_READFUNCTION,
                                     math_curl_read_callback);
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_READDATA, (void *)req);
      if (req->expected_body_len > 0) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_INFILESIZE_LARGE,
                                       (curl_off_t)req->expected_body_len);
      }
      break;
    default:
      break;
    }
  } else {
    switch (req->method) {
    case HTTP_GET:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_HTTPGET, 1L);
      break;
    case HTTP_POST:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POST, 1L);
      if (payload && payload_len > 0) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDS, payload);
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDSIZE,
                                       (long)payload_len);
      }
      break;
    case HTTP_PUT:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      if (payload && payload_len > 0) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDS, payload);
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDSIZE,
                                       (long)payload_len);
      }
      break;
    case HTTP_DELETE:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    case HTTP_HEAD:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_NOBODY, 1L);
      break;
    case HTTP_PATCH:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      if (payload && payload_len > 0) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDS, payload);
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDSIZE,
                                       (long)payload_len);
      }
      break;
    case HTTP_QUERY:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_CUSTOMREQUEST, "QUERY");
      if (payload && payload_len > 0) {
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDS, payload);
        ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_POSTFIELDSIZE,
                                       (long)payload_len);
      }
      break;
    default:
      ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_HTTPGET, 1L);
      break;
    }
  }

  for (i = 0; i < req->headers.count; ++i) {
    char *h_str = NULL;
    enum c_abstract_http_error rc_h = ABSTRACT_HTTP_FORMAT_HEADER(
        req->headers.headers[i].key, req->headers.headers[i].value, &h_str);
    if (rc_h != C_ABSTRACT_HTTP_SUCCESS || !h_str) {
      LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error ENOMEM in "
                "ABSTRACT_HTTP_FORMAT_HEADER");
      if (*out_headers) {
        curl_slist_free_all(*out_headers);
        *out_headers = NULL;
      }
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }
    new_list = ABSTRACT_HTTP_CURL_SLIST_APPEND(*out_headers, h_str);
    free(h_str);
    if (!new_list) {
      LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error ENOMEM in "
                "ABSTRACT_HTTP_CURL_SLIST_APPEND");
      if (*out_headers) {
        curl_slist_free_all(*out_headers);
        *out_headers = NULL;
      }
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }
    *out_headers = new_list;
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error %d", rc);
    return rc;
  }

  ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_HTTPHEADER, *out_headers);

  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_WRITEFUNCTION,
                                     math_write_memory_callback) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA,
                                     (void *)write_ctx) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error ABSTRACT_HTTP_FINISH_CURL_REQUEST(
    struct HttpTransportContext *ctx, CURL *curl, const struct HttpRequest *req,
    struct CurlWriteContext *write_ctx, struct curl_slist *headers,
    CURLcode res_code, struct HttpResponse **out_res) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  long response_code = 0;
  struct HttpResponse *new_res = NULL;

  LOG_DEBUG("ABSTRACT_HTTP_FINISH_CURL_REQUEST: Entering");

  if (res_code != CURLE_OK) {
    if (write_ctx->user_aborted != 0) {
      rc = write_ctx->user_aborted;
    } else {
      rc = ABSTRACT_HTTP_MAP_CURL_ERROR(res_code);
    }
    LOG_DEBUG(
        "ABSTRACT_HTTP_FINISH_CURL_REQUEST: Error res_code != CURLE_OK, rc=%d",
        rc);
    goto cleanup;
  }

  ABSTRACT_HTTP_CURL_EASY_GETINFO(curl, CURLINFO_RESPONSE_CODE, &response_code);

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    LOG_DEBUG("ABSTRACT_HTTP_FINISH_CURL_REQUEST: Error ENOMEM for new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  rc = http_response_init(new_res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "ABSTRACT_HTTP_FINISH_CURL_REQUEST: Error http_response_init failed");
    goto cleanup;
  }

  /* Sync cookies back to jar if provided */
  if (ctx->cookie_jar) {
    struct curl_slist *cookies = NULL;
    if (ABSTRACT_HTTP_CURL_EASY_GETINFO(curl, CURLINFO_COOKIELIST, &cookies) ==
            CURLE_OK &&
        cookies) {
      const struct curl_slist *each = cookies;
      while (each) {
        char domain[256], flag[16], path[256], secure[16], name[256],
            value[2048];
        long expiration;
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
        if (sscanf_s(each->data, "%255s\t%15s\t%255s\t%15s\t%ld\t%255s\t%2047s",
                     domain, (unsigned)sizeof(domain), flag,
                     (unsigned)sizeof(flag), path, (unsigned)sizeof(path),
                     secure, (unsigned)sizeof(secure), &expiration, name,
                     (unsigned)sizeof(name), value,
                     (unsigned)sizeof(value)) == 7) {
#else
        if (sscanf(
                each->data,
                "%255s\t%15s\t%255s\t%15s\t%ld\t%255s\t%2047s", /* LCOV_EXCL_LINE
                                                                 */
                domain, flag, path, secure, &expiration, name, value) == 7) {
#endif
          rc = http_cookie_jar_set(ctx->cookie_jar, name, value);
          if (rc != C_ABSTRACT_HTTP_SUCCESS) {
            LOG_DEBUG(
                "ABSTRACT_HTTP_FINISH_CURL_REQUEST: Error http_cookie_jar_set "
                "failed with %d",
                rc);
            curl_slist_free_all(cookies);
            goto cleanup;
          }
        }
        each = each->next;
      }
      curl_slist_free_all(cookies);
    }
  }

  new_res->status_code = (int)response_code;
  if (req->on_chunk) {
    free(write_ctx->chunk.memory);
    write_ctx->chunk.memory = NULL;
    write_ctx->chunk.size = 0;
  }
  new_res->body = write_ctx->chunk.memory;
  new_res->body_len = write_ctx->chunk.size;
  write_ctx->chunk.memory = NULL;

  *out_res = new_res;

cleanup:
  if (headers)
    curl_slist_free_all(headers);
  if (write_ctx->chunk.memory)
    free(write_ctx->chunk.memory);

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("ABSTRACT_HTTP_FINISH_CURL_REQUEST: Success");
  } else {
    LOG_DEBUG("ABSTRACT_HTTP_FINISH_CURL_REQUEST: Error returning %d", rc);
  }
  return rc;
}

enum c_abstract_http_error http_curl_send(struct HttpTransportContext *ctx,
                                          const struct HttpRequest *req,
                                          struct HttpResponse **const res) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  CURLcode res_code;
  struct curl_slist *headers = NULL;
  struct CurlWriteContext write_ctx;
  memset(&write_ctx, 0, sizeof(write_ctx));

  LOG_DEBUG("http_curl_send: Entering");
  if (!ctx || !ctx->curl || !req || !res) {
    LOG_DEBUG("http_curl_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = ABSTRACT_HTTP_SETUP_CURL_REQUEST(ctx->curl, req, &write_ctx, &headers);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG(
        "http_curl_send: Error ABSTRACT_HTTP_SETUP_CURL_REQUEST failed with %d",
        rc);
    if (write_ctx.chunk.memory)
      free(write_ctx.chunk.memory);
    if (headers)
      curl_slist_free_all(headers);
    return rc;
  }

  res_code = ABSTRACT_HTTP_CURL_EASY_PERFORM(ctx->curl);
  rc = ABSTRACT_HTTP_FINISH_CURL_REQUEST(ctx, ctx->curl, req, &write_ctx,
                                         headers, res_code, res);

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_curl_send: Error returning %d", rc);
    return rc;
  }
  LOG_DEBUG("http_curl_send: Success");
  return rc;
}
/** @brief Internal struct CurlMultiTask */
struct CurlMultiTask {
  /** @brief easy (variable) of struct CurlMultiTask */
  CURL *easy;
  /** @brief headers (variable) of struct CurlMultiTask */
  struct curl_slist *headers;
  /** @brief write_ctx (variable) of struct CurlMultiTask */
  struct CurlWriteContext write_ctx;
  /** @brief future (variable) of struct CurlMultiTask */
  struct HttpFuture *future;
  /** @brief ctx (variable) of struct CurlMultiTask */
  struct HttpTransportContext *ctx;
  /** @brief req (variable) of struct CurlMultiTask */
  const struct HttpRequest *req;
};

static void ABSTRACT_HTTP_CHECK_MULTI_INFO(struct HttpTransportContext *ctx) {
  CURLMsg *msg;
  int msgs_left;
  while ((msg = ABSTRACT_HTTP_CURL_MULTI_INFO_READ(ctx->multi, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      CURL *easy = msg->easy_handle;
      CURLcode res_code = msg->data.result;
      struct CurlMultiTask *task = NULL;

      ABSTRACT_HTTP_CURL_EASY_GETINFO(easy, CURLINFO_PRIVATE, &task);
      if (task) {
        struct HttpResponse *res = NULL;
        int rc = ABSTRACT_HTTP_FINISH_CURL_REQUEST(
            task->ctx, easy, task->req, &task->write_ctx, task->headers,
            res_code, &res);
        task->future->response = res;
        task->future->error_code = rc;
        task->future->is_ready = 1;
        free(task);
      }
      ABSTRACT_HTTP_CURL_MULTI_REMOVE_HANDLE(ctx->multi, easy);
      curl_easy_cleanup(easy);
    }
  }
}

static void multi_timer_cb(struct ModalityEventLoop *loop, int timer_id,
                           void *user_data) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)user_data;
  int running_handles;
  (void)loop;
  (void)timer_id;
  ABSTRACT_HTTP_CURL_MULTI_SOCKET_ACTION(ctx->multi, CURL_SOCKET_TIMEOUT, 0,
                                         &running_handles);
  ABSTRACT_HTTP_CHECK_MULTI_INFO(ctx);
}

static void multi_socket_cb(struct ModalityEventLoop *loop, int fd, int events,
                            void *user_data);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
void abstract_http_test_multi_socket_cb(struct ModalityEventLoop *loop, int fd,
                                        int events, void *user_data);
void abstract_http_test_multi_socket_cb(struct ModalityEventLoop *loop, int fd,
                                        int events, void *user_data) {
  multi_socket_cb(loop, fd, events, user_data);
}
#endif

static void multi_socket_cb(struct ModalityEventLoop *loop, int fd, int events,
                            void *user_data) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)user_data;
  int action = 0;
  int running_handles;
  (void)loop;
  if (events & HTTP_LOOP_READ)
    action |= CURL_CSELECT_IN;
  if (events & HTTP_LOOP_WRITE)
    action |= CURL_CSELECT_OUT;
  if (events & HTTP_LOOP_ERROR)
    action |= CURL_CSELECT_ERR;

  ABSTRACT_HTTP_CURL_MULTI_SOCKET_ACTION(ctx->multi, fd, action,
                                         &running_handles);
  ABSTRACT_HTTP_CHECK_MULTI_INFO(ctx);
}

static int multi_timer_function(CURLM *multi, long timeout_ms, void *userp) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)userp;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  (void)multi;

  if (ctx->timer_id > 0) {
    rc = http_loop_cancel_timer(ctx->loop, ctx->timer_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("multi_timer_function: http_loop_cancel_timer failed");
      goto timer_error;
    }
    ctx->timer_id = 0;
  }

  if (timeout_ms >= 0) {
    rc = http_loop_add_timer(ctx->loop, timeout_ms, multi_timer_cb, ctx,
                             &ctx->timer_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("multi_timer_function: http_loop_add_timer failed");
      goto timer_error;
    }
  }
  return 0; /* CURLM_OK */

timer_error:
  if (0)
    return (int)(unsigned long)rc;
  return -1;
}

static int multi_socket_function(CURL *easy, curl_socket_t s, int what,
                                 void *userp, void *socketp) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)userp;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;

  (void)easy;

  if (what == CURL_POLL_REMOVE) {
    rc = ABSTRACT_HTTP_HTTP_LOOP_REMOVE_FD(ctx->loop, (int)s);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG(
          "multi_socket_function: ABSTRACT_HTTP_HTTP_LOOP_REMOVE_FD failed");
      goto socket_error;
    }
    ABSTRACT_HTTP_CURL_MULTI_ASSIGN(ctx->multi, s, NULL);
  } else {
    int events = 0;
    if (what == CURL_POLL_IN || what == CURL_POLL_INOUT)
      events |= HTTP_LOOP_READ;
    if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT)
      events |= HTTP_LOOP_WRITE;

    if (!socketp) {
      rc = ABSTRACT_HTTP_HTTP_LOOP_ADD_FD(ctx->loop, (int)s, events,
                                          multi_socket_cb, ctx);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG(
            "multi_socket_function: ABSTRACT_HTTP_HTTP_LOOP_ADD_FD failed");
        goto socket_error;
      }
      ABSTRACT_HTTP_CURL_MULTI_ASSIGN(ctx->multi, s, (void *)1);
    } else {
      rc = ABSTRACT_HTTP_HTTP_LOOP_MOD_FD(ctx->loop, (int)s, events);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG(
            "multi_socket_function: ABSTRACT_HTTP_HTTP_LOOP_MOD_FD failed");
        goto socket_error;
      }
    }
  }
  return 0; /* CURLM_OK */

socket_error:
  if (0)
    return (int)(unsigned long)rc;
  return -1;
}

enum c_abstract_http_error http_curl_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  int rc;
  size_t i;

  LOG_DEBUG("http_curl_send_multi: Entering");
  if (!ctx || !ctx->multi || !loop || !multi || !futures) {
    LOG_DEBUG("http_curl_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->loop = loop;

  if (ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_SOCKETFUNCTION,
                                      multi_socket_function) != CURLM_OK ||
      ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_SOCKETDATA, ctx) !=
          CURLM_OK ||
      ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_TIMERFUNCTION,
                                      multi_timer_function) != CURLM_OK ||
      ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_TIMERDATA, ctx) !=
          CURLM_OK) {
    LOG_DEBUG(
        "http_curl_send_multi: Error ABSTRACT_HTTP_CURL_MULTI_SETOPT failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  for (i = 0; i < multi->count; ++i) {
    struct CurlMultiTask *task;

    task = (struct CurlMultiTask *)calloc(1, sizeof(struct CurlMultiTask));
    if (!task) {
      LOG_DEBUG("http_curl_send_multi: Error ENOMEM (task)");
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }

    task->easy = curl_easy_duphandle(ctx->curl);
    if (!task->easy) {
      LOG_DEBUG("http_curl_send_multi: Error ENOMEM (easy_duphandle)");
      free(task);
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }

    task->ctx = ctx;
    task->req = multi->requests[i];
    task->future = futures[i];
    futures[i]->internal_state = task;

    rc = ABSTRACT_HTTP_SETUP_CURL_REQUEST(task->easy, task->req,
                                          &task->write_ctx, &task->headers);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("http_curl_send_multi: Error ABSTRACT_HTTP_SETUP_CURL_REQUEST "
                "failed %d",
                rc);
      if (task->headers)
        curl_slist_free_all(task->headers);
      if (task->write_ctx.chunk.memory)
        free(task->write_ctx.chunk.memory);
      curl_easy_cleanup(task->easy);
      free(task);
      futures[i]->internal_state = NULL;
      break;
    }

    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(task->easy, CURLOPT_PRIVATE, task) !=
        CURLE_OK) {
      if (task->headers)
        curl_slist_free_all(task->headers);
      if (task->write_ctx.chunk.memory)
        free(task->write_ctx.chunk.memory);
      curl_easy_cleanup(task->easy);
      free(task);
      futures[i]->internal_state = NULL;
      rc = C_ABSTRACT_HTTP_ERR_IO;
      break;
    }
    if (ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE(ctx->multi, task->easy) !=
        CURLM_OK) {

      LOG_DEBUG("http_curl_send_multi: Error "
                "ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE failed");
      if (task->headers)
        curl_slist_free_all(task->headers);
      if (task->write_ctx.chunk.memory)
        free(task->write_ctx.chunk.memory);
      curl_easy_cleanup(task->easy);
      free(task);
      futures[i]->internal_state = NULL;
      rc = C_ABSTRACT_HTTP_ERR_IO;
      break;
    }
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    /* Cleanup any tasks successfully added before the failure */
    size_t j;
    for (j = 0; j < i; ++j) {
      struct CurlMultiTask *task =
          (struct CurlMultiTask *)futures[j]->internal_state;
      if (task) {
        if (task->headers)
          curl_slist_free_all(task->headers);
        if (task->write_ctx.chunk.memory)
          free(task->write_ctx.chunk.memory);
        ABSTRACT_HTTP_CURL_MULTI_REMOVE_HANDLE(ctx->multi, task->easy);
        curl_easy_cleanup(task->easy);
        free(task);
      }
    }
    return rc;
  }

  LOG_DEBUG("http_curl_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
