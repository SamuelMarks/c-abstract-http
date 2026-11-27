/**
 * @file mock_alloc.h
 * @brief OOM and syscall fault-injection mock declarations and enum accessors.
 */

#ifndef ABSTRACT_HTTP_MOCK_ALLOC_H
#define ABSTRACT_HTTP_MOCK_ALLOC_H

/* clang-format off */
#ifdef _WIN32
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#undef socket
#undef bind
#undef listen
#undef accept
#undef recv
#undef select
#endif
#ifndef __cplusplus
#if !defined(inline)
#if defined(_MSC_VER)
#define inline __inline
#elif defined(__GNUC__) || defined(__clang__)
#define inline __inline__
#endif
#endif
#endif
#include <winsock2.h>
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
#define socket c_abstract_http_mock_socket
#define bind c_abstract_http_mock_bind
#define listen c_abstract_http_mock_listen
#define accept c_abstract_http_mock_accept
#define recv c_abstract_http_mock_recv
#define select c_abstract_http_mock_select
#endif
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#endif

#include <stdio.h>
#include <c_abstract_http/http_types.h>
#include <c_abstract_http/event_loop.h>
#include <c89stringutils_string_extras.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ABSTRACT_HTTP_DECL_MOCK_INT_VAR(name)                                  \
  extern int g_mock_##name;                                                    \
  NO_DISCARD c_abstract_http_error_t abstract_http_mock_get_g_mock_##name(     \
      int **out_ptr);

ABSTRACT_HTTP_DECL_MOCK_INT_VAR(sha1_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(alloc_count)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(pthread_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(pthread_create_sync)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(pipe_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fork_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(waitpid_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(select_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(select_error_fds)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(time_jump)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(time_jump_count)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(time_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fwrite_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fclose_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(socket_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(bind_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(listen_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(accept_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(recv_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(config_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(response_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(multi_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(parts_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(headers_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(cond_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(mutex_free_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(getsockname_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(mutex_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(cond_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(strcasecmp_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(headers_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(parts_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(multi_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(sprintf_s_wrapper_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(urlencode_append_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(mask_key_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(serialize_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(pack_header_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_send_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_connect_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_gethostbyname_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_nonblocking_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_blocking_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(raw_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(aria2_system_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(aria2_fopen_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(aria2_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(aria2_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(xquic_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(xquic_engine_present)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(picoquic_create_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(picoquic_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(picoquic_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(picoquic_quic_null_on_free)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wasm_fetch_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wasm_fetch_timeout)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wasm_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wasm_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wasm_header_add_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wasm_headers_len_zero)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_global_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_parse_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_req_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_err_code)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_empty_body)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_response_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_upload_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_upload_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_loop_wakeup_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_stream_pipe)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(fetch_env_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_global_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_conn_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_response_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_send_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(nghttp3_loop_wakeup_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_global_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_base_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_conn_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_req_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_make_req_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_res_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_body_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_buf_add_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libevent_empty_body)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_global_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_req_buf_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_res_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_body_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_headers_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_addrinfo_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_connect_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_write_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_read_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libuv_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_global_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_engine_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_res_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_body_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(lsquic_read_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_global_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_session_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_msg_new_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_send_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_res_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_body_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(libsoup3_uri_parse_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_getenv_detached)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_getenv_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_attach_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_new_string_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_find_class_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_get_method_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_new_object_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_call_object_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_res_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_input_stream_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_error_stream_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_read_exception)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_body_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_final_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_cleanup_exception)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_read_chunks)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_status_code)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(android_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_api_open_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_config_open_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_conn_open_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_request_open_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_send_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_shutdown_error)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_header_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_getaddrinfo_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_getaddrinfo_null_result)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_mutex_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_config_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_global_lock_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_res_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_cond_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_send_lock_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_cond_wait_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_header_add_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_cb_mutex_lock_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_parse_url_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_extra_events)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_zero_addrlen)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(msh3_cond_wait_loop)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_open_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_connect_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_open_request_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_send_request_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_write_data_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_receive_response_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_query_headers_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_query_data_available_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_read_data_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_set_timeouts_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_set_option_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_crack_url_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_add_request_headers_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_queue_work_item_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_status_code)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_cookie_count)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_read_chunks)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_total_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_read_buf_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_close_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(winhttp_cookie_set_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_open_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_connect_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_open_request_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_send_request_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_send_request_ex_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_write_file_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_end_request_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_query_info_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_read_file_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_set_option_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_crack_url_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_add_headers_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_context_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_response_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_status_code)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_cookie_count)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_read_chunks)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_read_chunk_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_body_realloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_res_alloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_close_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(wininet_cookie_set_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(accept_fd)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(server_reading)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(timer_heap_swap_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(cond_init_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(queue_malloc_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(parser_destroy_fail)
ABSTRACT_HTTP_DECL_MOCK_INT_VAR(http_types_strdup_fail)

extern const char *g_mock_recv_data;
NO_DISCARD c_abstract_http_error_t
abstract_http_mock_get_g_mock_recv_data(const char ***out_ptr);
extern const unsigned char *g_mock_recv_bin;
extern size_t g_mock_recv_bin_len;
extern int g_mock_recv_bin_eof;

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT __declspec(restrict)
#define ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS __declspec(noalias)
#else
#define ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT
#define ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS
#endif

ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_malloc(size_t size);
ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_calloc(size_t count, size_t size);
ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_realloc(void *ptr, size_t size);
ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void c_abstract_http_mock_free(void *ptr);

NO_DISCARD c_abstract_http_error_t c_abstract_http_mock_strdup(const char *s,
                                                               char **out);

size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nmemb,
                                   FILE *stream);
int c_abstract_http_mock_fclose(FILE *stream);

#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
int c_abstract_http_mock_pipe(int fildes[2]);
pid_t c_abstract_http_mock_fork(void);
pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options);
int c_abstract_http_mock_pthread_key_create(pthread_key_t *key,
                                            void (*destructor)(void *));
int c_abstract_http_mock_pthread_mutex_init(pthread_mutex_t *mutex,
                                            const pthread_mutexattr_t *attr);
int c_abstract_http_mock_pthread_cond_init(pthread_cond_t *cond,
                                           const pthread_condattr_t *attr);
int c_abstract_http_mock_pthread_create(pthread_t *thread,
                                        const pthread_attr_t *attr,
                                        void *(*start_routine)(void *),
                                        void *arg);
int c_abstract_http_mock_pthread_join(pthread_t thread, void **value_ptr);
int c_abstract_http_mock_pthread_setspecific(pthread_key_t key,
                                             const void *value);
void *c_abstract_http_mock_pthread_getspecific(pthread_key_t key);
#endif

#if defined(_WIN32)
extern SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type,
                                                 int protocol);
extern int WSAAPI c_abstract_http_mock_bind(SOCKET socket,
                                            const struct sockaddr *address,
                                            int address_len);
extern int WSAAPI c_abstract_http_mock_listen(SOCKET socket, int backlog);
extern SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket,
                                                 struct sockaddr *address,
                                                 int *address_len);
extern int WSAAPI c_abstract_http_mock_recv(SOCKET socket, char *buffer,
                                            int length, int flags);
extern int WSAAPI c_abstract_http_mock_getsockname(SOCKET socket,
                                                   struct sockaddr *address,
                                                   int *address_len);
extern int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                              fd_set *writefds,
                                              fd_set *exceptfds,
                                              const struct timeval *timeout);
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
extern int c_abstract_http_mock_socket(int domain, int type, int protocol);
extern int c_abstract_http_mock_bind(int socket, const struct sockaddr *address,
                                     socklen_t address_len);
extern int c_abstract_http_mock_listen(int socket, int backlog);
extern int c_abstract_http_mock_accept(int socket, struct sockaddr *address,
                                       socklen_t *address_len);
extern ssize_t c_abstract_http_mock_recv(int socket, void *buffer,
                                         size_t length, int flags);
extern int c_abstract_http_mock_getsockname(int socket,
                                            struct sockaddr *address,
                                            socklen_t *address_len);
extern int c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *exceptfds,
                                       struct timeval *timeout);
extern int c_abstract_http_mock_execv(const char *path, char *const argv[]);
#endif

NO_DISCARD c_abstract_http_error_t
c_abstract_http_mock_get_time_ms(abstract_http_int64_t *out_ms);

NO_DISCARD c_abstract_http_error_t dummy_cb_thread(void *arg);
void *dummy_cb_pthread(void *arg);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_ws_send_mode(int mode);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_oauth2_redirect_env(const char *val);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_native_mux_fail(int v);
NO_DISCARD extern c_abstract_http_error_t
c_abstract_http_test_set_loop_op_fail(int op, int countdown);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ABSTRACT_HTTP_MOCK_ALLOC_H */
