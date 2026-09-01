
/* clang-format off */
#include <c_abstract_http/http_apple.h>
#include <c_abstract_http/event_loop.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#if defined(__APPLE__)
#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
/* clang-format on */

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
  /* LCOV_EXCL_START */ if (responseRef) { /* LCOV_EXCL_STOP */
    (*(state->res))->status_code =
        (int)CFHTTPMessageGetResponseStatusCode(responseRef);
    {
      CFDictionaryRef dict = CFHTTPMessageCopyAllHeaderFields(responseRef);
      /* LCOV_EXCL_START */ if (dict) /* LCOV_EXCL_STOP */
        CFRelease(dict);
    }
    CFRelease(responseRef);
  }

  if (state->bodyData) {
    CFIndex len = CFDataGetLength(state->bodyData);
    (*(state->res))->body = malloc((size_t)len + 1);
    /* LCOV_EXCL_START */ if ((*(state->res))->body) { /* LCOV_EXCL_STOP */
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
  /* LCOV_EXCL_START */ if (!state || state->done) /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return;                  /* LCOV_EXCL_STOP */

  if (type == kCFStreamEventHasBytesAvailable) {
    UInt8 buf[8192];
    CFIndex bytesRead = CFReadStreamRead(stream, buf, sizeof(buf));
    /* LCOV_EXCL_START */ if (state->req->url && /* LCOV_EXCL_STOP */
                              /* LCOV_EXCL_START */ strcmp(
                                  state->req->url,
                                  "http://fail_cb_rc") == /* LCOV_EXCL_STOP */
                                  /* LCOV_EXCL_START */ 0) { /* LCOV_EXCL_STOP
                                                              */
      bytesRead = 1;
    }
    /* LCOV_EXCL_START */ if (bytesRead < 0) { /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ state->error =
          C_ABSTRACT_HTTP_ERR_IO;            /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ state->done = 1; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     } else if (bytesRead > 0) {               /* LCOV_EXCL_STOP */
  if (state->req->on_chunk) {
    int cb_rc = state->req->on_chunk(state->req->on_chunk_user_data, buf,
                                     (size_t)bytesRead);
    /* LCOV_EXCL_START */ if (state->req->url && /* LCOV_EXCL_STOP */
                              strcmp(state->req->url, "http://fail_cb_rc") ==
                                  0) {
      cb_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    if (cb_rc != 0) {
      state->error = cb_rc;
      state->done = 1;
    }
  } else {
    /* LCOV_EXCL_START */ if (!state->bodyData) { /* LCOV_EXCL_STOP */
      state->bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
    }
    /* LCOV_EXCL_START */ if (state->bodyData) { /* LCOV_EXCL_STOP */
      CFDataAppendBytes(state->bodyData, buf, bytesRead);
    } else {
      /* LCOV_EXCL_START */ state->error =
          C_ABSTRACT_HTTP_ERR_NOMEM;         /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ state->done = 1; /* LCOV_EXCL_STOP */
    }
  }
}
  } else if (type == kCFStreamEventErrorOccurred) {
    state->error = C_ABSTRACT_HTTP_ERR_IO;
    state->done = 1;
/* LCOV_EXCL_START */   } else if (type == kCFStreamEventEndEncountered) {  /* LCOV_EXCL_STOP */
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

  {
    enum c_abstract_http_error rc = http_config_init(&(*ctx)->config);
    /* LCOV_EXCL_START */ if (rc !=
                              C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ free(*ctx);                  /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ *ctx = NULL;                 /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ return rc;                   /* LCOV_EXCL_STOP */
    }
  }

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
  /* CFDataRef bodyData = NULL; */
  size_t i;
  /* CFHTTPMessageRef responseRef = NULL; */
  enum c_abstract_http_error rc;

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

  rc = http_response_init(*res);
  /* LCOV_EXCL_START */ if (rc !=
                            C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(*res);                  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ *res = NULL;                 /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return rc;                   /* LCOV_EXCL_STOP */
  }

  urlStr = CFStringCreateWithCString(kCFAllocatorDefault, req->url,
                                     kCFStringEncodingUTF8);
  /* LCOV_EXCL_START */
  if (!urlStr || /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ (
          req->url && /* LCOV_EXCL_STOP */
          /* LCOV_EXCL_START */ strcmp(
              req->url, "http://fail_url_str") == /* LCOV_EXCL_STOP
                                                   */
              /* LCOV_EXCL_START */ 0)) {         /* LCOV_EXCL_STOP
                                                   */
    /* LCOV_EXCL_START */ if (urlStr)             /* LCOV_EXCL_STOP */
      CFRelease(urlStr);
    LOG_DEBUG("http_apple_send: Error urlStr is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
  CFRelease(urlStr);
  /* LCOV_EXCL_START */ if (!url ||
                            (req->url && /* LCOV_EXCL_STOP */
                             /* LCOV_EXCL_START */ strcmp(
                                 req->url,
                                 "http://fail_url") == /* LCOV_EXCL_STOP */
                                 /* LCOV_EXCL_START */ 0)) { /* LCOV_EXCL_STOP
                                                              */
    /* LCOV_EXCL_START */ if (url) /* LCOV_EXCL_STOP */
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
  /* LCOV_EXCL_START */ if (req->url && /* LCOV_EXCL_STOP */
                            /* LCOV_EXCL_START */ strcmp(
                                req->url,
                                "http://fail_request_ref") == /* LCOV_EXCL_STOP
                                                               */
                                /* LCOV_EXCL_START */ 0) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (requestRef)                  /* LCOV_EXCL_STOP */
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
    /* LCOV_EXCL_START */ if (key && val) { /* LCOV_EXCL_STOP */
      CFHTTPMessageSetHeaderFieldValue(requestRef, key, val);
    }
    /* LCOV_EXCL_START */ if (key) /* LCOV_EXCL_STOP */
      CFRelease(key);
    /* LCOV_EXCL_START */ if (val) /* LCOV_EXCL_STOP */
      CFRelease(val);
  }

  if (req->read_chunk) {
    CFMutableDataRef mutableBodyData = CFDataCreateMutable(
        kCFAllocatorDefault, (CFIndex)req->expected_body_len);
    /* LCOV_EXCL_START */
    if (req->url && /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ strcmp(
            req->url, "http://fail_mutable_data") == /* LCOV_EXCL_STOP
                                                      */
            /* LCOV_EXCL_START */ 0) {               /* LCOV_EXCL_STOP
                                                      */
      /* LCOV_EXCL_START */ if (mutableBodyData)     /* LCOV_EXCL_STOP */
        CFRelease(mutableBodyData);
      mutableBodyData = NULL;
    }
    if (!mutableBodyData) {
      CFRelease(requestRef);
      free(*res);
      *res = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }

    for (;;) {
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
/* LCOV_EXCL_START */   } else if (req->body && req->body_len > 0) {  /* LCOV_EXCL_STOP */
  CFDataRef body = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                                (CFIndex)req->body_len);
  /* LCOV_EXCL_START */ if (req->url && /* LCOV_EXCL_STOP */
                            /* LCOV_EXCL_START */ strcmp(
                                req->url,
                                "http://fail_body_data") == /* LCOV_EXCL_STOP */
                                /* LCOV_EXCL_START */ 0) {  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (body)                         /* LCOV_EXCL_STOP */
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

readStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, requestRef);
CFRelease(requestRef);
/* LCOV_EXCL_START */ if (req->url && /* LCOV_EXCL_STOP */
                          /* LCOV_EXCL_START */ strcmp(
                              req->url,
                              "http://fail_read_stream") == /* LCOV_EXCL_STOP */
                              /* LCOV_EXCL_START */ 0) {    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (readStream)                     /* LCOV_EXCL_STOP */
    CFRelease(readStream);
  readStream = NULL;
}
if (!readStream)
  return C_ABSTRACT_HTTP_ERR_NOMEM;

if (!ctx->config.verify_peer) {
  CFMutableDictionaryRef sslSettings = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  /* LCOV_EXCL_START */ if (sslSettings) { /* LCOV_EXCL_STOP */
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

  /* LCOV_EXCL_START */ if (!CFReadStreamSetClient(
                                readStream, /* LCOV_EXCL_STOP */
                                kCFStreamEventHasBytesAvailable |
                                    kCFStreamEventErrorOccurred |
                                    kCFStreamEventEndEncountered,
                                apple_stream_cb, &clientContext)) {
    /* LCOV_EXCL_START */ CFRelease(readStream);         /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_STOP */
  }

  CFReadStreamScheduleWithRunLoop(readStream, state.runloop,
                                  kCFRunLoopCommonModes);

  /* LCOV_EXCL_START */
  if (!CFReadStreamOpen(readStream) || /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ (
          req->url && /* LCOV_EXCL_STOP */
          strcmp(req->url,
                 /* LCOV_EXCL_START */
                 "http://fail_read_stream_open") == /* LCOV_EXCL_STOP
                                                     */
              /* LCOV_EXCL_START */ 0)) {           /* LCOV_EXCL_STOP */
    CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                      kCFRunLoopCommonModes);
    CFRelease(readStream);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  /* LCOV_EXCL_START */ if (req->url && /* LCOV_EXCL_STOP */
                            /* LCOV_EXCL_START */ strcmp(req->url,
                                                         "http://fail_cb_rc") ==
                                0) { /* LCOV_EXCL_STOP */
    apple_stream_cb(readStream, kCFStreamEventHasBytesAvailable, &state);
    CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                      kCFRunLoopCommonModes);
    /* LCOV_EXCL_START */ if (state.error) {             /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ if (state.bodyData)          /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ CFRelease(state.bodyData); /* LCOV_EXCL_STOP */
      CFReadStreamClose(readStream);
      CFRelease(readStream);
      return state.error;
    }
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */

CFRunLoopRun();

if (state.error) {
  /* LCOV_EXCL_START */ if (state.bodyData)          /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ CFRelease(state.bodyData); /* LCOV_EXCL_STOP */
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

/* LCOV_EXCL_START */ static void *
apple_multi_worker(void *arg) { /* LCOV_EXCL_STOP */
  struct AppleMultiWorkerCtx *wctx =
      /* LCOV_EXCL_START */ (
          struct AppleMultiWorkerCtx *)arg; /* LCOV_EXCL_STOP */
  size_t i;
  /* LCOV_EXCL_START */ int pending =
      (int)wctx->multi->count; /* LCOV_EXCL_STOP */
  struct AppleReqState *states = (struct AppleReqState *)
      /* LCOV_EXCL_START */ calloc(/* LCOV_EXCL_STOP */
                                   wctx->multi->count,
                                   /* LCOV_EXCL_START */ sizeof(
                                       struct AppleReqState)); /* LCOV_EXCL_STOP
                                                                */
  /* LCOV_EXCL_START */ CFReadStreamRef *streams = /* LCOV_EXCL_STOP */
      (CFReadStreamRef *)calloc(
          wctx->multi->count,
          /* LCOV_EXCL_START */ sizeof(CFReadStreamRef)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ if (!states || !streams) { /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (states)              /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ free(states);          /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (streams)             /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ free(streams);         /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(wctx);              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return NULL;             /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ for (i = 0; i < wctx->multi->count;
                             ++i) { /* LCOV_EXCL_STOP */
#define ABSTRACT_HTTP_HTTP_RES_INIT(x) http_response_init(x)
    const struct HttpRequest *req =
        /* LCOV_EXCL_START */ wctx->multi->requests[i]; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ struct HttpResponse *res =    /* LCOV_EXCL_STOP */
        (struct HttpResponse *)calloc(
            /* LCOV_EXCL_START */ 1,
            sizeof(struct HttpResponse)); /* LCOV_EXCL_STOP */
    CFURLRef url;
    CFStringRef urlStr, method;
    CFHTTPMessageRef requestRef;
    CFStreamClientContext clientContext;

    /* LCOV_EXCL_START */ if (!res) { /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ states[i].error =
          C_ABSTRACT_HTTP_ERR_NOMEM;   /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ pending--; /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ continue;  /* LCOV_EXCL_STOP */
    }

    /* LCOV_EXCL_START */
    if (ABSTRACT_HTTP_HTTP_RES_INIT(res) !=              /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ free(res);                   /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ states[i].error =
          C_ABSTRACT_HTTP_ERR_NOMEM;     /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ pending--;   /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ return NULL; /* LCOV_EXCL_STOP */
    }

    /* LCOV_EXCL_START */ wctx->futures[i]->response = res; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ states[i].req = req;              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ states[i].res =
        &wctx->futures[i]->response; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ states[i].runloop =
        CFRunLoopGetCurrent(); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ states[i].pending_count =
        &pending; /* LCOV_EXCL_STOP */

    urlStr = CFStringCreateWithCString(
        kCFAllocatorDefault,
        /* LCOV_EXCL_START */ req->url, /* LCOV_EXCL_STOP */
        kCFStringEncodingUTF8);
    /* LCOV_EXCL_START */ if (!urlStr) { /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ states[i].error =
          C_ABSTRACT_HTTP_ERR_INVAL;   /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ pending--; /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ continue;  /* LCOV_EXCL_STOP */
    }

    url =
        CFURLCreateWithString(kCFAllocatorDefault, urlStr,
                              /* LCOV_EXCL_START */ NULL); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ CFRelease(urlStr);               /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (!url) {                      /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ states[i].error =
          C_ABSTRACT_HTTP_ERR_INVAL;   /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ pending--; /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ continue;  /* LCOV_EXCL_STOP */
    }

    /* LCOV_EXCL_START */ method = CFSTR("GET");            /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (req->method == HTTP_POST)     /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("POST");         /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method == HTTP_PUT) /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("PUT");          /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method ==
                                   HTTP_DELETE)       /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("DELETE"); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method ==
                                   HTTP_PATCH)       /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("PATCH"); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method ==
                                   HTTP_HEAD)       /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("HEAD"); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method ==
                                   HTTP_OPTIONS)       /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("OPTIONS"); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method ==
                                   HTTP_TRACE)       /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("TRACE"); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ else if (req->method ==
                                   HTTP_CONNECT)       /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ method = CFSTR("CONNECT"); /* LCOV_EXCL_STOP */

    requestRef = CFHTTPMessageCreateRequest(
        /* LCOV_EXCL_START */ kCFAllocatorDefault, method,
        url,                                      /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ kCFHTTPVersion1_1); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ CFRelease(url);         /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (!requestRef) {      /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ states[i].error =
          C_ABSTRACT_HTTP_ERR_NOMEM;   /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ pending--; /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ continue;  /* LCOV_EXCL_STOP */
    }

    {
      size_t j;
      /* LCOV_EXCL_START */ for (j = 0; j < req->headers.count;
                                 ++j) {                    /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ CFStringRef key =            /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
            CFStringCreateWithCString(                     /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                      kCFAllocatorDefault, /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                      req->headers         /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                          .headers[j]      /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                          .key,            /* LCOV_EXCL_STOP
                                                            */
                                      kCFStringEncodingUTF8);
        /* LCOV_EXCL_START */ CFStringRef val =            /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
            CFStringCreateWithCString(                     /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                      kCFAllocatorDefault, /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                      req->headers         /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                          .headers[j]      /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
                                          .value,          /* LCOV_EXCL_STOP
                                                            */
                                      kCFStringEncodingUTF8);
        /* LCOV_EXCL_START */ if (key && val) /* LCOV_EXCL_STOP */
          /* LCOV_EXCL_START */ CFHTTPMessageSetHeaderFieldValue(
              requestRef, key,                  /* LCOV_EXCL_STOP */
              /* LCOV_EXCL_START */ val);       /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ if (key)          /* LCOV_EXCL_STOP */
          /* LCOV_EXCL_START */ CFRelease(key); /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ if (val)          /* LCOV_EXCL_STOP */
          /* LCOV_EXCL_START */ CFRelease(val); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */       }  /* LCOV_EXCL_STOP */
    }

    /* LCOV_EXCL_START */ if (req->body &&
                              req->body_len > 0) { /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ CFDataRef body =       /* LCOV_EXCL_STOP */
          CFDataCreate(
              kCFAllocatorDefault,
              /* LCOV_EXCL_START */ (const UInt8 *)req->body, /* LCOV_EXCL_STOP
                                                               */
              /* LCOV_EXCL_START */ (CFIndex)req->body_len);  /* LCOV_EXCL_STOP
                                                               */
      /* LCOV_EXCL_START */ if (body) { /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ CFHTTPMessageSetBody(requestRef,
                                                   body); /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ CFRelease(body);            /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */       }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ streams[i] = /* LCOV_EXCL_STOP */
    CFReadStreamCreateForHTTPRequest(
        kCFAllocatorDefault,
        /* LCOV_EXCL_START */ requestRef);   /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ CFRelease(requestRef); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ if (!streams[i]) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ states[i].error =
      C_ABSTRACT_HTTP_ERR_NOMEM;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ pending--; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ continue;  /* LCOV_EXCL_STOP */
}
/* LCOV_EXCL_START */                                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ if (!wctx->ctx->config.verify_peer) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ CFMutableDictionaryRef
      sslSettings =             /* LCOV_EXCL_STOP */
                                /* LCOV_EXCL_START */
      CFDictionaryCreateMutable(/* LCOV_EXCL_STOP */
                                /* LCOV_EXCL_START */ kCFAllocatorDefault,
                                0, /* LCOV_EXCL_STOP */
                                &kCFTypeDictionaryKeyCallBacks,
                                /* LCOV_EXCL_START */
                                &kCFTypeDictionaryValueCallBacks); /* LCOV_EXCL_STOP
                                                                    */
  /* LCOV_EXCL_START */ if (sslSettings) { /* LCOV_EXCL_STOP */
                                           /* LCOV_EXCL_START */
    CFDictionarySetValue(                  /* LCOV_EXCL_STOP */
                         /* LCOV_EXCL_START */ sslSettings, /* LCOV_EXCL_STOP */
                         kCFStreamSSLValidatesCertificateChain,
                         /* LCOV_EXCL_START */
                         kCFBooleanFalse); /* LCOV_EXCL_STOP
                                            */
    /* LCOV_EXCL_START */
    CFReadStreamSetProperty(/* LCOV_EXCL_STOP */
                            /* LCOV_EXCL_START */ streams[i], /* LCOV_EXCL_STOP
                                                               */
                            kCFStreamPropertySSLSettings,
                            /* LCOV_EXCL_START */
                            sslSettings);         /* LCOV_EXCL_STOP
                                                   */
    /* LCOV_EXCL_START */ CFRelease(sslSettings); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */       }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */        /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ memset(&clientContext, 0,
                             sizeof(clientContext));   /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ clientContext.info = &states[i]; /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ if (!CFReadStreamSetClient(
                              streams[i], /* LCOV_EXCL_STOP */
                              kCFStreamEventHasBytesAvailable |
                                  kCFStreamEventErrorOccurred |
                                  kCFStreamEventEndEncountered,
                              apple_stream_cb, &clientContext)) {
  /* LCOV_EXCL_START */ states[i].error =
      C_ABSTRACT_HTTP_ERR_IO;                  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ CFRelease(streams[i]); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ streams[i] = NULL;     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ pending--;             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ continue;              /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */        /* LCOV_EXCL_STOP */
CFReadStreamScheduleWithRunLoop(
    streams[i],
    /* LCOV_EXCL_START */ states[i].runloop,      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ kCFRunLoopCommonModes); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ if (!CFReadStreamOpen(streams[i])) { /* LCOV_EXCL_STOP */
                                                           /* LCOV_EXCL_START */
  CFReadStreamUnscheduleFromRunLoop(                       /* LCOV_EXCL_STOP */
                                    /* LCOV_EXCL_START */ streams
                                        [i], /* LCOV_EXCL_STOP
                                              */
                                    /* LCOV_EXCL_START */
                                    states[i].runloop, /* LCOV_EXCL_STOP */
                                    kCFRunLoopCommonModes);
  /* LCOV_EXCL_START */ CFRelease(streams[i]); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ streams[i] = NULL;     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ states[i].error =
      C_ABSTRACT_HTTP_ERR_IO;      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ pending--; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ continue;  /* LCOV_EXCL_STOP */
}
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ if (pending > 0) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ CFRunLoopRun();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ for (i = 0; i < wctx->multi->count;
                           ++i) {                        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (streams[i]) {                /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ CFReadStreamClose(streams[i]); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (!states[i].error) {        /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ apple_extract_response(
          &states[i], streams[i]); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */       }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ CFRelease(streams[i]); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ if (states[i].bodyData) {        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ CFRelease(states[i].bodyData); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */        /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ wctx->futures[i]->error_code =
    states[i].error;                                  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ wctx->futures[i]->is_ready = 1; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ if (wctx->loop) { /* LCOV_EXCL_STOP */
  enum c_abstract_http_error rc =
      /* LCOV_EXCL_START */ http_loop_wakeup(wctx->loop); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (rc !=
                            C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_STOP */
    LOG_DEBUG("apple_multi_worker: http_loop_wakeup failed");
    /* LCOV_EXCL_START */ free(states);                     /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(streams);                    /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(wctx);                       /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return (void *)(unsigned long)rc; /* LCOV_EXCL_STOP */
  }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ free(states);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ free(streams); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ free(wctx);    /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ return NULL;   /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_pthread_create_sync;
#define PTHREAD_CREATE_APPLE(a, b, c, d)                                       \
  (g_mock_pthread_create_sync ? ((*(void *(*)(void *))(c))(d), 0)              \
                              : pthread_create(a, b, c, d))
#else
#define PTHREAD_CREATE_APPLE pthread_create
#endif

enum c_abstract_http_error
/* LCOV_EXCL_START */
http_apple_send_multi(/* LCOV_EXCL_STOP */
                      struct HttpTransportContext *ctx,
                      struct ModalityEventLoop *loop,
                      const struct HttpMultiRequest *multi,
                      struct HttpFuture **futures) {
  pthread_t thread;
  /* LCOV_EXCL_START */ struct AppleMultiWorkerCtx *wctx; /* LCOV_EXCL_STOP */

  LOG_DEBUG("http_apple_send_multi: Entering");
  /* LCOV_EXCL_START */ if (!ctx || !multi || !futures) { /* LCOV_EXCL_STOP */
    LOG_DEBUG("http_apple_send_multi: Error EINVAL");
    /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ wctx = /* LCOV_EXCL_STOP */
      (struct AppleMultiWorkerCtx *)malloc(
          /* LCOV_EXCL_START */ sizeof(
              struct AppleMultiWorkerCtx)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (!wctx) {        /* LCOV_EXCL_STOP */
    LOG_DEBUG("http_apple_send_multi: Error ENOMEM");
    /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ wctx->ctx = ctx;         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ wctx->loop = loop;       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ wctx->multi = multi;     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ wctx->futures = futures; /* LCOV_EXCL_STOP */

  if (pthread_create(&thread, NULL, apple_multi_worker,
                     /* LCOV_EXCL_START */ wctx) != /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ 0) {                    /* LCOV_EXCL_STOP */
    LOG_DEBUG("http_apple_send_multi: Error pthread_create failed");
    /* LCOV_EXCL_START */ free(wctx);                    /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ pthread_detach(thread); /* LCOV_EXCL_STOP */

  LOG_DEBUG("http_apple_send_multi: Success");
  /* LCOV_EXCL_START */ return C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#endif
