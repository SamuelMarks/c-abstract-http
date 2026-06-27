/**
 * @file actor.h
 * @brief Simple Actor Model / Message Passing Bus API.
 *
 * Implements a lightweight publish/subscribe message bus and actor
 * abstractions for decoupled request/response routing.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_ACTOR_H
#define C_CDD_HTTP_ACTOR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stddef.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Opaque Message Bus type.
 */
struct CddMessageBus;

/**
 * @brief Opaque Actor type.
 */
struct CddActor;

/**
 * @brief Built-in standard message types.
 */
enum CddMessageType {
  CDD_MSG_HTTP_SEND = 1,     /**< Standard HTTP Send Request */
  CDD_MSG_HTTP_RESPONSE = 2, /**< Standard HTTP Response */
  CDD_MSG_SHUTDOWN = 3,      /**< Request to shutdown actor/bus */
  CDD_MSG_CUSTOM = 1000      /**< Starting point for custom messages */
};

/**
 * @brief A discrete message on the bus.
 */
struct CddMessage {
  int type;      /**< The message type from CddMessageType or custom */
  void *payload; /**< Owned by the sender until freed by the receiver */
  struct CddActor *sender;   /**< The actor that sent this message */
  struct CddActor *receiver; /**< The target actor for this message */
};

/**
 * @brief Actor message handler callback signature.
 * @param[in] actor The actor instance.
 * @param[in] msg The message received.
 * @return 0 on success.
 */
typedef int (*cdd_actor_handler_cb)(struct CddActor *actor,
                                    struct CddMessage *msg);

/**
 * @brief External hooks for actor management.
 */
struct CddActorHooks {
  int (*bus_init)(
      struct CddMessageBus **bus); /**< Hook for bus initialization */
  void (*bus_free)(struct CddMessageBus *bus); /**< Hook for bus destruction */
  int (*bus_process)(
      struct CddMessageBus *bus); /**< Hook to process messages */
  int (*actor_spawn)(struct CddMessageBus *bus, const char *name,
                     cdd_actor_handler_cb handler, void *state,
                     struct CddActor **actor); /**< Hook for spawning actors */
  int (*actor_send)(
      struct CddMessageBus *bus,
      const struct CddMessage *msg); /**< Hook for sending messages */
  int (*actor_get_state)(struct CddActor *actor,
                         void **state); /**< Hook for retrieving state */
  int (*actor_get_name)(const struct CddActor *actor,
                        const char **name); /**< Hook for retrieving name */
};

/**
 * @brief Register external actor hooks.
 * @param[in] hooks The hooks structure.
 */
extern void cdd_actor_set_hooks(const struct CddActorHooks *hooks);

/**
 * @brief Initialize a new global message bus.
 * @param[out] bus Pointer to receive the bus handle.
 * @return 0 on success.
 */
extern enum c_abstract_http_error
cdd_message_bus_init(struct CddMessageBus **bus);

/**
 * @brief Free a message bus.
 * @param[in] bus The bus handle.
 */
extern void cdd_message_bus_free(struct CddMessageBus *bus);

/**
 * @brief Process all pending messages in the bus queue (run the event loop).
 * Non-blocking if the queue is empty unless blocked explicitly by the
 * implementation.
 * @param[in] bus The bus handle.
 * @return Number of messages processed, or error code.
 */
extern enum c_abstract_http_error
cdd_message_bus_process(struct CddMessageBus *bus);

/**
 * @brief Spawn a new Actor and register it to the bus.
 * @param[in] bus The bus handle.
 * @param[in] name The human-readable name of the actor (for debugging/routing).
 * @param[in] handler The callback function for incoming messages.
 * @param[in] state Arbitrary state payload attached to the actor.
 * @param[out] actor Pointer to receive the actor handle.
 * @return 0 on success.
 */
extern enum c_abstract_http_error cdd_actor_spawn(struct CddMessageBus *bus,
                                                  const char *name,
                                                  cdd_actor_handler_cb handler,
                                                  void *state,
                                                  struct CddActor **actor);

/**
 * @brief Send a message directly to a target Actor asynchronously.
 * @param[in] bus The bus handle.
 * @param[in] msg The message structure (copied internally).
 * @return 0 on success.
 */
extern enum c_abstract_http_error cdd_actor_send(struct CddMessageBus *bus,
                                                 const struct CddMessage *msg);

/**
 * @brief Retrieve the state of an actor.
 * @param[in] actor The actor handle.
 * @param[out] state Pointer to receive the state.
 * @return 0 on success.
 */
extern enum c_abstract_http_error cdd_actor_get_state(struct CddActor *actor,
                                                      void **state);

/**
 * @brief Retrieve the name of an actor.
 * @param[in] actor The actor handle.
 * @param[out] name Pointer to receive the name.
 * @return 0 on success.
 */
extern enum c_abstract_http_error
cdd_actor_get_name(const struct CddActor *actor, const char **name);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_CDD_HTTP_ACTOR_H */
