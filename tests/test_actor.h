#ifndef TEST_ACTOR_H
#define TEST_ACTOR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/actor.h>
/* clang-format on */

struct TestActorState {
  int received_messages;
  int shutdown;
};

static int mock_actor_handler(struct CddActor *actor, struct CddMessage *msg) {
  struct TestActorState *state = NULL;
  if (cdd_actor_get_state(actor, (void **)&state) != 0 || !state)
    return EINVAL;

  if (msg->type == CDD_MSG_HTTP_SEND) {
    state->received_messages++;

    /* We need to get the bus from somewhere, typically passed in context or
       global. In this test, the sender is known to use the same bus. Actually,
       CddMessageBus is opaque and we don't have a cdd_actor_get_bus, but for
       this test we'll just verify the count. */
  } else if (msg->type == CDD_MSG_SHUTDOWN) {
    state->shutdown = 1;
  }

  return 0;
}

TEST test_actor_spawn_and_message(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor1 = NULL;
  struct CddActor *actor2 = NULL;
  struct TestActorState state1 = {0, 0};
  struct TestActorState state2 = {0, 0};
  struct CddMessage msg;
  const char *name1 = NULL;
  const char *name2 = NULL;
  (void)bus;

  ASSERT_EQ(0, cdd_message_bus_init(&bus));
  ASSERT_EQ(
      0, cdd_actor_spawn(bus, "Actor1", mock_actor_handler, &state1, &actor1));
  ASSERT_EQ(
      0, cdd_actor_spawn(bus, "Actor2", mock_actor_handler, &state2, &actor2));

  ASSERT_EQ(0, cdd_actor_get_name(actor1, &name1));
  ASSERT_STR_EQ("Actor1", name1);
  ASSERT_EQ(0, cdd_actor_get_name(actor2, &name2));
  ASSERT_STR_EQ("Actor2", name2);

  msg.type = CDD_MSG_HTTP_SEND;
  msg.payload = NULL;
  msg.sender = actor1;
  msg.receiver = actor2;

  ASSERT_EQ(0, cdd_actor_send(bus, &msg));

  /* Nothing processed yet */
  ASSERT_EQ(0, state2.received_messages);

  /* Process */
  ASSERT_EQ(1, cdd_message_bus_process(bus));

  /* Actor 2 should have received it */
  ASSERT_EQ(1, state2.received_messages);

  /* Send shutdown */
  msg.type = CDD_MSG_SHUTDOWN;
  msg.receiver = actor1;
  ASSERT_EQ(0, cdd_actor_send(bus, &msg));

  ASSERT_EQ(1, cdd_message_bus_process(bus));
  ASSERT_EQ(1, state1.shutdown);

  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);
  PASS();
}

static int mock_bus_init(struct CddMessageBus **bus) {
  if (!bus)
    return EINVAL;
  *bus = (struct CddMessageBus *)1;
  return 0;
}
static void mock_bus_free(struct CddMessageBus *bus) { (void)bus; }
static int mock_bus_process(struct CddMessageBus *bus) {
  if (!bus)
    return EINVAL;
  return 0;
}
static int mock_actor_spawn(struct CddMessageBus *bus, const char *name,
                            cdd_actor_handler_cb handler, void *state,
                            struct CddActor **actor) {
  if (!bus || !name || !actor)
    return EINVAL;
  (void)handler;
  (void)state;
  *actor = (struct CddActor *)1;
  return 0;
}
static int mock_actor_send(struct CddMessageBus *bus,
                           const struct CddMessage *msg) {
  if (!bus || !msg)
    return EINVAL;
  return 0;
}
static int mock_actor_get_state(struct CddActor *actor, void **state) {
  if (!actor || !state)
    return EINVAL;
  *state = NULL;
  return 0;
}
static int mock_actor_get_name(const struct CddActor *actor,
                               const char **name) {
  if (!actor || !name)
    return EINVAL;
  *name = "mock";
  return 0;
}

TEST test_actor_hooks(void) {
  struct CddActorHooks hooks;
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  struct CddMessage msg;
  void *state = NULL;
  const char *name = NULL;
  (void)bus;
  memset(&msg, 0, sizeof(msg));

  hooks.bus_init = mock_bus_init;
  hooks.bus_free = mock_bus_free;
  hooks.bus_process = mock_bus_process;
  hooks.actor_spawn = mock_actor_spawn;
  hooks.actor_send = mock_actor_send;
  hooks.actor_get_state = mock_actor_get_state;
  hooks.actor_get_name = mock_actor_get_name;

  cdd_actor_set_hooks(&hooks);

  ASSERT_EQ(0, cdd_message_bus_init(&bus));
  ASSERT_EQ(0, cdd_message_bus_process(bus));
  ASSERT_EQ(0, cdd_actor_spawn(bus, "mock", NULL, NULL, &actor));
  ASSERT_EQ(0, cdd_actor_send(bus, &msg));
  ASSERT_EQ(0, cdd_actor_get_state(actor, &state));
  ASSERT_EQ(0, cdd_actor_get_name(actor, &name));
  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);

  {
    struct CddActorHooks z;
    memset(&z, 0, sizeof(z));
    cdd_actor_set_hooks(&z);
  }
  PASS();
}

TEST test_actor_errors(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  struct CddMessage msg;
  (void)bus;
  memset(&msg, 0, sizeof(msg));

  ASSERT_EQ(EINVAL, cdd_message_bus_init(NULL));
  ASSERT_EQ(EINVAL, cdd_message_bus_process(NULL));
  ASSERT_EQ(EINVAL, cdd_actor_spawn(NULL, "test", NULL, NULL, &actor));
  ASSERT_EQ(EINVAL, cdd_actor_send(NULL, &msg));

  ASSERT_EQ(0, cdd_message_bus_init(&bus));
  ASSERT_EQ(EINVAL, cdd_actor_spawn(bus, NULL, NULL, NULL, &actor));
  ASSERT_EQ(EINVAL, cdd_actor_spawn(bus, "test", NULL, NULL, NULL));
  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);

  PASS();
}

static int dummy_handler(struct CddActor *self, struct CddMessage *msg) {
  (void)self;
  (void)msg;
  return 0;
}

TEST test_actor_capacity(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  int i;
  (void)bus;

  /* manual coverage for dummy_handler */
  dummy_handler(NULL, NULL);

  ASSERT_EQ(0, cdd_message_bus_init(&bus));

  /* Exceed initial capacity of 16 */
  for (i = 0; i < 20; i++) {
    ASSERT_EQ(0, cdd_actor_spawn(bus, "test", dummy_handler, NULL, &actor));
  }

  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);
  PASS();
}

TEST test_actor_getters(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  void *state = NULL;
  const char *name = NULL;
  (void)bus;

  cdd_message_bus_init(&bus);
  cdd_actor_spawn(bus, "myactor", dummy_handler, (void *)0x123, &actor);

  ASSERT_EQ(0, cdd_actor_get_state(actor, &state));
  ASSERT_EQ((void *)0x123, state);

  ASSERT_EQ(0, cdd_actor_get_name(actor, &name));
  ASSERT_STR_EQ("myactor", name);

  ASSERT_EQ(EINVAL, cdd_actor_get_state(NULL, &state));
  ASSERT_EQ(EINVAL, cdd_actor_get_state(actor, NULL));
  ASSERT_EQ(EINVAL, cdd_actor_get_name(NULL, &name));
  ASSERT_EQ(EINVAL, cdd_actor_get_name(actor, NULL));

  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);
  PASS();
}

#include "mock_alloc.h"

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_actor_oom(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  struct CddMessage msg;
  int rc;
  char *out_str = NULL;
  int str_rc;
  (void)bus;

  memset(&msg, 0, sizeof(msg));

  /* Test bus init OOM on calloc */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_message_bus_init(&bus);
  printf("cdd_message_bus_init returned %d\n", rc);
  ASSERT_EQ(ENOMEM, rc);

  /* Test bus init OOM on actors array */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = cdd_message_bus_init(&bus);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(ENOMEM, rc_test_tmp, "%d");
  }

  rc = cdd_message_bus_init(&bus);
  ASSERT_EQ(0, rc);

  str_rc = c_cdd_strdup("dummy", &out_str);
  ASSERT_EQ(0, str_rc);
  if (out_str)
    free(out_str);

  rc = cdd_actor_spawn(bus, "dummy", dummy_handler, NULL, &actor);
  ASSERT_EQ(0, rc);

  msg.receiver = actor;

  /* Test actor send OOM */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_actor_send(bus, &msg);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(ENOMEM, rc_test_tmp, "%d");
  }

  /* Test actor spawn OOM on realloc */
  {
    int i;
    for (i = 0; i < 15; i++) { /* 15 + dummy = 16 actors */
      cdd_actor_spawn(bus, "test", dummy_handler, NULL, &actor);
    }
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    rc = cdd_actor_spawn(bus, "test_oom", dummy_handler, NULL, &actor);
    {
      int rc_test_tmp = rc;
      g_mock_alloc_fail = 0;
      ASSERT_EQ_FMT(ENOMEM, rc_test_tmp, "%d");
    }

    /* Now successfully spawn one so the next tests don't shift */
    cdd_actor_spawn(bus, "test_success", dummy_handler, NULL, &actor);
  }

  /* Test actor spawn OOM on calloc */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_actor_spawn(bus, "test2", dummy_handler, NULL, &actor);
  ASSERT_EQ(ENOMEM, rc);

  /* Test actor spawn OOM on strdup */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = cdd_actor_spawn(bus, "test3", dummy_handler, NULL, &actor);
  printf("test3 spawn returned %d\n", rc);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(ENOMEM, rc_test_tmp, "%d");
  }

  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);
  PASS();
}
#endif

TEST test_actor_queued_free_and_tail(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  struct CddMessage msg1;
  struct CddMessage msg2;
  (void)bus;

  memset(&msg1, 0, sizeof(msg1));
  memset(&msg2, 0, sizeof(msg2));

  cdd_message_bus_init(&bus);
  cdd_actor_spawn(bus, "myactor", dummy_handler, NULL, &actor);

  msg1.receiver = actor;
  msg2.receiver = actor;

  cdd_actor_send(bus, &msg1);
  cdd_actor_send(bus, &msg2); /* hits tail->next logic */

  /* don't process, just free, hitting lines 100-102 */
  cdd_message_bus_free(bus);
  cdd_message_bus_free(NULL);
  PASS();
}

TEST test_actor_mock_nulls(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor = NULL;
  const char *name = NULL;
  void *state = NULL;
  (void)bus;

  ASSERT_EQ(EINVAL, mock_bus_init(NULL));
  ASSERT_EQ(EINVAL, mock_bus_process(NULL));
  ASSERT_EQ(EINVAL, mock_actor_spawn(NULL, "test", NULL, NULL, &actor));
  ASSERT_EQ(EINVAL, mock_actor_spawn((struct CddMessageBus *)1, NULL, NULL,
                                     NULL, &actor));
  ASSERT_EQ(EINVAL, mock_actor_spawn((struct CddMessageBus *)1, "test", NULL,
                                     NULL, NULL));
  ASSERT_EQ(EINVAL, mock_actor_send(NULL, NULL));
  ASSERT_EQ(EINVAL, mock_actor_send((struct CddMessageBus *)1, NULL));
  ASSERT_EQ(EINVAL, mock_actor_get_state(NULL, &state));
  ASSERT_EQ(EINVAL, mock_actor_get_state((struct CddActor *)1, NULL));
  ASSERT_EQ(EINVAL, mock_actor_get_name(NULL, &name));
  ASSERT_EQ(EINVAL, mock_actor_get_name((struct CddActor *)1, NULL));

  {
    struct CddMessage msg;
    memset(&msg, 0, sizeof(msg));
    ASSERT_EQ(EINVAL, mock_actor_handler(NULL, &msg));
  }

  PASS();
}

SUITE(actor_suite) {
  RUN_TEST(test_actor_getters);
  RUN_TEST(test_actor_queued_free_and_tail);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_actor_oom);
#endif
  RUN_TEST(test_actor_spawn_and_message);
  RUN_TEST(test_actor_hooks);
  RUN_TEST(test_actor_errors);
  RUN_TEST(test_actor_capacity);
  RUN_TEST(test_actor_mock_nulls);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
