#ifndef TEST_ACTOR_H
#define TEST_ACTOR_H

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
  struct TestActorState *state =
      (struct TestActorState *)cdd_actor_get_state(actor);
  if (!state)
    return EINVAL;

  if (msg->type == CDD_MSG_HTTP_SEND) {
    state->received_messages++;

    /* Respond */
    struct CddMessage response;
    response.type = CDD_MSG_HTTP_RESPONSE;
    response.payload = NULL;
    response.sender = actor;
    response.receiver = msg->sender;
    /* We need to get the bus from somewhere, typically passed in context or
       global. In this test, the sender is known to use the same bus. Actually,
       CddMessageBus is opaque and we don't have a cdd_actor_get_bus, but for
       this test we'll just verify the count. */
  } else if (msg->type == CDD_MSG_SHUTDOWN) {
    state->shutdown = 1;
  }

  return 0;
}

TEST test_actor_bus_messaging(void) {
  struct CddMessageBus *bus = NULL;
  struct CddActor *actor1 = NULL;
  struct CddActor *actor2 = NULL;
  struct TestActorState state1 = {0, 0};
  struct TestActorState state2 = {0, 0};
  struct CddMessage msg;

  ASSERT_EQ(0, cdd_message_bus_init(&bus));
  ASSERT_EQ(
      0, cdd_actor_spawn(bus, "Actor1", mock_actor_handler, &state1, &actor1));
  ASSERT_EQ(
      0, cdd_actor_spawn(bus, "Actor2", mock_actor_handler, &state2, &actor2));

  ASSERT_STR_EQ("Actor1", cdd_actor_get_name(actor1));
  ASSERT_STR_EQ("Actor2", cdd_actor_get_name(actor2));

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
  PASS();
}

SUITE(actor_suite) { RUN_TEST(test_actor_bus_messaging); }

#endif
