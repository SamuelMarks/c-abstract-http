
#ifndef ABSTRACT_HTTP_MOCK_ALLOC_H
#define ABSTRACT_HTTP_MOCK_ALLOC_H

/* clang-format off */
#ifdef _WIN32
#pragma push_macro("socket")
#pragma push_macro("bind")
#pragma push_macro("listen")
#pragma push_macro("accept")
#pragma push_macro("recv")
#pragma push_macro("select")
#undef socket
#undef bind
#undef listen
#undef accept
#undef recv
#undef select
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
#pragma pop_macro("select")
#pragma pop_macro("recv")
#pragma pop_macro("accept")
#pragma pop_macro("listen")
#pragma pop_macro("bind")
#pragma pop_macro("socket")
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#endif

#include <stdio.h>
#include <c_abstract_http/http_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int *abstract_http_mock_get_g_mock_sha1_fail(void);
extern int *abstract_http_mock_get_g_mock_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_alloc_count(void);
extern int *abstract_http_mock_get_g_mock_pthread_fail(void);
extern int *abstract_http_mock_get_g_mock_pthread_create_sync(void);
extern int *abstract_http_mock_get_g_mock_pipe_fail(void);
extern int *abstract_http_mock_get_g_mock_fork_fail(void);
extern int *abstract_http_mock_get_g_mock_waitpid_fail(void);
extern int *abstract_http_mock_get_g_mock_select_fail(void);
extern int *abstract_http_mock_get_g_mock_select_error_fds(void);
extern int *abstract_http_mock_get_g_mock_time_jump(void);
extern int *abstract_http_mock_get_g_mock_time_jump_count(void);
extern int *abstract_http_mock_get_g_mock_fwrite_fail(void);
extern int *abstract_http_mock_get_g_mock_fclose_fail(void);
extern int *abstract_http_mock_get_g_mock_socket_fail(void);
extern int *abstract_http_mock_get_g_mock_bind_fail(void);
extern int *abstract_http_mock_get_g_mock_listen_fail(void);
extern int *abstract_http_mock_get_g_mock_accept_fail(void);
extern int *abstract_http_mock_get_g_mock_recv_fail(void);
extern int *abstract_http_mock_get_g_mock_getsockname_fail(void);
extern int *abstract_http_mock_get_g_mock_mutex_fail(void);
extern int *abstract_http_mock_get_g_mock_cond_fail(void);
extern int *abstract_http_mock_get_g_mock_strcasecmp_fail(void);
extern int *abstract_http_mock_get_g_mock_headers_init_fail(void);
extern int *abstract_http_mock_get_g_mock_parts_init_fail(void);
extern int *abstract_http_mock_get_g_mock_multi_init_fail(void);
extern int *abstract_http_mock_get_g_mock_mask_key_fail(void);
extern int *abstract_http_mock_get_g_mock_pack_header_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_send_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_gethostbyname_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_nonblocking_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_blocking_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_raw_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_aria2_system_fail(void);
extern int *abstract_http_mock_get_g_mock_aria2_fopen_fail(void);
extern int *abstract_http_mock_get_g_mock_aria2_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_aria2_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_xquic_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_xquic_engine_present(void);
extern int *abstract_http_mock_get_g_mock_picoquic_create_fail(void);
extern int *abstract_http_mock_get_g_mock_picoquic_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_picoquic_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_picoquic_quic_null_on_free(void);
extern int *abstract_http_mock_get_g_mock_wasm_fetch_fail(void);
extern int *abstract_http_mock_get_g_mock_wasm_fetch_timeout(void);
extern int *abstract_http_mock_get_g_mock_wasm_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_wasm_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_wasm_header_add_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_parse_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_req_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_err_code(void);
extern int *abstract_http_mock_get_g_mock_fetch_empty_body(void);
extern int *abstract_http_mock_get_g_mock_fetch_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_response_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_upload_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_upload_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_conn_new_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_response_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_send_fail(void);
extern int *abstract_http_mock_get_g_mock_nghttp3_loop_wakeup_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_base_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_conn_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_req_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_make_req_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_buf_add_fail(void);
extern int *abstract_http_mock_get_g_mock_libevent_empty_body(void);
extern int *abstract_http_mock_get_g_mock_libuv_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_req_buf_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_headers_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_addrinfo_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_write_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_read_fail(void);
extern int *abstract_http_mock_get_g_mock_libuv_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_engine_new_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_lsquic_read_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_global_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_session_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_msg_new_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_send_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_libsoup3_uri_parse_fail(void);
extern int *abstract_http_mock_get_g_mock_android_getenv_detached(void);
extern int *abstract_http_mock_get_g_mock_android_getenv_fail(void);
extern int *abstract_http_mock_get_g_mock_android_attach_fail(void);
extern int *abstract_http_mock_get_g_mock_android_new_string_fail(void);
extern int *abstract_http_mock_get_g_mock_android_find_class_fail(void);
extern int *abstract_http_mock_get_g_mock_android_get_method_fail(void);
extern int *abstract_http_mock_get_g_mock_android_new_object_fail(void);
extern int *abstract_http_mock_get_g_mock_android_call_object_fail(void);
extern int *abstract_http_mock_get_g_mock_android_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_android_input_stream_fail(void);
extern int *abstract_http_mock_get_g_mock_android_error_stream_fail(void);
extern int *abstract_http_mock_get_g_mock_android_read_exception(void);
extern int *abstract_http_mock_get_g_mock_android_body_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_final_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_android_cleanup_exception(void);
extern int *abstract_http_mock_get_g_mock_android_read_chunks(void);
extern int *abstract_http_mock_get_g_mock_android_status_code(void);
extern int *abstract_http_mock_get_g_mock_android_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_api_open_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_config_open_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_conn_open_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_request_open_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_send_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_shutdown_error(void);
extern int *abstract_http_mock_get_g_mock_msh3_header_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_getaddrinfo_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_getaddrinfo_null_result(void);
extern int *abstract_http_mock_get_g_mock_msh3_mutex_init_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_config_init_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_global_lock_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_res_init_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_cond_init_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_send_lock_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_cond_wait_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_header_add_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_cb_mutex_lock_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_parse_url_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_msh3_extra_events(void);
extern int *abstract_http_mock_get_g_mock_winhttp_open_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_open_request_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_send_request_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_write_data_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_receive_response_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_query_headers_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_query_data_available_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_read_data_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_set_timeouts_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_set_option_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_crack_url_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_add_request_headers_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_queue_work_item_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_status_code(void);
extern int *abstract_http_mock_get_g_mock_winhttp_cookie_count(void);
extern int *abstract_http_mock_get_g_mock_winhttp_read_chunks(void);
extern int *abstract_http_mock_get_g_mock_winhttp_total_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_read_buf_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_winhttp_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_open_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_connect_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_open_request_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_send_request_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_send_request_ex_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_write_file_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_end_request_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_query_info_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_read_file_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_set_option_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_crack_url_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_add_headers_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_context_init_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_response_init_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_status_code(void);
extern int *abstract_http_mock_get_g_mock_wininet_cookie_count(void);
extern int *abstract_http_mock_get_g_mock_wininet_read_chunks(void);
extern int *abstract_http_mock_get_g_mock_wininet_read_chunk_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_body_realloc_fail(void);
extern int *abstract_http_mock_get_g_mock_wininet_res_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_accept_fd(void);
extern int *abstract_http_mock_get_g_mock_server_reading(void);
extern const char **abstract_http_mock_get_g_mock_recv_data(void);

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

size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nmemb,
                                   FILE *stream);
int c_abstract_http_mock_fclose(FILE *stream);
#ifndef _WIN32
int c_abstract_http_mock_pipe(int fildes[2]);
pid_t c_abstract_http_mock_fork(void);
pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options);
int c_abstract_http_mock_pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int c_abstract_http_mock_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int c_abstract_http_mock_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int c_abstract_http_mock_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int c_abstract_http_mock_pthread_join(pthread_t thread, void **value_ptr);
int c_abstract_http_mock_pthread_setspecific(pthread_key_t key, const void *value);
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
#else
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
#endif

#define g_mock_sha1_fail (*abstract_http_mock_get_g_mock_sha1_fail())
#define g_mock_alloc_fail (*abstract_http_mock_get_g_mock_alloc_fail())
#define g_mock_alloc_count (*abstract_http_mock_get_g_mock_alloc_count())
#define g_mock_pthread_fail (*abstract_http_mock_get_g_mock_pthread_fail())
#define g_mock_pthread_create_sync (*abstract_http_mock_get_g_mock_pthread_create_sync())
#define g_mock_pipe_fail (*abstract_http_mock_get_g_mock_pipe_fail())
#define g_mock_fork_fail (*abstract_http_mock_get_g_mock_fork_fail())
#define g_mock_waitpid_fail (*abstract_http_mock_get_g_mock_waitpid_fail())
#define g_mock_select_fail (*abstract_http_mock_get_g_mock_select_fail())
#define g_mock_select_error_fds (*abstract_http_mock_get_g_mock_select_error_fds())
#define g_mock_time_jump (*abstract_http_mock_get_g_mock_time_jump())
#define g_mock_time_jump_count (*abstract_http_mock_get_g_mock_time_jump_count())
#define g_mock_fwrite_fail (*abstract_http_mock_get_g_mock_fwrite_fail())
#define g_mock_fclose_fail (*abstract_http_mock_get_g_mock_fclose_fail())
#define g_mock_socket_fail (*abstract_http_mock_get_g_mock_socket_fail())
#define g_mock_bind_fail (*abstract_http_mock_get_g_mock_bind_fail())
#define g_mock_listen_fail (*abstract_http_mock_get_g_mock_listen_fail())
#define g_mock_accept_fail (*abstract_http_mock_get_g_mock_accept_fail())
#define g_mock_recv_fail (*abstract_http_mock_get_g_mock_recv_fail())
#define g_mock_getsockname_fail (*abstract_http_mock_get_g_mock_getsockname_fail())
#define g_mock_mutex_fail (*abstract_http_mock_get_g_mock_mutex_fail())
#define g_mock_cond_fail (*abstract_http_mock_get_g_mock_cond_fail())
#define g_mock_strcasecmp_fail (*abstract_http_mock_get_g_mock_strcasecmp_fail())
#define g_mock_headers_init_fail (*abstract_http_mock_get_g_mock_headers_init_fail())
#define g_mock_parts_init_fail (*abstract_http_mock_get_g_mock_parts_init_fail())
#define g_mock_multi_init_fail (*abstract_http_mock_get_g_mock_multi_init_fail())
#define g_mock_mask_key_fail (*abstract_http_mock_get_g_mock_mask_key_fail())
#define g_mock_pack_header_fail (*abstract_http_mock_get_g_mock_pack_header_fail())
#define g_mock_raw_send_fail (*abstract_http_mock_get_g_mock_raw_send_fail())
#define g_mock_raw_connect_fail (*abstract_http_mock_get_g_mock_raw_connect_fail())
#define g_mock_raw_gethostbyname_fail (*abstract_http_mock_get_g_mock_raw_gethostbyname_fail())
#define g_mock_raw_nonblocking_fail (*abstract_http_mock_get_g_mock_raw_nonblocking_fail())
#define g_mock_raw_blocking_fail (*abstract_http_mock_get_g_mock_raw_blocking_fail())
#define g_mock_raw_realloc_fail (*abstract_http_mock_get_g_mock_raw_realloc_fail())
#define g_mock_raw_response_init_fail (*abstract_http_mock_get_g_mock_raw_response_init_fail())
#define g_mock_aria2_system_fail (*abstract_http_mock_get_g_mock_aria2_system_fail())
#define g_mock_aria2_fopen_fail (*abstract_http_mock_get_g_mock_aria2_fopen_fail())
#define g_mock_aria2_response_init_fail (*abstract_http_mock_get_g_mock_aria2_response_init_fail())
#define g_mock_aria2_config_init_fail (*abstract_http_mock_get_g_mock_aria2_config_init_fail())
#define g_mock_xquic_config_init_fail (*abstract_http_mock_get_g_mock_xquic_config_init_fail())
#define g_mock_xquic_engine_present (*abstract_http_mock_get_g_mock_xquic_engine_present())
#define g_mock_picoquic_create_fail (*abstract_http_mock_get_g_mock_picoquic_create_fail())
#define g_mock_picoquic_config_init_fail (*abstract_http_mock_get_g_mock_picoquic_config_init_fail())
#define g_mock_picoquic_response_init_fail (*abstract_http_mock_get_g_mock_picoquic_response_init_fail())
#define g_mock_picoquic_quic_null_on_free (*abstract_http_mock_get_g_mock_picoquic_quic_null_on_free())
#define g_mock_wasm_fetch_fail (*abstract_http_mock_get_g_mock_wasm_fetch_fail())
#define g_mock_wasm_fetch_timeout (*abstract_http_mock_get_g_mock_wasm_fetch_timeout())
#define g_mock_wasm_config_init_fail (*abstract_http_mock_get_g_mock_wasm_config_init_fail())
#define g_mock_wasm_response_init_fail (*abstract_http_mock_get_g_mock_wasm_response_init_fail())
#define g_mock_wasm_header_add_fail (*abstract_http_mock_get_g_mock_wasm_header_add_fail())
#define g_mock_fetch_global_init_fail (*abstract_http_mock_get_g_mock_fetch_global_init_fail())
#define g_mock_fetch_context_init_fail (*abstract_http_mock_get_g_mock_fetch_context_init_fail())
#define g_mock_fetch_config_init_fail (*abstract_http_mock_get_g_mock_fetch_config_init_fail())
#define g_mock_fetch_parse_fail (*abstract_http_mock_get_g_mock_fetch_parse_fail())
#define g_mock_fetch_req_fail (*abstract_http_mock_get_g_mock_fetch_req_fail())
#define g_mock_fetch_err_code (*abstract_http_mock_get_g_mock_fetch_err_code())
#define g_mock_fetch_empty_body (*abstract_http_mock_get_g_mock_fetch_empty_body())
#define g_mock_fetch_response_init_fail (*abstract_http_mock_get_g_mock_fetch_response_init_fail())
#define g_mock_fetch_response_alloc_fail (*abstract_http_mock_get_g_mock_fetch_response_alloc_fail())
#define g_mock_fetch_upload_alloc_fail (*abstract_http_mock_get_g_mock_fetch_upload_alloc_fail())
#define g_mock_fetch_upload_realloc_fail (*abstract_http_mock_get_g_mock_fetch_upload_realloc_fail())
#define g_mock_fetch_body_realloc_fail (*abstract_http_mock_get_g_mock_fetch_body_realloc_fail())
#define g_mock_fetch_loop_wakeup_fail (*abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail())
#define g_mock_nghttp3_global_init_fail (*abstract_http_mock_get_g_mock_nghttp3_global_init_fail())
#define g_mock_nghttp3_context_init_fail (*abstract_http_mock_get_g_mock_nghttp3_context_init_fail())
#define g_mock_nghttp3_config_init_fail (*abstract_http_mock_get_g_mock_nghttp3_config_init_fail())
#define g_mock_nghttp3_conn_new_fail (*abstract_http_mock_get_g_mock_nghttp3_conn_new_fail())
#define g_mock_nghttp3_response_alloc_fail (*abstract_http_mock_get_g_mock_nghttp3_response_alloc_fail())
#define g_mock_nghttp3_response_init_fail (*abstract_http_mock_get_g_mock_nghttp3_response_init_fail())
#define g_mock_nghttp3_send_fail (*abstract_http_mock_get_g_mock_nghttp3_send_fail())
#define g_mock_nghttp3_loop_wakeup_fail (*abstract_http_mock_get_g_mock_nghttp3_loop_wakeup_fail())
#define g_mock_libevent_global_init_fail (*abstract_http_mock_get_g_mock_libevent_global_init_fail())
#define g_mock_libevent_context_init_fail (*abstract_http_mock_get_g_mock_libevent_context_init_fail())
#define g_mock_libevent_config_init_fail (*abstract_http_mock_get_g_mock_libevent_config_init_fail())
#define g_mock_libevent_base_new_fail (*abstract_http_mock_get_g_mock_libevent_base_new_fail())
#define g_mock_libevent_conn_new_fail (*abstract_http_mock_get_g_mock_libevent_conn_new_fail())
#define g_mock_libevent_req_new_fail (*abstract_http_mock_get_g_mock_libevent_req_new_fail())
#define g_mock_libevent_make_req_fail (*abstract_http_mock_get_g_mock_libevent_make_req_fail())
#define g_mock_libevent_res_alloc_fail (*abstract_http_mock_get_g_mock_libevent_res_alloc_fail())
#define g_mock_libevent_res_init_fail (*abstract_http_mock_get_g_mock_libevent_res_init_fail())
#define g_mock_libevent_body_alloc_fail (*abstract_http_mock_get_g_mock_libevent_body_alloc_fail())
#define g_mock_libevent_buf_add_fail (*abstract_http_mock_get_g_mock_libevent_buf_add_fail())
#define g_mock_libevent_empty_body (*abstract_http_mock_get_g_mock_libevent_empty_body())
#define g_mock_libuv_global_init_fail (*abstract_http_mock_get_g_mock_libuv_global_init_fail())
#define g_mock_libuv_context_init_fail (*abstract_http_mock_get_g_mock_libuv_context_init_fail())
#define g_mock_libuv_config_init_fail (*abstract_http_mock_get_g_mock_libuv_config_init_fail())
#define g_mock_libuv_req_buf_alloc_fail (*abstract_http_mock_get_g_mock_libuv_req_buf_alloc_fail())
#define g_mock_libuv_res_alloc_fail (*abstract_http_mock_get_g_mock_libuv_res_alloc_fail())
#define g_mock_libuv_res_init_fail (*abstract_http_mock_get_g_mock_libuv_res_init_fail())
#define g_mock_libuv_body_alloc_fail (*abstract_http_mock_get_g_mock_libuv_body_alloc_fail())
#define g_mock_libuv_body_realloc_fail (*abstract_http_mock_get_g_mock_libuv_body_realloc_fail())
#define g_mock_libuv_headers_realloc_fail (*abstract_http_mock_get_g_mock_libuv_headers_realloc_fail())
#define g_mock_libuv_addrinfo_fail (*abstract_http_mock_get_g_mock_libuv_addrinfo_fail())
#define g_mock_libuv_connect_fail (*abstract_http_mock_get_g_mock_libuv_connect_fail())
#define g_mock_libuv_write_fail (*abstract_http_mock_get_g_mock_libuv_write_fail())
#define g_mock_libuv_read_fail (*abstract_http_mock_get_g_mock_libuv_read_fail())
#define g_mock_libuv_alloc_fail (*abstract_http_mock_get_g_mock_libuv_alloc_fail())
#define g_mock_lsquic_global_init_fail (*abstract_http_mock_get_g_mock_lsquic_global_init_fail())
#define g_mock_lsquic_context_init_fail (*abstract_http_mock_get_g_mock_lsquic_context_init_fail())
#define g_mock_lsquic_config_init_fail (*abstract_http_mock_get_g_mock_lsquic_config_init_fail())
#define g_mock_lsquic_engine_new_fail (*abstract_http_mock_get_g_mock_lsquic_engine_new_fail())
#define g_mock_lsquic_res_alloc_fail (*abstract_http_mock_get_g_mock_lsquic_res_alloc_fail())
#define g_mock_lsquic_res_init_fail (*abstract_http_mock_get_g_mock_lsquic_res_init_fail())
#define g_mock_lsquic_body_alloc_fail (*abstract_http_mock_get_g_mock_lsquic_body_alloc_fail())
#define g_mock_lsquic_read_fail (*abstract_http_mock_get_g_mock_lsquic_read_fail())
#define g_mock_libsoup3_global_init_fail (*abstract_http_mock_get_g_mock_libsoup3_global_init_fail())
#define g_mock_libsoup3_context_init_fail (*abstract_http_mock_get_g_mock_libsoup3_context_init_fail())
#define g_mock_libsoup3_config_init_fail (*abstract_http_mock_get_g_mock_libsoup3_config_init_fail())
#define g_mock_libsoup3_session_new_fail (*abstract_http_mock_get_g_mock_libsoup3_session_new_fail())
#define g_mock_libsoup3_msg_new_fail (*abstract_http_mock_get_g_mock_libsoup3_msg_new_fail())
#define g_mock_libsoup3_send_fail (*abstract_http_mock_get_g_mock_libsoup3_send_fail())
#define g_mock_libsoup3_res_alloc_fail (*abstract_http_mock_get_g_mock_libsoup3_res_alloc_fail())
#define g_mock_libsoup3_res_init_fail (*abstract_http_mock_get_g_mock_libsoup3_res_init_fail())
#define g_mock_libsoup3_body_alloc_fail (*abstract_http_mock_get_g_mock_libsoup3_body_alloc_fail())
#define g_mock_libsoup3_uri_parse_fail (*abstract_http_mock_get_g_mock_libsoup3_uri_parse_fail())
#define g_mock_android_getenv_detached (*abstract_http_mock_get_g_mock_android_getenv_detached())
#define g_mock_android_getenv_fail (*abstract_http_mock_get_g_mock_android_getenv_fail())
#define g_mock_android_attach_fail (*abstract_http_mock_get_g_mock_android_attach_fail())
#define g_mock_android_new_string_fail (*abstract_http_mock_get_g_mock_android_new_string_fail())
#define g_mock_android_find_class_fail (*abstract_http_mock_get_g_mock_android_find_class_fail())
#define g_mock_android_get_method_fail (*abstract_http_mock_get_g_mock_android_get_method_fail())
#define g_mock_android_new_object_fail (*abstract_http_mock_get_g_mock_android_new_object_fail())
#define g_mock_android_call_object_fail (*abstract_http_mock_get_g_mock_android_call_object_fail())
#define g_mock_android_res_alloc_fail (*abstract_http_mock_get_g_mock_android_res_alloc_fail())
#define g_mock_android_res_init_fail (*abstract_http_mock_get_g_mock_android_res_init_fail())
#define g_mock_android_input_stream_fail (*abstract_http_mock_get_g_mock_android_input_stream_fail())
#define g_mock_android_error_stream_fail (*abstract_http_mock_get_g_mock_android_error_stream_fail())
#define g_mock_android_read_exception (*abstract_http_mock_get_g_mock_android_read_exception())
#define g_mock_android_body_alloc_fail (*abstract_http_mock_get_g_mock_android_body_alloc_fail())
#define g_mock_android_body_realloc_fail (*abstract_http_mock_get_g_mock_android_body_realloc_fail())
#define g_mock_android_final_body_realloc_fail (*abstract_http_mock_get_g_mock_android_final_body_realloc_fail())
#define g_mock_android_cleanup_exception (*abstract_http_mock_get_g_mock_android_cleanup_exception())
#define g_mock_android_read_chunks (*abstract_http_mock_get_g_mock_android_read_chunks())
#define g_mock_android_status_code (*abstract_http_mock_get_g_mock_android_status_code())
#define g_mock_android_config_init_fail (*abstract_http_mock_get_g_mock_android_config_init_fail())
#define g_mock_msh3_api_open_fail (*abstract_http_mock_get_g_mock_msh3_api_open_fail())
#define g_mock_msh3_config_open_fail (*abstract_http_mock_get_g_mock_msh3_config_open_fail())
#define g_mock_msh3_conn_open_fail (*abstract_http_mock_get_g_mock_msh3_conn_open_fail())
#define g_mock_msh3_request_open_fail (*abstract_http_mock_get_g_mock_msh3_request_open_fail())
#define g_mock_msh3_send_fail (*abstract_http_mock_get_g_mock_msh3_send_fail())
#define g_mock_msh3_shutdown_error (*abstract_http_mock_get_g_mock_msh3_shutdown_error())
#define g_mock_msh3_header_alloc_fail (*abstract_http_mock_get_g_mock_msh3_header_alloc_fail())
#define g_mock_msh3_body_realloc_fail (*abstract_http_mock_get_g_mock_msh3_body_realloc_fail())
#define g_mock_msh3_getaddrinfo_fail (*abstract_http_mock_get_g_mock_msh3_getaddrinfo_fail())
#define g_mock_msh3_getaddrinfo_null_result (*abstract_http_mock_get_g_mock_msh3_getaddrinfo_null_result())
#define g_mock_msh3_mutex_init_fail (*abstract_http_mock_get_g_mock_msh3_mutex_init_fail())
#define g_mock_msh3_config_init_fail (*abstract_http_mock_get_g_mock_msh3_config_init_fail())
#define g_mock_msh3_global_lock_fail (*abstract_http_mock_get_g_mock_msh3_global_lock_fail())
#define g_mock_msh3_res_alloc_fail (*abstract_http_mock_get_g_mock_msh3_res_alloc_fail())
#define g_mock_msh3_res_init_fail (*abstract_http_mock_get_g_mock_msh3_res_init_fail())
#define g_mock_msh3_cond_init_fail (*abstract_http_mock_get_g_mock_msh3_cond_init_fail())
#define g_mock_msh3_send_lock_fail (*abstract_http_mock_get_g_mock_msh3_send_lock_fail())
#define g_mock_msh3_cond_wait_fail (*abstract_http_mock_get_g_mock_msh3_cond_wait_fail())
#define g_mock_msh3_header_add_fail (*abstract_http_mock_get_g_mock_msh3_header_add_fail())
#define g_mock_msh3_cb_mutex_lock_fail (*abstract_http_mock_get_g_mock_msh3_cb_mutex_lock_fail())
#define g_mock_msh3_parse_url_alloc_fail (*abstract_http_mock_get_g_mock_msh3_parse_url_alloc_fail())
#define g_mock_msh3_extra_events (*abstract_http_mock_get_g_mock_msh3_extra_events())
#define g_mock_winhttp_open_fail (*abstract_http_mock_get_g_mock_winhttp_open_fail())
#define g_mock_winhttp_connect_fail (*abstract_http_mock_get_g_mock_winhttp_connect_fail())
#define g_mock_winhttp_open_request_fail (*abstract_http_mock_get_g_mock_winhttp_open_request_fail())
#define g_mock_winhttp_send_request_fail (*abstract_http_mock_get_g_mock_winhttp_send_request_fail())
#define g_mock_winhttp_write_data_fail (*abstract_http_mock_get_g_mock_winhttp_write_data_fail())
#define g_mock_winhttp_receive_response_fail (*abstract_http_mock_get_g_mock_winhttp_receive_response_fail())
#define g_mock_winhttp_query_headers_fail (*abstract_http_mock_get_g_mock_winhttp_query_headers_fail())
#define g_mock_winhttp_query_data_available_fail (*abstract_http_mock_get_g_mock_winhttp_query_data_available_fail())
#define g_mock_winhttp_read_data_fail (*abstract_http_mock_get_g_mock_winhttp_read_data_fail())
#define g_mock_winhttp_set_timeouts_fail (*abstract_http_mock_get_g_mock_winhttp_set_timeouts_fail())
#define g_mock_winhttp_set_option_fail (*abstract_http_mock_get_g_mock_winhttp_set_option_fail())
#define g_mock_winhttp_crack_url_fail (*abstract_http_mock_get_g_mock_winhttp_crack_url_fail())
#define g_mock_winhttp_add_request_headers_fail (*abstract_http_mock_get_g_mock_winhttp_add_request_headers_fail())
#define g_mock_winhttp_queue_work_item_fail (*abstract_http_mock_get_g_mock_winhttp_queue_work_item_fail())
#define g_mock_winhttp_context_init_fail (*abstract_http_mock_get_g_mock_winhttp_context_init_fail())
#define g_mock_winhttp_response_init_fail (*abstract_http_mock_get_g_mock_winhttp_response_init_fail())
#define g_mock_winhttp_status_code (*abstract_http_mock_get_g_mock_winhttp_status_code())
#define g_mock_winhttp_cookie_count (*abstract_http_mock_get_g_mock_winhttp_cookie_count())
#define g_mock_winhttp_read_chunks (*abstract_http_mock_get_g_mock_winhttp_read_chunks())
#define g_mock_winhttp_total_body_realloc_fail (*abstract_http_mock_get_g_mock_winhttp_total_body_realloc_fail())
#define g_mock_winhttp_read_buf_alloc_fail (*abstract_http_mock_get_g_mock_winhttp_read_buf_alloc_fail())
#define g_mock_winhttp_res_alloc_fail (*abstract_http_mock_get_g_mock_winhttp_res_alloc_fail())
#define g_mock_wininet_open_fail (*abstract_http_mock_get_g_mock_wininet_open_fail())
#define g_mock_wininet_connect_fail (*abstract_http_mock_get_g_mock_wininet_connect_fail())
#define g_mock_wininet_open_request_fail (*abstract_http_mock_get_g_mock_wininet_open_request_fail())
#define g_mock_wininet_send_request_fail (*abstract_http_mock_get_g_mock_wininet_send_request_fail())
#define g_mock_wininet_send_request_ex_fail (*abstract_http_mock_get_g_mock_wininet_send_request_ex_fail())
#define g_mock_wininet_write_file_fail (*abstract_http_mock_get_g_mock_wininet_write_file_fail())
#define g_mock_wininet_end_request_fail (*abstract_http_mock_get_g_mock_wininet_end_request_fail())
#define g_mock_wininet_query_info_fail (*abstract_http_mock_get_g_mock_wininet_query_info_fail())
#define g_mock_wininet_read_file_fail (*abstract_http_mock_get_g_mock_wininet_read_file_fail())
#define g_mock_wininet_set_option_fail (*abstract_http_mock_get_g_mock_wininet_set_option_fail())
#define g_mock_wininet_crack_url_fail (*abstract_http_mock_get_g_mock_wininet_crack_url_fail())
#define g_mock_wininet_add_headers_fail (*abstract_http_mock_get_g_mock_wininet_add_headers_fail())
#define g_mock_wininet_context_init_fail (*abstract_http_mock_get_g_mock_wininet_context_init_fail())
#define g_mock_wininet_response_init_fail (*abstract_http_mock_get_g_mock_wininet_response_init_fail())
#define g_mock_wininet_status_code (*abstract_http_mock_get_g_mock_wininet_status_code())
#define g_mock_wininet_cookie_count (*abstract_http_mock_get_g_mock_wininet_cookie_count())
#define g_mock_wininet_read_chunks (*abstract_http_mock_get_g_mock_wininet_read_chunks())
#define g_mock_wininet_read_chunk_alloc_fail (*abstract_http_mock_get_g_mock_wininet_read_chunk_alloc_fail())
#define g_mock_wininet_body_realloc_fail (*abstract_http_mock_get_g_mock_wininet_body_realloc_fail())
#define g_mock_wininet_res_alloc_fail (*abstract_http_mock_get_g_mock_wininet_res_alloc_fail())
#define g_mock_accept_fd (*abstract_http_mock_get_g_mock_accept_fd())
#define g_mock_server_reading (*abstract_http_mock_get_g_mock_server_reading())
#define g_mock_recv_data (*abstract_http_mock_get_g_mock_recv_data())

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
/* clang-format on */

uint64_t c_abstract_http_mock_math_get_current_time_ms(void);

void dummy_cb_thread(void *arg);
void *dummy_cb_pthread(void *arg);
extern enum c_abstract_http_error c_abstract_http_mock_strdup(const char *s,
                                                              char **out);

#ifdef __cplusplus
}
#endif

#endif
