/**
 * @file process.h
 * @brief Cross-platform Multiprocessing API for HTTP task dispatch.
 *
 * Implements child process spawning and basic serialization/deserialization
 * to handle HTTP tasks in separate processes (MODALITY_MULTIPROCESS).
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_PROCESS_H
#define C_CDD_HTTP_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif

/* clang-format off */
#include <stddef.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

/**
 * @brief Opaque process handle.
 */
struct CddProcess;

/**
 * @brief IPC Pipe endpoints.
 */
struct CddIpcPipe {
  void *read_handle;
  void *write_handle;
};

/**
 * @brief Initialize an IPC pipe pair.
 * @param[out] pipe The pipe structure to populate.
 * @return 0 on success, error code on failure.
 */
extern int cdd_ipc_pipe_init(struct CddIpcPipe *pipe);

/**
 * @brief Close an IPC pipe.
 * @param[in] pipe The pipe to close.
 */
extern void cdd_ipc_pipe_free(struct CddIpcPipe *pipe);

/**
 * @brief Spawn a child process.
 * The child process will execute the current executable but with a specific
 * internal argument flag that routes it to the worker main.
 *
 * @param[out] proc Pointer to receive the allocated process handle.
 * @param[out] parent_to_child The pipe for parent -> child communication.
 * @param[out] child_to_parent The pipe for child -> parent communication.
 * @return 0 on success, error code on failure.
 */
extern int cdd_process_spawn(struct CddProcess **proc,
                             struct CddIpcPipe *parent_to_child,
                             struct CddIpcPipe *child_to_parent);

/**
 * @brief Wait for a process to exit and free its handle.
 * @param[in] proc The process handle.
 * @param[out] exit_code Pointer to store the exit code (optional).
 * @return 0 on success.
 */
extern int cdd_process_wait_and_free(struct CddProcess *proc, int *exit_code);

/**
 * @brief Serialize an HttpRequest into a buffer.
 * @param[in] req The request.
 * @param[out] out_buf Pointer to receive the allocated buffer.
 * @param[out] out_len Length of the buffer.
 * @return 0 on success.
 */
extern int cdd_ipc_serialize_request(const struct HttpRequest *req,
                                     char **out_buf, size_t *out_len);

/**
 * @brief Deserialize a buffer into an HttpRequest.
 * @param[in] buf The buffer.
 * @param[in] len Length of the buffer.
 * @param[out] req The request to populate.
 * @return 0 on success.
 */
extern int cdd_ipc_deserialize_request(const char *buf, size_t len,
                                       struct HttpRequest *req);

/**
 * @brief Serialize an HttpResponse into a buffer.
 * @param[in] res The response.
 * @param[out] out_buf Pointer to receive the allocated buffer.
 * @param[out] out_len Length of the buffer.
 * @return 0 on success.
 */
extern int cdd_ipc_serialize_response(const struct HttpResponse *res,
                                      char **out_buf, size_t *out_len);

/**
 * @brief Deserialize a buffer into an HttpResponse.
 * @param[in] buf The buffer.
 * @param[in] len Length of the buffer.
 * @param[out] res The response to populate.
 * @return 0 on success.
 */
extern int cdd_ipc_deserialize_response(const char *buf, size_t len,
                                        struct HttpResponse *res);

/**
 * @brief Write data to an IPC pipe endpoint securely.
 */
extern int cdd_ipc_write(void *handle, const void *data, size_t len);

/**
 * @brief Read data from an IPC pipe endpoint securely.
 */
extern int cdd_ipc_read(void *handle, void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* C_CDD_HTTP_PROCESS_H */