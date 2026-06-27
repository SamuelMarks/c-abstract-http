
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
#include "str.h"
/* clang-format on */

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
    if (rc != 0) {
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
  int rc = 0;
  const struct HttpRequest *req = (const struct HttpRequest *)userdata;
  size_t max_bytes = size * nitems;
  size_t out_read = 0;

  if (!req || !req->read_chunk) {
    return CURL_READFUNC_ABORT;
  }

  rc = req->read_chunk(req->read_chunk_user_data, buffer, max_bytes, &out_read);
  if (rc != 0) {
    return CURL_READFUNC_ABORT;
  }

  return out_read;
}

static int format_header(const char *key, const char *value, char **_out_val) {
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

static int map_curl_error(CURLcode res) {
  switch (res) {
  case CURLE_OK:
    return C_ABSTRACT_HTTP_SUCCESS;
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
enum c_abstract_http_error http_curl_global_init(void) {
  if (g_curl_init_count == 0) {
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
  }
  g_curl_init_count++;
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_curl_global_cleanup(void) {
  if (g_curl_init_count > 0) {
    g_curl_init_count--;
    if (g_curl_init_count == 0) {
      curl_global_cleanup();
    }
  }
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
      curl_multi_cleanup((*ctx)->multi);
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
      curl_multi_cleanup(ctx->multi);
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
      if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                           CURL_HTTP_VERSION_3) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
    } else {
#if LIBCURL_VERSION_NUM >= 0x075000 /* 7.80.0 */
      if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                           CURL_HTTP_VERSION_3ONLY) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
#else
      if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                           CURL_HTTP_VERSION_3) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
#endif
    }
#else
    /* Fallback to default if libcurl is too old */
    if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                         CURL_HTTP_VERSION_NONE) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  } else if (config->version_mask & HTTP_VERSION_2) {
#if LIBCURL_VERSION_NUM >= 0x072100 /* 7.33.0 */
    if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                         CURL_HTTP_VERSION_2_0) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#else
    if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                         CURL_HTTP_VERSION_NONE) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  } else if (config->version_mask & HTTP_VERSION_1_1) {
    if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                         CURL_HTTP_VERSION_1_1) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  } else if (config->version_mask & HTTP_VERSION_1_0) {
    if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
                         CURL_HTTP_VERSION_1_0) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  } else {
    if (curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION,
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

    if (curl_easy_setopt(ctx->curl, CURLOPT_SSLVERSION,
                         ssl_version | ssl_version_max) != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#else
    if (curl_easy_setopt(ctx->curl, CURLOPT_SSLVERSION, ssl_version) !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
#endif
  }

  if (curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT_MS, config->timeout_ms) !=
      CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (curl_easy_setopt(ctx->curl, CURLOPT_CONNECTTIMEOUT_MS,
                       config->timeout_ms) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYPEER,
                       config->verify_peer ? 1L : 0L) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;
  if (curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYHOST,
                       config->verify_host ? 2L : 0L) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION,
                       config->follow_redirects ? 1L : 0L) != CURLE_OK)
    return C_ABSTRACT_HTTP_ERR_IO;

  if (config->user_agent) {
    if (curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT, config->user_agent) !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (config->proxy_url) {
    if (curl_easy_setopt(ctx->curl, CURLOPT_PROXY, config->proxy_url) !=
        CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;

    if (config->proxy_username && config->proxy_password) {
      if (curl_easy_setopt(ctx->curl, CURLOPT_PROXYUSERNAME,
                           config->proxy_username) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
      if (curl_easy_setopt(ctx->curl, CURLOPT_PROXYPASSWORD,
                           config->proxy_password) != CURLE_OK)
        return C_ABSTRACT_HTTP_ERR_IO;
    }
  } else {
    curl_easy_setopt(ctx->curl, CURLOPT_PROXY, "");
  }

  if (config->cookie_jar) {
    ctx->cookie_jar = config->cookie_jar;
    /* Enable curl's cookie engine without reading a file */
    if (curl_easy_setopt(ctx->curl, CURLOPT_COOKIEFILE, "") != CURLE_OK)
      return C_ABSTRACT_HTTP_ERR_IO;
    /* Instruct curl to write cookies to a dummy state (handled manually or via
     * curl's getinfo later) */
  } else {
    ctx->cookie_jar = NULL;
  }

  LOG_DEBUG("http_curl_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int setup_curl_request(CURL *curl, const struct HttpRequest *req,
                              struct CurlWriteContext *write_ctx,
                              struct curl_slist **out_headers) {
  int rc = 0;
  char *_ast_format_header_0;
  size_t i;
  void *payload = req->body;
  size_t payload_len = req->body_len;

  LOG_DEBUG("setup_curl_request: Entering");
  if (req->parts.count > 0 && !payload) {
    LOG_DEBUG("setup_curl_request: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  write_ctx->chunk.memory = (char *)malloc(1);
  write_ctx->chunk.size = 0;
  if (!write_ctx->chunk.memory) {
    LOG_DEBUG("setup_curl_request: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  write_ctx->chunk.memory[0] = '\0';
  write_ctx->req = req;
  write_ctx->user_aborted = 0;

  if (curl_easy_setopt(curl, CURLOPT_URL, req->url) != CURLE_OK) {
    LOG_DEBUG("setup_curl_request: Error curl_easy_setopt url failed");
  }

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
      } else if (req->method == HTTP_QUERY) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "QUERY");
      }
      curl_easy_setopt(curl, CURLOPT_READFUNCTION, math_curl_read_callback);
      curl_easy_setopt(curl, CURLOPT_READDATA, (void *)req);
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
      if (payload && payload_len > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
      }
      break;
    case HTTP_PUT:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      if (payload && payload_len > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
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
      if (payload && payload_len > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
      }
      break;
    case HTTP_QUERY:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "QUERY");
      if (payload && payload_len > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload_len);
      }
      break;
    default:
      curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
      break;
    }
  }

  for (i = 0; i < req->headers.count; ++i) {
    char *h_str =
        (format_header(req->headers.headers[i].key,
                       req->headers.headers[i].value, &_ast_format_header_0),
         _ast_format_header_0);
    if (!h_str) {
      LOG_DEBUG("setup_curl_request: Error ENOMEM in format_header");
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }
    *out_headers = curl_slist_append(*out_headers, h_str);
    free(h_str);
    if (!*out_headers) {
      LOG_DEBUG("setup_curl_request: Error ENOMEM in curl_slist_append");
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      break;
    }
  }

  if (rc != 0) {
    LOG_DEBUG("setup_curl_request: Error %d", rc);
    return rc;
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *out_headers);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, math_write_memory_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)write_ctx);

  LOG_DEBUG("setup_curl_request: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int finish_curl_request(struct HttpTransportContext *ctx, CURL *curl,
                               const struct HttpRequest *req,
                               struct CurlWriteContext *write_ctx,
                               struct curl_slist *headers, CURLcode res_code,
                               struct HttpResponse **out_res) {
  int rc = 0;
  long response_code = 0;
  struct HttpResponse *new_res = NULL;

  LOG_DEBUG("finish_curl_request: Entering");

  if (res_code != CURLE_OK) {
    if (write_ctx->user_aborted != 0) {
      rc = write_ctx->user_aborted;
    } else {
      rc = map_curl_error(res_code);
    }
    LOG_DEBUG("finish_curl_request: Error res_code != CURLE_OK, rc=%d", rc);
    goto cleanup;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    LOG_DEBUG("finish_curl_request: Error ENOMEM for new_res");
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    goto cleanup;
  }

  rc = http_response_init(new_res);
  if (rc != 0) {
    LOG_DEBUG("finish_curl_request: Error http_response_init failed with %d",
              rc);
    free(new_res);
    new_res = NULL;
    goto cleanup;
  }

  /* Sync cookies back to jar if provided */
  if (ctx->cookie_jar) {
    struct curl_slist *cookies = NULL;
    if (curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies) == CURLE_OK &&
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
        if (sscanf(each->data, "%255s\t%15s\t%255s\t%15s\t%ld\t%255s\t%2047s",
                   domain, flag, path, secure, &expiration, name, value) == 7) {
#endif
          rc = http_cookie_jar_set(ctx->cookie_jar, name, value);
          if (rc != 0) {
            LOG_DEBUG(
                "finish_curl_request: Error http_cookie_jar_set failed with %d",
                rc);
            /* We may choose not to abort the whole request for a cookie error,
             * but log it */
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

  if (rc == 0) {
    LOG_DEBUG("finish_curl_request: Success");
  } else {
    LOG_DEBUG("finish_curl_request: Error returning %d", rc);
  }
  return rc;
}

enum c_abstract_http_error http_curl_send(struct HttpTransportContext *ctx,
                                          const struct HttpRequest *req,
                                          struct HttpResponse **const res) {
  int rc = 0;
  CURLcode res_code;
  struct curl_slist *headers = NULL;
  struct CurlWriteContext write_ctx;

  LOG_DEBUG("http_curl_send: Entering");
  if (!ctx || !ctx->curl || !req || !res) {
    LOG_DEBUG("http_curl_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = setup_curl_request(ctx->curl, req, &write_ctx, &headers);
  if (rc != 0) {
    LOG_DEBUG("http_curl_send: Error setup_curl_request failed with %d", rc);
    if (headers)
      curl_slist_free_all(headers);
    if (write_ctx.chunk.memory)
      free(write_ctx.chunk.memory);
    return rc;
  }

  res_code = curl_easy_perform(ctx->curl);
  rc = finish_curl_request(ctx, ctx->curl, req, &write_ctx, headers, res_code,
                           res);

  if (rc == 0) {
    LOG_DEBUG("http_curl_send: Success");
  } else {
    LOG_DEBUG("http_curl_send: Error returning %d", rc);
  }
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

/* LCOV_EXCL_START */
static void check_multi_info(struct HttpTransportContext *ctx) {
  CURLMsg *msg;
  int msgs_left;
  while ((msg = curl_multi_info_read(ctx->multi, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      CURL *easy = msg->easy_handle;
      CURLcode res_code = msg->data.result;
      struct CurlMultiTask *task = NULL;

      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &task);
      if (task) {
        struct HttpResponse *res = NULL;
        int rc =
            finish_curl_request(task->ctx, easy, task->req, &task->write_ctx,
                                task->headers, res_code, &res);
        task->future->response = res;
        task->future->error_code = rc;
        task->future->is_ready = 1;

        curl_multi_remove_handle(ctx->multi, easy);
        curl_easy_cleanup(easy);
        free(task);
      }
    }
  }
}

static void multi_timer_cb(struct ModalityEventLoop *loop, int timer_id,
                           void *user_data) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)user_data;
  int running_handles;
  (void)loop;
  (void)timer_id;
  curl_multi_socket_action(ctx->multi, CURL_SOCKET_TIMEOUT, 0,
                           &running_handles);
  check_multi_info(ctx);
}

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

  curl_multi_socket_action(ctx->multi, fd, action, &running_handles);
  check_multi_info(ctx);
}

static int multi_timer_function(CURLM *multi, long timeout_ms, void *userp) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)userp;
  (void)multi;

  if (ctx->timer_id > 0) {
    http_loop_cancel_timer(ctx->loop, ctx->timer_id);
    ctx->timer_id = 0;
  }

  if (timeout_ms >= 0) {
    http_loop_add_timer(ctx->loop, timeout_ms, multi_timer_cb, ctx,
                        &ctx->timer_id);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int multi_socket_function(CURL *easy, curl_socket_t s, int what,
                                 void *userp, void *socketp) {
  struct HttpTransportContext *ctx = (struct HttpTransportContext *)userp;

  (void)easy;

  if (what == CURL_POLL_REMOVE) {
    http_loop_remove_fd(ctx->loop, (int)s);
    curl_multi_assign(ctx->multi, s, NULL);
  } else {
    int events = 0;
    if (what == CURL_POLL_IN || what == CURL_POLL_INOUT)
      events |= HTTP_LOOP_READ;
    if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT)
      events |= HTTP_LOOP_WRITE;

    if (!socketp) {
      http_loop_add_fd(ctx->loop, (int)s, events, multi_socket_cb, ctx);
      curl_multi_assign(ctx->multi, s, (void *)1);
    } else {
      http_loop_mod_fd(ctx->loop, (int)s, events);
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
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

  if (curl_multi_setopt(ctx->multi, CURLMOPT_SOCKETFUNCTION,
                        multi_socket_function) != CURLM_OK ||
      curl_multi_setopt(ctx->multi, CURLMOPT_SOCKETDATA, ctx) != CURLM_OK ||
      curl_multi_setopt(ctx->multi, CURLMOPT_TIMERFUNCTION,
                        multi_timer_function) != CURLM_OK ||
      curl_multi_setopt(ctx->multi, CURLMOPT_TIMERDATA, ctx) != CURLM_OK) {
    LOG_DEBUG("http_curl_send_multi: Error curl_multi_setopt failed");
  }

  for (i = 0; i < multi->count; ++i) {
    struct CurlMultiTask *task;

    task = (struct CurlMultiTask *)calloc(1, sizeof(struct CurlMultiTask));
    if (!task) {
      LOG_DEBUG("http_curl_send_multi: Error ENOMEM (task)");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    task->easy = curl_easy_duphandle(ctx->curl);
    if (!task->easy) {
      LOG_DEBUG("http_curl_send_multi: Error ENOMEM (easy_duphandle)");
      free(task);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    task->ctx = ctx;
    task->req = multi->requests[i];
    task->future = futures[i];

    rc = setup_curl_request(task->easy, task->req, &task->write_ctx,
                            &task->headers);
    if (rc != 0) {
      LOG_DEBUG("http_curl_send_multi: Error setup_curl_request failed %d", rc);
      if (task->headers)
        curl_slist_free_all(task->headers);
      if (task->write_ctx.chunk.memory)
        free(task->write_ctx.chunk.memory);
      curl_easy_cleanup(task->easy);
      free(task);
      return rc;
    }

    curl_easy_setopt(task->easy, CURLOPT_PRIVATE, task);
    if (curl_multi_add_handle(ctx->multi, task->easy) != CURLM_OK) {
      LOG_DEBUG("http_curl_send_multi: Error curl_multi_add_handle failed");
    }
  }

  LOG_DEBUG("http_curl_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
/* LCOV_EXCL_STOP */
