/**
 * @file http_aria2.c
 * @brief Implementation of the aria2 backend.
 *
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_aria2.h>
#include <c_abstract_http/str.h>
#include <c_abstract_http/http_types.h>

/* clang-format on */

struct HttpTransportContext {
  struct HttpConfig *config;
};

int http_aria2_global_init(void) { return 0; }
void http_aria2_global_cleanup(void) {}

int http_aria2_context_init(struct HttpTransportContext **const ctx) {
  if (!ctx)
    return EINVAL;
  *ctx = (struct HttpTransportContext *)calloc(
      1, sizeof(struct HttpTransportContext));
  return *ctx ? 0 : ENOMEM;
}

void http_aria2_context_free(struct HttpTransportContext *ctx) {
  if (ctx) {
    if (ctx->config) {
      free(ctx->config);
    }
    free(ctx);
  }
}

int http_aria2_config_apply(struct HttpTransportContext *ctx,
                            const struct HttpConfig *config) {
  if (!ctx || !config)
    return EINVAL;
  /* Shallow copy config for now */
  if (!ctx->config)
    ctx->config = (struct HttpConfig *)malloc(sizeof(struct HttpConfig));
  if (ctx->config)
    memcpy(ctx->config, config, sizeof(struct HttpConfig));
  return 0;
}

int http_aria2_send(struct HttpTransportContext *ctx,
                    const struct HttpRequest *req,
                    struct HttpResponse **const res) {
  char cmd[4096];
  char tmp_filename[256];
  int rc = 0;
  struct HttpResponse *new_res = NULL;
  FILE *f = NULL;
  long file_size = 0;
  size_t bytes_read = 0;

  if (!ctx || !req || !res || !req->url)
    return EINVAL;

#if defined(_MSC_VER)
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
    remove(tmp_filename);
    return EIO;
  }

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    remove(tmp_filename);
    return ENOMEM;
  }

  if (http_response_init(new_res) != 0) {
    free(new_res);
    remove(tmp_filename);
    return ENOMEM;
  }

  new_res->status_code = 200;

#if defined(_MSC_VER)
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
        rc = ENOMEM;
      }
    }
    fclose(f);
  } else {
    rc = EIO;
  }

  remove(tmp_filename);

  if (rc != 0) {
    http_response_free(new_res);
    free(new_res);
    return rc;
  }

  *res = new_res;
  return 0;
}

int http_aria2_send_multi(struct HttpTransportContext *ctx,
                          struct ModalityEventLoop *loop,
                          const struct HttpMultiRequest *multi,
                          struct HttpFuture **futures) {
  (void)ctx;
  (void)loop;
  (void)multi;
  (void)futures;
  return ENOTSUP;
}
