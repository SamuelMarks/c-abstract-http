
/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_aria2.h>
#include "c_abstract_http/log.h"
#include "str.h"
#include <c_abstract_http/http_types.h>
/* clang-format on */

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief config (variable) of struct HttpTransportContext */
  struct HttpConfig config;
};

enum c_abstract_http_error http_aria2_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}
enum c_abstract_http_error http_aria2_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
http_aria2_context_init(struct HttpTransportContext **ctx) {
  int rc;
  LOG_DEBUG("http_aria2_context_init: Entering");
  if (!ctx) {
    LOG_DEBUG("http_aria2_context_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *ctx = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  if (!*ctx) {
    LOG_DEBUG("http_aria2_context_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_config_init(&(*ctx)->config);
  if (rc != 0) {
    LOG_DEBUG("http_aria2_context_init: Error http_config_init failed with %d",
              rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_aria2_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void http_aria2_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_aria2_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_aria2_context_free: Exiting");
}

enum c_abstract_http_error
http_aria2_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  LOG_DEBUG("http_aria2_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_aria2_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config = *config;
  LOG_DEBUG("http_aria2_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error http_aria2_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **const res) {
  int rc;
  char cmd[4096];
  char tmp_filename[256];
  struct HttpResponse *new_res = NULL;
  FILE *f = NULL;
  long file_size;
  size_t bytes_read;

  LOG_DEBUG("http_aria2_send: Entering");
  if (!ctx || !req || !res || !req->url) {
    LOG_DEBUG("http_aria2_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(tmp_filename, sizeof(tmp_filename), "aria2c_tmp_%p.bin",
            (void *)req);
  sprintf_s(cmd, sizeof(cmd),
            "aria2c -q --allow-overwrite=true -d . -o %s \"%s\"", tmp_filename,
            req->url);
#else
  sprintf(tmp_filename, "aria2c_tmp_%p.bin", (void *)req);
  sprintf(cmd, "aria2c -q --allow-overwrite=true -d . -o %s \"%s\"",
          tmp_filename, req->url);
#endif

  rc = system(cmd);
  if (rc != 0) {
    LOG_DEBUG("http_aria2_send: Error system() failed with %d", rc);
    remove(tmp_filename);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    LOG_DEBUG("http_aria2_send: Error ENOMEM");
    remove(tmp_filename);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  rc = http_response_init(new_res);
  if (rc != 0) {
    LOG_DEBUG("http_aria2_send: Error http_response_init failed with %d", rc);
    free(new_res);
    remove(tmp_filename);
    return rc;
  }

  new_res->status_code = 200;

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  if (fopen_s(&f, tmp_filename, "rb") != 0) {
    f = NULL;
  }
#else
  f = fopen(tmp_filename, "rb");
#endif

  if (f) {
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size > 0) {
      new_res->body = malloc((size_t)file_size + 1);
      if (new_res->body) {
        bytes_read = fread(new_res->body, 1, (size_t)file_size, f);
        new_res->body_len = bytes_read;
        ((char *)new_res->body)[bytes_read] = '\0';
      } else {
        LOG_DEBUG("http_aria2_send: Error ENOMEM reading body");
        rc = C_ABSTRACT_HTTP_ERR_NOMEM;
      }
    }
    fclose(f);
  } else {
    LOG_DEBUG("http_aria2_send: Error EIO (cannot open temp file)");
    rc = C_ABSTRACT_HTTP_ERR_IO;
  }

  remove(tmp_filename);

  if (rc == 0) {
    *res = new_res;
    LOG_DEBUG("http_aria2_send: Success");
  } else {
    http_response_free(new_res);
    free(new_res);
    *res = NULL;
    LOG_DEBUG("http_aria2_send: Error returning %d", rc);
  }
  return rc;
}

enum c_abstract_http_error http_aria2_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  cah_cppcheck_mut_ptr((void *)ctx);
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_aria2_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    struct HttpResponse *res = NULL;
    int rc = http_aria2_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
