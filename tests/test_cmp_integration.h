#ifndef TEST_CMP_INTEGRATION_H
#define TEST_CMP_INTEGRATION_H

/* clang-format off */
#include "greatest.h"
#include <c_abstract_http/cmp_integration.h>
/* clang-format on */

TEST test_modality_adapter(void) {
  enum ExecutionModality out;

  ASSERT_EQ(0, cmp_http_modality_adapter(CMP_MODALITY_SYNC_SINGLE, &out));
  ASSERT_EQ(MODALITY_SYNC, out);

  ASSERT_EQ(0, cmp_http_modality_adapter(CMP_MODALITY_SYNC_MULTI, &out));
  ASSERT_EQ(MODALITY_THREAD_POOL, out);

  ASSERT_EQ(0, cmp_http_modality_adapter(CMP_MODALITY_ASYNC_SINGLE, &out));
  ASSERT_EQ(MODALITY_ASYNC, out);

  ASSERT_EQ(0, cmp_http_modality_adapter(CMP_MODALITY_ASYNC_MULTI, &out));
  ASSERT_EQ(MODALITY_ASYNC, out);

  ASSERT_EQ(0, cmp_http_modality_adapter(CMP_MODALITY_GREENTHREADS, &out));
  ASSERT_EQ(MODALITY_GREENTHREAD, out);

  ASSERT_EQ(0,
            cmp_http_modality_adapter(CMP_MODALITY_MULTIPROCESS_ACTOR, &out));
  ASSERT_EQ(MODALITY_MULTIPROCESS, out);

  ASSERT_EQ(EINVAL, cmp_http_modality_adapter(999, &out));

  PASS();
}

TEST test_inject_config(void) {
  struct CmpAppConfig cmp_cfg = {CMP_MODALITY_SYNC_MULTI, 4, 16};
  struct HttpConfig http_cfg;

  http_config_init(&http_cfg);
  ASSERT_EQ(0, cmp_http_inject_config(&cmp_cfg, &http_cfg));

  ASSERT_EQ(MODALITY_THREAD_POOL, http_cfg.modality);
  ASSERT_EQ(4, http_cfg.min_threads);
  ASSERT_EQ(16, http_cfg.max_threads);

  http_config_free(&http_cfg);
  PASS();
}

static float g_last_pct = 0.0f;
static int mock_update_progress(void *ui_component, float percentage) {
  (void)ui_component;
  g_last_pct = percentage;
  return 0;
}

TEST test_progress_adapter(void) {
  struct CmpProgressBinding binding = {NULL, mock_update_progress, 0};

  g_last_pct = 0.0f;
  ASSERT_EQ(0, cmp_http_progress_adapter(50, 100, &binding));
  ASSERT_EQ(0.5f, g_last_pct);

  binding.cancel_requested = 1;
  ASSERT_EQ(1, cmp_http_progress_adapter(75, 100, &binding));

  PASS();
}

SUITE(cmp_integration_suite) {
  RUN_TEST(test_modality_adapter);
  RUN_TEST(test_inject_config);
  RUN_TEST(test_progress_adapter);
}

#endif /* TEST_CMP_INTEGRATION_H */
