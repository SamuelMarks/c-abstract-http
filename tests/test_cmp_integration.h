/**
 * @file test_cmp_integration.h
 * @brief Unit tests for C-Multiplatform (CMP) modality, UI progress, and
 * cooperative cancellation bindings.
 */

#ifndef TEST_CMP_INTEGRATION_H
#define TEST_CMP_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
#include "greatest.h"
#include <c_abstract_http/cmp_integration.h>
/* clang-format on */

TEST test_modality_adapter(void) {
  enum ExecutionModality out;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_modality_adapter(CMP_MODALITY_SYNC_SINGLE, &out));
  ASSERT_EQ(MODALITY_SYNC, out);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_modality_adapter(CMP_MODALITY_SYNC_MULTI, &out));
  ASSERT_EQ(MODALITY_THREAD_POOL, out);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_modality_adapter(CMP_MODALITY_ASYNC_SINGLE, &out));
  ASSERT_EQ(MODALITY_ASYNC, out);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_modality_adapter(CMP_MODALITY_ASYNC_MULTI, &out));
  ASSERT_EQ(MODALITY_ASYNC, out);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_modality_adapter(CMP_MODALITY_GREENTHREADS, &out));
  ASSERT_EQ(MODALITY_GREENTHREAD, out);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_modality_adapter(CMP_MODALITY_MULTIPROCESS_ACTOR, &out));
  ASSERT_EQ(MODALITY_MULTIPROCESS, out);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cmp_http_modality_adapter(999, &out));

  PASS();
}

TEST test_inject_config(void) {
  struct CmpAppConfig cmp_cfg = {CMP_MODALITY_SYNC_MULTI, 4, 16};
  struct HttpConfig http_cfg;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&http_cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_inject_config(&cmp_cfg, &http_cfg));

  ASSERT_EQ(MODALITY_THREAD_POOL, http_cfg.modality);
  ASSERT_EQ(4, http_cfg.min_threads);
  ASSERT_EQ(16, http_cfg.max_threads);

  cmp_cfg.modality = CMP_MODALITY_SYNC_SINGLE;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_inject_config(&cmp_cfg, &http_cfg));
  ASSERT_EQ(MODALITY_SYNC, http_cfg.modality);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&http_cfg));
  PASS();
}

static float g_last_pct = 0.0f;
static float g_fw_pct = 0.0f;
static int g_cmp_fail_mode = 0;

NO_DISCARD static c_abstract_http_error_t
mock_update_progress(void *ui_component, float percentage) {
  (void)ui_component;
  g_last_pct = percentage;
  if (g_cmp_fail_mode == 1) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  if (g_cmp_fail_mode == 3) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
mock_fw_ui_hook(void *context, void *ui_component, float percentage) {
  int *ctx_counter = (int *)context;
  (void)ui_component;
  if (ctx_counter) {
    (*ctx_counter)++;
  }
  g_fw_pct = percentage;
  if (g_cmp_fail_mode == 2) {
    return C_ABSTRACT_HTTP_ERR_CANCELED;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

struct cmp_mock_send_cfg {
  int mode;
  int *cancel_on_send;
};

NO_DISCARD static c_abstract_http_error_t
cmp_mock_transport_send(struct HttpTransportContext *tctx,
                        const struct HttpRequest *req,
                        struct HttpResponse **res) {
  struct cmp_mock_send_cfg *cfg = (struct cmp_mock_send_cfg *)tctx;
  struct HttpResponse *r;
  c_abstract_http_error_t rc;

  if (cfg && cfg->mode == 4) {
    cfg->mode = 0;
    if (cfg->cancel_on_send) {
      *(cfg->cancel_on_send) = 1;
    }
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (req->read_chunk) {
    char ubuf[16];
    size_t got = 0;
    for (;;) {
      rc = req->read_chunk(req->read_chunk_user_data, ubuf, sizeof(ubuf), &got);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
      if (got == 0) {
        break;
      }
    }
  }

  r = (struct HttpResponse *)calloc(1, sizeof(*r));
  if (!r) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  rc = http_response_init(r);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    free(r);
    return rc;
  }
  r->status_code = 200;

  if (req->on_chunk) {
    rc = req->on_chunk(req->on_chunk_user_data, "abcd", 4);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      c_abstract_http_error_t frc = http_response_free(r);
      free(r);
      return frc != C_ABSTRACT_HTTP_SUCCESS ? frc : rc;
    }
  } else if (!cfg || cfg->mode != 5) {
    r->body = malloc(5);
    if (!r->body) {
      c_abstract_http_error_t frc = http_response_free(r);
      free(r);
      return frc != C_ABSTRACT_HTTP_SUCCESS ? frc : C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(r->body, "done", 5);
    r->body_len = 4;
  }

  if (cfg && cfg->cancel_on_send) {
    *(cfg->cancel_on_send) = 1;
  }

  *res = r;
  return C_ABSTRACT_HTTP_SUCCESS;
}

struct cmp_upload_src {
  int step;
  int *trigger_cancel;
  int fail_read;
};

NO_DISCARD static c_abstract_http_error_t
cmp_mock_read_cb(void *ud, void *buf, size_t cap, size_t *out_len) {
  struct cmp_upload_src *s = (struct cmp_upload_src *)ud;
  (void)cap;
  if (s->fail_read == 1) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (s->step == 0) {
    s->step = 1;
    memcpy(buf, "1234", 4);
    *out_len = 4;
    if (s->trigger_cancel && s->fail_read == 0) {
      *(s->trigger_cancel) = 1;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->trigger_cancel && s->fail_read == 2) {
    *(s->trigger_cancel) = 1;
  }
  *out_len = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

NO_DISCARD static c_abstract_http_error_t
cmp_mock_chunk_cb(void *ud, const void *chunk, size_t len) {
  size_t *acc = (size_t *)ud;
  (void)chunk;
  if (acc) {
    *acc += len;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

TEST test_progress_adapter(void) {
  struct CmpProgressBinding binding = {NULL, mock_update_progress, 0, NULL,
                                       NULL};
  CmpFrameworkUiHookFn fw_ptr = mock_fw_ui_hook;
  int fw_calls = 0;

  g_last_pct = 0.0f;
  g_fw_pct = 0.0f;
  g_cmp_fail_mode = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_progress_adapter(50, 100, &binding));
  ASSERT_EQ(0.5f, g_last_pct);

  /* Framework UI hook dispatch */
  memcpy(&binding.framework_cb, &fw_ptr, sizeof(fw_ptr));
  binding.context = &fw_calls;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_progress_adapter(25, 100, &binding));
  ASSERT_EQ(0.25f, g_fw_pct);
  ASSERT_EQ(1, fw_calls);

  /* Error percolation from update_progress and framework_cb */
  g_cmp_fail_mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            cmp_http_progress_adapter(50, 100, &binding));
  g_cmp_fail_mode = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            cmp_http_progress_adapter(50, 100, &binding));
  g_cmp_fail_mode = 0;

  /* Test total_bytes == 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cmp_http_progress_adapter(0, 0, &binding));

  binding.cancel_requested = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            cmp_http_progress_adapter(75, 100, &binding));

  PASS();
}

TEST test_cmp_request_binding_and_cancellation(void) {
  struct HttpClient client;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  struct CmpProgressBinding binding;
  struct cmp_mock_send_cfg tcfg;
  struct cmp_upload_src up_src;
  size_t dl_bytes = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cmp_http_bind_request(NULL, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_init(&client));
  memset(&tcfg, 0, sizeof(tcfg));
  client.transport = (struct HttpTransportContext *)&tcfg;
  client.send = cmp_mock_transport_send;

  memset(&binding, 0, sizeof(binding));
  binding.update_progress = mock_update_progress;
  g_cmp_fail_mode = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_strdup("http://127.0.0.1/cmp", &req.url));

  /* Bind and unbind */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cmp_http_bind_request(&req, NULL));
  ASSERT_EQ(NULL, req.cancel_token);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cmp_http_bind_request(&req, &binding));
  ASSERT(req.cancel_token == &binding.cancel_requested);

  /* Buffered response progress (non-empty and empty body) */
  g_last_pct = 0.0f;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_send(&client, &req, &res));
  ASSERT_EQ(1.0f, g_last_pct);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  tcfg.mode = 5;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_send(&client, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;
  tcfg.mode = 0;

  /* Buffered response on_progress abort (CANCELED and NOMEM) */
  g_cmp_fail_mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  g_cmp_fail_mode = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_client_send(&client, &req, &res));
  g_cmp_fail_mode = 0;

  /* Pre-flight cancellation */
  binding.cancel_requested = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  binding.cancel_requested = 0;

  /* Post-flight cancellation triggered during transport execution */
  tcfg.cancel_on_send = &binding.cancel_requested;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  tcfg.cancel_on_send = NULL;
  binding.cancel_requested = 0;

  /* Retry cancellation between attempts */
  client.config.retry_policy = HTTP_RETRY_LINEAR;
  client.config.retry_count = 1;
  tcfg.mode = 4;
  tcfg.cancel_on_send = &binding.cancel_requested;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  tcfg.cancel_on_send = NULL;
  binding.cancel_requested = 0;
  client.config.retry_policy = HTTP_RETRY_NONE;
  client.config.retry_count = 0;

  /* Streaming upload + download progress & cancellation paths */
  memset(&up_src, 0, sizeof(up_src));
  req.read_chunk = cmp_mock_read_cb;
  req.read_chunk_user_data = &up_src;
  req.expected_body_len = 4;
  req.on_chunk = cmp_mock_chunk_cb;
  req.on_chunk_user_data = &dl_bytes;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_send(&client, &req, &res));
  ASSERT_EQ(4U, dl_bytes);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  /* Unknown expected_body_len == 0 upload progress + read error */
  memset(&up_src, 0, sizeof(up_src));
  req.expected_body_len = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_send(&client, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  memset(&up_src, 0, sizeof(up_src));
  up_src.fail_read = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, http_client_send(&client, &req, &res));

  /* Cancel triggered at upload EOF -> caught in client_progress_on_chunk_cb */
  memset(&up_src, 0, sizeof(up_src));
  up_src.fail_read = 2;
  up_src.trigger_cancel = &binding.cancel_requested;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  binding.cancel_requested = 0;

  /* Upload progress callback abort (CANCELED and NOMEM) */
  memset(&up_src, 0, sizeof(up_src));
  g_cmp_fail_mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  memset(&up_src, 0, sizeof(up_src));
  g_cmp_fail_mode = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_client_send(&client, &req, &res));
  g_cmp_fail_mode = 0;

  /* Cancel triggered mid-upload before second read_chunk / on_chunk */
  memset(&up_src, 0, sizeof(up_src));
  up_src.trigger_cancel = &binding.cancel_requested;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  binding.cancel_requested = 0;

  /* Download chunk progress abort (CANCELED and NOMEM) and cancel-only wrap */
  req.read_chunk = NULL;
  g_cmp_fail_mode = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  g_cmp_fail_mode = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, http_client_send(&client, &req, &res));
  g_cmp_fail_mode = 0;

  /* cancel_token set without on_progress (success and mid-upload cancel) */
  req.on_progress = NULL;
  memset(&up_src, 0, sizeof(up_src));
  req.read_chunk = cmp_mock_read_cb;
  req.read_chunk_user_data = &up_src;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_send(&client, &req, &res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_free(res));
  free(res);
  res = NULL;

  memset(&up_src, 0, sizeof(up_src));
  up_src.trigger_cancel = &binding.cancel_requested;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_CANCELED,
            http_client_send(&client, &req, &res));
  binding.cancel_requested = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_client_free(&client));
  PASS();
}

TEST test_cmp_integration_errors(void) {
  struct CmpAppConfig cmp_cfg;
  struct HttpConfig http_cfg;
  struct CmpProgressBinding binding;
  enum ExecutionModality mod;

  memset(&cmp_cfg, 0, sizeof(cmp_cfg));
  memset(&http_cfg, 0, sizeof(http_cfg));
  memset(&binding, 0, sizeof(binding));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cmp_http_modality_adapter(0, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cmp_http_modality_adapter(999, &mod));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cmp_http_inject_config(NULL, &http_cfg));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cmp_http_inject_config(&cmp_cfg, NULL));

  cmp_cfg.modality = 999;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cmp_http_inject_config(&cmp_cfg, &http_cfg));

  /* Progress adapter without binding returns 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, cmp_http_progress_adapter(50, 100, NULL));

  /* Progress adapter without update_progress callback returns 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            cmp_http_progress_adapter(50, 100, &binding));

  PASS();
}

SUITE(cmp_integration_suite) {
  RUN_TEST(test_modality_adapter);
  RUN_TEST(test_inject_config);
  RUN_TEST(test_progress_adapter);
  RUN_TEST(test_cmp_request_binding_and_cancellation);
  RUN_TEST(test_cmp_integration_errors);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_CMP_INTEGRATION_H */
