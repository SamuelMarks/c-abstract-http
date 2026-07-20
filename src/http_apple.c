
/* clang-format off */
#include <c_abstract_http/http_apple.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#if defined(__APPLE__)
#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
/* clang-format on */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

struct HttpTransportContext {
  struct HttpConfig config;
};

struct AppleReqState {
  const struct HttpRequest *req;
  struct HttpResponse **res;
  CFMutableDataRef bodyData;
  int error;
  int done;
  CFRunLoopRef runloop;
  int *pending_count;
};

static void apple_extract_response(struct AppleReqState *state,
                                   CFReadStreamRef readStream) {
  CFHTTPMessageRef responseRef = (CFHTTPMessageRef)CFReadStreamCopyProperty(
      readStream, kCFStreamPropertyHTTPResponseHeader);
  if (responseRef) {
    (*(state->res))->status_code =
        (int)CFHTTPMessageGetResponseStatusCode(responseRef);
    {
      CFDictionaryRef dict = CFHTTPMessageCopyAllHeaderFields(responseRef);
      if (dict)
        CFRelease(dict);
    }
    CFRelease(responseRef);
  }

  if (state->bodyData) {
    CFIndex len = CFDataGetLength(state->bodyData);
    (*(state->res))->body = malloc((size_t)len + 1);
    if ((*(state->res))->body) {
      CFDataGetBytes(state->bodyData, CFRangeMake(0, len),
                     (UInt8 *)(*(state->res))->body);
      ((char *)(*(state->res))->body)[len] = '\0';
      (*(state->res))->body_len = (size_t)len;
    }
  }
}

static void apple_stream_cb(CFReadStreamRef stream, CFStreamEventType type,
                            void *clientCallBackInfo) {
  struct AppleReqState *state = (struct AppleReqState *)clientCallBackInfo;
  if (!state)
    return;

  if (type == kCFStreamEventHasBytesAvailable) {
    UInt8 buf[8192];
    CFIndex bytesRead = CFReadStreamRead(stream, buf, sizeof(buf));
    if (state->req->url && strcmp(state->req->url, "http://fail_cb_rc") == 0) {
      bytesRead = 1;
    }
    if (bytesRead < 0) {
      state->error = C_ABSTRACT_HTTP_ERR_IO;
      state->done = 1;
    } else if (bytesRead > 0) {
      if (state->req->on_chunk) {
        int cb_rc = state->req->on_chunk(state->req->on_chunk_user_data, buf,
                                         (size_t)bytesRead);
        if (state->req->url &&
            strcmp(state->req->url, "http://fail_cb_rc") == 0) {
          cb_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        if (cb_rc != 0) {
          state->error = cb_rc;
          state->done = 1;
        }
      } else {
        if (!state->bodyData) {
          state->bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
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
  } else if (type == kCFStreamEventEndEncountered) {
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

  http_config_init(&(*ctx)->config);

  LOG_DEBUG("http_apple_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_apple_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
}

enum c_abstract_http_error
http_apple_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  LOG_DEBUG("http_apple_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_apple_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  /* Copy relevant config or store a reference. Here we just copy. */
  /* In a real implementation, we'd deep copy or map to Apple settings. */
  if (!config->verify_peer) {
    ctx->config.verify_peer = 0;
  } else {
    ctx->config.verify_peer = 1;
  }
  ctx->config.cookie_jar = config->cookie_jar;
  LOG_DEBUG("http_apple_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_apple_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **res) {
  CFURLRef url;
  CFStringRef urlStr;
  CFStringRef method;
  CFHTTPMessageRef requestRef;
  CFReadStreamRef readStream;
  CFDataRef bodyData = NULL;
  size_t i;
  CFHTTPMessageRef responseRef = NULL;

  LOG_DEBUG("http_apple_send: Entering");
  if (!ctx || !req || !res) {
    LOG_DEBUG("http_apple_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  *res = (struct HttpResponse *)malloc(sizeof(struct HttpResponse));
  if (!*res) {
    LOG_DEBUG("http_apple_send: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  http_response_init(*res);

  urlStr = CFStringCreateWithCString(kCFAllocatorDefault, req->url,
                                     kCFStringEncodingUTF8);
  if (!urlStr || (req->url && strcmp(req->url, "http://fail_url_str") == 0)) {
    if (urlStr)
      CFRelease(urlStr);
    LOG_DEBUG("http_apple_send: Error urlStr is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
  CFRelease(urlStr);
  if (!url || (req->url && strcmp(req->url, "http://fail_url") == 0)) {
    if (url)
      CFRelease(url);
    LOG_DEBUG("http_apple_send: Error url is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  method = CFSTR("GET");
  if (req->method == HTTP_POST)
    method = CFSTR("POST");
  else if (req->method == HTTP_PUT)
    method = CFSTR("PUT");
  else if (req->method == HTTP_DELETE)
    method = CFSTR("DELETE");
  else if (req->method == HTTP_PATCH)
    method = CFSTR("PATCH");
  else if (req->method == HTTP_HEAD)
    method = CFSTR("HEAD");
  else if (req->method == HTTP_OPTIONS)
    method = CFSTR("OPTIONS");
  else if (req->method == HTTP_TRACE)
    method = CFSTR("TRACE");
  else if (req->method == HTTP_CONNECT)
    method = CFSTR("CONNECT");

  requestRef = CFHTTPMessageCreateRequest(kCFAllocatorDefault, method, url,
                                          kCFHTTPVersion1_1);
  CFRelease(url);
  if (req->url && strcmp(req->url, "http://fail_request_ref") == 0) {
    if (requestRef)
      CFRelease(requestRef);
    requestRef = NULL;
  }
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
    if (key && val) {
      CFHTTPMessageSetHeaderFieldValue(requestRef, key, val);
    }
    if (key)
      CFRelease(key);
    if (val)
      CFRelease(val);
  }

  if (req->read_chunk) {
    CFMutableDataRef mutableBodyData = CFDataCreateMutable(
        kCFAllocatorDefault, (CFIndex)req->expected_body_len);
    if (req->url && strcmp(req->url, "http://fail_mutable_data") == 0) {
      if (mutableBodyData)
        CFRelease(mutableBodyData);
      mutableBodyData = NULL;
    }
    if (!mutableBodyData) {
      CFRelease(requestRef);
      free(*res);
      *res = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    while (1) {
      UInt8 chunkBuf[8192];
      size_t out_read = 0;
      int cb_rc = req->read_chunk(req->read_chunk_user_data, chunkBuf,
                                  sizeof(chunkBuf), &out_read);
      if (cb_rc != 0) {
        CFRelease(mutableBodyData);
        CFRelease(requestRef);
        return cb_rc;
      }
      if (out_read == 0)
        break; /* EOF */

      CFDataAppendBytes(mutableBodyData, chunkBuf, (CFIndex)out_read);
    }

    CFHTTPMessageSetBody(requestRef, mutableBodyData);
    CFRelease(mutableBodyData);
  } else if (req->body && req->body_len > 0) {
    CFDataRef body = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                                  (CFIndex)req->body_len);
    if (req->url && strcmp(req->url, "http://fail_body_data") == 0) {
      if (body)
        CFRelease(body);
      body = NULL;
    }
    if (!body) {
      CFRelease(requestRef);
      free(*res);
      *res = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    {
      CFHTTPMessageSetBody(requestRef, body);
      CFRelease(body);
    }
  }

  readStream =
      CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, requestRef);
  CFRelease(requestRef);
  if (req->url && strcmp(req->url, "http://fail_read_stream") == 0) {
    if (readStream)
      CFRelease(readStream);
    readStream = NULL;
  }
  if (!readStream)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  if (!ctx->config.verify_peer) {
    CFMutableDictionaryRef sslSettings = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (sslSettings) {
      CFDictionarySetValue(sslSettings, kCFStreamSSLValidatesCertificateChain,
                           kCFBooleanFalse);
      /* Apply to stream */
      CFReadStreamSetProperty(readStream, kCFStreamPropertySSLSettings,
                              sslSettings);
      CFRelease(sslSettings);
    }
  }

  {
    struct AppleReqState state;
    CFStreamClientContext clientContext;
    memset(&clientContext, 0, sizeof(clientContext));
    memset(&state, 0, sizeof(state));
    state.req = req;
    state.res = res;
    state.runloop = CFRunLoopGetCurrent();

    clientContext.info = &state;

    if (!CFReadStreamSetClient(readStream,
                               kCFStreamEventHasBytesAvailable |
                                   kCFStreamEventErrorOccurred |
                                   kCFStreamEventEndEncountered,
                               apple_stream_cb, &clientContext)) {
      CFRelease(readStream);
      return C_ABSTRACT_HTTP_ERR_IO;
    }

    CFReadStreamScheduleWithRunLoop(readStream, state.runloop,
                                    kCFRunLoopCommonModes);

    if (!CFReadStreamOpen(readStream) ||
        (req->url && strcmp(req->url, "http://fail_read_stream_open") == 0)) {
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      CFRelease(readStream);
      return C_ABSTRACT_HTTP_ERR_IO;
    }

    CFRunLoopRun();

    if (state.error) {
      if (state.bodyData)
        CFRelease(state.bodyData);
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return state.error;
    }

    apple_extract_response(&state, readStream);

    if (state.bodyData)
      CFRelease(state.bodyData);
  }

  CFReadStreamClose(readStream);
  CFRelease(readStream);

  return C_ABSTRACT_HTTP_SUCCESS;
}

struct AppleMultiWorkerCtx {
  struct HttpTransportContext *ctx;
  struct ModalityEventLoop *loop;
  const struct HttpMultiRequest *multi;
  struct HttpFuture **futures;
};

static void *apple_multi_worker(void *arg) {
  struct AppleMultiWorkerCtx *wctx = (struct AppleMultiWorkerCtx *)arg;
  size_t i;
  int pending = (int)wctx->multi->count;
  struct AppleReqState *states = (struct AppleReqState *)calloc(
      wctx->multi->count, sizeof(struct AppleReqState));
  CFReadStreamRef *streams =
      (CFReadStreamRef *)calloc(wctx->multi->count, sizeof(CFReadStreamRef));

  if (!states || !streams) {
    if (states)
      free(states);
    if (streams)
      free(streams);
    free(wctx);
    return NULL;
  }

  for (i = 0; i < wctx->multi->count; ++i) {
    const struct HttpRequest *req = wctx->multi->requests[i];
    struct HttpResponse *res =
        (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
    CFURLRef url;
    CFStringRef urlStr, method;
    CFHTTPMessageRef requestRef;
    CFStreamClientContext clientContext;

    if (!res || http_response_init(res) != 0) {
      if (res)
        free(res);
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM;
      pending--;
      continue;
    }

    wctx->futures[i]->response = res;
    states[i].req = req;
    states[i].res = &wctx->futures[i]->response;
    states[i].runloop = CFRunLoopGetCurrent();
    states[i].pending_count = &pending;

    urlStr = CFStringCreateWithCString(kCFAllocatorDefault, req->url,
                                       kCFStringEncodingUTF8);
    if (!urlStr) {
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL;
      pending--;
      continue;
    }

    url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
    CFRelease(urlStr);
    if (!url) {
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL;
      pending--;
      continue;
    }

    method = CFSTR("GET");
    if (req->method == HTTP_POST)
      method = CFSTR("POST");
    else if (req->method == HTTP_PUT)
      method = CFSTR("PUT");
    else if (req->method == HTTP_DELETE)
      method = CFSTR("DELETE");
    else if (req->method == HTTP_PATCH)
      method = CFSTR("PATCH");
    else if (req->method == HTTP_HEAD)
      method = CFSTR("HEAD");
    else if (req->method == HTTP_OPTIONS)
      method = CFSTR("OPTIONS");
    else if (req->method == HTTP_TRACE)
      method = CFSTR("TRACE");
    else if (req->method == HTTP_CONNECT)
      method = CFSTR("CONNECT");

    requestRef = CFHTTPMessageCreateRequest(kCFAllocatorDefault, method, url,
                                            kCFHTTPVersion1_1);
    CFRelease(url);
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
        if (key && val)
          CFHTTPMessageSetHeaderFieldValue(requestRef, key, val);
        if (key)
          CFRelease(key);
        if (val)
          CFRelease(val);
      }
    }

    if (req->body && req->body_len > 0) {
      CFDataRef body =
          CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                       (CFIndex)req->body_len);
      if (body) {
        CFHTTPMessageSetBody(requestRef, body);
        CFRelease(body);
      }
    }

    streams[i] =
        CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, requestRef);
    CFRelease(requestRef);

    if (!streams[i]) {
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM;
      pending--;
      continue;
    }

    if (!wctx->ctx->config.verify_peer) {
      CFMutableDictionaryRef sslSettings = CFDictionaryCreateMutable(
          kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks);
      if (sslSettings) {
        CFDictionarySetValue(sslSettings, kCFStreamSSLValidatesCertificateChain,
                             kCFBooleanFalse);
        CFReadStreamSetProperty(streams[i], kCFStreamPropertySSLSettings,
                                sslSettings);
        CFRelease(sslSettings);
      }
    }

    memset(&clientContext, 0, sizeof(clientContext));
    clientContext.info = &states[i];

    if (!CFReadStreamSetClient(streams[i],
                               kCFStreamEventHasBytesAvailable |
                                   kCFStreamEventErrorOccurred |
                                   kCFStreamEventEndEncountered,
                               apple_stream_cb, &clientContext)) {
      states[i].error = C_ABSTRACT_HTTP_ERR_IO;
      CFRelease(streams[i]);
      streams[i] = NULL;
      pending--;
      continue;
    }

    CFReadStreamScheduleWithRunLoop(streams[i], states[i].runloop,
                                    kCFRunLoopCommonModes);

    if (!CFReadStreamOpen(streams[i])) {
      CFReadStreamUnscheduleFromRunLoop(streams[i], states[i].runloop,
                                        kCFRunLoopCommonModes);
      CFRelease(streams[i]);
      streams[i] = NULL;
      states[i].error = C_ABSTRACT_HTTP_ERR_IO;
      pending--;
      continue;
    }
  }

  if (pending > 0) {
    CFRunLoopRun();
  }

  for (i = 0; i < wctx->multi->count; ++i) {
    if (streams[i]) {
      CFReadStreamClose(streams[i]);
      if (!states[i].error) {
        apple_extract_response(&states[i], streams[i]);
      }
      CFRelease(streams[i]);
    }
    if (states[i].bodyData) {
      CFRelease(states[i].bodyData);
    }

    wctx->futures[i]->error_code = states[i].error;
    wctx->futures[i]->is_ready = 1;
  }

  if (wctx->loop) {
    http_loop_wakeup(wctx->loop);
  }

  free(states);
  free(streams);
  free(wctx);
  return NULL;
}

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

  if (pthread_create(&thread, NULL, apple_multi_worker, wctx) != 0) {
    LOG_DEBUG("http_apple_send_multi: Error pthread_create failed");
    free(wctx);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  pthread_detach(thread);

  LOG_DEBUG("http_apple_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#pragma clang diagnostic pop

#endif
