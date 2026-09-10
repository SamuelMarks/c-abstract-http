/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_aria2.h>
#include <c_abstract_http/http_types.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_aria2_system_fail;
extern int g_mock_aria2_fopen_fail;
extern int g_mock_aria2_response_init_fail;
extern int g_mock_aria2_config_init_fail;
#endif

/** @brief Internal struct HttpTransportContext */
struct HttpTransportContext {
  /** @brief Configuration settings */
  struct HttpConfig config;
};

/**
 * @brief Initialize the global aria2 environment safely.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_aria2_global_init(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Clean up the global aria2 environment.
 *
 * @return C_ABSTRACT_HTTP_SUCCESS on success.
 */
enum c_abstract_http_error http_aria2_global_cleanup(void) {
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Initialize a new aria2 context.
 *
 * @param[out] ctx Pointer to receive context pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_aria2_context_init(struct HttpTransportContext **const ctx) {
  enum c_abstract_http_error rc;
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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_aria2_config_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_config_init(&(*ctx)->config);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_aria2_context_init: Error http_config_init failed with %d",
              rc);
    free(*ctx);
    *ctx = NULL;
    return rc;
  }

  LOG_DEBUG("http_aria2_context_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Free an aria2 transport context.
 *
 * @param[in] ctx The context to free.
 */
void http_aria2_context_free(struct HttpTransportContext *ctx) {
  LOG_DEBUG("http_aria2_context_free: Entering");
  if (ctx) {
    http_config_free(&ctx->config);
    free(ctx);
  }
  LOG_DEBUG("http_aria2_context_free: Exiting");
}

/**
 * @brief Apply configuration to the aria2 transport context.
 *
 * @param[in] ctx The context.
 * @param[in] config The configuration to apply.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error
http_aria2_config_apply(struct HttpTransportContext *ctx,
                        const struct HttpConfig *config) {
  LOG_DEBUG("http_aria2_config_apply: Entering");
  if (!ctx || !config) {
    LOG_DEBUG("http_aria2_config_apply: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->config.timeout_ms = config->timeout_ms;
  ctx->config.verify_peer = config->verify_peer;
  ctx->config.verify_host = config->verify_host;
  ctx->config.follow_redirects = config->follow_redirects;
  LOG_DEBUG("http_aria2_config_apply: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

/**
 * @brief Perform a single HTTP request using aria2.
 *
 * @param[in] ctx The context.
 * @param[in] req The request to send.
 * @param[out] res Pointer to receive response pointer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_aria2_send(struct HttpTransportContext *ctx,
                                           const struct HttpRequest *req,
                                           struct HttpResponse **const res) {
  enum c_abstract_http_error rc;
  int sys_rc;
  char cmd[4096];
  char tmp_filename[256];
  struct HttpResponse *new_res;
  FILE *f;
  long file_size;
  size_t bytes_read;

  new_res = NULL;
  f = NULL;

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

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_aria2_system_fail == 1) {
    sys_rc = 1;
  } else if (g_mock_aria2_system_fail == 2) {
    FILE *mf = fopen(tmp_filename, "wb");
    fclose(mf);
    sys_rc = 0;
  } else {
    FILE *mf = fopen(tmp_filename, "wb");
    fputs("HTTP/1.1 200 OK\r\n\r\nMock response body from aria2", mf);
    fclose(mf);
    sys_rc = 0;
  }
#else
  sys_rc = system(cmd);
#endif

  if (sys_rc != 0) {
    LOG_DEBUG("http_aria2_send: Error system() failed with %d", sys_rc);
    remove(tmp_filename);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  new_res = (struct HttpResponse *)calloc(1, sizeof(struct HttpResponse));
  if (!new_res) {
    LOG_DEBUG("http_aria2_send: Error ENOMEM");
    remove(tmp_filename);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_aria2_response_init_fail) {
    rc = C_ABSTRACT_HTTP_ERR_NOMEM;
  } else
#endif
  {
    rc = http_response_init(new_res);
  }
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    LOG_DEBUG("http_aria2_send: Error http_response_init failed with %d", rc);
    free(new_res);
    remove(tmp_filename);
    return rc;
  }

  new_res->status_code = 200;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_aria2_fopen_fail) {
    f = NULL;
  } else {
#endif
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    fopen_s(&f, tmp_filename, "rb");
#else
  f = fopen(tmp_filename, "rb");
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  }
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

  if (rc == C_ABSTRACT_HTTP_SUCCESS) {
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

/**
 * @brief Perform multiple HTTP requests concurrently via aria2.
 *
 * @param[in] ctx The context.
 * @param[in] loop The event loop context (unused).
 * @param[in] multi The multi request definition.
 * @param[out] futures Array of futures to populate.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error http_aria2_send_multi(
    struct HttpTransportContext *ctx, struct ModalityEventLoop *loop,
    const struct HttpMultiRequest *multi, struct HttpFuture **futures) {
  size_t i;
  enum c_abstract_http_error rc;
  struct HttpResponse *res;

  cah_cppcheck_mut_ptr((void *)ctx);
  (void)loop;

  if (!ctx || !multi || !futures) {
    LOG_DEBUG("http_aria2_send_multi: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < multi->count; i++) {
    res = NULL;
    rc = http_aria2_send(ctx, multi->requests[i], &res);
    futures[i]->response = res;
    futures[i]->error_code = rc;
    futures[i]->is_ready = 1;
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}
