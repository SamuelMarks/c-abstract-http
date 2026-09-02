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

/* LCOV_EXCL_START */ static int mock_actor_handler(struct AbstractHttpActor *actor, struct AbstractHttpMessage *msg) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct TestActorState *state = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)msg;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   if (abstract_http_actor_get_state(actor, (void **)&state) != 0 || !state)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   if (msg->type == ABSTRACT_HTTP_MSG_HTTP_SEND) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     state->received_messages++;  /* LCOV_EXCL_STOP */

    /* We need to get the bus from somewhere, typically passed in context or
       global. In this test, the sender is known to use the same bus. Actually,
       AbstractHttpMessageBus is opaque and we don't have a abstract_http_actor_get_bus, but for
       this test we'll just verify the count. */
/* LCOV_EXCL_START */   } else if (msg->type == ABSTRACT_HTTP_MSG_SHUTDOWN) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     state->shutdown = 1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_actor_spawn_and_message(void) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpMessageBus *bus = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpActor *actor1 = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpActor *actor2 = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct TestActorState state1 = {0, 0};  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct TestActorState state2 = {0, 0};  /* LCOV_EXCL_STOP */
  struct AbstractHttpMessage msg;
/* LCOV_EXCL_START */   const char *name1 = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   const char *name2 = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)bus;  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(  /* LCOV_EXCL_STOP */
      0, abstract_http_actor_spawn(bus, "Actor1", mock_actor_handler, &state1, &actor1));
/* LCOV_EXCL_START */   ASSERT_EQ(  /* LCOV_EXCL_STOP */
      0, abstract_http_actor_spawn(bus, "Actor2", mock_actor_handler, &state2, &actor2));

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor1, &name1));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_STR_EQ("Actor1", name1);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor2, &name2));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_STR_EQ("Actor2", name2);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   msg.payload = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   msg.sender = actor1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   msg.receiver = actor2;  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));  /* LCOV_EXCL_STOP */

  /* Nothing processed yet */
/* LCOV_EXCL_START */   ASSERT_EQ(0, state2.received_messages);  /* LCOV_EXCL_STOP */

  {
/* LCOV_EXCL_START */     int processed_count = 0;  /* LCOV_EXCL_STOP */

    /* Process */
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_process(bus, &processed_count));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(1, processed_count);  /* LCOV_EXCL_STOP */
  }

  /* Actor 2 should have received it */
/* LCOV_EXCL_START */   ASSERT_EQ(1, state2.received_messages);  /* LCOV_EXCL_STOP */

  /* Send shutdown AND another message */
/* LCOV_EXCL_START */   msg.type = ABSTRACT_HTTP_MSG_HTTP_SEND;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   msg.receiver = actor1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   msg.type = ABSTRACT_HTTP_MSG_SHUTDOWN;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   msg.receiver = actor1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));  /* LCOV_EXCL_STOP */

  {
    int processed_count = 0;
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_process(bus, &processed_count));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(2, processed_count);  /* LCOV_EXCL_STOP */
  }
/* LCOV_EXCL_START */   ASSERT_EQ(1, state1.shutdown);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   abstract_http_message_bus_free(bus);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   abstract_http_message_bus_free(NULL);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   PASS();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ static int mock_bus_init(struct AbstractHttpMessageBus **bus) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   if (!bus)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   *bus = (struct AbstractHttpMessageBus *)(size_t)1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ static void mock_bus_free(struct AbstractHttpMessageBus *bus) { (void)bus; }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ static int mock_bus_process(struct AbstractHttpMessageBus *bus, int *out_processed) {  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   if (!bus || !out_processed)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   *out_processed = 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ static int mock_actor_spawn(struct AbstractHttpMessageBus *bus, const char *name,  /* LCOV_EXCL_STOP */
                            abstract_http_actor_handler_cb handler, void *state,
                            struct AbstractHttpActor **actor) {

/* LCOV_EXCL_START */   if (!bus || !name || !actor)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)handler;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)state;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   *actor = (struct AbstractHttpActor *)(size_t)1;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ static int mock_actor_send(struct AbstractHttpMessageBus *bus,  /* LCOV_EXCL_STOP */
                           const struct AbstractHttpMessage *msg) {

/* LCOV_EXCL_START */   if (!bus || !msg)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ static int mock_actor_get_state(struct AbstractHttpActor *actor, void **state) {  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   if (!actor || !state)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   *state = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ static int mock_actor_get_name(const struct AbstractHttpActor *actor,  /* LCOV_EXCL_STOP */
                               const char **name) {
/* LCOV_EXCL_START */   if (!actor || !name)  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     return C_ABSTRACT_HTTP_ERR_INVAL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   *name = "mock";  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

static int dummy_handler(struct AbstractHttpActor *self, struct AbstractHttpMessage *msg);

/* LCOV_EXCL_START */ TEST test_actor_hooks(void) {  /* LCOV_EXCL_STOP */
  struct AbstractHttpActorHooks hooks;
/* LCOV_EXCL_START */   struct AbstractHttpMessageBus *bus = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpActor *actor = NULL;  /* LCOV_EXCL_STOP */
  struct AbstractHttpMessage msg;
/* LCOV_EXCL_START */   void *state = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   const char *name = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)bus;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   memset(&msg, 0, sizeof(msg));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   hooks.bus_init = mock_bus_init;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   hooks.bus_free = mock_bus_free;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   hooks.bus_process = mock_bus_process;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   hooks.actor_spawn = mock_actor_spawn;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   hooks.actor_send = mock_actor_send;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   hooks.actor_get_state = mock_actor_get_state;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   hooks.actor_get_name = mock_actor_get_name;  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_actor_set_hooks(&hooks); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */

  {
/* LCOV_EXCL_START */     int out_processed = 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_process(bus, &out_processed));  /* LCOV_EXCL_STOP */
  }
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_spawn(bus, "mock", NULL, NULL, &actor));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_send(bus, &msg));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_state(actor, &state));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor, &name));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   abstract_http_message_bus_free(bus);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   abstract_http_message_bus_free(NULL);  /* LCOV_EXCL_STOP */

  {
    struct AbstractHttpActorHooks z;
/* LCOV_EXCL_START */     memset(&z, 0, sizeof(z));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     { enum c_abstract_http_error rc_test = abstract_http_actor_set_hooks(&z); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */
  }
/* LCOV_EXCL_START */   PASS();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_actor_errors(void) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpMessageBus *bus = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpActor *actor = NULL;  /* LCOV_EXCL_STOP */
  struct AbstractHttpMessage msg;
/* LCOV_EXCL_START */   (void)bus;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   memset(&msg, 0, sizeof(msg));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_message_bus_init(NULL));  /* LCOV_EXCL_STOP */
  {
/* LCOV_EXCL_START */     int out_processed = 0;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_message_bus_process(NULL, &out_processed));  /* LCOV_EXCL_STOP */
  }
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(NULL, "test", NULL, NULL, &actor));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(NULL, &msg));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, NULL, dummy_handler, NULL, &actor));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, "test", NULL, NULL, &actor));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, NULL));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   msg.receiver = actor;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(bus, NULL));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   msg.receiver = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_send(bus, &msg));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   abstract_http_message_bus_free(bus);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   abstract_http_message_bus_free(NULL);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_actor_set_hooks(NULL); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   PASS();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ static int dummy_handler(struct AbstractHttpActor *self, struct AbstractHttpMessage *msg) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)self;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)msg;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   return 0;  /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ TEST test_actor_capacity(void) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpMessageBus *bus = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpActor *actor = NULL;  /* LCOV_EXCL_STOP */
  int i;
/* LCOV_EXCL_START */   (void)bus;  /* LCOV_EXCL_STOP */

  /* manual coverage for dummy_handler */
/* LCOV_EXCL_START */   dummy_handler(NULL, NULL);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_message_bus_init(&bus));  /* LCOV_EXCL_STOP */

  /* Exceed initial capacity of 16 */
/* LCOV_EXCL_START */   for (i = 0; i < 20; i++) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */     ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_spawn(bus, "test", dummy_handler, NULL, &actor));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   abstract_http_message_bus_free(bus);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   abstract_http_message_bus_free(NULL);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   PASS();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST test_actor_getters(void) {  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpMessageBus *bus = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   struct AbstractHttpActor *actor = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   void *state = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   const char *name = NULL;  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   (void)bus;  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_message_bus_init(&bus); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   { enum c_abstract_http_error rc_test = abstract_http_actor_spawn(bus, "myactor", dummy_handler, NULL, &actor); if (rc_test != C_ABSTRACT_HTTP_SUCCESS) { printf("Error: %d\n", (int)rc_test); } }  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_state(actor, &state));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(NULL, state);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_actor_get_name(actor, &name));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_STR_EQ("myactor", name);  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_state(NULL, &state));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_state(actor, NULL));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_name(NULL, &name));  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, abstract_http_actor_get_name(actor, NULL));  /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */   abstract_http_message_bus_free(bus);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   abstract_http_message_bus_free(NULL);  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */   PASS();  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */

#include "mock_alloc.h"
/* clang-format on */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST test_actor_oom(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMessageBus *bus =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpActor *actor =
      NULL; /* LCOV_EXCL_STOP */
  struct AbstractHttpMessage msg;
  /* LCOV_EXCL_START */ char *out_str = NULL; /* LCOV_EXCL_STOP */
  int str_rc;
  /* LCOV_EXCL_START */ (void)bus; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&msg, 0, sizeof(msg)); /* LCOV_EXCL_STOP */

  /* Test bus init OOM on calloc */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_message_bus_init(&bus); /* LCOV_EXCL_STOP */
  printf("abstract_http_message_bus_init returned %d\n",
         /* LCOV_EXCL_START */ rc); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* Test bus init OOM on actors array */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_message_bus_init(&bus); /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ rc =
      abstract_http_message_bus_init(&bus); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ str_rc =
      c_abstract_http_mock_strdup("dummy", &out_str); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(0, str_rc);         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ if (out_str)                  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(out_str);              /* LCOV_EXCL_STOP */

  rc = abstract_http_actor_spawn(
      bus, "dummy", dummy_handler, NULL,
      /* LCOV_EXCL_START */ &actor); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ msg.receiver = actor; /* LCOV_EXCL_STOP */

  /* Test actor send OOM */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_actor_send(bus, &msg); /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* Test actor spawn OOM on realloc */
  {
    int i;
    /* LCOV_EXCL_START */ for (i = 0; i < 15; i++) {
      /* 15 + dummy = 16 actors */ /* LCOV_EXCL_STOP */
      (void)!abstract_http_actor_spawn(
          bus, "test", dummy_handler, NULL,
          /* LCOV_EXCL_START */ &actor);          /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ }                       /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ rc =                    /* LCOV_EXCL_STOP */
        abstract_http_actor_spawn(
            bus, "test_oom", dummy_handler, NULL,
            /* LCOV_EXCL_START */ &actor); /* LCOV_EXCL_STOP */
    {
      /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
      ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                    /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
    }

    /* Now successfully spawn one so the next tests don't shift */
    (void)!abstract_http_actor_spawn(
        bus, "test_success", dummy_handler, NULL,
        /* LCOV_EXCL_START */ &actor); /* LCOV_EXCL_STOP */
  }

  /* Test actor spawn OOM on calloc */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  rc = abstract_http_actor_spawn(
      bus, "test2", dummy_handler, NULL,
      /* LCOV_EXCL_START */ &actor); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                                  rc); /* LCOV_EXCL_STOP */

  /* Test actor spawn OOM on strdup */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1; /* LCOV_EXCL_STOP */
  rc = abstract_http_actor_spawn(
      bus, "test3", dummy_handler, NULL,
      /* LCOV_EXCL_START */ &actor); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ printf("test3 spawn returned %d\n",
                               rc); /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ abstract_http_message_bus_free(
      bus); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_message_bus_free(
      NULL);                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST
test_actor_queued_free_and_tail(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMessageBus *bus =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpActor *actor =
      NULL; /* LCOV_EXCL_STOP */
  struct AbstractHttpMessage msg1;
  struct AbstractHttpMessage msg2;
  /* LCOV_EXCL_START */ (void)bus; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&msg1, 0, sizeof(msg1)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&msg2, 0, sizeof(msg2)); /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = abstract_http_message_bus_init(&bus);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  (void)!abstract_http_actor_spawn(
      bus, "myactor", dummy_handler, NULL,
      /* LCOV_EXCL_START */ &actor); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ msg1.receiver = actor; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ msg2.receiver = actor; /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = abstract_http_actor_send(bus, &msg1);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = abstract_http_actor_send(bus, &msg2);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }
  /* LCOV_EXCL_START */ /* hits tail->next logic */ /* LCOV_EXCL_STOP */

  /* don't process, just free, hitting lines 100-102 */
  /* LCOV_EXCL_START */ abstract_http_message_bus_free(
      bus); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_message_bus_free(
      NULL);                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ TEST test_actor_mock_nulls(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpMessageBus *bus =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpActor *actor =
      NULL;                                      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ const char *name = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ void *state = NULL;      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)bus;               /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ mock_bus_init(NULL)); /* LCOV_EXCL_STOP */
  {
    int out_processed = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              /* LCOV_EXCL_START */ mock_bus_process(
                  NULL, &out_processed)); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      mock_actor_spawn(NULL, "test", NULL, NULL, &actor));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      mock_actor_spawn((struct AbstractHttpMessageBus *)(size_t)1, NULL, NULL,
                       NULL, &actor));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      mock_actor_spawn((struct AbstractHttpMessageBus *)(size_t)1, "test", NULL,
                       NULL, NULL));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      /* LCOV_EXCL_START */ mock_actor_send(NULL, NULL)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      mock_actor_send((struct AbstractHttpMessageBus *)(size_t)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ mock_actor_get_state(
                NULL, &state)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      mock_actor_get_state((struct AbstractHttpActor *)(size_t)1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ mock_actor_get_name(
                NULL, &name)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      mock_actor_get_name((struct AbstractHttpActor *)(size_t)1, NULL));

  {
    struct AbstractHttpMessage msg;
    /* LCOV_EXCL_START */ memset(&msg, 0, sizeof(msg)); /* LCOV_EXCL_STOP */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              /* LCOV_EXCL_START */ mock_actor_handler(
                  NULL, &msg)); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ SUITE(actor_suite) {            /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_actor_getters); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_actor_queued_free_and_tail); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(test_actor_oom); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ RUN_TEST(
      test_actor_spawn_and_message);                     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_actor_hooks);      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_actor_errors);     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_actor_capacity);   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_actor_mock_nulls); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                                  /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
