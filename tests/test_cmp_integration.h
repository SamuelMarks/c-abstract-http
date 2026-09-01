/* LCOV_EXCL_BR_START */
#ifndef TEST_CMP_INTEGRATION_H
#define TEST_CMP_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include "greatest.h"
#include <c_abstract_http/cmp_integration.h>
/* clang-format on */

/* LCOV_EXCL_START */ TEST test_modality_adapter(void) { /* LCOV_EXCL_STOP */
  enum ExecutionModality out;

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      cmp_http_modality_adapter(CMP_MODALITY_SYNC_SINGLE, &out));
  /* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_SYNC, out); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      cmp_http_modality_adapter(CMP_MODALITY_SYNC_MULTI, &out));
  /* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_THREAD_POOL,
                                  out); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      cmp_http_modality_adapter(CMP_MODALITY_ASYNC_SINGLE, &out));
  /* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_ASYNC, out); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      cmp_http_modality_adapter(CMP_MODALITY_ASYNC_MULTI, &out));
  /* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_ASYNC, out); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      cmp_http_modality_adapter(CMP_MODALITY_GREENTHREADS, &out));
  /* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_GREENTHREAD,
                                  out); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      cmp_http_modality_adapter(CMP_MODALITY_MULTIPROCESS_ACTOR, &out));
  /* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_MULTIPROCESS,
                                  out); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ cmp_http_modality_adapter(
                999, &out)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_inject_config(void) { /* LCOV_EXCL_STOP */
  struct CmpAppConfig cmp_cfg = {CMP_MODALITY_SYNC_MULTI, 4,
                                 /* LCOV_EXCL_START */ 16}; /* LCOV_EXCL_STOP */
  struct HttpConfig http_cfg;

  {
    enum c_abstract_http_error rc_test = http_config_init(&http_cfg);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
                                cmp_http_inject_config(&cmp_cfg, &http_cfg));

/* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_THREAD_POOL,
                                http_cfg.modality);        /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(4, http_cfg.min_threads);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(16, http_cfg.max_threads); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ cmp_cfg.modality =
    CMP_MODALITY_SYNC_SINGLE;                            /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
                                cmp_http_inject_config(&cmp_cfg, &http_cfg));
/* LCOV_EXCL_START */ ASSERT_EQ(MODALITY_SYNC,
                                http_cfg.modality); /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ http_config_free(&http_cfg); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ PASS();                      /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

static float g_last_pct = 0.0f;
static int mock_update_progress(
    void *ui_component,
    /* LCOV_EXCL_START */ float percentage) {    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)ui_component;      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_last_pct = percentage; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;                /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ TEST test_progress_adapter(void) { /* LCOV_EXCL_STOP */
  struct CmpProgressBinding binding = {
      NULL, mock_update_progress, 0,
      /* LCOV_EXCL_START */ NULL, /* LCOV_EXCL_STOP */
      NULL};

  /* LCOV_EXCL_START */ g_last_pct = 0.0f;                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
                                  cmp_http_progress_adapter(50, 100, &binding));
  /* LCOV_EXCL_START */ ASSERT_EQ(0.5f, g_last_pct); /* LCOV_EXCL_STOP */

  /* Test total_bytes == 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ cmp_http_progress_adapter(
                0, 0, &binding)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ binding.cancel_requested = 1; /* LCOV_EXCL_STOP */
  ASSERT_EQ(1,
            /* LCOV_EXCL_START */ cmp_http_progress_adapter(
                75, 100, &binding)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_cmp_integration_errors(void) { /* LCOV_EXCL_STOP */
  struct CmpAppConfig cmp_cfg;
  struct HttpConfig http_cfg;
  struct CmpProgressBinding binding;
  enum ExecutionModality mod;

  /* LCOV_EXCL_START */ memset(&cmp_cfg, 0,
                               sizeof(cmp_cfg)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&http_cfg, 0,
                               sizeof(http_cfg)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&binding, 0,
                               sizeof(binding)); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ cmp_http_modality_adapter(
                0, NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ cmp_http_modality_adapter(
                999, &mod)); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ cmp_http_inject_config(
                NULL, &http_cfg)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ cmp_http_inject_config(
                &cmp_cfg, NULL)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ cmp_cfg.modality = 999; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      cmp_http_inject_config(&cmp_cfg, &http_cfg));

  /* Progress adapter without binding returns 0 */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ cmp_http_progress_adapter(
                0, 0, NULL)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ binding.cancel_requested = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      1, cmp_http_progress_adapter(0, 0, &binding)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_cmp_progress_adapter_continue(void) { /* LCOV_EXCL_STOP */
  struct CmpProgressBinding binding;
  /* LCOV_EXCL_START */ memset(&binding, 0,
                               sizeof(binding));        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ binding.cancel_requested = 0;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ binding.update_progress = NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
                                  cmp_http_progress_adapter(10, 100, &binding));
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ SUITE(cmp_integration_suite) {     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_modality_adapter); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_inject_config);    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_progress_adapter); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_cmp_integration_errors); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_cmp_progress_adapter_continue); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TEST_CMP_INTEGRATION_H */

/* LCOV_EXCL_BR_STOP */
