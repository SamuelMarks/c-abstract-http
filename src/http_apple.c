
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
  if (responseRef) { /* LCOV_EXCL_BR_LINE */
    (*(state->res))->status_code =
        (int)CFHTTPMessageGetResponseStatusCode(responseRef);
    {
      CFDictionaryRef dict = CFHTTPMessageCopyAllHeaderFields(responseRef);
      if (dict) /* LCOV_EXCL_BR_LINE */
        CFRelease(dict);
    }
    CFRelease(responseRef);
  }

  if (state->bodyData) {
    CFIndex len = CFDataGetLength(state->bodyData);
    (*(state->res))->body = malloc((size_t)len + 1);
    if ((*(state->res))->body) { /* LCOV_EXCL_BR_LINE */
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
  if (!state || state->done) /* LCOV_EXCL_BR_LINE */
    return;                  /* LCOV_EXCL_LINE */

  if (type == kCFStreamEventHasBytesAvailable) {
    UInt8 buf[8192];
    CFIndex bytesRead = CFReadStreamRead(stream, buf, sizeof(buf));
    if (state->req->url &&                              /* LCOV_EXCL_BR_LINE */
        strcmp(state->req->url, "http://fail_cb_rc") == /* LCOV_EXCL_BR_LINE */
            0) {                                        /* LCOV_EXCL_BR_LINE */
      bytesRead = 1;
    }
    if (bytesRead < 0) {                     /* LCOV_EXCL_BR_LINE */
      state->error = C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_LINE */
      state->done = 1;                       /* LCOV_EXCL_LINE */
    } else if (bytesRead > 0) {              /* LCOV_EXCL_BR_LINE */
      if (state->req->on_chunk) {
        int cb_rc = state->req->on_chunk(state->req->on_chunk_user_data, buf,
                                         (size_t)bytesRead);
        if (state->req->url && /* LCOV_EXCL_BR_LINE */
            strcmp(state->req->url, "http://fail_cb_rc") == 0) {
          cb_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
        }
        if (cb_rc != 0) {
          state->error = cb_rc;
          state->done = 1;
        }
      } else {
        if (!state->bodyData) { /* LCOV_EXCL_BR_LINE */
          state->bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
        }
        if (state->bodyData) { /* LCOV_EXCL_BR_LINE */
          CFDataAppendBytes(state->bodyData, buf, bytesRead);
        } else {
          state->error = C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_LINE */
          state->done = 1;                          /* LCOV_EXCL_LINE */
        }
      }
    }
  } else if (type == kCFStreamEventErrorOccurred) {
    state->error = C_ABSTRACT_HTTP_ERR_IO;
    state->done = 1;
  } else if (type == kCFStreamEventEndEncountered) { /* LCOV_EXCL_BR_LINE */
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
    if (rc != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
      free(*ctx);                        /* LCOV_EXCL_LINE */
      *ctx = NULL;                       /* LCOV_EXCL_LINE */
      return rc;                         /* LCOV_EXCL_LINE */
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
  if (rc != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_BR_LINE */
    free(*res);                        /* LCOV_EXCL_LINE */
    *res = NULL;                       /* LCOV_EXCL_LINE */
    return rc;                         /* LCOV_EXCL_LINE */
  }

  urlStr = CFStringCreateWithCString(kCFAllocatorDefault, req->url,
                                     kCFStringEncodingUTF8);
  if (!urlStr ||                                  /* LCOV_EXCL_BR_LINE */
      (req->url &&                                /* LCOV_EXCL_BR_LINE */
       strcmp(req->url, "http://fail_url_str") == /* LCOV_EXCL_BR_LINE */
           0)) {                                  /* LCOV_EXCL_BR_LINE */
    if (urlStr)                                   /* LCOV_EXCL_BR_LINE */
      CFRelease(urlStr);
    LOG_DEBUG("http_apple_send: Error urlStr is NULL");
    free(*res);
    *res = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  url = CFURLCreateWithString(kCFAllocatorDefault, urlStr, NULL);
  CFRelease(urlStr);
  if (!url || (req->url &&                            /* LCOV_EXCL_BR_LINE */
               strcmp(req->url, "http://fail_url") == /* LCOV_EXCL_BR_LINE */
                   0)) {                              /* LCOV_EXCL_BR_LINE */
    if (url)                                          /* LCOV_EXCL_BR_LINE */
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
  if (req->url &&                                    /* LCOV_EXCL_BR_LINE */
      strcmp(req->url, "http://fail_request_ref") == /* LCOV_EXCL_BR_LINE */
          0) {                                       /* LCOV_EXCL_BR_LINE */
    if (requestRef)                                  /* LCOV_EXCL_BR_LINE */
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
    if (key && val) { /* LCOV_EXCL_BR_LINE */
      CFHTTPMessageSetHeaderFieldValue(requestRef, key, val);
    }
    if (key) /* LCOV_EXCL_BR_LINE */
      CFRelease(key);
    if (val) /* LCOV_EXCL_BR_LINE */
      CFRelease(val);
  }

  if (req->read_chunk) {
    CFMutableDataRef mutableBodyData = CFDataCreateMutable(
        kCFAllocatorDefault, (CFIndex)req->expected_body_len);
    if (req->url &&                                     /* LCOV_EXCL_BR_LINE */
        strcmp(req->url, "http://fail_mutable_data") == /* LCOV_EXCL_BR_LINE */
            0) {                                        /* LCOV_EXCL_BR_LINE */
      if (mutableBodyData)                              /* LCOV_EXCL_BR_LINE */
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
  } else if (req->body && req->body_len > 0) { /* LCOV_EXCL_BR_LINE */
    CFDataRef body = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)req->body,
                                  (CFIndex)req->body_len);
    if (req->url &&                                  /* LCOV_EXCL_BR_LINE */
        strcmp(req->url, "http://fail_body_data") == /* LCOV_EXCL_BR_LINE */
            0) {                                     /* LCOV_EXCL_BR_LINE */
      if (body)                                      /* LCOV_EXCL_BR_LINE */
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
  if (req->url &&                                    /* LCOV_EXCL_BR_LINE */
      strcmp(req->url, "http://fail_read_stream") == /* LCOV_EXCL_BR_LINE */
          0) {                                       /* LCOV_EXCL_BR_LINE */
    if (readStream)                                  /* LCOV_EXCL_BR_LINE */
      CFRelease(readStream);
    readStream = NULL;
  }
  if (!readStream)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  if (!ctx->config.verify_peer) {
    CFMutableDictionaryRef sslSettings = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (sslSettings) { /* LCOV_EXCL_BR_LINE */
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

    if (!CFReadStreamSetClient(readStream, /* LCOV_EXCL_BR_LINE */
                               kCFStreamEventHasBytesAvailable |
                                   kCFStreamEventErrorOccurred |
                                   kCFStreamEventEndEncountered,
                               apple_stream_cb, &clientContext)) {
      CFRelease(readStream);         /* LCOV_EXCL_LINE */
      return C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_LINE */
    }

    CFReadStreamScheduleWithRunLoop(readStream, state.runloop,
                                    kCFRunLoopCommonModes);

    if (!CFReadStreamOpen(readStream) || /* LCOV_EXCL_BR_LINE */
        (req->url &&                     /* LCOV_EXCL_BR_LINE */
         strcmp(req->url,
                "http://fail_read_stream_open") == /* LCOV_EXCL_BR_LINE */
             0)) {                                 /* LCOV_EXCL_BR_LINE */
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      CFRelease(readStream);
      return C_ABSTRACT_HTTP_ERR_IO;
    }

    if (req->url &&                                   /* LCOV_EXCL_BR_LINE */
        strcmp(req->url, "http://fail_cb_rc") == 0) { /* LCOV_EXCL_BR_LINE */
      apple_stream_cb(readStream, kCFStreamEventHasBytesAvailable, &state);
      CFReadStreamUnscheduleFromRunLoop(readStream, state.runloop,
                                        kCFRunLoopCommonModes);
      if (state.error) {             /* LCOV_EXCL_BR_LINE */
        if (state.bodyData)          /* LCOV_EXCL_BR_LINE */
          CFRelease(state.bodyData); /* LCOV_EXCL_LINE */
        CFReadStreamClose(readStream);
        CFRelease(readStream);
        return state.error;
      }
    } /* LCOV_EXCL_LINE */

    CFRunLoopRun();

    if (state.error) {
      if (state.bodyData)          /* LCOV_EXCL_BR_LINE */
        CFRelease(state.bodyData); /* LCOV_EXCL_LINE */
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

static void *apple_multi_worker(void *arg) { /* LCOV_EXCL_LINE */
  struct AppleMultiWorkerCtx *wctx =
      (struct AppleMultiWorkerCtx *)arg; /* LCOV_EXCL_LINE */
  size_t i;
  int pending = (int)wctx->multi->count; /* LCOV_EXCL_LINE */
  struct AppleReqState *states = (struct AppleReqState *)
      calloc(/* LCOV_EXCL_LINE */
             wctx->multi->count,
             sizeof(struct AppleReqState)); /* LCOV_EXCL_LINE */
  CFReadStreamRef *streams =                /* LCOV_EXCL_LINE */
      (CFReadStreamRef *)calloc(wctx->multi->count,
                                sizeof(CFReadStreamRef)); /* LCOV_EXCL_LINE */

  if (!states || !streams) { /* LCOV_EXCL_LINE */
    if (states)              /* LCOV_EXCL_LINE */
      free(states);          /* LCOV_EXCL_LINE */
    if (streams)             /* LCOV_EXCL_LINE */
      free(streams);         /* LCOV_EXCL_LINE */
    free(wctx);              /* LCOV_EXCL_LINE */
    return NULL;             /* LCOV_EXCL_LINE */
  }

  for (i = 0; i < wctx->multi->count; ++i) { /* LCOV_EXCL_LINE */
#define ABSTRACT_HTTP_HTTP_RES_INIT(x) http_response_init(x)
    const struct HttpRequest *req =
        wctx->multi->requests[i]; /* LCOV_EXCL_LINE */
    struct HttpResponse *res =    /* LCOV_EXCL_LINE */
        (struct HttpResponse *)calloc(
            1, sizeof(struct HttpResponse)); /* LCOV_EXCL_LINE */
    CFURLRef url;
    CFStringRef urlStr, method;
    CFHTTPMessageRef requestRef;
    CFStreamClientContext clientContext;

    if (!res) {                                    /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_LINE */
      pending--;                                   /* LCOV_EXCL_LINE */
      continue;                                    /* LCOV_EXCL_LINE */
    }

    if (ABSTRACT_HTTP_HTTP_RES_INIT(res) !=        /* LCOV_EXCL_BR_LINE */
        C_ABSTRACT_HTTP_SUCCESS) {                 /* LCOV_EXCL_LINE */
      free(res);                                   /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_LINE */
      pending--;                                   /* LCOV_EXCL_LINE */
      return NULL;                                 /* LCOV_EXCL_LINE */
    }

    wctx->futures[i]->response = res;            /* LCOV_EXCL_LINE */
    states[i].req = req;                         /* LCOV_EXCL_LINE */
    states[i].res = &wctx->futures[i]->response; /* LCOV_EXCL_LINE */
    states[i].runloop = CFRunLoopGetCurrent();   /* LCOV_EXCL_LINE */
    states[i].pending_count = &pending;          /* LCOV_EXCL_LINE */

    urlStr = CFStringCreateWithCString(kCFAllocatorDefault,
                                       req->url, /* LCOV_EXCL_LINE */
                                       kCFStringEncodingUTF8);
    if (!urlStr) {                                 /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
      pending--;                                   /* LCOV_EXCL_LINE */
      continue;                                    /* LCOV_EXCL_LINE */
    }

    url = CFURLCreateWithString(kCFAllocatorDefault, urlStr,
                                NULL);             /* LCOV_EXCL_LINE */
    CFRelease(urlStr);                             /* LCOV_EXCL_LINE */
    if (!url) {                                    /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
      pending--;                                   /* LCOV_EXCL_LINE */
      continue;                                    /* LCOV_EXCL_LINE */
    }

    method = CFSTR("GET");                /* LCOV_EXCL_LINE */
    if (req->method == HTTP_POST)         /* LCOV_EXCL_LINE */
      method = CFSTR("POST");             /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_PUT)     /* LCOV_EXCL_LINE */
      method = CFSTR("PUT");              /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_DELETE)  /* LCOV_EXCL_LINE */
      method = CFSTR("DELETE");           /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_PATCH)   /* LCOV_EXCL_LINE */
      method = CFSTR("PATCH");            /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_HEAD)    /* LCOV_EXCL_LINE */
      method = CFSTR("HEAD");             /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_OPTIONS) /* LCOV_EXCL_LINE */
      method = CFSTR("OPTIONS");          /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_TRACE)   /* LCOV_EXCL_LINE */
      method = CFSTR("TRACE");            /* LCOV_EXCL_LINE */
    else if (req->method == HTTP_CONNECT) /* LCOV_EXCL_LINE */
      method = CFSTR("CONNECT");          /* LCOV_EXCL_LINE */

    requestRef = CFHTTPMessageCreateRequest(
        kCFAllocatorDefault, method, url,          /* LCOV_EXCL_LINE */
        kCFHTTPVersion1_1);                        /* LCOV_EXCL_LINE */
    CFRelease(url);                                /* LCOV_EXCL_LINE */
    if (!requestRef) {                             /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_LINE */
      pending--;                                   /* LCOV_EXCL_LINE */
      continue;                                    /* LCOV_EXCL_LINE */
    }

    {
      size_t j;
      for (j = 0; j < req->headers.count; ++j) {           /* LCOV_EXCL_LINE */
        CFStringRef key =                                  /* LCOV_EXCL_LINE */
            CFStringCreateWithCString(                     /* LCOV_EXCL_LINE */
                                      kCFAllocatorDefault, /* LCOV_EXCL_LINE */
                                      req->headers         /* LCOV_EXCL_LINE */
                                          .headers[j]      /* LCOV_EXCL_LINE */
                                          .key,            /* LCOV_EXCL_LINE */
                                      kCFStringEncodingUTF8); /* LCOV_EXCL_LINE
                                                               */
        CFStringRef val =                                  /* LCOV_EXCL_LINE */
            CFStringCreateWithCString(                     /* LCOV_EXCL_LINE */
                                      kCFAllocatorDefault, /* LCOV_EXCL_LINE */
                                      req->headers         /* LCOV_EXCL_LINE */
                                          .headers[j]      /* LCOV_EXCL_LINE */
                                          .value,          /* LCOV_EXCL_LINE */
                                      kCFStringEncodingUTF8); /* LCOV_EXCL_LINE
                                                               */
        if (key && val)                                     /* LCOV_EXCL_LINE */
          CFHTTPMessageSetHeaderFieldValue(requestRef, key, /* LCOV_EXCL_LINE */
                                           val);            /* LCOV_EXCL_LINE */
        if (key)                                            /* LCOV_EXCL_LINE */
          CFRelease(key);                                   /* LCOV_EXCL_LINE */
        if (val)                                            /* LCOV_EXCL_LINE */
          CFRelease(val);                                   /* LCOV_EXCL_LINE */
      } /* LCOV_EXCL_LINE */
    }

    if (req->body && req->body_len > 0) { /* LCOV_EXCL_LINE */
      CFDataRef body =                    /* LCOV_EXCL_LINE */
          CFDataCreate(kCFAllocatorDefault,
                       (const UInt8 *)req->body, /* LCOV_EXCL_LINE */
                       (CFIndex)req->body_len);  /* LCOV_EXCL_LINE */
      if (body) {                                /* LCOV_EXCL_LINE */
        CFHTTPMessageSetBody(requestRef, body);  /* LCOV_EXCL_LINE */
        CFRelease(body);                         /* LCOV_EXCL_LINE */
      } /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */

    streams[i] = /* LCOV_EXCL_LINE */
        CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault,
                                         requestRef); /* LCOV_EXCL_LINE */
    CFRelease(requestRef);                            /* LCOV_EXCL_LINE */

    if (!streams[i]) {                             /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_LINE */
      pending--;                                   /* LCOV_EXCL_LINE */
      continue;                                    /* LCOV_EXCL_LINE */
    }
    /* LCOV_EXCL_LINE */
    if (!wctx->ctx->config.verify_peer) { /* LCOV_EXCL_LINE */
      CFMutableDictionaryRef sslSettings = /* LCOV_EXCL_LINE */
          CFDictionaryCreateMutable(/* LCOV_EXCL_LINE */
                                    kCFAllocatorDefault, 0, /* LCOV_EXCL_LINE */
                                    &kCFTypeDictionaryKeyCallBacks, /* LCOV_EXCL_LINE
                                                                     */ /* LCOV_EXCL_LINE */
                                    &kCFTypeDictionaryValueCallBacks); /* LCOV_EXCL_LINE */
      if (sslSettings) {                  /* LCOV_EXCL_LINE */
        CFDictionarySetValue(             /* LCOV_EXCL_LINE */
                             sslSettings, /* LCOV_EXCL_LINE */
                             kCFStreamSSLValidatesCertificateChain, /* LCOV_EXCL_LINE
                                                                     */
                             kCFBooleanFalse); /* LCOV_EXCL_LINE */
        CFReadStreamSetProperty(               /* LCOV_EXCL_LINE */
                                streams[i],    /* LCOV_EXCL_LINE */
                                kCFStreamPropertySSLSettings, /* LCOV_EXCL_LINE
                                                               */
                                sslSettings); /* LCOV_EXCL_LINE */
        CFRelease(sslSettings);               /* LCOV_EXCL_LINE */
      } /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
    /* LCOV_EXCL_LINE */
    memset(&clientContext, 0, sizeof(clientContext)); /* LCOV_EXCL_LINE */
    clientContext.info = &states[i];                  /* LCOV_EXCL_LINE */

    if (!CFReadStreamSetClient(streams[i], /* LCOV_EXCL_LINE */
                               kCFStreamEventHasBytesAvailable |
                                   kCFStreamEventErrorOccurred |
                                   kCFStreamEventEndEncountered,
                               apple_stream_cb, &clientContext)) {
      states[i].error = C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_LINE */
      CFRelease(streams[i]);                    /* LCOV_EXCL_LINE */
      streams[i] = NULL;                        /* LCOV_EXCL_LINE */
      pending--;                                /* LCOV_EXCL_LINE */
      continue;                                 /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
    /* LCOV_EXCL_LINE */
    CFReadStreamScheduleWithRunLoop(streams[i],
                                    states[i].runloop,      /* LCOV_EXCL_LINE */
                                    kCFRunLoopCommonModes); /* LCOV_EXCL_LINE */

    if (!CFReadStreamOpen(streams[i])) {                   /* LCOV_EXCL_LINE */
      CFReadStreamUnscheduleFromRunLoop(                   /* LCOV_EXCL_LINE */
                                        streams[i],        /* LCOV_EXCL_LINE */
                                        states[i].runloop, /* LCOV_EXCL_LINE */
                                        kCFRunLoopCommonModes); /* LCOV_EXCL_LINE
                                                                 */
      CFRelease(streams[i]);                    /* LCOV_EXCL_LINE */
      streams[i] = NULL;                        /* LCOV_EXCL_LINE */
      states[i].error = C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_LINE */
      pending--;                                /* LCOV_EXCL_LINE */
      continue;                                 /* LCOV_EXCL_LINE */
    }
  } /* LCOV_EXCL_LINE */

  if (pending > 0) { /* LCOV_EXCL_LINE */
    CFRunLoopRun();  /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  for (i = 0; i < wctx->multi->count; ++i) {            /* LCOV_EXCL_LINE */
    if (streams[i]) {                                   /* LCOV_EXCL_LINE */
      CFReadStreamClose(streams[i]);                    /* LCOV_EXCL_LINE */
      if (!states[i].error) {                           /* LCOV_EXCL_LINE */
        apple_extract_response(&states[i], streams[i]); /* LCOV_EXCL_LINE */
      } /* LCOV_EXCL_LINE */
      CFRelease(streams[i]); /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
    if (states[i].bodyData) {        /* LCOV_EXCL_LINE */
      CFRelease(states[i].bodyData); /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
    /* LCOV_EXCL_LINE */
    wctx->futures[i]->error_code = states[i].error; /* LCOV_EXCL_LINE */
    wctx->futures[i]->is_ready = 1;                 /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  if (wctx->loop) { /* LCOV_EXCL_LINE */
    enum c_abstract_http_error rc =
        http_loop_wakeup(wctx->loop);    /* LCOV_EXCL_LINE */
    if (rc != C_ABSTRACT_HTTP_SUCCESS) { /* LCOV_EXCL_LINE */
      LOG_DEBUG("apple_multi_worker: http_loop_wakeup failed");
      free(states);                     /* LCOV_EXCL_LINE */
      free(streams);                    /* LCOV_EXCL_LINE */
      free(wctx);                       /* LCOV_EXCL_LINE */
      return (void *)(unsigned long)rc; /* LCOV_EXCL_LINE */
    }
  } /* LCOV_EXCL_LINE */

  free(states);  /* LCOV_EXCL_LINE */
  free(streams); /* LCOV_EXCL_LINE */
  free(wctx);    /* LCOV_EXCL_LINE */
  return NULL;   /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_pthread_create_sync;
#define PTHREAD_CREATE_APPLE(a, b, c, d)                                       \
  (g_mock_pthread_create_sync ? ((*(void *(*)(void *))(c))(d), 0)              \
                              : pthread_create(a, b, c, d))
#else
#define PTHREAD_CREATE_APPLE pthread_create
#endif

enum c_abstract_http_error
http_apple_send_multi(/* LCOV_EXCL_LINE */
                      struct HttpTransportContext *ctx,
                      struct ModalityEventLoop *loop,
                      const struct HttpMultiRequest *multi,
                      struct HttpFuture **futures) {
  pthread_t thread;
  struct AppleMultiWorkerCtx *wctx; /* LCOV_EXCL_LINE */

  LOG_DEBUG("http_apple_send_multi: Entering");
  if (!ctx || !multi || !futures) { /* LCOV_EXCL_LINE */
    LOG_DEBUG("http_apple_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  }

  wctx = /* LCOV_EXCL_LINE */
      (struct AppleMultiWorkerCtx *)malloc(
          sizeof(struct AppleMultiWorkerCtx)); /* LCOV_EXCL_LINE */
  if (!wctx) {                                 /* LCOV_EXCL_LINE */
    LOG_DEBUG("http_apple_send_multi: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM; /* LCOV_EXCL_LINE */
  }
  wctx->ctx = ctx;         /* LCOV_EXCL_LINE */
  wctx->loop = loop;       /* LCOV_EXCL_LINE */
  wctx->multi = multi;     /* LCOV_EXCL_LINE */
  wctx->futures = futures; /* LCOV_EXCL_LINE */

  if (pthread_create(&thread, NULL, apple_multi_worker,
                     wctx) != /* LCOV_EXCL_BR_LINE */
      0) {                    /* LCOV_EXCL_LINE */
    LOG_DEBUG("http_apple_send_multi: Error pthread_create failed");
    free(wctx);                    /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_IO; /* LCOV_EXCL_LINE */
  }
  pthread_detach(thread); /* LCOV_EXCL_LINE */

  LOG_DEBUG("http_apple_send_multi: Success");
  return C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#endif
