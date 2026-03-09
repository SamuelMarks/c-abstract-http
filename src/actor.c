/**
 * @file actor.c
 * @brief Implementation of the Message Passing / Actor Model bus.
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/actor.h>
#include <c_abstract_http/str.h>
/* clang-format on */

struct CddActor {
  char *name;
  cdd_actor_handler_cb handler;
  void *state;
  struct CddMessageBus *bus;
};

struct MessageNode {
  struct CddMessage msg;
  struct MessageNode *next;
};

struct CddMessageBus {
  struct CddActor **actors;
  size_t actor_count;
  size_t actor_capacity;

  struct MessageNode *head;
  struct MessageNode *tail;
};

int cdd_message_bus_init(struct CddMessageBus **bus) {
  struct CddMessageBus *b;
  if (!bus)
    return EINVAL;

  b = (struct CddMessageBus *)calloc(1, sizeof(struct CddMessageBus));
  if (!b)
    return ENOMEM;

  b->actor_capacity = 16;
  b->actors =
      (struct CddActor **)malloc(b->actor_capacity * sizeof(struct CddActor *));
  if (!b->actors) {
    free(b);
    return ENOMEM;
  }

  *bus = b;
  return 0;
}

void cdd_message_bus_free(struct CddMessageBus *bus) {
  size_t i;
  struct MessageNode *node;

  if (!bus)
    return;

  /* Free pending messages */
  node = bus->head;
  while (node) {
    struct MessageNode *next = node->next;
    free(node);
    node = next;
  }

  /* Free actors */
  for (i = 0; i < bus->actor_count; ++i) {
    if (bus->actors[i]->name)
      free(bus->actors[i]->name);
    free(bus->actors[i]);
  }
  free(bus->actors);
  free(bus);
}

int cdd_message_bus_process(struct CddMessageBus *bus) {
  int count = 0;
  struct MessageNode *node;

  if (!bus)
    return EINVAL;

  while (bus->head) {
    node = bus->head;
    bus->head = node->next;
    if (!bus->head) {
      bus->tail = NULL;
    }

    if (node->msg.receiver && node->msg.receiver->handler) {
      node->msg.receiver->handler(node->msg.receiver, &node->msg);
    }

    free(node);
    count++;
  }

  return count;
}

int cdd_actor_spawn(struct CddMessageBus *bus, const char *name,
                    cdd_actor_handler_cb handler, void *state,
                    struct CddActor **actor) {
  struct CddActor *a;
  char *_ast_strdup_0 = NULL;

  if (!bus || !name || !handler || !actor)
    return EINVAL;

  if (bus->actor_count >= bus->actor_capacity) {
    size_t new_cap = bus->actor_capacity * 2;
    struct CddActor **new_arr = (struct CddActor **)realloc(
        bus->actors, new_cap * sizeof(struct CddActor *));
    if (!new_arr)
      return ENOMEM;
    bus->actors = new_arr;
    bus->actor_capacity = new_cap;
  }

  a = (struct CddActor *)calloc(1, sizeof(struct CddActor));
  if (!a)
    return ENOMEM;

  a->name = (c_cdd_strdup(name, &_ast_strdup_0), _ast_strdup_0);
  if (!a->name) {
    free(a);
    return ENOMEM;
  }

  a->handler = handler;
  a->state = state;
  a->bus = bus;

  bus->actors[bus->actor_count++] = a;
  *actor = a;

  return 0;
}

int cdd_actor_send(struct CddMessageBus *bus, const struct CddMessage *msg) {
  struct MessageNode *node;

  if (!bus || !msg || !msg->receiver)
    return EINVAL;

  node = (struct MessageNode *)malloc(sizeof(struct MessageNode));
  if (!node)
    return ENOMEM;

  node->msg = *msg; /* shallow copy */
  node->next = NULL;

  if (!bus->head) {
    bus->head = node;
    bus->tail = node;
  } else {
    bus->tail->next = node;
    bus->tail = node;
  }

  return 0;
}

void *cdd_actor_get_state(struct CddActor *actor) {
  return actor ? actor->state : NULL;
}

const char *cdd_actor_get_name(const struct CddActor *actor) {
  return actor ? actor->name : NULL;
}