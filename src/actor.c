
#include <c_abstract_http/http_types.h>
extern enum c_abstract_http_error c_abstract_http_mock_cdd_strdup(const char *s,
                                                                  char **out);
/* clang-format off */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/actor.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

#ifndef CDD_MALLOC
#define CDD_MALLOC malloc
#endif

#ifndef CDD_CALLOC
#define CDD_CALLOC calloc
#endif

#ifndef CDD_REALLOC
#define CDD_REALLOC realloc
#endif

#ifndef CDD_FREE
#define CDD_FREE free
#endif

#ifndef CDD_STRDUP
#define CDD_STRDUP c_cdd_strdup
#endif

static struct CddActorHooks g_actor_hooks = {NULL, NULL, NULL, NULL,
                                             NULL, NULL, NULL};

void cdd_actor_set_hooks(const struct CddActorHooks *hooks) {
  if (hooks) {
    g_actor_hooks = *hooks;
  }
}

/** @brief Internal struct CddActor */
struct CddActor {
  /** @brief name (variable) of struct CddActor */
  char *name;
  /** @brief handler (variable) of struct CddActor */
  cdd_actor_handler_cb handler;
  /** @brief state (variable) of struct CddActor */
  void *state;
  /** @brief bus (variable) of struct CddActor */
  struct CddMessageBus *bus;
};

/** @brief Internal struct MessageNode */
struct MessageNode {
  /** @brief msg (variable) of struct MessageNode */
  struct CddMessage msg;
  /** @brief next (variable) of struct MessageNode */
  struct MessageNode *next;
};

/** @brief Internal struct CddMessageBus */
struct CddMessageBus {
  /** @brief actors (variable) of struct CddMessageBus */
  struct CddActor **actors;
  /** @brief actor_count (variable) of struct CddMessageBus */
  size_t actor_count;
  /** @brief actor_capacity (variable) of struct CddMessageBus */
  size_t actor_capacity;
  /** @brief head (variable) of struct CddMessageBus */
  struct MessageNode *head;
  /** @brief tail (variable) of struct CddMessageBus */
  struct MessageNode *tail;
};

enum c_abstract_http_error cdd_message_bus_init(struct CddMessageBus **bus) {
  struct CddMessageBus *b;
  LOG_DEBUG("cdd_message_bus_init: Entering");

  if (g_actor_hooks.bus_init) {
    LOG_DEBUG("cdd_message_bus_init: Hooking");
    return g_actor_hooks.bus_init(bus);
  }

  if (!bus) {
    LOG_DEBUG("cdd_message_bus_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  b = (struct CddMessageBus *)CDD_CALLOC(1, sizeof(struct CddMessageBus));
  if (!b) {
    LOG_DEBUG("cdd_message_bus_init: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  b->actor_capacity = 16;
  b->actors = (struct CddActor **)CDD_MALLOC(b->actor_capacity *
                                             sizeof(struct CddActor *));
  if (!b->actors) {
    LOG_DEBUG("cdd_message_bus_init: Error ENOMEM (actors array)");
    CDD_FREE(b);
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  *bus = b;
  LOG_DEBUG("cdd_message_bus_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_message_bus_free(struct CddMessageBus *bus) {
  size_t i;
  struct MessageNode *node;

  LOG_DEBUG("cdd_message_bus_free: Entering");
  if (g_actor_hooks.bus_free) {
    LOG_DEBUG("cdd_message_bus_free: Hooking");
    g_actor_hooks.bus_free(bus);
    return;
  }

  if (!bus) {
    LOG_DEBUG("cdd_message_bus_free: Exiting early (bus NULL)");
    return;
  }

  /* Free pending messages */
  node = bus->head;
  while (node) {
    struct MessageNode *next = node->next;
    CDD_FREE(node);
    node = next;
  }

  /* Free actors */
  for (i = 0; i < bus->actor_count; ++i) {
    CDD_FREE(bus->actors[i]->name);
    CDD_FREE(bus->actors[i]);
  }
  CDD_FREE(bus->actors);
  CDD_FREE(bus);
  LOG_DEBUG("cdd_message_bus_free: Exiting");
}

enum c_abstract_http_error cdd_message_bus_process(struct CddMessageBus *bus) {
  int count = 0;
  struct MessageNode *node;

  LOG_DEBUG("cdd_message_bus_process: Entering");
  if (g_actor_hooks.bus_process) {
    LOG_DEBUG("cdd_message_bus_process: Hooking");
    return g_actor_hooks.bus_process(bus);
  }

  if (!bus) {
    LOG_DEBUG("cdd_message_bus_process: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  while (bus->head) {
    node = bus->head;
    bus->head = node->next;
    if (!bus->head) {
      bus->tail = NULL;
    }

    node->msg.receiver->handler(node->msg.receiver, &node->msg);

    CDD_FREE(node);
    count++;
  }

  LOG_DEBUG("cdd_message_bus_process: Success (%d processed)", count);
  return count;
}

enum c_abstract_http_error cdd_actor_spawn(struct CddMessageBus *bus,
                                           const char *name,
                                           cdd_actor_handler_cb handler,
                                           void *state,
                                           struct CddActor **actor) {
  struct CddActor *a;
  char *_ast_strdup_0 = NULL;

  LOG_DEBUG("cdd_actor_spawn: Entering");
  if (g_actor_hooks.actor_spawn) {
    LOG_DEBUG("cdd_actor_spawn: Hooking");
    return g_actor_hooks.actor_spawn(bus, name, handler, state, actor);
  }

  if (!bus || !name || !handler || !actor) {
    LOG_DEBUG("cdd_actor_spawn: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  if (bus->actor_count >= bus->actor_capacity) {
    size_t new_cap = bus->actor_capacity * 2;
    struct CddActor **new_arr = (struct CddActor **)CDD_REALLOC(
        bus->actors, new_cap * sizeof(struct CddActor *));
    if (!new_arr) {
      LOG_DEBUG("cdd_actor_spawn: Error ENOMEM reallocating actors array");
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    bus->actors = new_arr;
    bus->actor_capacity = new_cap;
  }

  a = (struct CddActor *)CDD_CALLOC(1, sizeof(struct CddActor));
  if (!a) {
    printf("cdd_actor_spawn: Error ENOMEM a is null\n");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  {
    CDD_STRDUP(name, &_ast_strdup_0);
    a->name = _ast_strdup_0;
    if (!a->name) {
      CDD_FREE(a);
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
  }

  a->handler = handler;
  a->state = state;
  a->bus = bus;

  bus->actors[bus->actor_count++] = a;
  *actor = a;

  LOG_DEBUG("cdd_actor_spawn: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_actor_send(struct CddMessageBus *bus,
                                          const struct CddMessage *msg) {
  struct MessageNode *node;

  LOG_DEBUG("cdd_actor_send: Entering");
  if (g_actor_hooks.actor_send) {
    LOG_DEBUG("cdd_actor_send: Hooking");
    return g_actor_hooks.actor_send(bus, msg);
  }

  if (!bus || !msg || !msg->receiver) {
    LOG_DEBUG("cdd_actor_send: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  node = (struct MessageNode *)CDD_MALLOC(sizeof(struct MessageNode));
  if (!node) {
    LOG_DEBUG("cdd_actor_send: Error ENOMEM");
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

  LOG_DEBUG("cdd_actor_send: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_actor_get_state(struct CddActor *actor,
                                               void **state) {
  LOG_DEBUG("cdd_actor_get_state: Entering");
  if (g_actor_hooks.actor_get_state) {
    LOG_DEBUG("cdd_actor_get_state: Hooking");
    return g_actor_hooks.actor_get_state(actor, state);
  }
  if (!actor || !state) {
    LOG_DEBUG("cdd_actor_get_state: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *state = actor->state;
  LOG_DEBUG("cdd_actor_get_state: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_actor_get_name(const struct CddActor *actor,
                                              const char **name) {
  LOG_DEBUG("cdd_actor_get_name: Entering");
  if (g_actor_hooks.actor_get_name) {
    LOG_DEBUG("cdd_actor_get_name: Hooking");
    return g_actor_hooks.actor_get_name(actor, name);
  }
  if (!actor || !name) {
    LOG_DEBUG("cdd_actor_get_name: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *name = actor->name;
  LOG_DEBUG("cdd_actor_get_name: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}
