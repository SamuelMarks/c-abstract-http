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
#include <AvailabilityMacros.h>
#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <pthread.h>
#endif
/* clang-format on */

#if defined(__APPLE__)

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
static enum c_abstract_http_error apple_method_cfstr(enum HttpMethod method,
                                                     CFStringRef *out) {
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
static enum c_abstract_http_error
apple_cleanup_failed_res(struct HttpResponse **res,
                         enum c_abstract_http_error orig_rc) {
  enum c_abstract_http_error free_rc = http_response_free(*res);
  free(*res);
  *res = NULL;
  if (free_rc != C_ABSTRACT_HTTP_SUCCESS) {
    return free_rc;
  }
  return orig_rc;
}

/* ========================================================================== */
/* Modern Apple NSURLSession Pure-C89 Objective-C Runtime Bridge              */
/* ========================================================================== */

/** @brief Unique address key for Objective-C associated delegate state */
static char g_cah_urlsession_state_key = 0;

/** @brief One-time delegate class registration guard */
static pthread_once_t g_cah_urlsession_once = PTHREAD_ONCE_INIT;

/** @brief Synchronization and buffer state for an NSURLSession data task */
struct AppleURLSessionState {
  const struct HttpRequest *req;    /**< Originating HTTP request */
  struct HttpResponse *res;         /**< Output HTTP response */
  CFMutableDataRef body_data;       /**< Buffered response payload */
  enum c_abstract_http_error error; /**< Recorded transport/callback error */
  int done;                         /**< Task completion flag */
  pthread_mutex_t mtx;              /**< Completion mutex */
  pthread_cond_t cv;                /**< Completion condition variable */
};

/**
 * @brief Copy NSHTTPURLResponse allHeaderFields dictionary into HttpHeaders.
 * @param hdrs Toll-free bridged CFDictionaryRef of response headers.
 * @param out_headers Destination HttpHeaders collection.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
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
      enum c_abstract_http_error rc = http_headers_add(out_headers, kbuf, vbuf);
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
static enum c_abstract_http_error
cah_urlsession_handle_data(struct AppleURLSessionState *st, id data_task,
                           CFDataRef data) {
  CFIndex len = CFDataGetLength(data);
  const UInt8 *bytes = CFDataGetBytePtr(data);
  if (st->req->on_chunk) {
    enum c_abstract_http_error cb_rc =
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
          self, &g_cah_urlsession_state_key);
  enum c_abstract_http_error rc;
  (void)_cmd;
  (void)session;
  rc = cah_urlsession_handle_data(st, data_task, (CFDataRef)data);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("cah_urlsession_did_receive_data: chunk aborted %d", (int)rc);
  }
}

/**
 * @brief Process NSURLSession task completion, status/headers, and wakeup.
 * @param st Active task state.
 * @param session NSURLSession instance to invalidate.
 * @param task Completed NSURLSessionTask instance.
 * @param error Optional NSError / CFErrorRef on failure, or NULL on success.
 * @return C_ABSTRACT_HTTP_SUCCESS or mapped error code.
 */
static enum c_abstract_http_error
cah_urlsession_handle_complete(struct AppleURLSessionState *st, id session,
                               id task, CFErrorRef error) {
  enum c_abstract_http_error rc = st->error;
  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
    if (error) {
      CFIndex code = CFErrorGetCode(error);
      rc = (code == -1001) ? C_ABSTRACT_HTTP_ERR_TIMEOUT
                           : C_ABSTRACT_HTTP_ERR_IO;
    } else {
      id resp =
          ((id(*)(id, SEL))objc_msgSend)(task, sel_registerName("response"));
      long status = ((long (*)(id, SEL))objc_msgSend)(
          resp, sel_registerName("statusCode"));
      CFDictionaryRef hdrs = (CFDictionaryRef)((id(*)(id, SEL))objc_msgSend)(
          resp, sel_registerName("allHeaderFields"));
      st->res->status_code = (int)status;
      rc = cah_copy_nshttp_headers(hdrs, &st->res->headers);
    }
    st->error = rc;
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
          self, &g_cah_urlsession_state_key);
  enum c_abstract_http_error rc;
  (void)_cmd;
  rc = cah_urlsession_handle_complete(st, session, task, (CFErrorRef)error);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("cah_urlsession_did_complete: finished with %d", (int)rc);
  }
}

/**
 * @brief Register the CAHURLSessionDelegate Objective-C class once.
 */
static void cah_register_urlsession_delegate(void) {
  Class sup = objc_getClass("NSObject");
  Class cls = objc_allocateClassPair(sup, "CAHURLSessionDelegate", 0);
  class_addMethod(cls, sel_registerName("URLSession:dataTask:didReceiveData:"),
                  (IMP)cah_urlsession_did_receive_data, "v@:@@@");
  class_addMethod(cls,
                  sel_registerName("URLSession:task:didCompleteWithError:"),
                  (IMP)cah_urlsession_did_complete, "v@:@@@");
  objc_registerClassPair(cls);
}

/**
 * @brief Dispatch a single HTTP request synchronously via NSURLSession.
 * @param ctx Transport context.
 * @param req Request to send.
 * @param res Output response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
http_apple_nsurlsession_send(struct HttpTransportContext *ctx,
                             const struct HttpRequest *req,
                             struct HttpResponse **res) {
  CFStringRef method_cf = NULL;
  CFStringRef url_str;
  CFURLRef url;
  CFDataRef upload_data = NULL;
  struct AppleURLSessionState st;
  pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t init_cv = PTHREAD_COND_INITIALIZER;
  enum c_abstract_http_error rc;
  size_t i;

  rc = apple_method_cfstr(req->method, &method_cf);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  url_str = CFStringCreateWithCString(
      kCFAllocatorDefault, req->url ? req->url : "", kCFStringEncodingUTF8);
  url = CFURLCreateWithString(kCFAllocatorDefault, url_str, NULL);
  CFRelease(url_str);
  if (!url || !req->url || req->url[0] == '\0') {
    if (url) {
      CFRelease(url);
    }
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  if (!*res) {
    CFRelease(url);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = http_response_init(*res);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(*res);
    *res = NULL;
    CFRelease(url);
    return rc;
  }

  if (req->read_chunk) {
    CFMutableDataRef mut = CFDataCreateMutable(kCFAllocatorDefault,
                                               (CFIndex)req->expected_body_len);
    for (;;) {
      UInt8 chunk_buf[8192];
      size_t out_read = 0;
      enum c_abstract_http_error cb_rc = req->read_chunk(
          req->read_chunk_user_data, chunk_buf, sizeof(chunk_buf), &out_read);
      if (cb_rc != C_ABSTRACT_HTTP_SUCCESS) {
        CFRelease(mut);
        CFRelease(url);
        return apple_cleanup_failed_res(res, cb_rc);
      }
      if (out_read == 0) {
        break;
      }
      CFDataAppendBytes(mut, chunk_buf, (CFIndex)out_read);
    }
    upload_data = mut;
  } else if (req->body && req->body_len > 0) {
    upload_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                               (CFIndex)req->body_len);
  }

  memset(&st, 0, sizeof(st));
  st.req = req;
  st.res = *res;
  st.body_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
  st.error = C_ABSTRACT_HTTP_SUCCESS;
  st.mtx = init_mtx;
  st.cv = init_cv;

  pthread_once(&g_cah_urlsession_once, cah_register_urlsession_delegate);

  {
    id pool = ((id(*)(id, SEL))objc_msgSend)(
        ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSAutoreleasePool"),
                                          sel_registerName("alloc")),
        sel_registerName("init"));
    id nsreq = ((id(*)(id, SEL, CFTypeRef))objc_msgSend)(
        ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSMutableURLRequest"),
                                          sel_registerName("alloc")),
        sel_registerName("initWithURL:"), url);
    id del;
    id scfg;
    id session;
    id task;

    CFRelease(url);
    ((void (*)(id, SEL, CFTypeRef))objc_msgSend)(
        nsreq, sel_registerName("setHTTPMethod:"), method_cf);

    if (ctx->config.timeout_ms > 0) {
      ((void (*)(id, SEL, double))objc_msgSend)(
          nsreq, sel_registerName("setTimeoutInterval:"),
          (double)ctx->config.timeout_ms / 1000.0);
    }

    for (i = 0; i < req->headers.count; ++i) {
      CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault,
                                                req->headers.headers[i].key,
                                                kCFStringEncodingUTF8);
      CFStringRef v = CFStringCreateWithCString(kCFAllocatorDefault,
                                                req->headers.headers[i].value,
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

    del = ((id(*)(id, SEL))objc_msgSend)(
        ((id(*)(Class, SEL))objc_msgSend)(
            objc_getClass("CAHURLSessionDelegate"), sel_registerName("alloc")),
        sel_registerName("init"));
    objc_setAssociatedObject(del, &g_cah_urlsession_state_key, (id)(void *)&st,
                             OBJC_ASSOCIATION_ASSIGN);

    scfg = ((id(*)(Class, SEL))objc_msgSend)(
        objc_getClass("NSURLSessionConfiguration"),
        sel_registerName("ephemeralSessionConfiguration"));
    session = ((id(*)(Class, SEL, id, id, id))objc_msgSend)(
        objc_getClass("NSURLSession"),
        sel_registerName("sessionWithConfiguration:delegate:delegateQueue:"),
        scfg, del, NULL);
    ((void (*)(id, SEL))objc_msgSend)(del, sel_registerName("release"));

    task = ((id(*)(id, SEL, id))objc_msgSend)(
        session, sel_registerName("dataTaskWithRequest:"), nsreq);
    ((void (*)(id, SEL))objc_msgSend)(nsreq, sel_registerName("release"));
    ((void (*)(id, SEL))objc_msgSend)(task, sel_registerName("resume"));
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
  }

  pthread_mutex_lock(&st.mtx);
  while (!st.done) {
    pthread_cond_wait(&st.cv, &st.mtx);
  }
  pthread_mutex_unlock(&st.mtx);
  pthread_mutex_destroy(&st.mtx);
  pthread_cond_destroy(&st.cv);

  if (st.error != C_ABSTRACT_HTTP_SUCCESS) {
    CFRelease(st.body_data);
    return apple_cleanup_failed_res(res, st.error);
  }

  if (!req->on_chunk) {
    CFIndex blen = CFDataGetLength(st.body_data);
    (*res)->body = malloc((size_t)blen + 1U);
    if (!(*res)->body) {
      CFRelease(st.body_data);
      return apple_cleanup_failed_res(res, C_ABSTRACT_HTTP_ERR_NOMEM);
    }
    if (blen > 0) {
      CFDataGetBytes(st.body_data, CFRangeMake(0, blen), (UInt8 *)(*res)->body);
    }
    ((char *)(*res)->body)[blen] = '\0';
    (*res)->body_len = (size_t)blen;
  }

  CFRelease(st.body_data);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Multi-request worker implementation using NSURLSession C-bridge.
 * @param wctx Worker context (freed before returning).
 * @return C_ABSTRACT_HTTP_SUCCESS or first item / wakeup error code.
 */
static enum c_abstract_http_error
apple_nsurlsession_multi_worker_impl(struct AppleMultiWorkerCtx *wctx) {
  size_t i;
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  enum c_abstract_http_error batch_err = C_ABSTRACT_HTTP_SUCCESS;
  for (i = 0; i < wctx->multi->count; ++i) {
    struct HttpResponse *res = NULL;
    rc =
        http_apple_nsurlsession_send(wctx->ctx, wctx->multi->requests[i], &res);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      batch_err = rc;
      wctx->futures[i]->response = res;
      wctx->futures[i]->error_code = rc;
      wctx->futures[i]->is_ready = 1;
      goto next_ns_item;
    }
    wctx->futures[i]->response = res;
    wctx->futures[i]->error_code = C_ABSTRACT_HTTP_SUCCESS;
    wctx->futures[i]->is_ready = 1;
  next_ns_item:;
  }
  if (wctx->loop) {
    enum c_abstract_http_error wake_rc = http_loop_wakeup(wctx->loop);
    if (wake_rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(wctx);
      return wake_rc;
    }
  }
  free(wctx);
  if (batch_err != C_ABSTRACT_HTTP_SUCCESS) {
    rc = batch_err;
    return rc;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/* ========================================================================== */
/* Legacy CFReadStreamCreateForHTTPRequest Backend (< macOS 10.11)            */
/* ========================================================================== */

#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM

/** @brief Internal Apple CFStream request state */
struct AppleReqState {
  const struct HttpRequest *req;    /**< Request pointer */
  struct HttpResponse **res;        /**< Response output pointer */
  CFMutableDataRef bodyData;        /**< Response body buffer */
  enum c_abstract_http_error error; /**< Error code */
  int done;                         /**< Completion flag */
  CFRunLoopRef runloop;             /**< CFRunLoop reference */
  int *pending_count;               /**< Pending request counter */
};

/**
 * @brief Extract HTTP status code and buffered body from a CFReadStream.
 * @param state Stream request state.
 * @param readStream Completed CFReadStreamRef.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
static enum c_abstract_http_error
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
static enum c_abstract_http_error
apple_stream_handle_chunk(struct AppleReqState *state, const UInt8 *buf,
                          CFIndex bytesRead) {
  enum c_abstract_http_error cb_rc = state->req->on_chunk(
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
        enum c_abstract_http_error rc =
            apple_stream_handle_chunk(state, buf, bytesRead);
        if (rc != C_ABSTRACT_HTTP_SUCCESS) {
          LOG_DEBUG("apple_stream_cb: chunk callback aborted with %d", (int)rc);
        }
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
static enum c_abstract_http_error
http_apple_cfstream_send(struct HttpTransportContext *ctx,
                         const struct HttpRequest *req,
                         struct HttpResponse **res) {
  CFURLRef url;
  CFStringRef urlStr;
  CFStringRef method = NULL;
  CFHTTPMessageRef requestRef;
  CFReadStreamRef readStream;
  size_t i;
  enum c_abstract_http_error rc;

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
      enum c_abstract_http_error cb_rc;
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
    enum c_abstract_http_error ext_rc;
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

    if (state.bodyData)
      CFRelease(state.bodyData);

    if (ext_rc != C_ABSTRACT_HTTP_SUCCESS) {
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return ext_rc;
    }
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
static enum c_abstract_http_error
apple_multi_extract_into(struct AppleReqState *st, CFReadStreamRef stream) {
  enum c_abstract_http_error ext_rc = apple_extract_response(st, stream);
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
static enum c_abstract_http_error
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
    enum c_abstract_http_error m_rc;

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
    enum c_abstract_http_error ext_rc = C_ABSTRACT_HTTP_SUCCESS;
    enum c_abstract_http_error batch_ext_rc = C_ABSTRACT_HTTP_SUCCESS;
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
      enum c_abstract_http_error wake_rc = http_loop_wakeup(wctx->loop);
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

enum c_abstract_http_error http_apple_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_apple_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
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
    enum c_abstract_http_error rc = http_config_init(&(*ctx)->config);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      free(*ctx);
      *ctx = NULL;
      return rc;
    }
  }
  (*ctx)->use_nsurlsession = C_ABSTRACT_HTTP_APPLE_DEFAULT_NSURLSESSION;

  LOG_DEBUG("http_apple_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_apple_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    enum c_abstract_http_error rch = http_config_free(&ctx->config);
    if (rch != C_ABSTRACT_HTTP_SUCCESS)
      return rch;
    free(ctx);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_apple_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  LOG_DEBUG("http_apple_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_apple_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config.verify_peer = config->verify_peer ? 1 : 0;
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.follow_redirects = config->follow_redirects;
  ctx->config.cookie_jar = config->cookie_jar;
  LOG_DEBUG("http_apple_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
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

enum c_abstract_http_error http_apple_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **res) {
  LOG_DEBUG("http_apple_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_apple_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  if (!ctx->use_nsurlsession) {
    return http_apple_cfstream_send(ctx, req, res);
  }
#endif
  return http_apple_nsurlsession_send(ctx, req, res);
}

/**
 * @brief Dispatch multi-request worker batch to NSURLSession or CFStream.
 * @param wctx Worker context (freed by callee).
 * @return C_ABSTRACT_HTTP_SUCCESS or error code.
 */
static enum c_abstract_http_error
apple_multi_dispatch(struct AppleMultiWorkerCtx *wctx) {
#if C_ABSTRACT_HTTP_APPLE_HAVE_CFSTREAM
  if (!wctx->ctx->use_nsurlsession) {
    return apple_cfstream_multi_worker_impl(wctx);
  }
#endif
  return apple_nsurlsession_multi_worker_impl(wctx);
}

/**
 * @brief Background worker entry point dispatching either NSURLSession or
 * CFStream.
 * @param arg Pointer to struct AppleMultiWorkerCtx.
 * @return NULL.
 */
static void *apple_multi_worker(void *arg) {
  struct AppleMultiWorkerCtx *wctx = (struct AppleMultiWorkerCtx *)arg;
  enum c_abstract_http_error rc = apple_multi_dispatch(wctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("apple_multi_worker: completed with %d", (int)rc);
    return (void *)((size_t)(int)rc * 0);
  }
  return NULL;
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_pthread_create_sync;
/** @brief Thread spawn wrapper supporting deterministic synchronous test
 * execution */
#define PTHREAD_CREATE_APPLE(a, b, c, d)                                       \
  (g_mock_pthread_create_sync ? ((*(void *(*)(void *))(c))(d), 0)              \
                              : pthread_create(a, b, c, d))
#else
/** @brief Thread spawn wrapper */
#define PTHREAD_CREATE_APPLE pthread_create
#endif

enum c_abstract_http_error http_apple_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  pthread_t thread;
  struct AppleMultiWorkerCtx *wctx;

  LOG_DEBUG("http_apple_send_multi: Entering");
  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_apple_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  wctx =
      (struct AppleMultiWorkerCtx *)malloc(sizeof(struct AppleMultiWorkerCtx));
  if (!wctx) {
    LOG_DEBUG("http_apple_send_multi: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  wctx->ctx = ctx;
  wctx->loop = loop;
  wctx->multi = multi;
  wctx->futures = futures;

  if (PTHREAD_CREATE_APPLE(&thread, NULL, apple_multi_worker, wctx) != 0) {
    LOG_DEBUG("http_apple_send_multi: Error pthread_create failed");
    free(wctx);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (!g_mock_pthread_create_sync)
#endif
    pthread_detach(thread);

  LOG_DEBUG("http_apple_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* defined(__APPLE__) */
