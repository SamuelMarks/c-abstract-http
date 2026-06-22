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

  ASSERT_EQ(EINVAL, transport_factory_init_client(NULL));

  ASSERT_EQ(0, transport_factory_init_client(&client));
  ASSERT(client.send != NULL);

  transport_factory_cleanup_client(NULL);

  transport_factory_cleanup_client(&client);
  ASSERT(client.transport == NULL);

  transport_factory_cleanup_client(
      &client); /* test client->transport == NULL case */

  g_mock_alloc_count = 0;
  g_mock_alloc_fail = 1;
  ASSERT_EQ(ENOMEM, transport_factory_init_client(&client));
  g_mock_alloc_fail = 0;

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
