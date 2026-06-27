#ifndef TEST_MOCK_COVERAGE_H
#define TEST_MOCK_COVERAGE_H
/* clang-format off */
#include "cdd_test_helpers/mock_server.h"
#include "greatest.h"
#include "mock_alloc.h"
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern char *c_abstract_http_mock_strdup(const char *s, char **out);
#if !defined(_WIN32)
extern void *c_abstract_http_mock_pthread_getspecific(unsigned long key);
extern int c_abstract_http_mock_pthread_create(void *thread, const void *attr,
                                               void *(*start_routine)(void *),
                                               void *arg);
#endif
/* extern int g_mock_recv_fail; */

TEST test_mock_alloc_coverage(void) {
  char *out = NULL;
  void *ptr;

  ASSERT_EQ(NULL, c_abstract_http_mock_strdup(NULL, &out));
  ASSERT_EQ(NULL, c_abstract_http_mock_strdup(NULL, NULL));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(NULL, c_abstract_http_mock_strdup("test", &out));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(NULL, c_abstract_http_mock_strdup("test", NULL));
  g_mock_alloc_fail = 0;

  ptr = c_abstract_http_mock_strdup("test", &out);
  ASSERT(ptr != NULL);
  free(ptr);
  ptr = c_abstract_http_mock_strdup("test", NULL);
  ASSERT(ptr != NULL);
  free(ptr);

  dummy_cb_thread(NULL);

  g_mock_select_fail = 1;
  ASSERT_EQ(-1, c_abstract_http_mock_select(0, NULL, NULL, NULL, NULL));
  g_mock_select_fail = 0;

  g_mock_time_jump = 1;
  g_mock_time_jump_count = 2;
  c_abstract_http_mock_math_get_current_time_ms();
  c_abstract_http_mock_math_get_current_time_ms();
  c_abstract_http_mock_math_get_current_time_ms();
  g_mock_time_jump = 0;

  PASS();
}

TEST test_mock_alloc_more(void) {
  char *out2 = NULL;

#if !defined(_WIN32)
  g_mock_pthread_fail = 1;
  ASSERT_EQ(NULL, c_abstract_http_mock_pthread_getspecific(0));

  g_mock_pthread_fail = 2;
  g_mock_alloc_count = 0;
  ASSERT_EQ(1, c_abstract_http_mock_pthread_create(NULL, NULL, NULL, NULL));
  g_mock_pthread_fail = 0;
#endif

  ASSERT_EQ(22, c_abstract_http_mock_cdd_strdup(NULL, &out2));
  ASSERT_EQ(22, c_abstract_http_mock_cdd_strdup(NULL, NULL));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_mock_cdd_strdup("test", &out2));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = c_abstract_http_mock_cdd_strdup("test", NULL);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  g_mock_recv_fail = 1;
  ASSERT_EQ(-1, c_abstract_http_mock_recv(0, NULL, 0, 0));
  g_mock_recv_fail = 0;

  PASS();
}

TEST test_mock_server_coverage(void) {
  struct MockServer_ *srv = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(1, mock_server_init((MockServerPtr *)&srv));
  ASSERT_EQ(NULL, srv);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(1, mock_server_init(NULL));
  g_mock_alloc_fail = 0;

  PASS();
}

SUITE(mock_coverage_suite) {
  RUN_TEST(test_mock_alloc_coverage);
  RUN_TEST(test_mock_alloc_more);
  RUN_TEST(test_mock_server_coverage);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
