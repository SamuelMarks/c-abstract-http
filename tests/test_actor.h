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

/** @brief Documented */
struct TestActorState {
/** @brief Documented */
  int received_messages;
/** @brief Documented */
  int shutdown;
};

static int mock_actor_handler(struct AbstractHttpActor *actor, struct AbstractHttpMessage *msg) {
  struct TestActorState *state = NULL;
  (void)msg;
  if (abstract_http_actor_get_state(actor, (void **)&state) != 0 || !state)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  if (msg->type == ABSTRACT_HTTP_MSG_HTTP_SEND) {
    state->received_messages++;

    /* We need to get the bus from somewhere, typically passed in context or
       global. In this test, the sender is known to use the same bus. Actually,
       AbstractHttpMessageBus is opaque and we don't have a abstract_http_actor_get_bus, but for
       this test we'll just verify the count. */
  } else if (msg->type == ABSTRACT_HTTP_MSG_SHUTDOWN) {
    state->shutdown = 1;
  }

  return 0;
}

TEST test_actor_spawn_and_message(void) {
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor1 = NULL;
  struct AbstractHttpActor *actor2 = NULL;
  struct TestActorState state1 = {0, 0};
  struct TestActorState state2 = {0, 0};
  struct AbstractHttpMessage msg;
  const char *name1 = NULL;
  const char *name2 = NULL;
  (void)bus;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));
  ASSERT_EQ(
      0, abstract_http_actor_spawn(bus, "Actor1", mock_actor_handler, &state1, &actor1));
  ASSERT_EQ(
      0, abstract_http_actor_spawn(bus, "Actor2", mock_actor_handler, &state2, &actor2));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor1, &name1));
  ASSERT_STR_EQ("Actor1", name1);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor2, &name2));
  ASSERT_STR_EQ("Actor2", name2);

  msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND;
  msg.payload = NULL;
  msg.sender = actor1;
  msg.receiver = actor2;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));

  /* Nothing processed yet */
  ASSERT_EQ(0, state2.received_messages);

  /* Process */
  ASSERT_EQ(1, abstract_http_message_bus_process(bus));

  /* Actor 2 should have received it */
  ASSERT_EQ(1, state2.received_messages);

  /* Send shutdown AND another message */
  msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND;
  msg.receiver = actor1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));

  msg.type = ABSTRACT_HTTP_MSG_SHUTDOWN;
  msg.receiver = actor1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));

  ASSERT_EQ(2, abstract_http_message_bus_process(bus));
  ASSERT_EQ(1, state1.shutdown);

  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);
  PASS();
}

static int mock_bus_init(struct AbstractHttpMessageBus **bus) {
  if (!bus)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *bus = (struct AbstractHttpMessageBus *)1;
  return 0;
}
static void mock_bus_free(struct AbstractHttpMessageBus *bus) { (void)bus; }
static int mock_bus_process(struct AbstractHttpMessageBus *bus) {

  if (!bus)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return 0;
}
static int mock_actor_spawn(struct AbstractHttpMessageBus *bus, const char *name,
                            abstract_http_actor_handler_cb handler, void *state,
                            struct AbstractHttpActor **actor) {

  if (!bus || !name || !actor)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  (void)handler;
  (void)state;
  *actor = (struct AbstractHttpActor *)1;
  return 0;
}
static int mock_actor_send(struct AbstractHttpMessageBus *bus,
                           const struct AbstractHttpMessage *msg) {

  if (!bus || !msg)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return 0;
}
static int mock_actor_get_state(struct AbstractHttpActor *actor, void **state) {

  if (!actor || !state)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *state = NULL;
  return 0;
}
static int mock_actor_get_name(const struct AbstractHttpActor *actor,
                               const char **name) {
  if (!actor || !name)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *name = "mock";
  return 0;
}

static int dummy_handler(struct AbstractHttpActor *self, struct AbstractHttpMessage *msg);

TEST test_actor_hooks(void) {
  struct AbstractHttpActorHooks hooks;
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  struct AbstractHttpMessage msg;
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

  abstract_http_actor_set_hooks(&hooks);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_process(bus));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_spawn(bus, "mock", NULL, NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_state(actor, &state));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor, &name));
  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);

  {
    struct AbstractHttpActorHooks z;
    memset(&z, 0, sizeof(z));
    abstract_http_actor_set_hooks(&z);
  }
  PASS();
}

TEST test_actor_errors(void) {
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  struct AbstractHttpMessage msg;
  (void)bus;
  memset(&msg, 0, sizeof(msg));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_message_bus_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_message_bus_process(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(NULL, "test", NULL, NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(NULL, &msg));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, NULL, dummy_handler, NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, "test", NULL, NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, NULL));

  msg.receiver = actor;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(bus, NULL));
  msg.receiver = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(bus, &msg));

  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);

  abstract_http_actor_set_hooks(NULL);

  PASS();
}

static int dummy_handler(struct AbstractHttpActor *self, struct AbstractHttpMessage *msg) {
  (void)self;
  (void)msg;
  return 0;
}

TEST test_actor_capacity(void) {
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  int i;
  (void)bus;

  /* manual coverage for dummy_handler */
  dummy_handler(NULL, NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));

  /* Exceed initial capacity of 16 */
  for (i = 0; i < 20; i++) {
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, &actor));
  }

  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);
  PASS();
}

TEST test_actor_getters(void) {
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  void *state = NULL;
  const char *name = NULL;
  (void)bus;

  abstract_http_message_bus_init(&bus);
  abstract_http_actor_spawn(bus, "myactor", dummy_handler, (void *)0x123, &actor);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_state(actor, &state));
  ASSERT_EQ((void *)0x123, state);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor, &name));
  ASSERT_STR_EQ("myactor", name);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_state(NULL, &state));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_state(actor, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_name(NULL, &name));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_name(actor, NULL));

  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);
  PASS();
}

#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_actor_oom(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  struct AbstractHttpMessage msg;
  char *out_str = NULL;
  int str_rc;
  (void)bus;

  memset(&msg, 0, sizeof(msg));

  /* Test bus init OOM on calloc */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_message_bus_init(&bus);
  printf("abstract_http_message_bus_init returned %d\n", rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* Test bus init OOM on actors array */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = abstract_http_message_bus_init(&bus);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  rc = abstract_http_message_bus_init(&bus);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  str_rc = c_abstract_http_mock_strdup("dummy", &out_str);
  ASSERT_EQ(0, str_rc);
  if (out_str)
    free(out_str);

  rc = abstract_http_actor_spawn(bus, "dummy", dummy_handler, NULL, &actor);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  msg.receiver = actor;

  /* Test actor send OOM */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_actor_send(bus, &msg);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  /* Test actor spawn OOM on realloc */
  {
    int i;
    for (i = 0; i < 15; i++) { /* 15 + dummy = 16 actors */
      abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, &actor);
    }
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    rc =
        abstract_http_actor_spawn(bus, "test_oom", dummy_handler, NULL, &actor);
    {
      int rc_test_tmp = rc;
      g_mock_alloc_fail = 0;
      ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
    }

    /* Now successfully spawn one so the next tests don't shift */
    abstract_http_actor_spawn(bus, "test_success", dummy_handler, NULL, &actor);
  }

  /* Test actor spawn OOM on calloc */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = abstract_http_actor_spawn(bus, "test2", dummy_handler, NULL, &actor);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);

  /* Test actor spawn OOM on strdup */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  rc = abstract_http_actor_spawn(bus, "test3", dummy_handler, NULL, &actor);
  printf("test3 spawn returned %d\n", rc);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);
  PASS();
}
#endif

TEST test_actor_queued_free_and_tail(void) {
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  struct AbstractHttpMessage msg1;
  struct AbstractHttpMessage msg2;
  (void)bus;

  memset(&msg1, 0, sizeof(msg1));
  memset(&msg2, 0, sizeof(msg2));

  abstract_http_message_bus_init(&bus);
  abstract_http_actor_spawn(bus, "myactor", dummy_handler, NULL, &actor);

  msg1.receiver = actor;
  msg2.receiver = actor;

  abstract_http_actor_send(bus, &msg1);
  abstract_http_actor_send(bus, &msg2); /* hits tail->next logic */

  /* don't process, just free, hitting lines 100-102 */
  abstract_http_message_bus_free(bus);
  abstract_http_message_bus_free(NULL);
  PASS();
}

TEST test_actor_mock_nulls(void) {
  struct AbstractHttpMessageBus *bus = NULL;
  struct AbstractHttpActor *actor = NULL;
  const char *name = NULL;
  void *state = NULL;
  (void)bus;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_bus_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_bus_process(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_spawn(NULL, "test", NULL, NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_spawn((struct AbstractHttpMessageBus *)1, NULL, NULL,
                             NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_spawn((struct AbstractHttpMessageBus *)1, "test", NULL,
                             NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_actor_send(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_send((struct AbstractHttpMessageBus *)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_actor_get_state(NULL, &state));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_get_state((struct AbstractHttpActor *)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_actor_get_name(NULL, &name));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_get_name((struct AbstractHttpActor *)1, NULL));

  {
    struct AbstractHttpMessage msg;
    memset(&msg, 0, sizeof(msg));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_actor_handler(NULL, &msg));
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
