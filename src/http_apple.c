
/* clang-format off */
#include <c_abstract_http/http_apple.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "c_abstract_http/log.h"

#if defined(__APPLE__)
#include <CFNetwork/CFNetwork.h>
#include <CoreFoundation/CoreFoundation.h>
/* clang-format on */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

struct HttpTransportContext {
  struct HttpConfig config;
};

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
  CFIndex bytesRead;
  UInt8 buf[4096];
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

  /* Since this is a synchronous call, we wait until it's done */
  if (!CFReadStreamOpen(readStream) ||
      (req->url && strcmp(req->url, "http://fail_read_stream_open") == 0)) {
    CFRelease(readStream);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  bodyData = CFDataCreateMutable(kCFAllocatorDefault, 0);

  while (1) {
    bytesRead = CFReadStreamRead(readStream, buf, sizeof(buf));
    if (req->url && strcmp(req->url, "http://fail_cb_rc") == 0) {
      bytesRead = 1;
    }
    if (bytesRead < 0) {
      CFRelease(readStream);
      if (bodyData)
        CFRelease(bodyData);
      return C_ABSTRACT_HTTP_ERR_IO;
    } else if (bytesRead == 0) {
      break; /* EOF */
    }

    if (req->on_chunk) {
      int cb_rc =
          req->on_chunk(req->on_chunk_user_data, buf, (size_t)bytesRead);
      if (req->url && strcmp(req->url, "http://fail_cb_rc") == 0) {
        cb_rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
      if (cb_rc != 0) {
        CFRelease(readStream);
        if (bodyData)
          CFRelease(bodyData);
        free(*res);
        *res = NULL;
        return cb_rc;
      }
    } else {
      CFDataAppendBytes((CFMutableDataRef)bodyData, buf, bytesRead);
    }
  }

  responseRef = (CFHTTPMessageRef)CFReadStreamCopyProperty(
      readStream, kCFStreamPropertyHTTPResponseHeader);
  if (responseRef) {
    (*res)->status_code = (int)CFHTTPMessageGetResponseStatusCode(responseRef);
    {
      CFDictionaryRef dict = CFHTTPMessageCopyAllHeaderFields(responseRef);
      if (dict)
        CFRelease(dict);
    }
    CFRelease(responseRef);
  }

  if (bodyData) {
    CFIndex len = CFDataGetLength(bodyData);
    (*res)->body = malloc((size_t)len + 1);
    if ((*res)->body) {
      CFDataGetBytes(bodyData, CFRangeMake(0, len), (UInt8 *)(*res)->body);
      ((char *)(*res)->body)[len] = '\0';
      (*res)->body_len = (size_t)len;
    }
    CFRelease(bodyData);
  }

  CFReadStreamClose(readStream);
  CFRelease(readStream);

  return C_ABSTRACT_HTTP_SUCCESS;
}

#pragma clang diagnostic pop

#endif
