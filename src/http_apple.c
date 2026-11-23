/**
 * @file http_apple.c
 * @brief Apple CFNetwork and NSURLSession C-bridge implementation of the
 * Abstract Network Interface (ANI).
 * @author Samuel Marks
 */

/* clang-format off */
#include <c_abstract_http/http_apple.h>
#include <c_abstract_http/event_loop.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#if defined(__APPLE__)
#ifndef OBJC_OLD_DISPATCH_PROTOTYPES
/** @brief Enforce non-variadic objc_msgSend declaration */
#define OBJC_OLD_DISPATCH_PROTOTYPES 0
#endif
#include <AvailabilityMacros.h>
#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>
#include <unistd.h>
#endif
#include "crypto_utils.h"
/* clang-format on */

#if defined(__APPLE__)

/** @brief Generic function pointer to `objc_msgSend` for ISO C89 typed dispatch
 */
static void (*const cah_objc_msg_send)(void) = &objc_msgSend;
#undef objc_msgSend
/** @brief Redirect `objc_msgSend` casts through `cah_objc_msg_send` variable */
#define objc_msgSend cah_objc_msg_send

#if (defined(MAC_OS_X_VERSION_MIN_REQUIRED) &&                                 \
     MAC_OS_X_VERSION_MIN_REQUIRED >= 101100) ||                               \
    defined(C_ABSTRACT_HTTP_APPLE_NO_CFSTREAM)
/** @brief Zero when legacy CFHTTPStream APIs are deprecated on target OS */
#define C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM 0
#else
/** @brief Non-zero when legacy CFHTTPStream APIs are available without
 * deprecation */
#define C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM 1
#endif

#if !C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM ||                                    \
    defined(C_ABSTRACT_HTTP_APPLE_USE_NSURLSESSION)
/** @brief Default runtime selection for NSURLSession C-bridge */
#define C_ABSTRACT_HTTP_APPLE_DEFAULT_NSURLSESSION 1
#else
/** @brief Default runtime selection for legacy CFStream */
#define C_ABSTRACT_HTTP_APPLE_DEFAULT_NSURLSESSION 0
#endif

/** @brief Internal Apple transport context */
struct HttpTransportContext {
  struct HttpConfig config; /**< Active HTTP configuration */
  int use_nsurlsession;     /**< 1 for NSURLSession bridge, 0 for CFStream */
#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  pthread_mutex_t cf_worker_lock; /**< Synchronizes active_cf_workers */
  pthread_cond_t cf_worker_cond;  /**< Signaled when CFStream workers exit */
  int active_cf_workers;          /**< Count of running CFStream workers */
#endif
};

/** @brief Worker context for Apple multi-request dispatch */
struct AppleMultiWorkerCtx {
  struct HttpTransportContext *ctx;     /**< Transport context */
  struct ModalityEventLoop *loop;       /**< Event loop */
  const struct HttpMultiRequest *multi; /**< Multi-request batch */
  struct HttpFuture **futures;          /**< Futures array */
};

/**
 * @brief Map an HttpMethod enum to a CoreFoundation HTTP method string.
 * @param method Input HTTP method enumerator.
 * @param out Output CFStringRef constant.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD static c_abstract_http_error_t
apple_method_cfstr(enum HttpMethod method, CFStringRef *out) {
  switch ((int)method) {
  case HTTP_GET:
    *out = CFSTR("GET");
    break;
  case HTTP_POST:
    *out = CFSTR("POST");
    break;
  case HTTP_PUT:
    *out = CFSTR("PUT");
    break;
  case HTTP_DELETE:
    *out = CFSTR("DELETE");
    break;
  case HTTP_PATCH:
    *out = CFSTR("PATCH");
    break;
  case HTTP_HEAD:
    *out = CFSTR("HEAD");
    break;
  case HTTP_OPTIONS:
    *out = CFSTR("OPTIONS");
    break;
  case HTTP_TRACE:
    *out = CFSTR("TRACE");
    break;
  case HTTP_CONNECT:
    *out = CFSTR("CONNECT");
    break;
  default:
    *out = CFSTR("GET");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Release a partially initialized response object and return error code.
 * @param res In/out response pointer.
 * @param orig_rc Primary error code to return if cleanup succeeds.
 * @return Cleanup error if cleanup failed, otherwise @p orig_rc.
 */
NO_DISCARD static c_abstract_http_error_t
apple_cleanup_failed_res(struct HttpResponse **res,
                         c_abstract_http_error_t orig_rc) {
  struct HttpResponse *tmp;
  c_abstract_http_error_t free_rc;
  if (!*res) {
    return orig_rc;
  }
  tmp = *res;
  *res = NULL;
  free_rc = http_response_free(tmp);
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(tmp);
    return free_rc;
  }
  free(tmp);
  return orig_rc;
}

/* ========================================================================== */
/* Modern Apple NSURLSession Pure-C89 Objective-C Runtime Bridge              */
/* ========================================================================== */

/** @brief Unique address key for Objective-C associated delegate state */
static char g_cah_urlsession_state_key = 0;

/** @brief One-time delegate class registration guard */
static pthread_once_t g_cah_urlsession_once = PTHREAD_ONCE_INIT;

/** @brief Shared non-blocking multi-request batch state for NSURLSession */
struct AppleURLSessionBatch {
  struct ModalityEventLoop *loop;     /**< Event loop woken on completion */
  struct AppleURLSessionState *tasks; /**< Per-task states array */
  size_t count;                       /**< Total slots in `tasks` */
  size_t remaining;                   /**< In-flight NSURLSessionDataTasks */
  int notify_pipe[2];                 /**< kqueue fd bridge pipe */
  int has_pipe;                       /**< Non-zero when pipe is on `loop` */
  int sync_wait;                      /**< Non-zero when caller awaits cv */
  int batch_done;                     /**< Non-zero once remaining == 0 */
  c_abstract_http_error_t wake_rc;    /**< Wakeup status recorded on finish */
  pthread_mutex_t mtx;                /**< Batch synchronization mutex */
  pthread_cond_t cv;                  /**< Batch completion condition var */
};

/** @brief Synchronization and buffer state for an NSURLSession data task */
struct AppleURLSessionState {
  const struct HttpRequest *req;      /**< Originating HTTP request */
  struct HttpRequest flat_req;        /**< Scratch flattened multipart req */
  int flattened;                      /**< Non-zero if `flat_req` is active */
  struct HttpResponse *res;           /**< Output HTTP response */
  struct HttpFuture *async_future;    /**< Optional async future slot */
  struct AppleURLSessionBatch *batch; /**< Optional owning multi-batch */
  CFMutableDataRef body_data;         /**< Buffered response payload */
  c_abstract_http_error_t error;      /**< Recorded transport/callback error */
  int done;                           /**< Task completion flag */
  pthread_mutex_t mtx;                /**< Completion mutex */
  pthread_cond_t cv;                  /**< Completion condition variable */
};

/**
 * @brief Free partially built `flat_req` on multipart preparation failure.
 * @param flat_req Scratch request structure.
 * @param orig_rc Primary error enum.
 * @return Cleanup error if `http_headers_free` fails, otherwise `orig_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
apple_fail_flat_req(struct HttpRequest *flat_req,
                    c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t frc;
  free(flat_req->body);
  flat_req->body = NULL;
  frc = http_headers_free(&flat_req->headers);
  if (frc != C_ABSTRACT_HTTP_SUCCESS) {
    return frc;
  }
  return orig_rc;
}

/**
 * @brief Flatten `req->parts` into `flat_req` when multipart parts are set.
 * @param req Input request.
 * @param flat_req Scratch request struct receiving flattened body/headers.
 * @param out_flattened Set to 1 if `flat_req` owns allocated buffers.
 * @param out_eff Output pointer to effective request to transmit.
 * @return C_ABSTRACT_HTTP_SUCCESS or error enum.
 */
NO_DISCARD static c_abstract_http_error_t apple_prepare_effective_request(
    const struct HttpRequest *req, struct HttpRequest *flat_req,
    int *out_flattened, const struct HttpRequest **out_eff) {
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

  for (i = 0; i < req->headers.count; ++i) {
    rc = http_headers_add(&flat_req->headers, req->headers.headers[i].key,
                          req->headers.headers[i].value);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return apple_fail_flat_req(flat_req, rc);
    }
  }
  rc = http_request_flatten_parts(flat_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return apple_fail_flat_req(flat_req, rc);
  }
  *out_flattened = 1;
  *out_eff = flat_req;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Release flattened multipart buffers in `flat_req` when active.
 * @param flat_req Scratch request struct.
 * @param flattened Non-zero when `flat_req` owns flattened allocations.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup error enum.
 */
NO_DISCARD static c_abstract_http_error_t
apple_cleanup_effective_request(struct HttpRequest *flat_req, int flattened) {
  if (!flattened) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  free(flat_req->body);
  flat_req->body = NULL;
  return http_headers_free(&flat_req->headers);
}

/**
 * @brief Build a `Basic <base64>` Proxy-Authorization value.
 * @param user Proxy username.
 * @param pass Optional proxy password.
 * @param out_hdr Output allocated header value.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
apple_build_proxy_auth(const char *user, const char *pass, char **out_hdr) {
  const char *pw = pass ? pass : "";
  size_t ulen = strlen(user);
  size_t plen = strlen(pw);
  char *raw;
  char *b64 = NULL;
  size_t b64_len = 0;
  char *hdr;
  c_abstract_http_error_t rc;

  raw = (char *)malloc(ulen + 1U + plen + 1U);
  if (!raw) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(raw, user, ulen);
  raw[ulen] = ':';
  memcpy(raw + ulen + 1U, pw, plen);
  raw[ulen + 1U + plen] = '\0';

  rc = base64_encode((const unsigned char *)raw, ulen + 1U + plen, &b64,
                     &b64_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(raw);
    return rc;
  }
  free(raw);

  hdr = (char *)malloc(6U + b64_len + 1U);
  if (!hdr) {
    free(b64);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  memcpy(hdr, "Basic ", 6U);
  memcpy(hdr + 6U, b64, b64_len);
  hdr[6U + b64_len] = '\0';
  free(b64);
  *out_hdr = hdr;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Apply `HttpConfig` session-level timeouts, TLS version, and proxy
 * dictionary onto `NSURLSessionConfiguration`.
 * @param cfg Active `HttpConfig`.
 * @param scfg `NSURLSessionConfiguration` Objective-C object.
 * @return C_ABSTRACT_HTTP_SUCCESS.
 */
NO_DISCARD static c_abstract_http_error_t
apple_configure_session_only(const struct HttpConfig *cfg, id scfg) {
  long req_ms = cfg->read_timeout_ms > 0
                    ? cfg->read_timeout_ms
                    : (cfg->connect_timeout_ms > 0 ? cfg->connect_timeout_ms
                                                   : cfg->timeout_ms);
  long res_ms = cfg->timeout_ms > 0
                    ? cfg->timeout_ms
                    : (cfg->connect_timeout_ms + cfg->read_timeout_ms +
                       cfg->write_timeout_ms);
  if (req_ms > 0) {
    ((void (*)(id, SEL, double))objc_msgSend)(
        scfg, sel_registerName("setTimeoutIntervalForRequest:"),
        (double)req_ms / 1000.0);
  }
  if (res_ms > 0) {
    ((void (*)(id, SEL, double))objc_msgSend)(
        scfg, sel_registerName("setTimeoutIntervalForResource:"),
        (double)res_ms / 1000.0);
  }
  if (cfg->tls_version_mask != HTTP_TLS_VERSION_DEFAULT) {
    SEL tls_sel = sel_registerName("setTLSMinimumSupportedProtocolVersion:");
    if (((BOOL(*)(id, SEL, SEL))objc_msgSend)(
            scfg, sel_registerName("respondsToSelector:"), tls_sel)) {
      uint16_t min_ver = (cfg->tls_version_mask == HTTP_TLS_VERSION_1_3)
                             ? (uint16_t)0x0304
                             : (uint16_t)0x0303;
      ((void (*)(id, SEL, uint16_t))objc_msgSend)(scfg, tls_sel, min_ver);
    }
  }
  if (cfg->proxy_url) {
    const char *p = strstr(cfg->proxy_url, "://");
    const char *host_s = p + 3;
    const char *colon = strchr(host_s, ':');
    const char *slash = strchr(host_s, '/');
    size_t hlen = colon && (!slash || colon < slash)
                      ? (size_t)(colon - host_s)
                      : (slash ? (size_t)(slash - host_s) : strlen(host_s));
    int pport = colon && (!slash || colon < slash) ? atoi(colon + 1) : 8080;
    char hbuf[256];
    if (hlen == 0 || hlen >= sizeof(hbuf)) {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    {
      CFMutableDictionaryRef pdict;
      CFStringRef hcf;
      CFNumberRef pnum;
      int one = 1;
      CFNumberRef en_num;
      memcpy(hbuf, host_s, hlen);
      hbuf[hlen] = '\0';
      pdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 4,
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks);
      hcf = CFStringCreateWithCString(kCFAllocatorDefault, hbuf,
                                      kCFStringEncodingUTF8);
      pnum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pport);
      en_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one);
      CFDictionarySetValue(pdict, kCFNetworkProxiesHTTPEnable, en_num);
      CFDictionarySetValue(pdict, kCFNetworkProxiesHTTPProxy, hcf);
      CFDictionarySetValue(pdict, kCFNetworkProxiesHTTPPort, pnum);
      ((void (*)(id, SEL, CFDictionaryRef))objc_msgSend)(
          scfg, sel_registerName("setConnectionProxyDictionary:"), pdict);
      CFRelease(en_num);
      CFRelease(pnum);
      CFRelease(hcf);
      CFRelease(pdict);
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Apply `HttpConfig` per-request options onto `NSMutableURLRequest`.
 * @param cfg Active `HttpConfig`.
 * @param nsreq `NSMutableURLRequest` Objective-C object.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
apple_apply_nsurl_options(const struct HttpConfig *cfg, id nsreq) {
  long req_ms = cfg->read_timeout_ms > 0
                    ? cfg->read_timeout_ms
                    : (cfg->connect_timeout_ms > 0 ? cfg->connect_timeout_ms
                                                   : cfg->timeout_ms);

  if (req_ms > 0) {
    ((void (*)(id, SEL, double))objc_msgSend)(
        nsreq, sel_registerName("setTimeoutInterval:"),
        (double)req_ms / 1000.0);
  }
  if (cfg->user_agent) {
    CFStringRef ua_k = CFSTR("User-Agent");
    CFStringRef ua_v = CFStringCreateWithCString(
        kCFAllocatorDefault, cfg->user_agent, kCFStringEncodingUTF8);
    if (ua_v) {
      ((void (*)(id, SEL, CFTypeRef, CFTypeRef))objc_msgSend)(
          nsreq, sel_registerName("setValue:forHTTPHeaderField:"), ua_v, ua_k);
      CFRelease(ua_v);
    }
  }
  if (cfg->proxy_username) {
    char *auth_val = NULL;
    c_abstract_http_error_t arc;
    arc = apple_build_proxy_auth(cfg->proxy_username, cfg->proxy_password,
                                 &auth_val);
    if (arc != C_ABSTRACT_HTTP_SUCCESS) {
      return arc;
    }
    {
      CFStringRef pk = CFSTR("Proxy-Authorization");
      CFStringRef pv = CFStringCreateWithCString(kCFAllocatorDefault, auth_val,
                                                 kCFStringEncodingUTF8);
      free(auth_val);
      if (pv) {
        ((void (*)(id, SEL, CFTypeRef, CFTypeRef))objc_msgSend)(
            nsreq, sel_registerName("setValue:forHTTPHeaderField:"), pv, pk);
        CFRelease(pv);
      }
    }
  }
  if ((cfg->version_mask & HTTP_VERSION_3) && !cfg->http3_fallback) {
    SEL h3_sel = sel_registerName("setAssumesHTTP3Capable:");
    if (((BOOL(*)(id, SEL, SEL))objc_msgSend)(
            nsreq, sel_registerName("respondsToSelector:"), h3_sel)) {
      ((void (*)(id, SEL, BOOL))objc_msgSend)(nsreq, h3_sel, (BOOL)1);
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Copy NSHTTPURLResponse allHeaderFields dictionary into HttpHeaders.
 * @param hdrs Toll-free bridged CFDictionaryRef of response headers.
 * @param out_headers Destination HttpHeaders collection.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
cah_copy_nshttp_headers(CFDictionaryRef hdrs, struct HttpHeaders *out_headers) {
  CFIndex count = CFDictionaryGetCount(hdrs);
  const void **keys;
  const void **vals;
  CFIndex i;

  keys = (const void **)malloc(((size_t)count + 1U) * sizeof(const void *));
  if (!keys) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  vals = (const void **)malloc(((size_t)count + 1U) * sizeof(const void *));
  if (!vals) {
    free((void *)keys);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  CFDictionaryGetKeysAndValues(hdrs, keys, vals);
  for (i = 0; i < count; ++i) {
    char kbuf[512];
    char vbuf[2048];
    kbuf[0] = '\0';
    vbuf[0] = '\0';
    CFStringGetCString((CFStringRef)keys[i], kbuf, (CFIndex)sizeof(kbuf),
                       kCFStringEncodingUTF8);
    CFStringGetCString((CFStringRef)vals[i], vbuf, (CFIndex)sizeof(vbuf),
                       kCFStringEncodingUTF8);
    {
      c_abstract_http_error_t rc = http_headers_add(out_headers, kbuf, vbuf);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        free((void *)keys);
        free((void *)vals);
        return rc;
      }
    }
  }
  free((void *)keys);
  free((void *)vals);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Process a received NSData payload chunk for an NSURLSession task.
 * @param st Active task state.
 * @param data_task Objective-C NSURLSessionDataTask handle.
 * @param data Toll-free bridged CFDataRef chunk.
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error code.
 */
NO_DISCARD static c_abstract_http_error_t
cah_urlsession_handle_data(struct AppleURLSessionState *st, id data_task,
                           CFDataRef data) {
  CFIndex len = CFDataGetLength(data);
  const UInt8 *bytes = CFDataGetBytePtr(data);
  if (st->req->on_chunk) {
    c_abstract_http_error_t cb_rc =
        st->req->on_chunk(st->req->on_chunk_user_data, bytes, (size_t)len);
    if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
      st->error = cb_rc;
      ((void (*)(id, SEL))objc_msgSend)(data_task, sel_registerName("cancel"));
      return cb_rc;
    }
  } else {
    CFDataAppendBytes(st->body_data, bytes, len);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Dispatch an enum-returning helper inside a void Apple callback */
#define APPLE_VOID_CB_DISPATCH(expr)                                           \
  do {                                                                         \
    c_abstract_http_error_t _apple_rc = (expr);                                \
    if (_apple_rc != C_ABSTRACT_HTTP_SUCCESS) {                                \
      LOG_DEBUG("apple_cb: helper returned %d", (int)_apple_rc);               \
    }                                                                          \
  } while (0)

/**
 * @brief Objective-C IMP for URLSession:dataTask:didReceiveData:.
 * @param self Delegate instance.
 * @param _cmd Selector.
 * @param session NSURLSession instance.
 * @param data_task NSURLSessionDataTask instance.
 * @param data NSData chunk.
 */
static void cah_urlsession_did_receive_data(id self, SEL _cmd, id session,
                                            id data_task, id data) {
  struct AppleURLSessionState *st =
      (struct AppleURLSessionState *)(void *)objc_getAssociatedObject(
          data_task, &g_cah_urlsession_state_key);
  (void)self;
  (void)_cmd;
  (void)session;
  APPLE_VOID_CB_DISPATCH(
      cah_urlsession_handle_data(st, data_task, (CFDataRef)data));
}

/**
 * @brief Destroy synchronization primitives, free `batch`, and return
 * `orig_rc`.
 * @param batch Batch to destroy.
 * @param orig_rc Status enum to return.
 * @return `orig_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
apple_destroy_batch_and_return(struct AppleURLSessionBatch *batch,
                               c_abstract_http_error_t orig_rc) {
  pthread_mutex_destroy(&batch->mtx);
  pthread_cond_destroy(&batch->cv);
  free(batch->tasks);
  free(batch);
  return orig_rc;
}

/**
 * @brief Publish all slot results onto `batch->tasks[k].async_future` and free
 * `batch`.
 * @param batch Completed batch structure to publish and destroy.
 * @param orig_rc Status enum to return.
 * @return `orig_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
apple_publish_and_free_batch(struct AppleURLSessionBatch *batch,
                             c_abstract_http_error_t orig_rc) {
  size_t k;
  for (k = 0; k < batch->count; ++k) {
    if (batch->tasks[k].async_future) {
      batch->tasks[k].async_future->response = batch->tasks[k].res;
      batch->tasks[k].async_future->error_code = (int)batch->tasks[k].error;
      batch->tasks[k].async_future->is_ready = 1;
    }
  }
  return apple_destroy_batch_and_return(batch, orig_rc);
}

/**
 * @brief Notify `ModalityEventLoop` or complete batch when the final
 * `NSURLSessionDataTask` finishes.
 * @param batch Batch whose `remaining` counter reached `0`.
 * @param session Shared `NSURLSession` to invalidate.
 * @param sync_wait Non-zero when caller awaits `batch->cv`.
 * @param slot_rc Status of the final completed slot.
 * @return Wakeup error if non-zero, otherwise `slot_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
apple_publish_and_finish_batch(struct AppleURLSessionBatch *batch, id session,
                               int sync_wait, c_abstract_http_error_t slot_rc) {
  size_t k;
  ((void (*)(id, SEL))objc_msgSend)(
      session, sel_registerName("finishTasksAndInvalidate"));

  if (batch->has_pipe) {
    char b = 'd';
    ssize_t nw = write(batch->notify_pipe[1], &b, 1);
    close(batch->notify_pipe[1]);
    return (nw < 0 && slot_rc == C_ABSTRACT_HTTP_SUCCESS)
               ? C_ABSTRACT_HTTP_ERR_IO
               : slot_rc;
  }

  for (k = 0; k < batch->count; ++k) {
    if (batch->tasks[k].async_future) {
      batch->tasks[k].async_future->response = batch->tasks[k].res;
      batch->tasks[k].async_future->error_code = (int)batch->tasks[k].error;
      batch->tasks[k].async_future->is_ready = 1;
    }
  }
  if (batch->loop) {
    c_abstract_http_error_t w_rc = http_loop_wakeup(batch->loop);
    if (w_rc != C_ABSTRACT_HTTP_SUCCESS) {
      if (sync_wait) {
        pthread_mutex_lock(&batch->mtx);
        batch->wake_rc = w_rc;
        batch->batch_done = 1;
        pthread_cond_broadcast(&batch->cv);
        pthread_mutex_unlock(&batch->mtx);
        return w_rc;
      }
      return apple_destroy_batch_and_return(batch, w_rc);
    }
  }
  if (sync_wait) {
    pthread_mutex_lock(&batch->mtx);
    batch->wake_rc = C_ABSTRACT_HTTP_SUCCESS;
    batch->batch_done = 1;
    pthread_cond_broadcast(&batch->cv);
    pthread_mutex_unlock(&batch->mtx);
    return slot_rc;
  }
  return apple_destroy_batch_and_return(batch, slot_rc);
}

/**
 * @brief Finalize a task's response body, flattened request, and error state.
 * @param st Task state to finalize.
 * @return Final status enum for this task slot.
 */
NO_DISCARD static c_abstract_http_error_t
cah_finalize_slot_payload(struct AppleURLSessionState *st) {
  c_abstract_http_error_t rc = st->error;
  c_abstract_http_error_t eff_rc;
  int flat = st->flattened;

  if (rc == C_ABSTRACT_HTTP_SUCCESS && !st->req->on_chunk) {
    CFIndex blen = CFDataGetLength(st->body_data);
    st->res->body = malloc((size_t)blen + 1U);
    if (!st->res->body) {
      rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    } else {
      if (blen > 0) {
        CFDataGetBytes(st->body_data, CFRangeMake(0, blen),
                       (UInt8 *)st->res->body);
      }
      ((char *)st->res->body)[blen] = '\0';
      st->res->body_len = (size_t)blen;
    }
  }
  CFRelease(st->body_data);
  st->body_data = NULL;
  st->flattened = 0;

  eff_rc = apple_cleanup_effective_request(&st->flat_req, flat);
  if (eff_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return apple_cleanup_failed_res(&st->res, eff_rc);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return apple_cleanup_failed_res(&st->res, rc);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Atomically decrement `batch->remaining` and report whether it hit `0`.
 * @param batch Owning batch.
 * @param out_sync_wait Receives `batch->sync_wait` when `remaining == 0`.
 * @return 1 if final task in batch, 0 otherwise.
 */
static int math_decrement_batch_remaining(struct AppleURLSessionBatch *batch,
                                          int *out_sync_wait) {
  int is_last = 0;
  pthread_mutex_lock(&batch->mtx);
  batch->remaining--;
  if (batch->remaining == 0) {
    is_last = 1;
    *out_sync_wait = batch->sync_wait;
  }
  pthread_mutex_unlock(&batch->mtx);
  return is_last;
}

/**
 * @brief Finalize an async multi-batch task slot and wake `ModalityEventLoop`
 * once all concurrent `NSURLSessionDataTask`s finish.
 * @param st Completed task state belonging to `st->batch`.
 * @param session Shared `NSURLSession` to invalidate when `remaining == 0`.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup/wakeup error enum.
 */
NO_DISCARD static c_abstract_http_error_t
cah_urlsession_finish_batch_slot(struct AppleURLSessionState *st, id session) {
  struct AppleURLSessionBatch *batch = st->batch;
  c_abstract_http_error_t slot_rc;
  int sync_wait = 0;

  slot_rc = cah_finalize_slot_payload(st);
  if (slot_rc != C_ABSTRACT_HTTP_SUCCESS) {
    st->error = slot_rc;
    if (math_decrement_batch_remaining(batch, &sync_wait)) {
      return apple_publish_and_finish_batch(batch, session, sync_wait, slot_rc);
    }
    return slot_rc;
  }

  st->error = C_ABSTRACT_HTTP_SUCCESS;
  if (math_decrement_batch_remaining(batch, &sync_wait)) {
    return apple_publish_and_finish_batch(batch, session, sync_wait,
                                          C_ABSTRACT_HTTP_SUCCESS);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Process NSURLSession task completion, status/headers, and wakeup.
 * @param st Active task state.
 * @param session NSURLSession instance to invalidate.
 * @param task Completed NSURLSessionTask instance.
 * @param error Optional NSError / CFErrorRef on failure, or NULL on success.
 * @return C_ABSTRACT_HTTP_SUCCESS or mapped error code.
 */
NO_DISCARD static c_abstract_http_error_t
cah_urlsession_handle_complete(struct AppleURLSessionState *st, id session,
                               id task, CFErrorRef error) {
  c_abstract_http_error_t rc = st->error;
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    if (error) {
      CFIndex code = CFErrorGetCode(error);
      rc = (code == -1001) ? C_ABSTRACT_HTTP_ERR_TIMEOUT
                           : C_ABSTRACT_HTTP_ERR_IO;
      goto finalize_complete;
    } else {
      id resp =
          ((id(*)(id, SEL))objc_msgSend)(task, sel_registerName("response"));
      long status = ((long (*)(id, SEL))objc_msgSend)(
          resp, sel_registerName("statusCode"));
      CFDictionaryRef hdrs = (CFDictionaryRef)((id(*)(id, SEL))objc_msgSend)(
          resp, sel_registerName("allHeaderFields"));
      st->res->status_code = (int)status;
      rc = cah_copy_nshttp_headers(hdrs, &st->res->headers);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        goto finalize_complete;
      }
    }
  finalize_complete:
    st->error = rc;
  }
  if (st->batch) {
    return cah_urlsession_finish_batch_slot(st, session);
  }
  ((void (*)(id, SEL))objc_msgSend)(
      session, sel_registerName("finishTasksAndInvalidate"));
  pthread_mutex_lock(&st->mtx);
  st->done = 1;
  pthread_cond_signal(&st->cv);
  pthread_mutex_unlock(&st->mtx);
  return rc;
}

/**
 * @brief Objective-C IMP for URLSession:task:didCompleteWithError:.
 * @param self Delegate instance.
 * @param _cmd Selector.
 * @param session NSURLSession instance.
 * @param task NSURLSessionTask instance.
 * @param error NSError instance or nil.
 */
static void cah_urlsession_did_complete(id self, SEL _cmd, id session, id task,
                                        id error) {
  struct AppleURLSessionState *st =
      (struct AppleURLSessionState *)(void *)objc_getAssociatedObject(
          task, &g_cah_urlsession_state_key);
  (void)self;
  (void)_cmd;
  APPLE_VOID_CB_DISPATCH(
      cah_urlsession_handle_complete(st, session, task, (CFErrorRef)error));
}

/**
 * @brief Register the CAHURLSessionDelegate Objective-C class implementation.
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
NO_DISCARD static c_abstract_http_error_t
cah_register_urlsession_delegate_impl(void) {
  Class sup = objc_getClass("NSObject");
  Class cls = objc_allocateClassPair(sup, "CAHURLSessionDelegate", 0);
  class_addMethod(cls, sel_registerName("URLSession:dataTask:didReceiveData:"),
                  (IMP)cah_urlsession_did_receive_data, "v@:@@@");
  class_addMethod(cls,
                  sel_registerName("URLSession:task:didCompleteWithError:"),
                  (IMP)cah_urlsession_did_complete, "v@:@@@");
  objc_registerClassPair(cls);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Register the CAHURLSessionDelegate Objective-C class once
 * (pthread_once ABI).
 */
static void cah_register_urlsession_delegate(void) {
  APPLE_VOID_CB_DISPATCH(cah_register_urlsession_delegate_impl());
}

/**
 * @brief Construct an `NSURLSessionDataTask` and populate `st` for single or
 * batch execution.
 * @param cfg Active `HttpConfig`.
 * @param session Owning `NSURLSession` Objective-C object.
 * @param eff_req Effective `HttpRequest` (already multipart-flattened).
 * @param st Task state slot to populate.
 * @param out_task Output `NSURLSessionDataTask` Objective-C object.
 * @return C_ABSTRACT_HTTP_SUCCESS or validation/allocation error enum.
 */
NO_DISCARD static c_abstract_http_error_t
apple_create_nsurlsession_task(const struct HttpConfig *cfg, id session,
                               const struct HttpRequest *eff_req,
                               struct AppleURLSessionState *st, id *out_task) {
  CFStringRef method_cf = NULL;
  CFStringRef url_str;
  CFURLRef url;
  CFDataRef upload_data = NULL;
  id nsreq;
  id task;
  size_t i;
  c_abstract_http_error_t rc;

  rc = apple_method_cfstr(eff_req->method, &method_cf);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  url_str = CFStringCreateWithCString(kCFAllocatorDefault,
                                      eff_req->url ? eff_req->url : "",
                                      kCFStringEncodingUTF8);
  url = CFURLCreateWithString(kCFAllocatorDefault, url_str, NULL);
  CFRelease(url_str);
  if (!url || !eff_req->url || eff_req->url[0] == '\0') {
    if (url) {
      CFRelease(url);
    }
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  st->res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  if (!st->res) {
    CFRelease(url);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = http_response_init(st->res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(st->res);
    st->res = NULL;
    CFRelease(url);
    return rc;
  }

  if (eff_req->read_chunk) {
    CFMutableDataRef mut = CFDataCreateMutable(
        kCFAllocatorDefault, (CFIndex)eff_req->expected_body_len);
    for (;;) {
      UInt8 chunk_buf[8192];
      size_t out_read = 0;
      c_abstract_http_error_t cb_rc =
          eff_req->read_chunk(eff_req->read_chunk_user_data, chunk_buf,
                              sizeof(chunk_buf), &out_read);
      if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        CFRelease(mut);
        CFRelease(url);
        return apple_cleanup_failed_res(&st->res, cb_rc);
      }
      if (out_read == 0) {
        break;
      }
      CFDataAppendBytes(mut, chunk_buf, (CFIndex)out_read);
    }
    upload_data = mut;
  } else if (eff_req->body && eff_req->body_len > 0) {
    upload_data =
        CFDataCreate(kCFAllocatorDefault, (const UInt8 *)eff_req->body,
                     (CFIndex)eff_req->body_len);
  }

  st->req = eff_req;
  st->body_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
  st->error = C_ABSTRACT_HTTP_SUCCESS;

  nsreq = ((id(*)(id, SEL, CFTypeRef))objc_msgSend)(
      ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSMutableURLRequest"),
                                        sel_registerName("alloc")),
      sel_registerName("initWithURL:"), url);
  CFRelease(url);
  ((void (*)(id, SEL, CFTypeRef))objc_msgSend)(
      nsreq, sel_registerName("setHTTPMethod:"), method_cf);

  rc = apple_apply_nsurl_options(cfg, nsreq);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    if (upload_data) {
      CFRelease(upload_data);
    }
    ((void (*)(id, SEL))objc_msgSend)(nsreq, sel_registerName("release"));
    CFRelease(st->body_data);
    st->body_data = NULL;
    return apple_cleanup_failed_res(&st->res, rc);
  }

  for (i = 0; i < eff_req->headers.count; ++i) {
    CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault,
                                              eff_req->headers.headers[i].key,
                                              kCFStringEncodingUTF8);
    CFStringRef v = CFStringCreateWithCString(kCFAllocatorDefault,
                                              eff_req->headers.headers[i].value,
                                              kCFStringEncodingUTF8);
    ((void (*)(id, SEL, CFTypeRef, CFTypeRef))objc_msgSend)(
        nsreq, sel_registerName("setValue:forHTTPHeaderField:"), v, k);
    CFRelease(k);
    CFRelease(v);
  }

  if (upload_data) {
    ((void (*)(id, SEL, CFTypeRef))objc_msgSend)(
        nsreq, sel_registerName("setHTTPBody:"), upload_data);
    CFRelease(upload_data);
  }

  task = ((id(*)(id, SEL, id))objc_msgSend)(
      session, sel_registerName("dataTaskWithRequest:"), nsreq);
  objc_setAssociatedObject(task, &g_cah_urlsession_state_key, (id)(void *)st,
                           OBJC_ASSOCIATION_ASSIGN);
  ((void (*)(id, SEL))objc_msgSend)(nsreq, sel_registerName("release"));
  *out_task = task;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Dispatch a single HTTP request synchronously via NSURLSession.
 * @param ctx Transport context.
 * @param req Request to send.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
http_apple_nsurlsession_send(struct HttpTransportContext *ctx,
                             const struct HttpRequest *req,
                             struct HttpResponse **res) {
  struct AppleURLSessionState st;
  pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t init_cv = PTHREAD_COND_INITIALIZER;
  id pool;
  id del;
  id scfg;
  id session;
  id task = (id)0;
  c_abstract_http_error_t rc;

  memset(&st, 0, sizeof(st));
  st.mtx = init_mtx;
  st.cv = init_cv;

  pthread_once(&g_cah_urlsession_once, cah_register_urlsession_delegate);

  pool = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSAutoreleasePool"),
                                        sel_registerName("alloc")),
      sel_registerName("init"));
  del = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("CAHURLSessionDelegate"),
                                        sel_registerName("alloc")),
      sel_registerName("init"));
  scfg = ((id(*)(Class, SEL))objc_msgSend)(
      objc_getClass("NSURLSessionConfiguration"),
      sel_registerName("ephemeralSessionConfiguration"));
  rc = apple_configure_session_only(&ctx->config, scfg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    ((void (*)(id, SEL))objc_msgSend)(del, sel_registerName("release"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
    pthread_mutex_destroy(&st.mtx);
    pthread_cond_destroy(&st.cv);
    return rc;
  }
  session = ((id(*)(Class, SEL, id, id, id))objc_msgSend)(
      objc_getClass("NSURLSession"),
      sel_registerName("sessionWithConfiguration:delegate:delegateQueue:"),
      scfg, del, NULL);
  ((void (*)(id, SEL))objc_msgSend)(del, sel_registerName("release"));

  rc = apple_create_nsurlsession_task(&ctx->config, session, req, &st, &task);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    ((void (*)(id, SEL))objc_msgSend)(session,
                                      sel_registerName("invalidateAndCancel"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
    pthread_mutex_destroy(&st.mtx);
    pthread_cond_destroy(&st.cv);
    return rc;
  }

  ((void (*)(id, SEL))objc_msgSend)(task, sel_registerName("resume"));
  ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));

  pthread_mutex_lock(&st.mtx);
  while (!st.done) {
    pthread_cond_wait(&st.cv, &st.mtx);
  }
  pthread_mutex_unlock(&st.mtx);
  pthread_mutex_destroy(&st.mtx);
  pthread_cond_destroy(&st.cv);

  *res = st.res;
  rc = cah_finalize_slot_payload(&st);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    *res = NULL;
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief `ModalityEventLoop` kqueue notification pipe callback that detaches
 * `notify_pipe[0]`, publishes all completed `NSURLSessionDataTask` futures, and
 * frees `batch` on the event-loop thread.
 * @param loop Active event loop.
 * @param fd Pipe read descriptor.
 * @param revents Triggered event mask.
 * @param user_data Pointer to `struct AppleURLSessionBatch`.
 * @return C_ABSTRACT_HTTP_SUCCESS or `http_loop_remove_fd` error enum.
 */
NO_DISCARD static c_abstract_http_error_t
apple_multi_pipe_cb(struct ModalityEventLoop *loop, int fd, int revents,
                    void *user_data) {
  struct AppleURLSessionBatch *batch = (struct AppleURLSessionBatch *)user_data;
  char drain[16];
  c_abstract_http_error_t rm_rc;
  (void)revents;
  while (read(fd, drain, sizeof(drain)) > 0) {
  }
  batch->has_pipe = 0;
  rm_rc = http_loop_remove_fd(loop, fd);
  if (rm_rc != C_ABSTRACT_HTTP_SUCCESS) {
    close(fd);
    return apple_publish_and_free_batch(batch, rm_rc);
  }
  close(fd);
  return apple_publish_and_free_batch(batch, C_ABSTRACT_HTTP_SUCCESS);
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_pthread_fail;
extern int g_mock_pthread_create_sync;
#endif

/**
 * @brief Attach optional kqueue notification pipe onto `loop` for a batch.
 * @param batch Batch state receiving `notify_pipe`.
 * @param loop Optional `ModalityEventLoop`.
 * @return C_ABSTRACT_HTTP_SUCCESS or `add_fd` error enum.
 */
NO_DISCARD static c_abstract_http_error_t
apple_try_attach_batch_pipe(struct AppleURLSessionBatch *batch,
                            struct ModalityEventLoop *loop) {
  if (loop && !batch->sync_wait && pipe(batch->notify_pipe) == 0) {
    c_abstract_http_error_t add_rc;
    fcntl(batch->notify_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(batch->notify_pipe[1], F_SETFL, O_NONBLOCK);
    add_rc = http_loop_add_fd(loop, batch->notify_pipe[0], HTTP_LOOP_READ,
                              apple_multi_pipe_cb, batch);
    if (add_rc != C_ABSTRACT_HTTP_SUCCESS) {
      close(batch->notify_pipe[0]);
      close(batch->notify_pipe[1]);
      return (add_rc == C_ABSTRACT_HTTP_ERR_NOTSUP) ? C_ABSTRACT_HTTP_SUCCESS
                                                    : add_rc;
    }
    batch->has_pipe = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Prepare one batch slot into `out_task`, recording validation/setup
 * failures directly into `future`.
 * @param cfg Active `HttpConfig`.
 * @param session Shared `NSURLSession`.
 * @param orig_req Input request pointer (may be NULL).
 * @param st Slot state.
 * @param future Output future slot.
 * @param out_task Output `NSURLSessionDataTask` (`0` if slot failed early).
 * @param fail_fast Non-zero to return per-slot error immediately.
 * @return C_ABSTRACT_HTTP_SUCCESS or cleanup/slot error enum.
 */
NO_DISCARD static c_abstract_http_error_t apple_prepare_batch_slot(
    const struct HttpConfig *cfg, id session,
    const struct HttpRequest *orig_req, struct AppleURLSessionState *st,
    struct HttpFuture *future, id *out_task, int fail_fast) {
  const struct HttpRequest *eff_req = NULL;
  c_abstract_http_error_t rc;

  *out_task = (id)0;
  if (!orig_req) {
    future->response = NULL;
    future->error_code = (int)C_ABSTRACT_HTTP_ERR_INVAL;
    future->is_ready = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  rc = apple_prepare_effective_request(orig_req, &st->flat_req, &st->flattened,
                                       &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    future->response = NULL;
    future->error_code = (int)rc;
    future->is_ready = 1;
    return fail_fast ? rc : C_ABSTRACT_HTTP_SUCCESS;
  }

  rc = apple_create_nsurlsession_task(cfg, session, eff_req, st, out_task);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t cl_rc;
    int flat = st->flattened;
    st->flattened = 0;
    cl_rc = apple_cleanup_effective_request(&st->flat_req, flat);
    if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
      future->response = NULL;
      future->error_code = (int)cl_rc;
      future->is_ready = 1;
      return cl_rc;
    }
    future->response = NULL;
    future->error_code = (int)rc;
    future->is_ready = 1;
    return fail_fast ? rc : C_ABSTRACT_HTTP_SUCCESS;
  }

  st->async_future = future;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Tear down an unlaunched `AppleURLSessionBatch`, detaching pipe and
 * waking `loop`.
 * @param batch Batch to free.
 * @param session Shared `NSURLSession` to cancel (may be nil).
 * @param pool `NSAutoreleasePool` to drain.
 * @param tasks_to_resume Temporary task array to free.
 * @param orig_rc Primary status enum to return if cleanup succeeds.
 * @return Cleanup error if pipe removal/wakeup fails, otherwise `orig_rc`.
 */
NO_DISCARD static c_abstract_http_error_t
apple_abort_empty_batch(struct AppleURLSessionBatch *batch, id session, id pool,
                        id *tasks_to_resume, c_abstract_http_error_t orig_rc) {
  struct ModalityEventLoop *loop = batch->loop;
  if (session) {
    ((void (*)(id, SEL))objc_msgSend)(session,
                                      sel_registerName("invalidateAndCancel"));
  }
  ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
  free(tasks_to_resume);
  if (batch->has_pipe) {
    int rfd = batch->notify_pipe[0];
    int wfd = batch->notify_pipe[1];
    c_abstract_http_error_t rm_rc;
    batch->has_pipe = 0;
    rm_rc = http_loop_remove_fd(loop, rfd);
    if (rm_rc != C_ABSTRACT_HTTP_SUCCESS) {
      close(rfd);
      close(wfd);
      return apple_destroy_batch_and_return(batch, rm_rc);
    }
    close(rfd);
    close(wfd);
  }
  if (loop && orig_rc == C_ABSTRACT_HTTP_SUCCESS) {
    c_abstract_http_error_t wk_rc = http_loop_wakeup(loop);
    if (wk_rc != C_ABSTRACT_HTTP_SUCCESS) {
      return apple_destroy_batch_and_return(batch, wk_rc);
    }
  }
  return apple_destroy_batch_and_return(batch, orig_rc);
}

/**
 * @brief Multiplex a batch of requests concurrently and non-blockingly over a
 * single shared `NSURLSession` bridged to `ModalityEventLoop` (`kqueue`).
 * @param ctx Transport context.
 * @param loop Optional `ModalityEventLoop` to register/wake.
 * @param multi Multi-request batch.
 * @param futures Array of `HttpFuture` pointers.
 * @return C_ABSTRACT_HTTP_SUCCESS or allocation/I/O error enum.
 */
NO_DISCARD static c_abstract_http_error_t apple_nsurlsession_send_multi_async(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  struct AppleURLSessionBatch *batch;
  pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t init_cv = PTHREAD_COND_INITIALIZER;
  id pool;
  id del;
  id scfg;
  id session;
  id *tasks_to_resume = NULL;
  size_t launched = 0;
  size_t i;
  int sync_wait = 0;
  c_abstract_http_error_t rc;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_pthread_fail) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  sync_wait = g_mock_pthread_create_sync ? 1 : 0;
#endif

  if (multi->count == 0) {
    if (loop) {
      return http_loop_wakeup(loop);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  batch = (struct AppleURLSessionBatch *)calloc(1, sizeof(*batch));
  if (!batch) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  batch->tasks = (struct AppleURLSessionState *)calloc(
      multi->count, sizeof(struct AppleURLSessionState));
  if (!batch->tasks) {
    free(batch);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  tasks_to_resume = (id *)calloc(multi->count, sizeof(id));
  if (!tasks_to_resume) {
    free(batch->tasks);
    free(batch);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  batch->loop = loop;
  batch->count = multi->count;
  batch->mtx = init_mtx;
  batch->cv = init_cv;
  batch->sync_wait = sync_wait;

  rc = apple_try_attach_batch_pipe(batch, loop);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(tasks_to_resume);
    return apple_destroy_batch_and_return(batch, rc);
  }

  pthread_once(&g_cah_urlsession_once, cah_register_urlsession_delegate);

  pool = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSAutoreleasePool"),
                                        sel_registerName("alloc")),
      sel_registerName("init"));
  del = ((id(*)(id, SEL))objc_msgSend)(
      ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("CAHURLSessionDelegate"),
                                        sel_registerName("alloc")),
      sel_registerName("init"));
  scfg = ((id(*)(Class, SEL))objc_msgSend)(
      objc_getClass("NSURLSessionConfiguration"),
      sel_registerName("ephemeralSessionConfiguration"));
  rc = apple_configure_session_only(&ctx->config, scfg);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    ((void (*)(id, SEL))objc_msgSend)(del, sel_registerName("release"));
    return apple_abort_empty_batch(batch, (id)0, pool, tasks_to_resume, rc);
  }
  session = ((id(*)(Class, SEL, id, id, id))objc_msgSend)(
      objc_getClass("NSURLSession"),
      sel_registerName("sessionWithConfiguration:delegate:delegateQueue:"),
      scfg, del, NULL);
  ((void (*)(id, SEL))objc_msgSend)(del, sel_registerName("release"));

  for (i = 0; i < multi->count; ++i) {
    id task = (id)0;
    batch->tasks[i].batch = batch;
    rc = apple_prepare_batch_slot(&ctx->config, session, multi->requests[i],
                                  &batch->tasks[i], futures[i], &task, 0);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return apple_abort_empty_batch(batch, session, pool, tasks_to_resume, rc);
    }
    if (task != (id)0) {
      tasks_to_resume[launched++] = task;
    }
  }

  if (launched == 0) {
    return apple_abort_empty_batch(batch, session, pool, tasks_to_resume,
                                   C_ABSTRACT_HTTP_SUCCESS);
  }

  batch->remaining = launched;
  for (i = 0; i < launched; ++i) {
    ((void (*)(id, SEL))objc_msgSend)(tasks_to_resume[i],
                                      sel_registerName("resume"));
  }
  ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
  free(tasks_to_resume);

  if (sync_wait) {
    pthread_mutex_lock(&batch->mtx);
    while (!batch->batch_done) {
      pthread_cond_wait(&batch->cv, &batch->mtx);
    }
    pthread_mutex_unlock(&batch->mtx);
    pthread_mutex_destroy(&batch->mtx);
    pthread_cond_destroy(&batch->cv);
    free(batch->tasks);
    free(batch);
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

/* ========================================================================== */
/* Legacy CFReadStreamCreateForHTTPRequest Backend (< macOS 10.11)            */
/* ========================================================================== */

#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM

/** @brief Internal Apple CFStream request state */
struct AppleReqState {
  const struct HttpRequest *req; /**< Request pointer */
  struct HttpResponse **res;     /**< Response output pointer */
  CFMutableDataRef bodyData;     /**< Response body buffer */
  c_abstract_http_error_t error; /**< Error code */
  int done;                      /**< Completion flag */
  CFRunLoopRef runloop;          /**< CFRunLoop reference */
  int *pending_count;            /**< Pending request counter */
};

/**
 * @brief Extract HTTP status code and buffered body from a CFReadStream.
 * @param state Stream request state.
 * @param readStream Completed CFReadStreamRef.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t
apple_extract_response(struct AppleReqState *state,
                       CFReadStreamRef readStream) {
  CFHTTPMessageRef responseRef;
  CFTypeRef propRef;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (strstr(state->req->url, "fail_extract_response") != NULL) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif

  propRef =
      CFReadStreamCopyProperty(readStream, kCFStreamPropertyHTTPResponseHeader);
  responseRef = (CFHTTPMessageRef)(size_t)propRef;
  if (responseRef) {
    (*(state->res))->status_code =
        (int)CFHTTPMessageGetResponseStatusCode(responseRef);
    CFRelease(responseRef);
  }

  if (state->bodyData) {
    CFIndex len = CFDataGetLength(state->bodyData);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    (*(state->res))->body =
        (strcmp(state->req->url, "http://fail_body_malloc") == 0)
            ? NULL
            : malloc((size_t)len + 1);
#else
    (*(state->res))->body = malloc((size_t)len + 1);
#endif
    if (!(*(state->res))->body) {
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    CFDataGetBytes(state->bodyData, CFRangeMake(0, len),
                   (UInt8 *)(*(state->res))->body);
    ((char *)(*(state->res))->body)[len] = '\0';
    (*(state->res))->body_len = (size_t)len;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Forward a CFStream chunk to the caller's on_chunk callback.
 * @param state Stream request state.
 * @param buf Chunk buffer.
 * @param bytesRead Number of bytes read.
 * @return C_ABSTRACT_HTTP_SUCCESS or callback error code.
 */
NO_DISCARD static c_abstract_http_error_t
apple_stream_handle_chunk(struct AppleReqState *state, const UInt8 *buf,
                          CFIndex bytesRead) {
  c_abstract_http_error_t cb_rc = state->req->on_chunk(
      state->req->on_chunk_user_data, buf, (size_t)bytesRead);
  if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
    state->error = cb_rc;
    state->done = 1;
    return cb_rc;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (strcmp(state->req->url, "http://fail_cb_rc") == 0) {
    state->error = C_ABSTRACT_HTTP_ERR_NOMEM;
    state->done = 1;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief CFReadStream event callback trampoline.
 * @param stream Active CFReadStreamRef.
 * @param type CFStream event mask.
 * @param clientCallBackInfo Pointer to struct AppleReqState.
 */
static void apple_stream_cb(CFReadStreamRef stream, CFStreamEventType type,
                            void *clientCallBackInfo) {
  struct AppleReqState *state = (struct AppleReqState *)clientCallBackInfo;
  if (!state || state->done)
    return;

  if (type == kCFStreamEventHasBytesAvailable) {
    UInt8 buf[8192];
    CFIndex bytesRead = CFReadStreamRead(stream, buf, sizeof(buf));
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(state->req->url, "http://fail_cb_rc") == 0 ||
        strcmp(state->req->url, "http://fail_body_alloc") == 0) {
      bytesRead = 1;
    }
    if (strcmp(state->req->url, "http://fail_read_negative") == 0) {
      bytesRead = -1;
    }
    if (strcmp(state->req->url, "http://fail_after_body") == 0) {
      state->bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
      state->error = C_ABSTRACT_HTTP_ERR_IO;
      state->done = 1;
      bytesRead = 0;
    }
#endif
    if (bytesRead < 0) {
      state->error = C_ABSTRACT_HTTP_ERR_IO;
      state->done = 1;
    } else if (bytesRead > 0) {
      if (state->req->on_chunk) {
        APPLE_VOID_CB_DISPATCH(
            apple_stream_handle_chunk(state, buf, bytesRead));
      } else {
        if (!state->bodyData) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
          state->bodyData =
              (strcmp(state->req->url, "http://fail_body_alloc") == 0)
                  ? NULL
                  : CFDataCreateMutable(kCFAllocatorDefault, 0);
#else
          state->bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
#endif
        }
        if (state->bodyData) {
          CFDataAppendBytes(state->bodyData, buf, bytesRead);
        } else {
          state->error = C_ABSTRACT_HTTP_ERR_NOMEM;
          state->done = 1;
        }
      }
    }
  } else if (type == kCFStreamEventErrorOccurred) {
    state->error = C_ABSTRACT_HTTP_ERR_IO;
    state->done = 1;
  } else {
    state->done = 1;
  }

  if (state->done) {
    CFReadStreamUnscheduleFromRunLoop(stream, state->runloop,
                                      kCFRunLoopCommonModes);
    if (state->pending_count) {
      (*state->pending_count)--;
      if (*state->pending_count <= 0) {
        CFRunLoopStop(state->runloop);
      }
    } else {
      CFRunLoopStop(state->runloop);
    }
  }
}

/**
 * @brief Dispatch a single HTTP request synchronously via legacy CFStream.
 * @param ctx Transport context.
 * @param req Request to send.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
http_apple_cfstream_send(struct HttpTransportContext *ctx,
                         const struct HttpRequest *req,
                         struct HttpResponse **res) {
  CFURLRef url;
  CFStringRef urlStr;
  CFStringRef method = NULL;
  CFHTTPMessageRef requestRef;
  CFReadStreamRef readStream;
  size_t i;
  c_abstract_http_error_t rc;

  rc = apple_method_cfstr(req->method, &method);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  *res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  if (!*res) {
    LOG_DEBUG("http_apple_send: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    return rc;
  }

  urlStr = CFStringCreateWithCString(kCFAllocatorDefault, req->url,
                                     kCFStringEncodingUTF8);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (strcmp(req->url, "http://fail_url_str") == 0) {
    CFRelease(urlStr);
    LOG_DEBUG("http_apple_send: Error urlStr is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#endif

  url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
  CFRelease(urlStr);
  if (!url) {
    LOG_DEBUG("http_apple_send: Error url is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (strcmp(req->url, "http://fail_url") == 0) {
    CFRelease(url);
    LOG_DEBUG("http_apple_send: Error url is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#endif

  requestRef = CFHTTPMessageCreateRequest(kCFAllocatorDefault, method, url,
                                          kCFHTTPVersion1_1);
  CFRelease(url);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (strcmp(req->url, "http://fail_request_ref") == 0) {
    CFRelease(requestRef);
    requestRef = NULL;
  }
#endif
  if (!requestRef) {
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  for (i = 0; i < req->headers.count; ++i) {
    CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault,
                                                req->headers.headers[i].key,
                                                kCFStringEncodingUTF8);
    CFStringRef val = CFStringCreateWithCString(kCFAllocatorDefault,
                                                req->headers.headers[i].value,
                                                kCFStringEncodingUTF8);
    CFHTTPMessageSetHeaderFieldValue(requestRef, key, val);
    CFRelease(key);
    CFRelease(val);
  }

  if (req->read_chunk) {
    CFMutableDataRef mutableBodyData = CFDataCreateMutable(
        kCFAllocatorDefault, (CFIndex)req->expected_body_len);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_mutable_data") == 0) {
      CFRelease(mutableBodyData);
      mutableBodyData = NULL;
    }
#endif
    if (!mutableBodyData) {
      CFRelease(requestRef);
      free(*res);
      *res = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    for (;;) {
      c_abstract_http_error_t cb_rc;
      UInt8 chunkBuf[8192];
      size_t out_read = 0;
      cb_rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                              sizeof(chunkBuf), &out_read);
      if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        CFRelease(mutableBodyData);
        CFRelease(requestRef);
        free(*res);
        *res = NULL;
        return cb_rc;
      }
      if (out_read == 0)
        break; /* EOF */

      CFDataAppendBytes(mutableBodyData, chunkBuf, (CFIndex)out_read);
    }

    CFHTTPMessageSetBody(requestRef, mutableBodyData);
    CFRelease(mutableBodyData);
  } else if (req->body) {
    if (req->body_len > 0) {
      CFDataRef body =
          CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                       (CFIndex)req->body_len);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
      if (strcmp(req->url, "http://fail_body_data") == 0) {
        CFRelease(body);
        body = NULL;
      }
#endif
      if (!body) {
        CFRelease(requestRef);
        free(*res);
        *res = NULL;
        return C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      CFHTTPMessageSetBody(requestRef, body);
      CFRelease(body);
    }
  }

  readStream =
      CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, requestRef);
  CFRelease(requestRef);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (strcmp(req->url, "http://fail_read_stream") == 0) {
    CFRelease(readStream);
    readStream = NULL;
  }
#endif
  if (!readStream)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  if (!ctx->config.verify_peer) {
    CFMutableDictionaryRef sslSettings = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(sslSettings, kCFStreamSSLValidatesCertificateChain,
                         kCFBooleanFalse);
    CFReadStreamSetProperty(readStream, kCFStreamPropertySSLSettings,
                            sslSettings);
    CFRelease(sslSettings);
  }

  {
    struct AppleReqState state;
    CFStreamClientContext clientContext;
    c_abstract_http_error_t ext_rc;
    memset(&clientContext, 0, sizeof(clientContext));
    memset(&state, 0, sizeof(state));
    state.req = req;
    state.res = res;
    state.runloop = CFRunLoopGetCurrent();

    clientContext.info = &state;

    CFReadStreamSetClient(readStream,
                          kCFStreamEventHasBytesAvailable |
                              kCFStreamEventErrorOccurred |
                              kCFStreamEventEndEncountered,
                          apple_stream_cb, &clientContext);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_stream_client") == 0) {
      CFRelease(readStream);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#endif

    CFReadStreamScheduleWithRunLoop(readStream, state.runloop,
                                    kCFRunLoopCommonModes);

    CFReadStreamOpen(readStream);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_read_stream_open") == 0) {
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      CFRelease(readStream);
      return C_ABSTRACT_HTTP_ERR_IO;
    }

    if (strcmp(req->url, "http://fail_cb_rc") == 0) {
      apple_stream_cb(readStream, kCFStreamEventHasBytesAvailable, &state);
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return state.error;
    }
    if (strcmp(req->url, "http://fail_cb_edges") == 0) {
      apple_stream_cb(readStream, kCFStreamEventHasBytesAvailable, NULL);
      state.done = 1;
      apple_stream_cb(readStream, kCFStreamEventHasBytesAvailable, &state);
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (strcmp(req->url, "http://fail_after_body") == 0 ||
        strcmp(req->url, "http://fail_read_negative") == 0 ||
        strcmp(req->url, "http://fail_body_alloc") == 0) {
      apple_stream_cb(readStream, kCFStreamEventHasBytesAvailable, &state);
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      if (state.bodyData)
        CFRelease(state.bodyData);
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return state.error;
    }

    if (strcmp(req->url, "http://fail_runloop_with_body") == 0) {
      state.bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
      state.error = C_ABSTRACT_HTTP_ERR_IO;
    } else if (strcmp(req->url, "http://fail_body_malloc") == 0) {
      state.bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
    } else if (strcmp(req->url, "http://fail_extract_response") == 0) {
      /* Bypass runloop to test extract_response failure */
    } else
#endif
    {
      CFRunLoopRun();
    }

    if (state.error) {
      if (state.bodyData)
        CFRelease(state.bodyData);
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return state.error;
    }

    ext_rc = apple_extract_response(&state, readStream);
    if (ext_rc != C_ABSTRACT_HTTP_SUCCESS) {
      if (state.bodyData)
        CFRelease(state.bodyData);
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return ext_rc;
    }

    if (state.bodyData)
      CFRelease(state.bodyData);
  }

  CFReadStreamClose(readStream);
  CFRelease(readStream);

  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Extract multi-request stream response and record error in state.
 * @param st Per-request state.
 * @param stream Completed CFReadStreamRef.
 * @return C_ABSTRACT_HTTP_SUCCESS or extraction error code.
 */
NO_DISCARD static c_abstract_http_error_t
apple_multi_extract_into(struct AppleReqState *st, CFReadStreamRef stream) {
  c_abstract_http_error_t ext_rc = apple_extract_response(st, stream);
  if (ext_rc != C_ABSTRACT_HTTP_SUCCESS) {
    st->error = ext_rc;
    return ext_rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Multi-request worker implementation using legacy CFStream.
 * @param wctx Worker context (freed before returning).
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
NO_DISCARD static c_abstract_http_error_t
apple_cfstream_multi_worker_impl(struct AppleMultiWorkerCtx *wctx) {
  size_t i;
  int pending;
  struct AppleReqState *states = (struct AppleReqState *)calloc(
      wctx->multi->count, sizeof(struct AppleReqState));
  CFReadStreamRef *streams =
      (CFReadStreamRef *)calloc(wctx->multi->count, sizeof(CFReadStreamRef));

  pending = (int)wctx->multi->count;

  if (!states || !streams) {
    if (states)
      free(states);
    if (streams)
      free(streams);
    free(wctx);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  for (i = 0; i < wctx->multi->count; ++i) {
    const struct HttpRequest *req = wctx->multi->requests[i];
    struct HttpResponse *res =
        (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
    CFURLRef url;
    CFStringRef urlStr;
    CFStringRef method = NULL;
    CFHTTPMessageRef requestRef;
    CFStreamClientContext clientContext;
    c_abstract_http_error_t m_rc;

    if (!res) {
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM;
      pending--;
      continue;
    }

    m_rc = http_response_init(res);
    if (m_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(res);
      free(states);
      free(streams);
      free(wctx);
      return m_rc;
    }

    wctx->futures[i]->response = res;
    states[i].req = req;
    states[i].res = &wctx->futures[i]->response;
    states[i].runloop = CFRunLoopGetCurrent();
    states[i].pending_count = &pending;

    m_rc = apple_method_cfstr(req->method, &method);
    if (m_rc != C_ABSTRACT_HTTP_SUCCESS) {
      states[i].error = m_rc;
      pending--;
      goto next_cf_item;
    }

    urlStr = CFStringCreateWithCString(kCFAllocatorDefault, req->url,
                                       kCFStringEncodingUTF8);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_multi_url_str") == 0) {
      CFRelease(urlStr);
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL;
      pending--;
      continue;
    }
#endif

    url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
    CFRelease(urlStr);
    if (!url) {
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL;
      pending--;
      continue;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_multi_url") == 0) {
      CFRelease(url);
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL;
      pending--;
      continue;
    }
#endif

    requestRef = CFHTTPMessageCreateRequest(kCFAllocatorDefault, method, url,
                                            kCFHTTPVersion1_1);
    CFRelease(url);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_multi_request_ref") == 0) {
      CFRelease(requestRef);
      requestRef = NULL;
    }
#endif
    if (!requestRef) {
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM;
      pending--;
      continue;
    }

    {
      size_t j;
      for (j = 0; j < req->headers.count; ++j) {
        CFStringRef key = CFStringCreateWithCString(kCFAllocatorDefault,
                                                    req->headers.headers[j].key,
                                                    kCFStringEncodingUTF8);
        CFStringRef val = CFStringCreateWithCString(
            kCFAllocatorDefault, req->headers.headers[j].value,
            kCFStringEncodingUTF8);
        CFHTTPMessageSetHeaderFieldValue(requestRef, key, val);
        CFRelease(key);
        CFRelease(val);
      }
    }

    if (req->body && req->body_len > 0) {
      CFDataRef body =
          CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                       (CFIndex)req->body_len);
      CFHTTPMessageSetBody(requestRef, body);
      CFRelease(body);
    }

    streams[i] =
        CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, requestRef);
    CFRelease(requestRef);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_multi_stream") == 0) {
      CFRelease(streams[i]);
      streams[i] = NULL;
    }
#endif
    if (!streams[i]) {
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM;
      pending--;
      continue;
    }

    if (!wctx->ctx->config.verify_peer) {
      CFMutableDictionaryRef sslSettings = CFDictionaryCreateMutable(
          kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks);
      CFDictionarySetValue(sslSettings, kCFStreamSSLValidatesCertificateChain,
                           kCFBooleanFalse);
      CFReadStreamSetProperty(streams[i], kCFStreamPropertySSLSettings,
                              sslSettings);
      CFRelease(sslSettings);
    }

    memset(&clientContext, 0, sizeof(clientContext));
    clientContext.info = &states[i];

    CFReadStreamSetClient(streams[i],
                          kCFStreamEventHasBytesAvailable |
                              kCFStreamEventErrorOccurred |
                              kCFStreamEventEndEncountered,
                          apple_stream_cb, &clientContext);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_multi_client") == 0) {
      states[i].error = C_ABSTRACT_HTTP_ERR_IO;
      CFRelease(streams[i]);
      streams[i] = NULL;
      pending--;
      continue;
    }
#endif

    CFReadStreamScheduleWithRunLoop(streams[i], states[i].runloop,
                                    kCFRunLoopCommonModes);

    CFReadStreamOpen(streams[i]);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (strcmp(req->url, "http://fail_multi_open") == 0) {
      CFReadStreamUnscheduleFromRunLoop(streams[i], states[i].runloop,
                                        kCFRunLoopCommonModes);
      CFRelease(streams[i]);
      streams[i] = NULL;
      states[i].error = C_ABSTRACT_HTTP_ERR_IO;
      pending--;
      continue;
    }
#endif
  next_cf_item:;
  }

  if (pending > 0) {
    CFRunLoopRun();
  }

  {
    c_abstract_http_error_t ext_rc = C_ABSTRACT_HTTP_SUCCESS;
    c_abstract_http_error_t batch_ext_rc = C_ABSTRACT_HTTP_SUCCESS;
    for (i = 0; i < wctx->multi->count; ++i) {
      if (streams[i]) {
        CFReadStreamClose(streams[i]);
        if (!states[i].error) {
          ext_rc = apple_multi_extract_into(&states[i], streams[i]);
          if (ext_rc != C_ABSTRACT_HTTP_SUCCESS) {
            batch_ext_rc = ext_rc;
            goto after_cf_extract;
          }
        }
      after_cf_extract:
        CFRelease(streams[i]);
      }
      if (states[i].bodyData) {
        CFRelease(states[i].bodyData);
      }

      wctx->futures[i]->error_code = states[i].error;
      wctx->futures[i]->is_ready = 1;
    }

    if (wctx->loop) {
      c_abstract_http_error_t wake_rc = http_loop_wakeup(wctx->loop);
      if (wake_rc != C_ABSTRACT_HTTP_SUCCESS) {
        LOG_DEBUG("http_loop_wakeup failed in apple_multi_worker");
        free(states);
        free(streams);
        free(wctx);
        return wake_rc;
      }
    }

    free(states);
    free(streams);
    free(wctx);
    if (batch_ext_rc != C_ABSTRACT_HTTP_SUCCESS) {
      ext_rc = batch_ext_rc;
      return ext_rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM */

/* ========================================================================== */
/* Public Apple Transport Interface                                           */
/* ========================================================================== */

NO_DISCARD c_abstract_http_error_t http_apple_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t http_apple_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_apple_context_init(struct HttpTransportContext **ctx) {
  LOG_DEBUG("http_apple_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_apple_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *ctx = (struct HttpTransportContext *)malloc(
      sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_apple_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  {
    c_abstract_http_error_t rc = http_config_init(&(*ctx)->config);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*ctx);
      *ctx = NULL;
      return rc;
    }
  }
  (*ctx)->use_nsurlsession = C_ABSTRACT_HTTP_APPLE_DEFAULT_NSURLSESSION;
#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  pthread_mutex_init(&(*ctx)->cf_worker_lock, NULL);
  pthread_cond_init(&(*ctx)->cf_worker_cond, NULL);
  (*ctx)->active_cf_workers = 0;
#endif

  LOG_DEBUG("http_apple_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_apple_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    c_abstract_http_error_t rch;
#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
    pthread_mutex_lock(&ctx->cf_worker_lock);
    while (ctx->active_cf_workers > 0) {
      pthread_cond_wait(&ctx->cf_worker_cond, &ctx->cf_worker_lock);
    }
    pthread_mutex_unlock(&ctx->cf_worker_lock);
    pthread_cond_destroy(&ctx->cf_worker_cond);
    pthread_mutex_destroy(&ctx->cf_worker_lock);
#endif
    rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    free(ctx);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Replace a heap-duplicated string field in `HttpConfig`.
 * @param dst Destination string pointer.
 * @param src Optional source string to duplicate.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD static c_abstract_http_error_t apple_replace_str(char **dst,
                                                            const char *src) {
  free(*dst);
  *dst = NULL;
  if (!src) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return c_abstract_http_strdup(src, dst);
}

NO_DISCARD c_abstract_http_error_t http_apple_config_apply(
    struct HttpTransportContext *ctx, const struct HttpConfig *config) {
  c_abstract_http_error_t rc;
  LOG_DEBUG("http_apple_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_apple_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->proxy_url && strstr(config->proxy_url, "://") == NULL) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  rc = apple_replace_str(&ctx->config.user_agent, config->user_agent);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = apple_replace_str(&ctx->config.proxy_url, config->proxy_url);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = apple_replace_str(&ctx->config.proxy_username, config->proxy_username);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = apple_replace_str(&ctx->config.proxy_password, config->proxy_password);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  ctx->config.verify_peer = config->verify_peer ? 1 : 0;
  ctx->config.verify_host = config->verify_host ? 1 : 0;
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.connect_timeout_ms = config->connect_timeout_ms;
  ctx->config.read_timeout_ms = config->read_timeout_ms;
  ctx->config.write_timeout_ms = config->write_timeout_ms;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.cookie_jar = config->cookie_jar;
  ctx->config.version_mask = config->version_mask;
  ctx->config.tls_version_mask = config->tls_version_mask;
  ctx->config.http3_fallback = config->http3_fallback;
  LOG_DEBUG("http_apple_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD c_abstract_http_error_t
http_apple_set_use_nsurlsession(struct HttpTransportContext *ctx, int enabled) {
  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if !C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  if (!enabled) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
#endif
  ctx->use_nsurlsession = enabled ? 1 : 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up flattened request scratch state after `http_apple_send`.
 * @param flat_req Scratch flattened request.
 * @param flattened Non-zero when `flat_req` owns allocations.
 * @param res In/out response pointer (freed on cleanup failure).
 * @param orig_rc Primary send result.
 * @return Final status enum.
 */
NO_DISCARD static c_abstract_http_error_t
apple_finish_send(struct HttpRequest *flat_req, int flattened,
                  struct HttpResponse **res, c_abstract_http_error_t orig_rc) {
  c_abstract_http_error_t cl_rc;
  cl_rc = apple_cleanup_effective_request(flat_req, flattened);
  if (cl_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return apple_cleanup_failed_res(res, cl_rc);
  }
  return orig_rc;
}

NO_DISCARD c_abstract_http_error_t
http_apple_send(struct HttpTransportContext *ctx, const struct HttpRequest *req,
                struct HttpResponse **res) {
  struct HttpRequest flat_req;
  const struct HttpRequest *eff_req = NULL;
  int flattened = 0;
  c_abstract_http_error_t rc;

  LOG_DEBUG("http_apple_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_apple_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *res = NULL;
  rc = apple_prepare_effective_request(req, &flat_req, &flattened, &eff_req);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  if (!ctx->use_nsurlsession) {
    rc = http_apple_cfstream_send(ctx, eff_req, res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return apple_finish_send(&flat_req, flattened, res, rc);
    }
    return apple_finish_send(&flat_req, flattened, res,
                             C_ABSTRACT_HTTP_SUCCESS);
  }
#endif
  rc = http_apple_nsurlsession_send(ctx, eff_req, res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return apple_finish_send(&flat_req, flattened, res, rc);
  }
  return apple_finish_send(&flat_req, flattened, res, C_ABSTRACT_HTTP_SUCCESS);
}

#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
/**
 * @brief Background worker entry point dispatching legacy CFStream batch.
 * @param arg Pointer to struct AppleMultiWorkerCtx.
 * @return NULL.
 */
static void *apple_multi_worker(void *arg) {
  struct AppleMultiWorkerCtx *wctx = (struct AppleMultiWorkerCtx *)arg;
  struct HttpTransportContext *parent_ctx = wctx->ctx;
  c_abstract_http_error_t rc = apple_cfstream_multi_worker_impl(wctx);
  pthread_mutex_lock(&parent_ctx->cf_worker_lock);
  parent_ctx->active_cf_workers--;
  if (parent_ctx->active_cf_workers == 0) {
    pthread_cond_broadcast(&parent_ctx->cf_worker_cond);
  }
  pthread_mutex_unlock(&parent_ctx->cf_worker_lock);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("apple_multi_worker: completed with %d", (int)rc);
    return (void *)((size_t)(int)rc * 0);
  }
  return NULL;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Thread spawn wrapper supporting deterministic synchronous test
 * execution */
#define PTHREAD_CREATE_APPLE(a, b, c, d)                                       \
  (g_mock_pthread_create_sync ? ((*(void *(*)(void *))(c))(d), 0)              \
                              : pthread_create(a, b, c, d))
#else
/** @brief Thread spawn wrapper */
#define PTHREAD_CREATE_APPLE pthread_create
#endif
#endif /* C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM */

NO_DISCARD c_abstract_http_error_t http_apple_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  LOG_DEBUG("http_apple_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_apple_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (ctx->use_nsurlsession) {
    return apple_nsurlsession_send_multi_async(ctx, loop, multi, futures);
  }

#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  {
    pthread_t thread;
    struct AppleMultiWorkerCtx *wctx = (struct AppleMultiWorkerCtx *)malloc(
        sizeof(struct AppleMultiWorkerCtx));
    if (!wctx) {
      LOG_DEBUG("http_apple_send_multi: Error ENOMEM");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    wctx->ctx = ctx;
    wctx->loop = loop;
    wctx->multi = multi;
    wctx->futures = futures;

    pthread_mutex_lock(&ctx->cf_worker_lock);
    ctx->active_cf_workers++;
    pthread_mutex_unlock(&ctx->cf_worker_lock);

    if (PTHREAD_CREATE_APPLE(&thread, NULL, apple_multi_worker, wctx) != 0) {
      LOG_DEBUG("http_apple_send_multi: Error pthread_create failed");
      pthread_mutex_lock(&ctx->cf_worker_lock);
      ctx->active_cf_workers--;
      pthread_mutex_unlock(&ctx->cf_worker_lock);
      free(wctx);
      return C_ABSTRACT_HTTP_ERR_IO;
    }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    if (!g_mock_pthread_create_sync)
#endif
      pthread_detach(thread);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#else
  return C_ABSTRACT_HTTP_ERR_NOTSUP;
#endif
}

#endif /* defined(__APPLE__) */
