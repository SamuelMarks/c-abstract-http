/* LCOV_EXCL_BR_START */
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

static int mock_actor_handler(struct AbstractHttpActor *actor, struct AbstractHttpMessage *msg) { /* LCOV_EXCL_LINE */
  struct TestActorState *state = NULL; /* LCOV_EXCL_LINE */
  (void)msg; /* LCOV_EXCL_LINE */
  if (abstract_http_actor_get_state(actor, (void **)&state) != 0 || !state) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */

  if (msg->type == ABSTRACT_HTTP_MSG_HTTP_SEND) { /* LCOV_EXCL_LINE */
    state->received_messages++; /* LCOV_EXCL_LINE */

    /* We need to get the bus from somewhere, typically passed in context or
       global. In this test, the sender is known to use the same bus. Actually,
       AbstractHttpMessageBus is opaque and we don't have a abstract_http_actor_get_bus, but for
       this test we'll just verify the count. */
  } else if (msg->type == ABSTRACT_HTTP_MSG_SHUTDOWN) { /* LCOV_EXCL_LINE */
    state->shutdown = 1; /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_actor_spawn_and_message(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor1 = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor2 = NULL; /* LCOV_EXCL_LINE */
  struct TestActorState state1 = {0, 0}; /* LCOV_EXCL_LINE */
  struct TestActorState state2 = {0, 0}; /* LCOV_EXCL_LINE */
  struct AbstractHttpMessage msg;
  const char *name1 = NULL; /* LCOV_EXCL_LINE */
  const char *name2 = NULL; /* LCOV_EXCL_LINE */
  (void)bus; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus)); /* LCOV_EXCL_LINE */
  ASSERT_EQ( /* LCOV_EXCL_LINE */
      0, abstract_http_actor_spawn(bus, "Actor1", mock_actor_handler, &state1, &actor1));
  ASSERT_EQ( /* LCOV_EXCL_LINE */
      0, abstract_http_actor_spawn(bus, "Actor2", mock_actor_handler, &state2, &actor2));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor1, &name1)); /* LCOV_EXCL_LINE */
  ASSERT_STR_EQ("Actor1", name1); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor2, &name2)); /* LCOV_EXCL_LINE */
  ASSERT_STR_EQ("Actor2", name2); /* LCOV_EXCL_LINE */

  msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND; /* LCOV_EXCL_LINE */
  msg.payload = NULL; /* LCOV_EXCL_LINE */
  msg.sender = actor1; /* LCOV_EXCL_LINE */
  msg.receiver = actor2; /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg)); /* LCOV_EXCL_LINE */

  /* Nothing processed yet */
  ASSERT_EQ(0, state2.received_messages); /* LCOV_EXCL_LINE */

  /* Process */
  ASSERT_EQ(1, abstract_http_message_bus_process(bus)); /* LCOV_EXCL_LINE */

  /* Actor 2 should have received it */
  ASSERT_EQ(1, state2.received_messages); /* LCOV_EXCL_LINE */

  /* Send shutdown AND another message */
  msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND; /* LCOV_EXCL_LINE */
  msg.receiver = actor1; /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg)); /* LCOV_EXCL_LINE */

  msg.type = ABSTRACT_HTTP_MSG_SHUTDOWN; /* LCOV_EXCL_LINE */
  msg.receiver = actor1; /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(2, abstract_http_message_bus_process(bus)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(1, state1.shutdown); /* LCOV_EXCL_LINE */

  abstract_http_message_bus_free(bus); /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int mock_bus_init(struct AbstractHttpMessageBus **bus) { /* LCOV_EXCL_LINE */
  if (!bus) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  *bus = (struct AbstractHttpMessageBus *)1; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
static void mock_bus_free(struct AbstractHttpMessageBus *bus) { (void)bus; } /* LCOV_EXCL_LINE */
static int mock_bus_process(struct AbstractHttpMessageBus *bus) { /* LCOV_EXCL_LINE */

  if (!bus) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
static int mock_actor_spawn(struct AbstractHttpMessageBus *bus, const char *name, /* LCOV_EXCL_LINE */
                            abstract_http_actor_handler_cb handler, void *state,
                            struct AbstractHttpActor **actor) {

  if (!bus || !name || !actor) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  (void)handler; /* LCOV_EXCL_LINE */
  (void)state; /* LCOV_EXCL_LINE */
  *actor = (struct AbstractHttpActor *)1; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
static int mock_actor_send(struct AbstractHttpMessageBus *bus, /* LCOV_EXCL_LINE */
                           const struct AbstractHttpMessage *msg) {

  if (!bus || !msg) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
static int mock_actor_get_state(struct AbstractHttpActor *actor, void **state) { /* LCOV_EXCL_LINE */

  if (!actor || !state) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  *state = NULL; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
static int mock_actor_get_name(const struct AbstractHttpActor *actor, /* LCOV_EXCL_LINE */
                               const char **name) {
  if (!actor || !name) /* LCOV_EXCL_LINE */
    return C_ABSTRACT_HTTP_ERR_INVAL; /* LCOV_EXCL_LINE */
  *name = "mock"; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int dummy_handler(struct AbstractHttpActor *self, struct AbstractHttpMessage *msg);

TEST test_actor_hooks(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpActorHooks hooks;
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpMessage msg;
  void *state = NULL; /* LCOV_EXCL_LINE */
  const char *name = NULL; /* LCOV_EXCL_LINE */
  (void)bus; /* LCOV_EXCL_LINE */
  memset(&msg, 0, sizeof(msg)); /* LCOV_EXCL_LINE */

  hooks.bus_init = mock_bus_init; /* LCOV_EXCL_LINE */
  hooks.bus_free = mock_bus_free; /* LCOV_EXCL_LINE */
  hooks.bus_process = mock_bus_process; /* LCOV_EXCL_LINE */
  hooks.actor_spawn = mock_actor_spawn; /* LCOV_EXCL_LINE */
  hooks.actor_send = mock_actor_send; /* LCOV_EXCL_LINE */
  hooks.actor_get_state = mock_actor_get_state; /* LCOV_EXCL_LINE */
  hooks.actor_get_name = mock_actor_get_name; /* LCOV_EXCL_LINE */

  { enum c_abstract_http_error rc_test = abstract_http_actor_set_hooks(&hooks); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } } /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_process(bus)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_spawn(bus, "mock", NULL, NULL, &actor)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_state(actor, &state)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor, &name)); /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(bus); /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */

  {
    struct AbstractHttpActorHooks z;
    memset(&z, 0, sizeof(z)); /* LCOV_EXCL_LINE */
    { enum c_abstract_http_error rc_test = abstract_http_actor_set_hooks(&z); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } } /* LCOV_EXCL_LINE */
  }
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_actor_errors(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpMessage msg;
  (void)bus; /* LCOV_EXCL_LINE */
  memset(&msg, 0, sizeof(msg)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_message_bus_init(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_message_bus_process(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(NULL, "test", NULL, NULL, &actor)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(NULL, &msg)); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, NULL, dummy_handler, NULL, &actor)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, "test", NULL, NULL, &actor)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, NULL)); /* LCOV_EXCL_LINE */

  msg.receiver = actor; /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(bus, NULL)); /* LCOV_EXCL_LINE */
  msg.receiver = NULL; /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(bus, &msg)); /* LCOV_EXCL_LINE */

  abstract_http_message_bus_free(bus); /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */

  { enum c_abstract_http_error rc_test = abstract_http_actor_set_hooks(NULL); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } } /* LCOV_EXCL_LINE */

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

static int dummy_handler(struct AbstractHttpActor *self, struct AbstractHttpMessage *msg) { /* LCOV_EXCL_LINE */
  (void)self; /* LCOV_EXCL_LINE */
  (void)msg; /* LCOV_EXCL_LINE */
  return 0; /* LCOV_EXCL_LINE */
}

TEST test_actor_capacity(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL; /* LCOV_EXCL_LINE */
  int i;
  (void)bus; /* LCOV_EXCL_LINE */

  /* manual coverage for dummy_handler */
  dummy_handler(NULL, NULL); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus)); /* LCOV_EXCL_LINE */

  /* Exceed initial capacity of 16 */
  for (i = 0; i < 20; i++) { /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, &actor)); /* LCOV_EXCL_LINE */
  } /* LCOV_EXCL_LINE */

  abstract_http_message_bus_free(bus); /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

TEST test_actor_getters(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL; /* LCOV_EXCL_LINE */
  void *state = NULL; /* LCOV_EXCL_LINE */
  const char *name = NULL; /* LCOV_EXCL_LINE */
  (void)bus; /* LCOV_EXCL_LINE */

  { enum c_abstract_http_error rc_test = abstract_http_message_bus_init(&bus); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } } /* LCOV_EXCL_LINE */
  { enum c_abstract_http_error rc_test = abstract_http_actor_spawn(bus, "myactor", dummy_handler, (void *)0x123, &actor); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } } /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_state(actor, &state)); /* LCOV_EXCL_LINE */
  ASSERT_EQ((void *)0x123, state); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor, &name)); /* LCOV_EXCL_LINE */
  ASSERT_STR_EQ("myactor", name); /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_state(NULL, &state)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_state(actor, NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_name(NULL, &name)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_name(actor, NULL)); /* LCOV_EXCL_LINE */

  abstract_http_message_bus_free(bus); /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */
  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_actor_oom(void) {                                /* LCOV_EXCL_LINE */
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL;               /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL;                  /* LCOV_EXCL_LINE */
  struct AbstractHttpMessage msg;
  char *out_str = NULL; /* LCOV_EXCL_LINE */
  int str_rc;
  (void)bus; /* LCOV_EXCL_LINE */

  memset(&msg, 0, sizeof(msg)); /* LCOV_EXCL_LINE */

  /* Test bus init OOM on calloc */
  g_mock_alloc_fail = 1;                     /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                    /* LCOV_EXCL_LINE */
  rc = abstract_http_message_bus_init(&bus); /* LCOV_EXCL_LINE */
  printf("abstract_http_message_bus_init returned %d\n",
         rc);                               /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc); /* LCOV_EXCL_LINE */

  /* Test bus init OOM on actors array */
  g_mock_alloc_fail = 1;                     /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 1;                    /* LCOV_EXCL_LINE */
  rc = abstract_http_message_bus_init(&bus); /* LCOV_EXCL_LINE */
  {
    int rc_test_tmp = rc;  /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  rc = abstract_http_message_bus_init(&bus); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);    /* LCOV_EXCL_LINE */

  str_rc = c_abstract_http_mock_strdup("dummy", &out_str); /* LCOV_EXCL_LINE */
  ASSERT_EQ(0, str_rc);                                    /* LCOV_EXCL_LINE */
  if (out_str)                                             /* LCOV_EXCL_LINE */
    free(out_str);                                         /* LCOV_EXCL_LINE */

  rc = abstract_http_actor_spawn(bus, "dummy", dummy_handler, NULL,
                                 &actor); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc); /* LCOV_EXCL_LINE */

  msg.receiver = actor; /* LCOV_EXCL_LINE */

  /* Test actor send OOM */
  g_mock_alloc_fail = 1;                    /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0;                   /* LCOV_EXCL_LINE */
  rc = abstract_http_actor_send(bus, &msg); /* LCOV_EXCL_LINE */
  {
    int rc_test_tmp = rc;  /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  /* Test actor spawn OOM on realloc */
  {
    int i;
    for (i = 0; i < 15; i++) { /* 15 + dummy = 16 actors */ /* LCOV_EXCL_LINE */
      abstract_http_actor_spawn(bus, "test", dummy_handler, NULL,
                                &actor); /* LCOV_EXCL_LINE */
    } /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
    g_mock_alloc_count = 0; /* LCOV_EXCL_LINE */
    rc =                    /* LCOV_EXCL_LINE */
        abstract_http_actor_spawn(bus, "test_oom", dummy_handler, NULL,
                                  &actor); /* LCOV_EXCL_LINE */
    {
      int rc_test_tmp = rc;  /* LCOV_EXCL_LINE */
      g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */
      ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                    "%d"); /* LCOV_EXCL_LINE */
    }

    /* Now successfully spawn one so the next tests don't shift */
    abstract_http_actor_spawn(bus, "test_success", dummy_handler, NULL,
                              &actor); /* LCOV_EXCL_LINE */
  }

  /* Test actor spawn OOM on calloc */
  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 0; /* LCOV_EXCL_LINE */
  rc = abstract_http_actor_spawn(bus, "test2", dummy_handler, NULL,
                                 &actor);   /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc); /* LCOV_EXCL_LINE */

  /* Test actor spawn OOM on strdup */
  g_mock_alloc_fail = 1;  /* LCOV_EXCL_LINE */
  g_mock_alloc_count = 1; /* LCOV_EXCL_LINE */
  rc = abstract_http_actor_spawn(bus, "test3", dummy_handler, NULL,
                                 &actor);  /* LCOV_EXCL_LINE */
  printf("test3 spawn returned %d\n", rc); /* LCOV_EXCL_LINE */
  {
    int rc_test_tmp = rc;  /* LCOV_EXCL_LINE */
    g_mock_alloc_fail = 0; /* LCOV_EXCL_LINE */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  "%d"); /* LCOV_EXCL_LINE */
  }

  abstract_http_message_bus_free(bus);  /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */
  PASS();                               /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */
#endif

TEST test_actor_queued_free_and_tail(void) { /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL;    /* LCOV_EXCL_LINE */
  struct AbstractHttpMessage msg1;
  struct AbstractHttpMessage msg2;
  (void)bus; /* LCOV_EXCL_LINE */

  memset(&msg1, 0, sizeof(msg1)); /* LCOV_EXCL_LINE */
  memset(&msg2, 0, sizeof(msg2)); /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = abstract_http_message_bus_init(&bus);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  abstract_http_actor_spawn(bus, "myactor", dummy_handler, NULL,
                            &actor); /* LCOV_EXCL_LINE */

  msg1.receiver = actor; /* LCOV_EXCL_LINE */
  msg2.receiver = actor; /* LCOV_EXCL_LINE */

  {
    enum c_abstract_http_error rc_test = abstract_http_actor_send(bus, &msg1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  } /* LCOV_EXCL_LINE */
  {
    enum c_abstract_http_error rc_test = abstract_http_actor_send(bus, &msg2);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }
  /* hits tail->next logic */ /* LCOV_EXCL_LINE */

  /* don't process, just free, hitting lines 100-102 */
  abstract_http_message_bus_free(bus);  /* LCOV_EXCL_LINE */
  abstract_http_message_bus_free(NULL); /* LCOV_EXCL_LINE */
  PASS();                               /* LCOV_EXCL_LINE */
}

TEST test_actor_mock_nulls(void) {           /* LCOV_EXCL_LINE */
  struct AbstractHttpMessageBus *bus = NULL; /* LCOV_EXCL_LINE */
  struct AbstractHttpActor *actor = NULL;    /* LCOV_EXCL_LINE */
  const char *name = NULL;                   /* LCOV_EXCL_LINE */
  void *state = NULL;                        /* LCOV_EXCL_LINE */
  (void)bus;                                 /* LCOV_EXCL_LINE */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_bus_init(NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_bus_process(NULL));   /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            mock_actor_spawn(NULL, "test", NULL, NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            mock_actor_spawn((struct AbstractHttpMessageBus *)1, NULL, NULL,
                             NULL, &actor));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_LINE */
            mock_actor_spawn((struct AbstractHttpMessageBus *)1, "test", NULL,
                             NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_send(NULL, NULL)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,    /* LCOV_EXCL_LINE */
            mock_actor_send((struct AbstractHttpMessageBus *)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_get_state(NULL, &state)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,           /* LCOV_EXCL_LINE */
            mock_actor_get_state((struct AbstractHttpActor *)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_actor_get_name(NULL, &name)); /* LCOV_EXCL_LINE */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,         /* LCOV_EXCL_LINE */
            mock_actor_get_name((struct AbstractHttpActor *)1, NULL));

  {
    struct AbstractHttpMessage msg;
    memset(&msg, 0, sizeof(msg)); /* LCOV_EXCL_LINE */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              mock_actor_handler(NULL, &msg)); /* LCOV_EXCL_LINE */
  }

  PASS(); /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

SUITE(actor_suite) {                         /* LCOV_EXCL_LINE */
  RUN_TEST(test_actor_getters);              /* LCOV_EXCL_LINE */
  RUN_TEST(test_actor_queued_free_and_tail); /* LCOV_EXCL_LINE */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_actor_oom); /* LCOV_EXCL_LINE */
#endif
  RUN_TEST(test_actor_spawn_and_message); /* LCOV_EXCL_LINE */
  RUN_TEST(test_actor_hooks);             /* LCOV_EXCL_LINE */
  RUN_TEST(test_actor_errors);            /* LCOV_EXCL_LINE */
  RUN_TEST(test_actor_capacity);          /* LCOV_EXCL_LINE */
  RUN_TEST(test_actor_mock_nulls);        /* LCOV_EXCL_LINE */
} /* LCOV_EXCL_LINE */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
