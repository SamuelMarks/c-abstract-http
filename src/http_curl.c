
/**
 * @file http_curl.c
 * @brief libcurl transport backend implementation of the Abstract Network
 * Interface (ANI).
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Defines for C89 string functions if missing */
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
/** @brief Alias POSIX strdup to MSVC _strdup */
#define strdup _strdup
#endif
#else
#endif

#include <curl/curl.h>

#include <c_abstract_http/event_loop.h>
#include <c_abstract_http/http_curl.h>
#include "c_abstract_http/log.h"
/* clang-format on */

/** @brief Wrapper macro for curl_global_init */
#define ABSTRACT_HTTP_CURL_GLOBAL_INIT curl_global_init
/** @brief Wrapper macro for curl_multi_add_handle */
#define ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE curl_multi_add_handle
/** @brief Wrapper macro for curl_multi_remove_handle */
#define ABSTRACT_HTTP_CURL_MULTI_REMOVE_HANDLE curl_multi_remove_handle
/** @brief Wrapper macro for curl_easy_perform */
#define ABSTRACT_HTTP_CURL_EASY_PERFORM curl_easy_perform
/** @brief Wrapper macro for curl_multi_socket_action */
#define ABSTRACT_HTTP_CURL_MULTI_SOCKET_ACTION curl_multi_socket_action
/** @brief Wrapper macro for map_curl_error */
#define ABSTRACT_HTTP_MAP_CURL_ERROR map_curl_error
/** @brief Wrapper macro for http_loop_remove_fd */
#define ABSTRACT_HTTP_HTTP_LOOP_REMOVE_FD http_loop_remove_fd
/** @brief Wrapper macro for http_loop_add_fd */
#define ABSTRACT_HTTP_HTTP_LOOP_ADD_FD http_loop_add_fd
/** @brief Wrapper macro for http_loop_mod_fd */
#define ABSTRACT_HTTP_HTTP_LOOP_MOD_FD http_loop_mod_fd
/** @brief Wrapper macro for curl_multi_cleanup */
#define ABSTRACT_HTTP_CURL_MULTI_CLEANUP curl_multi_cleanup
/** @brief Wrapper macro for curl_multi_info_read */
#define ABSTRACT_HTTP_CURL_MULTI_INFO_READ curl_multi_info_read
/** @brief Wrapper macro for curl_easy_getinfo */
#define ABSTRACT_HTTP_CURL_EASY_GETINFO curl_easy_getinfo
/** @brief Wrapper macro for curl_easy_setopt */
#define ABSTRACT_HTTP_CURL_EASY_SETOPT curl_easy_setopt
/** @brief Wrapper macro for curl_multi_setopt */
#define ABSTRACT_HTTP_CURL_MULTI_SETOPT curl_multi_setopt
/** @brief Wrapper macro for curl_slist_append */
#define ABSTRACT_HTTP_CURL_SLIST_APPEND curl_slist_append
/** @brief Wrapper macro for format_header */
#define ABSTRACT_HTTP_FORMAT_HEADER format_header
/** @brief Wrapper macro for setup_curl_request */
#define ABSTRACT_HTTP_SETUP_CURL_REQUEST setup_curl_request
/** @brief Wrapper macro for finish_curl_request */
#define ABSTRACT_HTTP_FINISH_CURL_REQUEST finish_curl_request
/** @brief Wrapper macro for check_multi_info */
#define ABSTRACT_HTTP_CHECK_MULTI_INFO check_multi_info
/** @brief Wrapper macro for curl_multi_assign */
#define ABSTRACT_HTTP_CURL_MULTI_ASSIGN curl_multi_assign
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Mock result override for curl_easy_perform */
CURLcode g_mock_curl_perform_res = CURLE_OK;
/** @brief Fault-injection enable flag for curl setopt calls */
int g_mock_curl_setopt_fail = 0;
/** @brief Countdown until simulated curl setopt failure triggers */
int g_mock_curl_setopt_count = 0;
/** @brief Fault-injection mode for curl global/easy/multi init calls */
int g_mock_curl_init_fail = 0;
/** @brief Fault-injection flag for http_response_init inside http_curl */
int g_mock_http_response_init_fail = 0;
/** @brief Fault-injection flag for event loop operations inside http_curl */
int g_mock_http_loop_fail = 0;
/** @brief Fault-injection flag for check_multi_info */
int g_mock_curl_check_multi_info_fail = 0;

#undef http_response_init
/**
 * @brief Fault-injectable http_response_init wrapper.
 * @param res Response struct pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
#define http_response_init(res)                                                \
  (g_mock_http_response_init_fail ? C_ABSTRACT_HTTP_ERR_INVAL                  \
                                  : (http_response_init)(res))

#undef http_loop_cancel_timer
/**
 * @brief Fault-injectable http_loop_cancel_timer wrapper.
 * @param loop Event loop pointer.
 * @param id Timer identifier.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
#define http_loop_cancel_timer(loop, id)                                       \
  (g_mock_http_loop_fail ? C_ABSTRACT_HTTP_ERR_NOMEM                           \
                         : (http_loop_cancel_timer)(loop, id))

#undef http_loop_add_timer
/**
 * @brief Fault-injectable http_loop_add_timer wrapper.
 * @param loop Event loop pointer.
 * @param ms Timeout in milliseconds.
 * @param cb Timer callback.
 * @param u User data pointer.
 * @param id Output timer identifier pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
#define http_loop_add_timer(loop, ms, cb, u, id)                               \
  (g_mock_http_loop_fail ? C_ABSTRACT_HTTP_ERR_NOMEM                           \
                         : (http_loop_add_timer)(loop, ms, cb, u, id))

#undef ABSTRACT_HTTP_HTTP_LOOP_REMOVE_FD
/**
 * @brief Fault-injectable http_loop_remove_fd wrapper.
 * @param loop Event loop pointer.
 * @param fd Socket descriptor.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
#define ABSTRACT_HTTP_HTTP_LOOP_REMOVE_FD(loop, fd)                            \
  (g_mock_http_loop_fail ? C_ABSTRACT_HTTP_ERR_NOMEM                           \
                         : (http_loop_remove_fd)(loop, fd))

#undef ABSTRACT_HTTP_HTTP_LOOP_ADD_FD
/**
 * @brief Fault-injectable http_loop_add_fd wrapper.
 * @param loop Event loop pointer.
 * @param fd Socket descriptor.
 * @param ev Event bitmask.
 * @param cb Event callback.
 * @param u User data pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
#define ABSTRACT_HTTP_HTTP_LOOP_ADD_FD(loop, fd, ev, cb, u)                    \
  (g_mock_http_loop_fail ? C_ABSTRACT_HTTP_ERR_NOMEM                           \
                         : (http_loop_add_fd)(loop, fd, ev, cb, u))

#undef ABSTRACT_HTTP_HTTP_LOOP_MOD_FD
/**
 * @brief Fault-injectable http_loop_mod_fd wrapper.
 * @param loop Event loop pointer.
 * @param fd Socket descriptor.
 * @param ev Updated event bitmask.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error.
 */
#define ABSTRACT_HTTP_HTTP_LOOP_MOD_FD(loop, fd, ev)                           \
  (g_mock_http_loop_fail ? C_ABSTRACT_HTTP_ERR_NOMEM                           \
                         : (http_loop_mod_fd)(loop, fd, ev))

#undef ABSTRACT_HTTP_CURL_GLOBAL_INIT
/**
 * @brief Fault-injectable curl_global_init wrapper.
 * @param flags Global init bitmask.
 * @return CURLE_OK or simulated failure.
 */
#define ABSTRACT_HTTP_CURL_GLOBAL_INIT(flags)                                  \
  (g_mock_curl_init_fail == 3 ? CURLE_FAILED_INIT : (curl_global_init)(flags))

#undef curl_easy_init
/**
 * @brief Fault-injectable curl_easy_init wrapper.
 * @return CURL handle or NULL on simulated failure.
 */
#define curl_easy_init()                                                       \
  (g_mock_curl_init_fail == 1 ? NULL : (curl_easy_init)())

#undef curl_multi_init
/**
 * @brief Fault-injectable curl_multi_init wrapper.
 * @return CURLM handle or NULL on simulated failure.
 */
#define curl_multi_init()                                                      \
  (g_mock_curl_init_fail == 2 ? NULL : (curl_multi_init)())

#undef ABSTRACT_HTTP_CURL_EASY_PERFORM
/**
 * @brief Fault-injectable curl_easy_perform wrapper.
 * @param handle Easy handle.
 * @return CURLcode result.
 */
#define ABSTRACT_HTTP_CURL_EASY_PERFORM(handle)                                \
  (g_mock_curl_perform_res != CURLE_OK ? g_mock_curl_perform_res               \
                                       : (curl_easy_perform)(handle))

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Check if mock setopt should fail on this invocation.
 * @return C_ABSTRACT_HTTP_ERR_IO if failure is triggered,
 * C_ABSTRACT_HTTP_SUCCESS otherwise.
 */
NO_DISCARD static c_abstract_http_error_t check_mock_setopt_fail(void) {
  if (g_mock_curl_setopt_fail) {
    if (g_mock_curl_setopt_count == 0) {
      g_mock_curl_setopt_count--;
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    g_mock_curl_setopt_count--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#undef ABSTRACT_HTTP_CURL_EASY_SETOPT
/**
 * @brief Fault-injectable curl_easy_setopt wrapper.
 * @param handle Easy handle.
 * @param option CURLoption selector.
 * @param param Option argument.
 * @return CURLcode result.
 */
#define ABSTRACT_HTTP_CURL_EASY_SETOPT(handle, option, param)                  \
  (check_mock_setopt_fail() != C_ABSTRACT_HTTP_SUCCESS                         \
       ? CURLE_OUT_OF_MEMORY                                                   \
       : curl_easy_setopt(handle, option, param))

#undef ABSTRACT_HTTP_CURL_MULTI_SETOPT
/**
 * @brief Fault-injectable curl_multi_setopt wrapper.
 * @param handle Multi handle.
 * @param option CURLMoption selector.
 * @param param Option argument.
 * @return CURLMcode result.
 */
#define ABSTRACT_HTTP_CURL_MULTI_SETOPT(handle, option, param)                 \
  (check_mock_setopt_fail() != C_ABSTRACT_HTTP_SUCCESS                         \
       ? CURLM_OUT_OF_MEMORY                                                   \
       : curl_multi_setopt(handle, option, param))
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection mode for curl_multi_add_handle */
int g_mock_curl_multi_add_fail = 0;
/**
 * @brief Mock wrapper for curl_multi_add_handle to simulate failures.
 * @param multi Curl multi handle.
 * @param handle Curl easy handle.
 * @return CURLMcode result.
 */
static CURLMcode mock_curl_multi_add_handle(CURLM *multi, CURL *handle) {
  if (g_mock_curl_multi_add_fail == 1)
    return CURLM_BAD_HANDLE;
  if (g_mock_curl_multi_add_fail == 2) {
    static int s_multi_add_count = 0;
    if (++s_multi_add_count >= 2) {
      s_multi_add_count = 0;
      return CURLM_BAD_HANDLE;
    }
  }
  if (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0)
    return CURLM_OUT_OF_MEMORY;
  return curl_multi_add_handle(multi, handle);
}
#undef ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE
/**
 * @brief Redirect curl_multi_add_handle to mock wrapper.
 * @param multi Curl multi handle.
 * @param handle Curl easy handle.
 * @return CURLMcode result.
 */
#define ABSTRACT_HTTP_CURL_MULTI_ADD_HANDLE(multi, handle)                     \
  mock_curl_multi_add_handle(multi, handle)
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Mock cookie list injected into CURLINFO_COOKIELIST queries */
extern struct curl_slist *g_mock_curl_cookies;

/**
 * @brief Mock wrapper for curl_easy_getinfo intercepting CURLINFO_COOKIELIST.
 * @param curl Easy handle.
 * @param info Info selector.
 * @param param Output pointer receiving info value.
 * @return CURLcode result.
 */
static CURLcode mock_curl_easy_getinfo(CURL *curl, CURLINFO info, void *param) {
  if (info == CURLINFO_COOKIELIST) {
    if (g_mock_curl_cookies) {
      struct curl_slist *copy = NULL;
      struct curl_slist *each = g_mock_curl_cookies;
      while (each) {
        copy = curl_slist_append(copy, each->data);
        each = each->next;
      }
      memcpy(param, &copy, sizeof(struct curl_slist *));
      return CURLE_OK;
    }
    *(struct curl_slist **)param = NULL;
    return CURLE_OK;
  }
  return curl_easy_getinfo(curl, info, param);
}

#undef ABSTRACT_HTTP_CURL_EASY_GETINFO
/** @brief Redirect ABSTRACT_HTTP_CURL_EASY_GETINFO to mock wrapper */
#define ABSTRACT_HTTP_CURL_EASY_GETINFO mock_curl_easy_getinfo
#else
#undef ABSTRACT_HTTP_CURL_EASY_GETINFO
/**
 * @brief Invoke curl_easy_getinfo directly.
 * @param curl Easy handle.
 * @param info Info selector.
 * @param param Output pointer.
 * @return CURLcode result.
 */
#define ABSTRACT_HTTP_CURL_EASY_GETINFO(curl, info, param)                     \
  curl_easy_getinfo(curl, info, param)
#endif

#undef ABSTRACT_HTTP_CURL_SLIST_APPEND
/**
 * @brief Fault-injectable curl_slist_append wrapper.
 * @param list Existing slist head.
 * @param str Header string to append.
 * @return Updated slist head or NULL on failure.
 */
#define ABSTRACT_HTTP_CURL_SLIST_APPEND(list, str)                             \
  (g_mock_curl_setopt_fail && g_mock_curl_setopt_count-- == 0                  \
       ? NULL                                                                  \
       : (curl_slist_append)(list, str))

#undef curl_easy_duphandle
/**
 * @brief Fault-injectable curl_easy_duphandle wrapper.
 * @param curl Source easy handle.
 * @return Duplicated easy handle or NULL on failure.
 */
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
  c_abstract_http_error_t user_aborted;
};

/**
 * @brief Append or forward received libcurl response body bytes.
 * @param contents Incoming payload bytes.
 * @param realsize Number of bytes in contents.
 * @param ctx Active write context.
 * @param out_written Output pointer receiving bytes processed.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on OOM/callback abort.
 */
NO_DISCARD static c_abstract_http_error_t
write_memory_callback_impl(void *contents, size_t realsize,
                           struct CurlWriteContext *ctx, size_t *out_written) {
  char *ptr;

  if (ctx->req->on_chunk) {
    c_abstract_http_error_t rc =
        ctx->req->on_chunk(ctx->req->on_chunk_user_data, contents, realsize);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      ctx->user_aborted = rc;
      *out_written = 0;
      return rc;
    }
    *out_written = realsize;
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  /* Realloc to size + new_bytes + 1 (for null terminator) */
  ptr = (char *)realloc(ctx->chunk.memory, ctx->chunk.size + realsize + 1);
  if (!ptr) {
    *out_written = 0;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  ctx->chunk.memory = ptr;
  memcpy(&(ctx->chunk.memory[ctx->chunk.size]), contents, realsize);
  ctx->chunk.size += realsize;
  ctx->chunk.memory[ctx->chunk.size] = 0; /* Null terminate for text safety */

  *out_written = realsize;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief libcurl CURLOPT_WRITEFUNCTION callback trampoline.
 * @param contents Incoming payload bytes.
 * @param size Element size in bytes.
 * @param nmemb Element count.
 * @param userp Pointer to struct CurlWriteContext.
 * @return Number of bytes consumed (0 to signal abort).
 */
static size_t math_write_memory_callback(void *contents, size_t size,
                                         size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  size_t written = 0;
  struct CurlWriteContext *ctx = (struct CurlWriteContext *)userp;
  c_abstract_http_error_t rc =
      write_memory_callback_impl(contents, realsize, ctx, &written);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return (size_t)((int)rc * 0);
  }
  return written;
}

/**
 * @brief Read upload body bytes from req->read_chunk.
 * @param buffer Destination libcurl upload buffer.
 * @param max_bytes Capacity of buffer in bytes.
 * @param req Active HTTP request.
 * @param out_read Output pointer receiving bytes read.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on callback failure.
 */
NO_DISCARD static c_abstract_http_error_t
curl_read_callback_impl(char *buffer, size_t max_bytes,
                        const struct HttpRequest *req, size_t *out_read) {
  c_abstract_http_error_t rc =
      req->read_chunk(req->read_chunk_user_data, buffer, max_bytes, out_read);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief libcurl CURLOPT_READFUNCTION callback trampoline.
 * @param buffer Destination upload buffer.
 * @param size Element size in bytes.
 * @param nitems Element count.
 * @param userdata Pointer to const struct HttpRequest.
 * @return Bytes read or CURL_READFUNC_ABORT on error.
 */
static size_t math_curl_read_callback(char *buffer, size_t size, size_t nitems,
                                      void *userdata) {
  const struct HttpRequest *req = (const struct HttpRequest *)userdata;
  size_t max_bytes = size * nitems;
  size_t out_read = 0;
  size_t abort_val = CURL_READFUNC_ABORT;
  c_abstract_http_error_t rc =
      curl_read_callback_impl(buffer, max_bytes, req, &out_read);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return abort_val + (size_t)((int)rc * 0);
  }

  return out_read;
}

/**
 * @brief Format a "Key: Value" HTTP header line into a newly allocated string.
 * @param key Header name.
 * @param value Header value.
 * @param _out_val Output pointer receiving allocated header string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
ABSTRACT_HTTP_FORMAT_HEADER(const char *key, const char *value,
                            char **_out_val) {
  size_t len = strlen(key) + 2 + strlen(value) + 1;
  char *buf = (char *)malloc(len);
  if (!buf) {
    *_out_val = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  /* MSVC Safe Path */
  sprintf_s(buf, len, "%s: %s", key, value);
#else
  /* Standard POSIX / GCC Path */
  sprintf(buf, "%s: %s", key, value);
#endif
  *_out_val = buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Map a libcurl CURLcode to c_abstract_http_error_t.
 * @param res libcurl error code.
 * @return Mapped abstract HTTP error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ABSTRACT_HTTP_MAP_CURL_ERROR(CURLcode res) {
  switch (res) {
  case CURLE_UNSUPPORTED_PROTOCOL:
    return C_ABSTRACT_HTTP_ERR_INVAL;
  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
    return C_ABSTRACT_HTTP_ERR_HOSTUNREACH;
  case CURLE_COULDNT_CONNECT:
    return C_ABSTRACT_HTTP_ERR_CONNREFUSED;
  case CURLE_OPERATION_TIMEDOUT:
    return C_ABSTRACT_HTTP_ERR_TIMEOUT;
  case CURLE_SSL_CONNECT_ERROR:
  case CURLE_PEER_FAILED_VERIFICATION:
    return C_ABSTRACT_HTTP_ERR_ACCES;
  case CURLE_OUT_OF_MEMORY:
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  case CURLE_TOO_MANY_REDIRECTS:
    return C_ABSTRACT_HTTP_ERR_LOOP;
  case CURLE_SEND_ERROR:
  case CURLE_RECV_ERROR:
    return C_ABSTRACT_HTTP_ERR_IO;
  default:
    return C_ABSTRACT_HTTP_ERR_IO;
  }
}

/** @brief Reference count of global libcurl initializations */
static int g_curl_init_count = 0;

/* Initialize global libcurl state */
NO_DISCARD c_abstract_http_error_t http_curl_global_init(void) {
  if (g_curl_init_count == 0) {
    CURLcode init_rc = ABSTRACT_HTTP_CURL_GLOBAL_INIT(CURL_GLOBAL_ALL);
    if (init_rc != CURLE_OK) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  g_curl_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Clean up global libcurl state */
NO_DISCARD c_abstract_http_error_t http_curl_global_cleanup(void) {
  if (g_curl_init_count > 0) {
    g_curl_init_count--;
    if (g_curl_init_count == 0) {
      curl_global_cleanup();
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize a libcurl transport context */
NO_DISCARD c_abstract_http_error_t
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

/** @brief Free a libcurl transport context */
NO_DISCARD c_abstract_http_error_t
http_curl_context_free(struct HttpTransportContext *const ctx) {
  LOG_DEBUG("http_curl_context_free: Entering");
  if (ctx) {
    curl_easy_cleanup(ctx->curl);
    ABSTRACT_HTTP_CURL_MULTI_CLEANUP(ctx->multi);
    free(ctx);
  }
  LOG_DEBUG("http_curl_context_free: Exiting");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Apply HTTP configuration to a libcurl transport context */
NO_DISCARD c_abstract_http_error_t http_curl_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  long ssl_version_max = 0;
#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
  long http_version = 0;
#endif
  LOG_DEBUG("http_curl_config_apply: Entering");
  if (!ctx || !ctx->curl || !config) {
    LOG_DEBUG("http_curl_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (config->version_mask & HTTP_VERSION_3) {
#if LIBCURL_VERSION_NUM >= 0x074200 /* 7.66.0 */
#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
    http_version = CURL_HTTP_VERSION_3;
#if LIBCURL_VERSION_NUM >= 0x075000 /* 7.80.0 */
    if (!((config->version_mask &
           (HTTP_VERSION_2 | HTTP_VERSION_1_1 | HTTP_VERSION_1_0)) ||
          config->http3_fallback)) {
      http_version = CURL_HTTP_VERSION_3ONLY;
    }
#endif
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    {
      c_abstract_http_error_t mock_rc = check_mock_setopt_fail();
      if (mock_rc != C_ABSTRACT_HTTP_SUCCESS)
        return mock_rc;
    }
#else
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       http_version) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
#else
    /* Fallback to default if libcurl is too old */
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       (long)CURL_HTTP_VERSION_NONE) !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  } else if (config->version_mask & HTTP_VERSION_2) {
#if LIBCURL_VERSION_NUM >= 0x072100 /* 7.33.0 */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    {
      c_abstract_http_error_t mock_rc = check_mock_setopt_fail();
      if (mock_rc != C_ABSTRACT_HTTP_SUCCESS)
        return mock_rc;
    }
#else
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       (long)CURL_HTTP_VERSION_2_0) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
#else
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       (long)CURL_HTTP_VERSION_NONE) !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  } else if (config->version_mask & HTTP_VERSION_1_1) {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       (long)CURL_HTTP_VERSION_1_1) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  } else if (config->version_mask & HTTP_VERSION_1_0) {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       (long)CURL_HTTP_VERSION_1_0) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  } else {
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_HTTP_VERSION,
                                       (long)CURL_HTTP_VERSION_NONE) !=
        CURLE_OK)
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
    else
      ssl_version = CURL_SSLVERSION_TLSv1_3;

#if LIBCURL_VERSION_NUM >= 0x073600 /* 7.54.0 */
    ssl_version_max = CURL_SSLVERSION_MAX_DEFAULT;
    if (config->tls_version_mask & HTTP_TLS_VERSION_1_3)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_3;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_2)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_2;
    else if (config->tls_version_mask & HTTP_TLS_VERSION_1_1)
      ssl_version_max = CURL_SSLVERSION_MAX_TLSv1_1;
    else
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
                                     config->connect_timeout_ms > 0
                                         ? config->connect_timeout_ms
                                         : config->timeout_ms) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (config->read_timeout_ms > 0 || config->write_timeout_ms > 0) {
    long rw_ms = config->read_timeout_ms > 0 ? config->read_timeout_ms
                                             : config->write_timeout_ms;
    long low_speed_sec = (rw_ms + 999L) / 1000L;
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_LOW_SPEED_LIMIT,
                                       1L) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_LOW_SPEED_TIME,
                                       low_speed_sec) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  }

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

    if (config->proxy_username) {
      if (config->proxy_password) {
        if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_PROXYUSERNAME,
                                           config->proxy_username) != CURLE_OK)
          return C_ABSTRACT_HTTP_ERR_IO;
        if (ABSTRACT_HTTP_CURL_EASY_SETOPT(ctx->curl, CURLOPT_PROXYPASSWORD,
                                           config->proxy_password) != CURLE_OK)
          return C_ABSTRACT_HTTP_ERR_IO;
      }
    }
  } else {
    curl_easy_setopt(ctx->curl, CURLOPT_PROXY, "");
  }

  if (config->cookie_jar) {
    ctx->cookie_jar = config->cookie_jar;
    /* Enable curl's cookie engine without reading a file */
    curl_easy_setopt(ctx->curl, CURLOPT_COOKIEFILE, "");
    /* Instruct curl to write cookies to a dummy state (handled manually or via
     * curl's getinfo later) */
  } else {
    ctx->cookie_jar = NULL;
  }

  LOG_DEBUG("http_curl_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Configure an easy handle with URL, method, headers, body, and
 * callbacks.
 * @param curl Easy handle to configure.
 * @param req Source HTTP request.
 * @param write_ctx Write callback context to initialize.
 * @param out_headers Output pointer receiving built curl_slist header list.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum on OOM/setopt failure.
 */
NO_DISCARD static c_abstract_http_error_t
ABSTRACT_HTTP_SETUP_CURL_REQUEST(CURL *curl, const struct HttpRequest *req,
                                 struct CurlWriteContext *write_ctx,
                                 struct curl_slist **out_headers) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  size_t i;
  void *payload = req->body;
  size_t payload_len = req->body_len;
  struct curl_slist *new_list;

  LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Entering");
  if (req->parts.count > 0 && (!payload || !req->parts.parts)) {
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
  write_ctx->user_aborted = C_ABSTRACT_HTTP_SUCCESS;

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
  curl_easy_setopt(curl, CURLOPT_READDATA, NULL);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)-1);

  curl_easy_setopt(curl, CURLOPT_URL, req->url);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  if (req->read_chunk) {
    switch (req->method) {
    case HTTP_PUT:
    case HTTP_POST:
    case HTTP_PATCH:
    case HTTP_QUERY:
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
      if (req->method == HTTP_PUT) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      } else if (req->method == HTTP_POST) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
      } else if (req->method == HTTP_PATCH) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "QUERY");
      }
      curl_easy_setopt(curl, CURLOPT_READFUNCTION, math_curl_read_callback);
      curl_easy_setopt(curl, CURLOPT_READDATA, (void *)(size_t)req);
      if (req->expected_body_len > 0) {
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                         (curl_off_t)req->expected_body_len);
      }
      break;
    default:
      break;
    }
  } else {
    switch (req->method) {
    case HTTP_GET:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
      break;
    case HTTP_POST:
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      if (payload) {
        if (payload_len > 0) {
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
        }
      }
      break;
    case HTTP_PUT:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      if (payload) {
        if (payload_len > 0) {
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
        }
      }
      break;
    case HTTP_DELETE:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    case HTTP_HEAD:
      curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
      break;
    case HTTP_PATCH:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      if (payload) {
        if (payload_len > 0) {
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
        }
      }
      break;
    case HTTP_QUERY:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "QUERY");
      if (payload) {
        if (payload_len > 0) {
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
        }
      }
      break;
    default:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
      break;
    }
  }

  for (i = 0; i < req->headers.count; ++i) {
    char *h_str = NULL;
    c_abstract_http_error_t rc_h = ABSTRACT_HTTP_FORMAT_HEADER(
        req->headers.headers[i].key, req->headers.headers[i].value, &h_str);
    if (rc_h != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error ENOMEM in "
                "ABSTRACT_HTTP_FORMAT_HEADER");
      curl_slist_free_all(*out_headers);
      *out_headers = NULL;
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }
    new_list = ABSTRACT_HTTP_CURL_SLIST_APPEND(*out_headers, h_str);
    free(h_str);
    if (!new_list) {
      LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error ENOMEM in "
                "ABSTRACT_HTTP_CURL_SLIST_APPEND");
      curl_slist_free_all(*out_headers);
      *out_headers = NULL;
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }
    *out_headers = new_list;
  }

  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Error %d", rc);
    return rc;
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *out_headers);

  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_WRITEFUNCTION,
                                     math_write_memory_callback) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (ABSTRACT_HTTP_CURL_EASY_SETOPT(curl, CURLOPT_WRITEDATA,
                                     (void *)write_ctx) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  LOG_DEBUG("ABSTRACT_HTTP_SETUP_CURL_REQUEST: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Populate HttpResponse, harvest cookies, and release request resources.
 * @param ctx Transport context.
 * @param curl Completed easy handle.
 * @param req Original HTTP request.
 * @param write_ctx Write callback buffer state.
 * @param headers Header slist to free (may be NULL).
 * @param res_code libcurl completion status code.
 * @param out_res Output pointer receiving allocated HttpResponse.
 * @return C_ABSTRACT_HTTP_SUCCESS or mapped error enum.
 */
NO_DISCARD static c_abstract_http_error_t ABSTRACT_HTTP_FINISH_CURL_REQUEST(
    struct HttpTransportContext *ctx, CURL *curl, const struct HttpRequest *req,
    struct CurlWriteContext *write_ctx, struct curl_slist *headers,
    CURLcode res_code, struct HttpResponse **out_res) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
    ABSTRACT_HTTP_CURL_EASY_GETINFO(curl, CURLINFO_COOKIELIST, &cookies);
    if (cookies) {
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
        if (sscanf(each->data, "%255s\t%15s\t%255s\t%15s\t%ld\t%255s\t%2047s",
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
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (new_res) {
      c_abstract_http_error_t rch = http_response_free(new_res);
      if (rch != C_ABSTRACT_HTTP_SUCCESS)
        return rch;
      free(new_res);
    }
  }
  curl_slist_free_all(headers);
  free(write_ctx->chunk.memory);

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("ABSTRACT_HTTP_FINISH_CURL_REQUEST: Success");
  } else {
    LOG_DEBUG("ABSTRACT_HTTP_FINISH_CURL_REQUEST: Error returning %d", rc);
  }
  return rc;
}

/** @brief Send an HTTP request via libcurl */
NO_DISCARD c_abstract_http_error_t
http_curl_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
               struct HttpResponse **const res) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
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
    free(write_ctx.chunk.memory);
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

/**
 * @brief Drain completed transfers from the libcurl multi handle into futures.
 * @param ctx Transport context owning the multi handle.
 * @return C_ABSTRACT_HTTP_SUCCESS or simulated error enum.
 */
NO_DISCARD static c_abstract_http_error_t
ABSTRACT_HTTP_CHECK_MULTI_INFO(struct HttpTransportContext *ctx) {
  CURLMsg *msg;
  int msgs_left;
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_curl_check_multi_info_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  while ((msg = ABSTRACT_HTTP_CURL_MULTI_INFO_READ(ctx->multi, &msgs_left))) {
    CURL *easy = msg->easy_handle;
    CURLcode res_code = msg->data.result;
    struct CurlMultiTask *task = NULL;

    ABSTRACT_HTTP_CURL_EASY_GETINFO(easy, CURLINFO_PRIVATE, &task);
    {
      struct HttpResponse *res = NULL;
      int rc = ABSTRACT_HTTP_FINISH_CURL_REQUEST(task->ctx, easy, task->req,
                                                 &task->write_ctx,
                                                 task->headers, res_code, &res);
      task->future->response = res;
      task->future->error_code = rc;
      task->future->is_ready = 1;
      free(task);
    }
    ABSTRACT_HTTP_CURL_MULTI_REMOVE_HANDLE(ctx->multi, easy);
    curl_easy_cleanup(easy);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Event-loop timer callback driving libcurl multi timeout actions.
 * @param loop Active event loop.
 * @param timer_id Fired timer identifier.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum from check_multi_info.
 */
NO_DISCARD static c_abstract_http_error_t
multi_timer_cb(struct ModalityEventLoop *loop, int timer_id, void *user_data) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)user_data;
  int running_handles;
  c_abstract_http_error_t rc;
  (void)loop;
  (void)timer_id;
  ABSTRACT_HTTP_CURL_MULTI_SOCKET_ACTION(ctx->multi, CURL_SOCKET_TIMEOUT, 0,
                                         &running_handles);
  rc = ABSTRACT_HTTP_CHECK_MULTI_INFO(ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("multi_timer_cb: check_multi_info failed");
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Event-loop socket readiness callback driving libcurl multi I/O.
 * @param loop Active event loop.
 * @param fd Ready socket descriptor.
 * @param events Readiness event bitmask.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum from check_multi_info.
 */
NO_DISCARD static c_abstract_http_error_t
multi_socket_cb(struct ModalityEventLoop *loop, int fd, int events,
                void *user_data);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Test wrapper exposing multi_timer_cb.
 * @param loop Active event loop.
 * @param timer_id Fired timer identifier.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return Result from multi_timer_cb.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_timer_cb(
    struct ModalityEventLoop *loop, int timer_id, void *user_data);
/**
 * @brief Test wrapper exposing multi_timer_cb.
 * @param loop Active event loop.
 * @param timer_id Fired timer identifier.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return Result from multi_timer_cb.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_timer_cb(
    struct ModalityEventLoop *loop, int timer_id, void *user_data) {
  return multi_timer_cb(loop, timer_id, user_data);
}

/**
 * @brief Test wrapper exposing multi_socket_cb.
 * @param loop Active event loop.
 * @param fd Ready socket descriptor.
 * @param events Readiness event bitmask.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return Result from multi_socket_cb.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_socket_cb(
    struct ModalityEventLoop *loop, int fd, int events, void *user_data);
/**
 * @brief Test wrapper exposing multi_socket_cb.
 * @param loop Active event loop.
 * @param fd Ready socket descriptor.
 * @param events Readiness event bitmask.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return Result from multi_socket_cb.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_socket_cb(
    struct ModalityEventLoop *loop, int fd, int events, void *user_data) {
  return multi_socket_cb(loop, fd, events, user_data);
}
#endif

/**
 * @brief Event-loop socket readiness callback driving libcurl multi I/O.
 * @param loop Active event loop.
 * @param fd Ready socket descriptor.
 * @param events Readiness event bitmask.
 * @param user_data Pointer to struct HttpTransportContext.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum from check_multi_info.
 */
NO_DISCARD static c_abstract_http_error_t
multi_socket_cb(struct ModalityEventLoop *loop, int fd, int events,
                void *user_data) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)user_data;
  int action = 0;
  int running_handles;
  c_abstract_http_error_t rc;
  (void)loop;
  if (events & HTTP_LOOP_READ)
    action |= CURL_CSELECT_IN;
  if (events & HTTP_LOOP_WRITE)
    action |= CURL_CSELECT_OUT;
  if (events & HTTP_LOOP_ERROR)
    action |= CURL_CSELECT_ERR;

  ABSTRACT_HTTP_CURL_MULTI_SOCKET_ACTION(ctx->multi, fd, action,
                                         &running_handles);
  rc = ABSTRACT_HTTP_CHECK_MULTI_INFO(ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("multi_socket_cb: check_multi_info failed");
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Update event-loop timer registration requested by libcurl multi.
 * @param ctx Transport context.
 * @param timeout_ms Requested timeout in milliseconds (-1 cancels timer).
 * @return C_ABSTRACT_HTTP_SUCCESS or event-loop error enum.
 */
NO_DISCARD static c_abstract_http_error_t
multi_timer_function_impl(struct HttpTransportContext *ctx, long timeout_ms) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

  if (ctx->timer_id > 0) {
    rc = http_loop_cancel_timer(ctx->loop, ctx->timer_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS && rc != C_ABSTRACT_HTTP_ERR_INVAL) {
      LOG_DEBUG("multi_timer_function: http_loop_cancel_timer failed (%d)", rc);
      return rc;
    }
    ctx->timer_id = 0;
  }

  if (timeout_ms >= 0) {
    rc = http_loop_add_timer(ctx->loop, timeout_ms, multi_timer_cb, ctx,
                             &ctx->timer_id);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      LOG_DEBUG("multi_timer_function: http_loop_add_timer failed (%d)", rc);
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief libcurl CURLMOPT_TIMERFUNCTION callback trampoline.
 * @param multi Multi handle.
 * @param timeout_ms Requested timeout in milliseconds (-1 cancels timer).
 * @param userp Pointer to struct HttpTransportContext.
 * @return 0 on success or -1 on error.
 */
static int multi_timer_function(CURLM *multi, long timeout_ms, void *userp) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)userp;
  (void)multi;

  return (multi_timer_function_impl(ctx, timeout_ms) != C_ABSTRACT_HTTP_SUCCESS)
             ? -1
             : 0;
}

/**
 * @brief libcurl CURLMOPT_SOCKETFUNCTION callback updating event-loop fd
 * watches.
 * @param easy Easy handle associated with the socket.
 * @param s Socket descriptor.
 * @param what Poll action requested by libcurl.
 * @param userp Pointer to struct HttpTransportContext.
 * @param socketp Per-socket private state flag.
 * @return 0 on success or -1 on error.
 */
static int multi_socket_function(CURL *easy, curl_socket_t s, int what,
                                 void *userp, void *socketp) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)userp;
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;

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
      ABSTRACT_HTTP_CURL_MULTI_ASSIGN(ctx->multi, s, (void *)ctx);
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
  LOG_DEBUG("multi_socket_function: returning -1 due to internal error %d", rc);
  return -1;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/**
 * @brief Invoke multi_timer_function for unit test verification.
 * @param ctx Context pointer.
 * @param timeout_ms Timeout in milliseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_timer_function(
    struct HttpTransportContext *ctx, long timeout_ms);

/**
 * @brief Invoke multi_socket_function for unit test verification.
 * @param ctx Context pointer.
 * @param s Socket descriptor.
 * @param what Action flags.
 * @param socketp Socket state pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_socket_function(
    struct HttpTransportContext *ctx, curl_socket_t s, int what, void *socketp);

/**
 * @brief Set timer ID for test verification.
 * @param ctx Context pointer.
 * @param timer_id Timer ID value.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t
abstract_http_test_set_timer_id(struct HttpTransportContext *ctx, int timer_id);

/**
 * @brief Get timer ID for test verification.
 * @param ctx Context pointer.
 * @param out_timer_id Output pointer for timer ID.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_get_timer_id(
    struct HttpTransportContext *ctx, int *out_timer_id);

/**
 * @brief Set event loop for test verification.
 * @param ctx Context pointer.
 * @param loop Event loop pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_set_loop(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop);

/**
 * @brief Invoke multi_timer_function for unit test verification.
 * @param ctx Context pointer.
 * @param timeout_ms Timeout in milliseconds.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_timer_function(
    struct HttpTransportContext *ctx, long timeout_ms) {
  int r = multi_timer_function(ctx->multi, timeout_ms, ctx);
  return (c_abstract_http_error_t)r;
}

/* Invoke multi_socket_function for unit test verification. */
NO_DISCARD c_abstract_http_error_t abstract_http_test_multi_socket_function(
    struct HttpTransportContext *ctx, curl_socket_t s, int what,
    void *socketp) {
  int r = multi_socket_function(ctx->curl, s, what, ctx, socketp);
  return (c_abstract_http_error_t)r;
}

/**
 * @brief Set timer ID for test verification.
 * @param ctx Context pointer.
 * @param timer_id Timer ID value.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_set_timer_id(
    struct HttpTransportContext *ctx, int timer_id) {
  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  ctx->timer_id = timer_id;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Get timer ID for test verification.
 * @param ctx Context pointer.
 * @param out_timer_id Output pointer for timer ID.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_get_timer_id(
    struct HttpTransportContext *ctx, int *out_timer_id) {
  if (!ctx || !out_timer_id)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *out_timer_id = ctx->timer_id;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Set event loop for test verification.
 * @param ctx Context pointer.
 * @param loop Event loop pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
NO_DISCARD c_abstract_http_error_t abstract_http_test_set_loop(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop) {
  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  ctx->loop = loop;
  return C_ABSTRACT_HTTP_SUCCESS;
}
#endif

/** @brief Dispatch multiple HTTP requests asynchronously via libcurl multi */
NO_DISCARD c_abstract_http_error_t http_curl_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  c_abstract_http_error_t rc = C_ABSTRACT_HTTP_SUCCESS;
  size_t i;

  LOG_DEBUG("http_curl_send_multi: Entering");
  if (!ctx || !ctx->multi || !loop || !multi || !futures) {
    LOG_DEBUG("http_curl_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  ctx->loop = loop;

  if (ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_SOCKETFUNCTION,
                                      multi_socket_function) != CURLM_OK) {
    LOG_DEBUG(
        "http_curl_send_multi: Error ABSTRACT_HTTP_CURL_MULTI_SETOPT failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_SOCKETDATA, ctx) !=
      CURLM_OK) {
    LOG_DEBUG(
        "http_curl_send_multi: Error ABSTRACT_HTTP_CURL_MULTI_SETOPT failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_TIMERFUNCTION,
                                      multi_timer_function) != CURLM_OK) {
    LOG_DEBUG(
        "http_curl_send_multi: Error ABSTRACT_HTTP_CURL_MULTI_SETOPT failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (ABSTRACT_HTTP_CURL_MULTI_SETOPT(ctx->multi, CURLMOPT_TIMERDATA, ctx) !=
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
      curl_slist_free_all(task->headers);
      free(task->write_ctx.chunk.memory);
      curl_easy_cleanup(task->easy);
      free(task);
      futures[i]->internal_state = NULL;
      break;
    }

    if (ABSTRACT_HTTP_CURL_EASY_SETOPT(task->easy, CURLOPT_PRIVATE, task) !=
        CURLE_OK) {
      curl_slist_free_all(task->headers);
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
      curl_slist_free_all(task->headers);
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
      curl_slist_free_all(task->headers);
      free(task->write_ctx.chunk.memory);
      ABSTRACT_HTTP_CURL_MULTI_REMOVE_HANDLE(ctx->multi, task->easy);
      curl_easy_cleanup(task->easy);
      free(task);
    }
    return rc;
  }

  LOG_DEBUG("http_curl_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
