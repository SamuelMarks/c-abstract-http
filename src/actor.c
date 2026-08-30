
/* clang-format off */
#include <c_abstract_http/http_types.h>
extern enum c_abstract_http_error c_abstract_http_mock_strdup(const char *s,
                                                                  char **out);
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/actor.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

#ifndef ABSTRACT_HTTP_MALLOC
#define ABSTRACT_HTTP_MALLOC malloc
#endif

#ifndef ABSTRACT_HTTP_CALLOC
#define ABSTRACT_HTTP_CALLOC calloc
#endif

#ifndef ABSTRACT_HTTP_REALLOC
#define ABSTRACT_HTTP_REALLOC realloc
#endif

#ifndef ABSTRACT_HTTP_FREE
#define ABSTRACT_HTTP_FREE free
#endif

#ifndef c_abstract_http_strdup
#define c_abstract_http_strdup c_abstract_http_strdup
#endif

static struct AbstractHttpActorHooks g_actor_hooks = {NULL, NULL, NULL, NULL,
                                                      NULL, NULL, NULL};

enum c_abstract_http_error
abstract_http_actor_set_hooks(const struct AbstractHttpActorHooks *hooks) {
  if (hooks) {
    g_actor_hooks = *hooks;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Internal struct AbstractHttpActor */
struct AbstractHttpActor {
  /** @brief name (variable) of struct AbstractHttpActor */
  char *name;
  /** @brief handler (variable) of struct AbstractHttpActor */
  abstract_http_actor_handler_cb handler;
  /** @brief state (variable) of struct AbstractHttpActor */
  void *state;
  /** @brief bus (variable) of struct AbstractHttpActor */
  struct AbstractHttpMessageBus *bus;
};

/** @brief Internal struct MessageNode */
struct MessageNode {
  /** @brief msg (variable) of struct MessageNode */
  struct AbstractHttpMessage msg;
  /** @brief next (variable) of struct MessageNode */
  struct MessageNode *next;
};

/** @brief Internal struct AbstractHttpMessageBus */
struct AbstractHttpMessageBus {
  /** @brief actors (variable) of struct AbstractHttpMessageBus */
  struct AbstractHttpActor **actors;
  /** @brief actor_count (variable) of struct AbstractHttpMessageBus */
  size_t actor_count;
  /** @brief actor_capacity (variable) of struct AbstractHttpMessageBus */
  size_t actor_capacity;
  /** @brief head (variable) of struct AbstractHttpMessageBus */
  struct MessageNode *head;
  /** @brief tail (variable) of struct AbstractHttpMessageBus */
  struct MessageNode *tail;
};

enum c_abstract_http_error
abstract_http_message_bus_init(struct AbstractHttpMessageBus **bus) {
  struct AbstractHttpMessageBus *b;
  LOG_DEBUG("abstract_http_message_bus_init: Entering");

  if (g_actor_hooks.bus_init) {
    LOG_DEBUG("abstract_http_message_bus_init: Hooking");
    return g_actor_hooks.bus_init(bus);
  }

  if (!bus) {
    LOG_DEBUG("abstract_http_message_bus_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  b = (struct AbstractHttpMessageBus *)ABSTRACT_HTTP_CALLOC(
      1, sizeof(struct AbstractHttpMessageBus));
  if (!b) {
    LOG_DEBUG("abstract_http_message_bus_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  b->actor_capacity = 16;
  b->actors = (struct AbstractHttpActor **)ABSTRACT_HTTP_MALLOC(
      b->actor_capacity * sizeof(struct AbstractHttpActor *));
  if (!b->actors) {
    LOG_DEBUG("abstract_http_message_bus_init: Error ENOMEM (actors array)");
    ABSTRACT_HTTP_FREE(b);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  *bus = b;
  LOG_DEBUG("abstract_http_message_bus_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void abstract_http_message_bus_free(struct AbstractHttpMessageBus *bus) {
  size_t i;
  struct MessageNode *node;

  LOG_DEBUG("abstract_http_message_bus_free: Entering");
  if (g_actor_hooks.bus_free) {
    LOG_DEBUG("abstract_http_message_bus_free: Hooking");
    g_actor_hooks.bus_free(bus);
    return;
  }

  if (!bus) {
    LOG_DEBUG("abstract_http_message_bus_free: Exiting early (bus NULL)");
    return;
  }

  /* Free pending messages */
  node = bus->head;
  while (node) {
    struct MessageNode *next = node->next;
    ABSTRACT_HTTP_FREE(node);
    node = next;
  }

  /* Free actors */
  for (i = 0; i < bus->actor_count; ++i) {
    ABSTRACT_HTTP_FREE(bus->actors[i]->name);
    ABSTRACT_HTTP_FREE(bus->actors[i]);
  }
  ABSTRACT_HTTP_FREE(bus->actors);
  ABSTRACT_HTTP_FREE(bus);
  LOG_DEBUG("abstract_http_message_bus_free: Exiting");
}

enum c_abstract_http_error
abstract_http_message_bus_process(struct AbstractHttpMessageBus *bus,
                                  int *out_processed) {
  int count = 0;
  struct MessageNode *node;

  LOG_DEBUG("abstract_http_message_bus_process: Entering");
  if (g_actor_hooks.bus_process) {
    LOG_DEBUG("abstract_http_message_bus_process: Hooking");
    return g_actor_hooks.bus_process(bus, out_processed);
  }

  if (!bus || !out_processed) {
    LOG_DEBUG("abstract_http_message_bus_process: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  while (bus->head) {
    node = bus->head;
    bus->head = node->next;
    if (!bus->head) {
      bus->tail = NULL;
    }

    node->msg.receiver->handler(node->msg.receiver, &node->msg);

    ABSTRACT_HTTP_FREE(node);
    count++;
  }

  *out_processed = count;
  LOG_DEBUG("abstract_http_message_bus_process: Success (%d processed)", count);
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_actor_spawn(struct AbstractHttpMessageBus *bus, const char *name,
                          abstract_http_actor_handler_cb handler, void *state,
                          struct AbstractHttpActor **actor) {
  struct AbstractHttpActor *a;
  char *_ast_strdup_0 = NULL;

  LOG_DEBUG("abstract_http_actor_spawn: Entering");
  if (g_actor_hooks.actor_spawn) {
    LOG_DEBUG("abstract_http_actor_spawn: Hooking");
    return g_actor_hooks.actor_spawn(bus, name, handler, state, actor);
  }

  if (!bus || !name || !handler || !actor) {
    LOG_DEBUG("abstract_http_actor_spawn: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (bus->actor_count >= bus->actor_capacity) {
    size_t new_cap = bus->actor_capacity * 2;
    struct AbstractHttpActor **new_arr =
        (struct AbstractHttpActor **)ABSTRACT_HTTP_REALLOC(
            bus->actors, new_cap * sizeof(struct AbstractHttpActor *));
    if (!new_arr) {
      LOG_DEBUG(
          "abstract_http_actor_spawn: Error ENOMEM reallocating actors array");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    bus->actors = new_arr;
    bus->actor_capacity = new_cap;
  }

  a = (struct AbstractHttpActor *)ABSTRACT_HTTP_CALLOC(
      1, sizeof(struct AbstractHttpActor));
  if (!a) {
    printf("abstract_http_actor_spawn: Error ENOMEM a is null\n");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  {
    c_abstract_http_strdup(name, &_ast_strdup_0);
    a->name = _ast_strdup_0;
    if (!a->name) {
      ABSTRACT_HTTP_FREE(a);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }

  a->handler = handler;
  a->state = state;
  a->bus = bus;

  bus->actors[bus->actor_count++] = a;
  *actor = a;

  LOG_DEBUG("abstract_http_actor_spawn: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_actor_send(struct AbstractHttpMessageBus *bus,
                         const struct AbstractHttpMessage *msg) {
  struct MessageNode *node;

  LOG_DEBUG("abstract_http_actor_send: Entering");
  if (g_actor_hooks.actor_send) {
    LOG_DEBUG("abstract_http_actor_send: Hooking");
    return g_actor_hooks.actor_send(bus, msg);
  }

  if (!bus || !msg || !msg->receiver) {
    LOG_DEBUG("abstract_http_actor_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  node = (struct MessageNode *)ABSTRACT_HTTP_MALLOC(sizeof(struct MessageNode));
  if (!node) {
    LOG_DEBUG("abstract_http_actor_send: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  node->msg = *msg; /* shallow copy */
  node->next = NULL;

  if (!bus->head) {
    bus->head = node;
    bus->tail = node;
  } else {
    bus->tail->next = node;
    bus->tail = node;
  }

  LOG_DEBUG("abstract_http_actor_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_actor_get_state(struct AbstractHttpActor *actor, void **state) {
  LOG_DEBUG("abstract_http_actor_get_state: Entering");
  if (g_actor_hooks.actor_get_state) {
    LOG_DEBUG("abstract_http_actor_get_state: Hooking");
    return g_actor_hooks.actor_get_state(actor, state);
  }
  if (!actor || !state) {
    LOG_DEBUG("abstract_http_actor_get_state: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *state = actor->state;
  LOG_DEBUG("abstract_http_actor_get_state: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
abstract_http_actor_get_name(const struct AbstractHttpActor *actor,
                             const char **name) {
  LOG_DEBUG("abstract_http_actor_get_name: Entering");
  if (g_actor_hooks.actor_get_name) {
    LOG_DEBUG("abstract_http_actor_get_name: Hooking");
    return g_actor_hooks.actor_get_name(actor, name);
  }
  if (!actor || !name) {
    LOG_DEBUG("abstract_http_actor_get_name: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *name = actor->name;
  LOG_DEBUG("abstract_http_actor_get_name: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
