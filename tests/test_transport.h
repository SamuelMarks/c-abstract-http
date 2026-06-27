#ifndef TEST_TRANSPORT_H
#define TEST_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include "greatest.h"
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/transport.h>
#include "mock_alloc.h"
/* clang-format on */

TEST test_transport_global(void) {
  ASSERT_EQ(0, transport_global_init());
  transport_global_cleanup();
  PASS();
}

TEST test_transport_factory(void) {
  struct HttpClient client = {0};

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, transport_factory_init_client(NULL));

  ASSERT_EQ(0, transport_factory_init_client(&client));
  ASSERT(client.send != NULL);

  transport_factory_cleanup_client(NULL);

  transport_factory_cleanup_client(&client);
  ASSERT(client.transport == NULL);

  transport_factory_cleanup_client(
      &client); /* test client->transport == NULL case */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  {
    int rc = transport_factory_init_client(&client);
#if defined(_WIN32)
    if (rc != C_ABSTRACT_HTTP_ERR_IO && rc != C_ABSTRACT_HTTP_ERR_NOTSUP) {
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
    }
#else
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
#endif
  }
  g_mock_alloc_fail = 0;
#endif

  PASS();
}

SUITE(transport_suite) {
  RUN_TEST(test_transport_global);
  RUN_TEST(test_transport_factory);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* TEST_TRANSPORT_H */
