/**
 * @file actor.h
 * @brief Simple Actor Model / Message Passing Bus API.
 *
 * Implements a lightweight publish/subscribe message bus and actor
 * abstractions for decoupled request/response routing.
 *
 * @author Samuel Marks
 */

#ifndef C_ABSTRACT_HTTP_HTTP_ACTOR_H
#define C_ABSTRACT_HTTP_HTTP_ACTOR_H

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
struct AbstractHttpMessageBus;

/**
 * @brief Opaque Actor type.
 */
struct AbstractHttpActor;

/**
 * @brief Built-in standard message types.
 */
enum AbstractHttpMessageType {
  ABSTRACT_HTTP_MSG_HTTP_SEND = 1,     /**< Standard HTTP Send Request */
  ABSTRACT_HTTP_MSG_HTTP_RESPONSE = 2, /**< Standard HTTP Response */
  ABSTRACT_HTTP_MSG_SHUTDOWN = 3,      /**< Request to shutdown actor/bus */
  ABSTRACT_HTTP_MSG_CUSTOM = 1000 /**< Starting point for custom messages */
};

/**
 * @brief A discrete message on the bus.
 */
struct AbstractHttpMessage {
  int type;      /**< The message type from AbstractHttpMessageType or custom */
  void *payload; /**< Owned by the sender until freed by the receiver */
  struct AbstractHttpActor *sender;   /**< The actor that sent this message */
  struct AbstractHttpActor *receiver; /**< The target actor for this message */
};

/**
 * @brief Actor message handler callback signature.
 * @param[in] actor The actor instance.
 * @param[in] msg The message received.
 * @return 0 on success.
 */
typedef int (*abstract_http_actor_handler_cb)(struct AbstractHttpActor *actor,
                                              struct AbstractHttpMessage *msg);

/**
 * @brief External hooks for actor management.
 */
struct AbstractHttpActorHooks {
  int (*bus_init)(
      struct AbstractHttpMessageBus **bus); /**< Hook for bus initialization */
  void (*bus_free)(
      struct AbstractHttpMessageBus *bus); /**< Hook for bus destruction */
  int (*bus_process)(struct AbstractHttpMessageBus *bus,
                     int *out_processed); /**< Hook to process messages */
  int (*actor_spawn)(
      struct AbstractHttpMessageBus *bus, const char *name,
      abstract_http_actor_handler_cb handler, void *state,
      struct AbstractHttpActor **actor); /**< Hook for spawning actors */
  int (*actor_send)(
      struct AbstractHttpMessageBus *bus,
      const struct AbstractHttpMessage *msg); /**< Hook for sending messages */
  int (*actor_get_state)(struct AbstractHttpActor *actor,
                         void **state); /**< Hook for retrieving state */
  int (*actor_get_name)(const struct AbstractHttpActor *actor,
                        const char **name); /**< Hook for retrieving name */
};

/**
 * @brief Register external actor hooks.
 * @param[in] hooks The hooks structure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_actor_set_hooks(const struct AbstractHttpActorHooks *hooks);

/**
 * @brief Initialize a new global message bus.
 * @param[out] bus Pointer to receive the bus handle.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_message_bus_init(struct AbstractHttpMessageBus **bus);

/**
 * @brief Free a message bus.
 * @param[in] bus The bus handle.
 */
C_ABSTRACT_HTTP_API void
abstract_http_message_bus_free(struct AbstractHttpMessageBus *bus);

/**
 * @brief Process all pending messages in the bus queue (run the event loop).
 * Non-blocking if the queue is empty unless blocked explicitly by the
 * implementation.
 * @param[in] bus The bus handle.
 * @param[out] out_processed Pointer to store the number of messages processed.
 * @return Error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_message_bus_process(struct AbstractHttpMessageBus *bus,
                                  int *out_processed);

/**
 * @brief Spawn a new Actor and register it to the bus.
 * @param[in] bus The bus handle.
 * @param[in] name The human-readable name of the actor (for debugging/routing).
 * @param[in] handler The callback function for incoming messages.
 * @param[in] state Arbitrary state payload attached to the actor.
 * @param[out] actor Pointer to receive the actor handle.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_actor_spawn(struct AbstractHttpMessageBus *bus, const char *name,
                          abstract_http_actor_handler_cb handler, void *state,
                          struct AbstractHttpActor **actor);

/**
 * @brief Send a message directly to a target Actor asynchronously.
 * @param[in] bus The bus handle.
 * @param[in] msg The message structure (copied internally).
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t abstract_http_actor_send(
    struct AbstractHttpMessageBus *bus, const struct AbstractHttpMessage *msg);

/**
 * @brief Retrieve the state of an actor.
 * @param[in] actor The actor handle.
 * @param[out] state Pointer to receive the state.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_actor_get_state(struct AbstractHttpActor *actor, void **state);

/**
 * @brief Retrieve the name of an actor.
 * @param[in] actor The actor handle.
 * @param[out] name Pointer to receive the name.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
abstract_http_actor_get_name(const struct AbstractHttpActor *actor,
                             const char **name);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_HTTP_ACTOR_H */
