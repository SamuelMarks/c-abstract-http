
#if defined(_WIN32) && defined(_MSC_VER)
#endif
#if defined(_MSC_VER)
#endif
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef pthread_key_create
#undef pthread_setspecific
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
#undef pthread_join
#undef pipe
#undef fork
#undef waitpid
#undef select
#undef math_get_current_time_ms
#undef pthread_getspecific
#undef fwrite
#undef fclose
#undef socket
#undef bind
#undef listen
#undef accept
#undef getsockname

/* clang-format off */
#include "mock_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#undef FD_SET
#define FD_SET(fd, set) do { \
    u_int __i; \
    for (__i = 0; __i < ((fd_set FAR *)(set))->fd_count; __i++) { \
        if (((fd_set FAR *)(set))->fd_array[__i] == (fd)) { \
            break; \
        } \
    } \
    if (__i == ((fd_set FAR *)(set))->fd_count) { \
        if (((fd_set FAR *)(set))->fd_count < FD_SETSIZE) { \
            ((fd_set FAR *)(set))->fd_array[__i] = (fd); \
            ((fd_set FAR *)(set))->fd_count++; \
        } \
    } \
} while((void)0, 0)
#endif

#undef g_mock_getsockname_fail

int g_mock_getsockname_fail = 0;
int *abstract_http_mock_get_g_mock_getsockname_fail(void) {
  return &g_mock_getsockname_fail;
}
#if defined(_MSC_VER)
#endif

#ifdef _WIN32
int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *errorfds,
                                       const struct timeval *timeout);
#else
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout);
#endif
size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nitems,
                                   FILE *stream);
int c_abstract_http_mock_fclose(FILE *stream);
#ifdef _WIN32
#include <basetsd.h>
#include <winsock2.h>
typedef SSIZE_T ssize_t;
typedef int socklen_t;
SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type, int protocol);
int WSAAPI c_abstract_http_mock_bind(SOCKET socket,
                                     const struct sockaddr *address,
                                     socklen_t address_len);
int WSAAPI c_abstract_http_mock_listen(SOCKET socket, int backlog);
SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket,
                                          struct sockaddr *address,
                                          int *address_len);
int WSAAPI c_abstract_http_mock_recv(SOCKET socket, char *buffer, int length,
                                     int flags);
#else
#include <sys/socket.h>
int c_abstract_http_mock_socket(int domain, int type, int protocol);
int c_abstract_http_mock_bind(int socket, const struct sockaddr *address,
                              socklen_t address_len);
int c_abstract_http_mock_listen(int socket, int backlog);
int c_abstract_http_mock_accept(int socket, struct sockaddr *address,
                                socklen_t *address_len);
ssize_t c_abstract_http_mock_recv(int socket, void *buffer, size_t length,
                                  int flags);
#endif

#undef g_mock_alloc_fail
#undef g_mock_alloc_count
#undef g_mock_pthread_fail
#undef g_mock_pipe_fail
#undef g_mock_fork_fail
#undef g_mock_waitpid_fail
#undef g_mock_select_fail
#undef g_mock_select_error_fds
#undef g_mock_time_jump
#undef g_mock_time_jump_count
#undef g_mock_fwrite_fail
#undef g_mock_fclose_fail
#undef g_mock_socket_fail
#undef g_mock_bind_fail
#undef g_mock_listen_fail
#undef g_mock_accept_fail
#undef g_mock_recv_fail
#undef g_mock_alloc_fail
#undef g_mock_alloc_count
#undef g_mock_pthread_fail
#undef g_mock_pipe_fail
#undef g_mock_fork_fail
#undef g_mock_waitpid_fail
#undef g_mock_select_fail
#undef g_mock_select_error_fds
#undef g_mock_time_jump
#undef g_mock_time_jump_count
#undef g_mock_fwrite_fail
#undef g_mock_fclose_fail
#undef g_mock_socket_fail
#undef g_mock_bind_fail
#undef g_mock_listen_fail
#undef g_mock_accept_fail
#undef g_mock_recv_fail
#undef g_mock_pthread_create_sync
#undef g_mock_mutex_fail
#undef g_mock_cond_fail
#undef g_mock_strcasecmp_fail
#undef g_mock_headers_init_fail
#undef g_mock_parts_init_fail
#undef g_mock_multi_init_fail
#undef g_mock_mask_key_fail
#undef g_mock_pack_header_fail
#undef g_mock_raw_send_fail
#undef g_mock_raw_connect_fail
#undef g_mock_raw_gethostbyname_fail
#undef g_mock_raw_nonblocking_fail
#undef g_mock_raw_blocking_fail
#undef g_mock_raw_realloc_fail
#undef g_mock_raw_response_init_fail
#undef g_mock_aria2_system_fail
#undef g_mock_aria2_fopen_fail
#undef g_mock_aria2_response_init_fail
#undef g_mock_aria2_config_init_fail
#undef g_mock_xquic_config_init_fail
#undef g_mock_xquic_engine_present
#undef g_mock_picoquic_create_fail
#undef g_mock_picoquic_config_init_fail
#undef g_mock_picoquic_response_init_fail
#undef g_mock_picoquic_quic_null_on_free
#undef g_mock_wasm_fetch_fail
#undef g_mock_wasm_fetch_timeout
#undef g_mock_wasm_config_init_fail
#undef g_mock_wasm_response_init_fail
#undef g_mock_wasm_header_add_fail
#undef g_mock_fetch_global_init_fail
#undef g_mock_fetch_context_init_fail
#undef g_mock_fetch_config_init_fail
#undef g_mock_fetch_parse_fail
#undef g_mock_fetch_req_fail
#undef g_mock_fetch_err_code
#undef g_mock_fetch_empty_body
#undef g_mock_fetch_response_init_fail
#undef g_mock_fetch_response_alloc_fail
#undef g_mock_fetch_upload_alloc_fail
#undef g_mock_fetch_upload_realloc_fail
#undef g_mock_fetch_body_realloc_fail
#undef g_mock_fetch_loop_wakeup_fail
#undef g_mock_nghttp3_global_init_fail
#undef g_mock_nghttp3_context_init_fail
#undef g_mock_nghttp3_config_init_fail
#undef g_mock_nghttp3_conn_new_fail
#undef g_mock_nghttp3_response_alloc_fail
#undef g_mock_nghttp3_response_init_fail
#undef g_mock_nghttp3_send_fail
#undef g_mock_nghttp3_loop_wakeup_fail
#undef g_mock_libevent_global_init_fail
#undef g_mock_libevent_context_init_fail
#undef g_mock_libevent_config_init_fail
#undef g_mock_libevent_base_new_fail
#undef g_mock_libevent_conn_new_fail
#undef g_mock_libevent_req_new_fail
#undef g_mock_libevent_make_req_fail
#undef g_mock_libevent_res_alloc_fail
#undef g_mock_libevent_res_init_fail
#undef g_mock_libevent_body_alloc_fail
#undef g_mock_libevent_buf_add_fail
#undef g_mock_libevent_empty_body
#undef g_mock_libuv_global_init_fail
#undef g_mock_libuv_context_init_fail
#undef g_mock_libuv_config_init_fail
#undef g_mock_libuv_req_buf_alloc_fail
#undef g_mock_libuv_res_alloc_fail
#undef g_mock_libuv_res_init_fail
#undef g_mock_libuv_body_alloc_fail
#undef g_mock_libuv_body_realloc_fail
#undef g_mock_libuv_headers_realloc_fail
#undef g_mock_libuv_addrinfo_fail
#undef g_mock_libuv_connect_fail
#undef g_mock_libuv_write_fail
#undef g_mock_libuv_read_fail
#undef g_mock_libuv_alloc_fail
#undef g_mock_lsquic_global_init_fail
#undef g_mock_lsquic_context_init_fail
#undef g_mock_lsquic_config_init_fail
#undef g_mock_lsquic_engine_new_fail
#undef g_mock_lsquic_res_alloc_fail
#undef g_mock_lsquic_res_init_fail
#undef g_mock_lsquic_body_alloc_fail
#undef g_mock_lsquic_read_fail
#undef g_mock_libsoup3_global_init_fail
#undef g_mock_libsoup3_context_init_fail
#undef g_mock_libsoup3_config_init_fail
#undef g_mock_libsoup3_session_new_fail
#undef g_mock_libsoup3_msg_new_fail
#undef g_mock_libsoup3_send_fail
#undef g_mock_libsoup3_res_alloc_fail
#undef g_mock_libsoup3_res_init_fail
#undef g_mock_libsoup3_body_alloc_fail
#undef g_mock_libsoup3_uri_parse_fail
#undef g_mock_android_getenv_detached
#undef g_mock_android_getenv_fail
#undef g_mock_android_attach_fail
#undef g_mock_android_new_string_fail
#undef g_mock_android_find_class_fail
#undef g_mock_android_get_method_fail
#undef g_mock_android_new_object_fail
#undef g_mock_android_call_object_fail
#undef g_mock_android_res_alloc_fail
#undef g_mock_android_res_init_fail
#undef g_mock_android_input_stream_fail
#undef g_mock_android_error_stream_fail
#undef g_mock_android_read_exception
#undef g_mock_android_body_alloc_fail
#undef g_mock_android_body_realloc_fail
#undef g_mock_android_final_body_realloc_fail
#undef g_mock_android_cleanup_exception
#undef g_mock_android_read_chunks
#undef g_mock_android_status_code
#undef g_mock_android_config_init_fail
#undef g_mock_msh3_api_open_fail
#undef g_mock_msh3_config_open_fail
#undef g_mock_msh3_conn_open_fail
#undef g_mock_msh3_request_open_fail
#undef g_mock_msh3_send_fail
#undef g_mock_msh3_shutdown_error
#undef g_mock_msh3_header_alloc_fail
#undef g_mock_msh3_body_realloc_fail
#undef g_mock_msh3_getaddrinfo_fail
#undef g_mock_msh3_getaddrinfo_null_result
#undef g_mock_msh3_mutex_init_fail
#undef g_mock_msh3_config_init_fail
#undef g_mock_msh3_global_lock_fail
#undef g_mock_msh3_res_alloc_fail
#undef g_mock_msh3_res_init_fail
#undef g_mock_msh3_cond_init_fail
#undef g_mock_msh3_send_lock_fail
#undef g_mock_msh3_cond_wait_fail
#undef g_mock_msh3_header_add_fail
#undef g_mock_msh3_cb_mutex_lock_fail
#undef g_mock_msh3_parse_url_alloc_fail
#undef g_mock_msh3_extra_events
#undef g_mock_winhttp_open_fail
#undef g_mock_winhttp_connect_fail
#undef g_mock_winhttp_open_request_fail
#undef g_mock_winhttp_send_request_fail
#undef g_mock_winhttp_write_data_fail
#undef g_mock_winhttp_receive_response_fail
#undef g_mock_winhttp_query_headers_fail
#undef g_mock_winhttp_query_data_available_fail
#undef g_mock_winhttp_read_data_fail
#undef g_mock_winhttp_set_timeouts_fail
#undef g_mock_winhttp_set_option_fail
#undef g_mock_winhttp_crack_url_fail
#undef g_mock_winhttp_add_request_headers_fail
#undef g_mock_winhttp_queue_work_item_fail
#undef g_mock_winhttp_context_init_fail
#undef g_mock_winhttp_response_init_fail
#undef g_mock_winhttp_status_code
#undef g_mock_winhttp_cookie_count
#undef g_mock_winhttp_read_chunks
#undef g_mock_winhttp_total_body_realloc_fail
#undef g_mock_winhttp_read_buf_alloc_fail
#undef g_mock_winhttp_res_alloc_fail
#undef g_mock_wininet_open_fail
#undef g_mock_wininet_connect_fail
#undef g_mock_wininet_open_request_fail
#undef g_mock_wininet_send_request_fail
#undef g_mock_wininet_send_request_ex_fail
#undef g_mock_wininet_write_file_fail
#undef g_mock_wininet_end_request_fail
#undef g_mock_wininet_query_info_fail
#undef g_mock_wininet_read_file_fail
#undef g_mock_wininet_set_option_fail
#undef g_mock_wininet_crack_url_fail
#undef g_mock_wininet_add_headers_fail
#undef g_mock_wininet_context_init_fail
#undef g_mock_wininet_response_init_fail
#undef g_mock_wininet_status_code
#undef g_mock_wininet_cookie_count
#undef g_mock_wininet_read_chunks
#undef g_mock_wininet_read_chunk_alloc_fail
#undef g_mock_wininet_body_realloc_fail
#undef g_mock_wininet_res_alloc_fail
#undef g_mock_accept_fd
#undef g_mock_server_reading
#undef g_mock_recv_data
#undef g_mock_sha1_fail
int g_mock_sha1_fail = 0;

int g_mock_alloc_fail = 0;
int g_mock_alloc_count = 0;
int g_mock_pthread_fail = 0;
int g_mock_pthread_create_sync = 0;
int g_mock_pipe_fail = 0;
int g_mock_fork_fail = 0;
int g_mock_waitpid_fail = 0;
int g_mock_select_fail = 0;
int g_mock_select_error_fds = 0;
int g_mock_time_jump = 0;
int g_mock_time_jump_count = 0;
int g_mock_fwrite_fail = 0;
int g_mock_fclose_fail = 0;
int g_mock_socket_fail = 0;
int g_mock_bind_fail = 0;
int g_mock_listen_fail = 0;
int g_mock_accept_fail = 0;
int g_mock_recv_fail = 0;
int g_mock_mutex_fail = 0;
int g_mock_cond_fail = 0;
int g_mock_strcasecmp_fail = 0;
int g_mock_headers_init_fail = 0;
int g_mock_parts_init_fail = 0;
int g_mock_multi_init_fail = 0;
int g_mock_mask_key_fail = 0;
int g_mock_pack_header_fail = 0;
int g_mock_raw_send_fail = 0;
int g_mock_raw_connect_fail = 0;
int g_mock_raw_gethostbyname_fail = 0;
int g_mock_raw_nonblocking_fail = 0;
int g_mock_raw_blocking_fail = 0;
int g_mock_raw_realloc_fail = 0;
int g_mock_raw_response_init_fail = 0;
int g_mock_aria2_system_fail = 0;
int g_mock_aria2_fopen_fail = 0;
int g_mock_aria2_response_init_fail = 0;
int g_mock_aria2_config_init_fail = 0;
int g_mock_xquic_config_init_fail = 0;
int g_mock_xquic_engine_present = 0;
int g_mock_picoquic_create_fail = 0;
int g_mock_picoquic_config_init_fail = 0;
int g_mock_picoquic_response_init_fail = 0;
int g_mock_picoquic_quic_null_on_free = 0;
int g_mock_wasm_fetch_fail = 0;
int g_mock_wasm_fetch_timeout = 0;
int g_mock_wasm_config_init_fail = 0;
int g_mock_wasm_response_init_fail = 0;
int g_mock_wasm_header_add_fail = 0;
int g_mock_fetch_global_init_fail = 0;
int g_mock_fetch_context_init_fail = 0;
int g_mock_fetch_config_init_fail = 0;
int g_mock_fetch_parse_fail = 0;
int g_mock_fetch_req_fail = 0;
int g_mock_fetch_err_code = 0;
int g_mock_fetch_empty_body = 0;
int g_mock_fetch_response_init_fail = 0;
int g_mock_fetch_response_alloc_fail = 0;
int g_mock_fetch_upload_alloc_fail = 0;
int g_mock_fetch_upload_realloc_fail = 0;
int g_mock_fetch_body_realloc_fail = 0;
int g_mock_fetch_loop_wakeup_fail = 0;
int g_mock_nghttp3_global_init_fail = 0;
int g_mock_nghttp3_context_init_fail = 0;
int g_mock_nghttp3_config_init_fail = 0;
int g_mock_nghttp3_conn_new_fail = 0;
int g_mock_nghttp3_response_alloc_fail = 0;
int g_mock_nghttp3_response_init_fail = 0;
int g_mock_nghttp3_send_fail = 0;
int g_mock_nghttp3_loop_wakeup_fail = 0;
int g_mock_libevent_global_init_fail = 0;
int g_mock_libevent_context_init_fail = 0;
int g_mock_libevent_config_init_fail = 0;
int g_mock_libevent_base_new_fail = 0;
int g_mock_libevent_conn_new_fail = 0;
int g_mock_libevent_req_new_fail = 0;
int g_mock_libevent_make_req_fail = 0;
int g_mock_libevent_res_alloc_fail = 0;
int g_mock_libevent_res_init_fail = 0;
int g_mock_libevent_body_alloc_fail = 0;
int g_mock_libevent_buf_add_fail = 0;
int g_mock_libevent_empty_body = 0;
int g_mock_libuv_global_init_fail = 0;
int g_mock_libuv_context_init_fail = 0;
int g_mock_libuv_config_init_fail = 0;
int g_mock_libuv_req_buf_alloc_fail = 0;
int g_mock_libuv_res_alloc_fail = 0;
int g_mock_libuv_res_init_fail = 0;
int g_mock_libuv_body_alloc_fail = 0;
int g_mock_libuv_body_realloc_fail = 0;
int g_mock_libuv_headers_realloc_fail = 0;
int g_mock_libuv_addrinfo_fail = 0;
int g_mock_libuv_connect_fail = 0;
int g_mock_libuv_write_fail = 0;
int g_mock_libuv_read_fail = 0;
int g_mock_libuv_alloc_fail = 0;
int g_mock_lsquic_global_init_fail = 0;
int g_mock_lsquic_context_init_fail = 0;
int g_mock_lsquic_config_init_fail = 0;
int g_mock_lsquic_engine_new_fail = 0;
int g_mock_lsquic_res_alloc_fail = 0;
int g_mock_lsquic_res_init_fail = 0;
int g_mock_lsquic_body_alloc_fail = 0;
int g_mock_lsquic_read_fail = 0;
int g_mock_libsoup3_global_init_fail = 0;
int g_mock_libsoup3_context_init_fail = 0;
int g_mock_libsoup3_config_init_fail = 0;
int g_mock_libsoup3_session_new_fail = 0;
int g_mock_libsoup3_msg_new_fail = 0;
int g_mock_libsoup3_send_fail = 0;
int g_mock_libsoup3_res_alloc_fail = 0;
int g_mock_libsoup3_res_init_fail = 0;
int g_mock_libsoup3_body_alloc_fail = 0;
int g_mock_libsoup3_uri_parse_fail = 0;
int g_mock_android_getenv_detached = 0;
int g_mock_android_getenv_fail = 0;
int g_mock_android_attach_fail = 0;
int g_mock_android_new_string_fail = 0;
int g_mock_android_find_class_fail = 0;
int g_mock_android_get_method_fail = 0;
int g_mock_android_new_object_fail = 0;
int g_mock_android_call_object_fail = 0;
int g_mock_android_res_alloc_fail = 0;
int g_mock_android_res_init_fail = 0;
int g_mock_android_input_stream_fail = 0;
int g_mock_android_error_stream_fail = 0;
int g_mock_android_read_exception = 0;
int g_mock_android_body_alloc_fail = 0;
int g_mock_android_body_realloc_fail = 0;
int g_mock_android_final_body_realloc_fail = 0;
int g_mock_android_cleanup_exception = 0;
int g_mock_android_read_chunks = 0;
int g_mock_android_status_code = 0;
int g_mock_android_config_init_fail = 0;
int g_mock_msh3_api_open_fail = 0;
int g_mock_msh3_config_open_fail = 0;
int g_mock_msh3_conn_open_fail = 0;
int g_mock_msh3_request_open_fail = 0;
int g_mock_msh3_send_fail = 0;
int g_mock_msh3_shutdown_error = 0;
int g_mock_msh3_header_alloc_fail = 0;
int g_mock_msh3_body_realloc_fail = 0;
int g_mock_msh3_getaddrinfo_fail = 0;
int g_mock_msh3_getaddrinfo_null_result = 0;
int g_mock_msh3_mutex_init_fail = 0;
int g_mock_msh3_config_init_fail = 0;
int g_mock_msh3_global_lock_fail = 0;
int g_mock_msh3_res_alloc_fail = 0;
int g_mock_msh3_res_init_fail = 0;
int g_mock_msh3_cond_init_fail = 0;
int g_mock_msh3_send_lock_fail = 0;
int g_mock_msh3_cond_wait_fail = 0;
int g_mock_msh3_header_add_fail = 0;
int g_mock_msh3_cb_mutex_lock_fail = 0;
int g_mock_msh3_parse_url_alloc_fail = 0;
int g_mock_msh3_extra_events = 0;
int g_mock_winhttp_open_fail = 0;
int g_mock_winhttp_connect_fail = 0;
int g_mock_winhttp_open_request_fail = 0;
int g_mock_winhttp_send_request_fail = 0;
int g_mock_winhttp_write_data_fail = 0;
int g_mock_winhttp_receive_response_fail = 0;
int g_mock_winhttp_query_headers_fail = 0;
int g_mock_winhttp_query_data_available_fail = 0;
int g_mock_winhttp_read_data_fail = 0;
int g_mock_winhttp_set_timeouts_fail = 0;
int g_mock_winhttp_set_option_fail = 0;
int g_mock_winhttp_crack_url_fail = 0;
int g_mock_winhttp_add_request_headers_fail = 0;
int g_mock_winhttp_queue_work_item_fail = 0;
int g_mock_winhttp_context_init_fail = 0;
int g_mock_winhttp_response_init_fail = 0;
int g_mock_winhttp_status_code = 0;
int g_mock_winhttp_cookie_count = 0;
int g_mock_winhttp_read_chunks = 0;
int g_mock_winhttp_total_body_realloc_fail = 0;
int g_mock_winhttp_read_buf_alloc_fail = 0;
int g_mock_winhttp_res_alloc_fail = 0;
int g_mock_wininet_open_fail = 0;
int g_mock_wininet_connect_fail = 0;
int g_mock_wininet_open_request_fail = 0;
int g_mock_wininet_send_request_fail = 0;
int g_mock_wininet_send_request_ex_fail = 0;
int g_mock_wininet_write_file_fail = 0;
int g_mock_wininet_end_request_fail = 0;
int g_mock_wininet_query_info_fail = 0;
int g_mock_wininet_read_file_fail = 0;
int g_mock_wininet_set_option_fail = 0;
int g_mock_wininet_crack_url_fail = 0;
int g_mock_wininet_add_headers_fail = 0;
int g_mock_wininet_context_init_fail = 0;
int g_mock_wininet_response_init_fail = 0;
int g_mock_wininet_status_code = 0;
int g_mock_wininet_cookie_count = 0;
int g_mock_wininet_read_chunks = 0;
int g_mock_wininet_read_chunk_alloc_fail = 0;
int g_mock_wininet_body_realloc_fail = 0;
int g_mock_wininet_res_alloc_fail = 0;
int g_mock_accept_fd = -1;
int g_mock_server_reading = 0;
const char *g_mock_recv_data = NULL;

int *abstract_http_mock_get_g_mock_sha1_fail(void) { return &g_mock_sha1_fail; }

int *abstract_http_mock_get_g_mock_alloc_fail(void) {
  return &g_mock_alloc_fail;
}
int *abstract_http_mock_get_g_mock_alloc_count(void) {
  return &g_mock_alloc_count;
}
int *abstract_http_mock_get_g_mock_pthread_fail(void) {
  return &g_mock_pthread_fail;
}
int *abstract_http_mock_get_g_mock_pthread_create_sync(void) {
  return &g_mock_pthread_create_sync;
}
int *abstract_http_mock_get_g_mock_pipe_fail(void) { return &g_mock_pipe_fail; }
int *abstract_http_mock_get_g_mock_fork_fail(void) { return &g_mock_fork_fail; }
int *abstract_http_mock_get_g_mock_waitpid_fail(void) {
  return &g_mock_waitpid_fail;
}
int *abstract_http_mock_get_g_mock_select_fail(void) {
  return &g_mock_select_fail;
}
int *abstract_http_mock_get_g_mock_select_error_fds(void) {
  return &g_mock_select_error_fds;
}
int *abstract_http_mock_get_g_mock_time_jump(void) { return &g_mock_time_jump; }
int *abstract_http_mock_get_g_mock_time_jump_count(void) {
  return &g_mock_time_jump_count;
}
int *abstract_http_mock_get_g_mock_fwrite_fail(void) {
  return &g_mock_fwrite_fail;
}
int *abstract_http_mock_get_g_mock_fclose_fail(void) {
  return &g_mock_fclose_fail;
}
int *abstract_http_mock_get_g_mock_socket_fail(void) {
  return &g_mock_socket_fail;
}
int *abstract_http_mock_get_g_mock_bind_fail(void) { return &g_mock_bind_fail; }
int *abstract_http_mock_get_g_mock_listen_fail(void) {
  return &g_mock_listen_fail;
}
int *abstract_http_mock_get_g_mock_accept_fail(void) {
  return &g_mock_accept_fail;
}
int *abstract_http_mock_get_g_mock_recv_fail(void) { return &g_mock_recv_fail; }
int *abstract_http_mock_get_g_mock_mutex_fail(void) { return &g_mock_mutex_fail; }
int *abstract_http_mock_get_g_mock_cond_fail(void) { return &g_mock_cond_fail; }
int *abstract_http_mock_get_g_mock_strcasecmp_fail(void) {
  return &g_mock_strcasecmp_fail;
}
int *abstract_http_mock_get_g_mock_headers_init_fail(void) {
  return &g_mock_headers_init_fail;
}
int *abstract_http_mock_get_g_mock_parts_init_fail(void) {
  return &g_mock_parts_init_fail;
}
int *abstract_http_mock_get_g_mock_multi_init_fail(void) {
  return &g_mock_multi_init_fail;
}
int *abstract_http_mock_get_g_mock_mask_key_fail(void) {
  return &g_mock_mask_key_fail;
}
int *abstract_http_mock_get_g_mock_pack_header_fail(void) {
  return &g_mock_pack_header_fail;
}
int *abstract_http_mock_get_g_mock_raw_send_fail(void) {
  return &g_mock_raw_send_fail;
}
int *abstract_http_mock_get_g_mock_raw_connect_fail(void) {
  return &g_mock_raw_connect_fail;
}
int *abstract_http_mock_get_g_mock_raw_gethostbyname_fail(void) {
  return &g_mock_raw_gethostbyname_fail;
}
int *abstract_http_mock_get_g_mock_raw_nonblocking_fail(void) {
  return &g_mock_raw_nonblocking_fail;
}
int *abstract_http_mock_get_g_mock_raw_blocking_fail(void) {
  return &g_mock_raw_blocking_fail;
}
int *abstract_http_mock_get_g_mock_raw_realloc_fail(void) {
  return &g_mock_raw_realloc_fail;
}
int *abstract_http_mock_get_g_mock_raw_response_init_fail(void) {
  return &g_mock_raw_response_init_fail;
}
int *abstract_http_mock_get_g_mock_aria2_system_fail(void) {
  return &g_mock_aria2_system_fail;
}
int *abstract_http_mock_get_g_mock_aria2_fopen_fail(void) {
  return &g_mock_aria2_fopen_fail;
}
int *abstract_http_mock_get_g_mock_aria2_response_init_fail(void) {
  return &g_mock_aria2_response_init_fail;
}
int *abstract_http_mock_get_g_mock_aria2_config_init_fail(void) {
  return &g_mock_aria2_config_init_fail;
}
int *abstract_http_mock_get_g_mock_xquic_config_init_fail(void) {
  return &g_mock_xquic_config_init_fail;
}
int *abstract_http_mock_get_g_mock_xquic_engine_present(void) {
  return &g_mock_xquic_engine_present;
}
int *abstract_http_mock_get_g_mock_picoquic_create_fail(void) {
  return &g_mock_picoquic_create_fail;
}
int *abstract_http_mock_get_g_mock_picoquic_config_init_fail(void) {
  return &g_mock_picoquic_config_init_fail;
}
int *abstract_http_mock_get_g_mock_picoquic_response_init_fail(void) {
  return &g_mock_picoquic_response_init_fail;
}
int *abstract_http_mock_get_g_mock_picoquic_quic_null_on_free(void) {
  return &g_mock_picoquic_quic_null_on_free;
}
int *abstract_http_mock_get_g_mock_wasm_fetch_fail(void) {
  return &g_mock_wasm_fetch_fail;
}
int *abstract_http_mock_get_g_mock_wasm_fetch_timeout(void) {
  return &g_mock_wasm_fetch_timeout;
}
int *abstract_http_mock_get_g_mock_wasm_config_init_fail(void) {
  return &g_mock_wasm_config_init_fail;
}
int *abstract_http_mock_get_g_mock_wasm_response_init_fail(void) {
  return &g_mock_wasm_response_init_fail;
}
int *abstract_http_mock_get_g_mock_wasm_header_add_fail(void) {
  return &g_mock_wasm_header_add_fail;
}
int *abstract_http_mock_get_g_mock_fetch_global_init_fail(void) {
  return &g_mock_fetch_global_init_fail;
}
int *abstract_http_mock_get_g_mock_fetch_context_init_fail(void) {
  return &g_mock_fetch_context_init_fail;
}
int *abstract_http_mock_get_g_mock_fetch_config_init_fail(void) {
  return &g_mock_fetch_config_init_fail;
}
int *abstract_http_mock_get_g_mock_fetch_parse_fail(void) {
  return &g_mock_fetch_parse_fail;
}
int *abstract_http_mock_get_g_mock_fetch_req_fail(void) {
  return &g_mock_fetch_req_fail;
}
int *abstract_http_mock_get_g_mock_fetch_err_code(void) {
  return &g_mock_fetch_err_code;
}
int *abstract_http_mock_get_g_mock_fetch_empty_body(void) {
  return &g_mock_fetch_empty_body;
}
int *abstract_http_mock_get_g_mock_fetch_response_init_fail(void) {
  return &g_mock_fetch_response_init_fail;
}
int *abstract_http_mock_get_g_mock_fetch_response_alloc_fail(void) {
  return &g_mock_fetch_response_alloc_fail;
}
int *abstract_http_mock_get_g_mock_fetch_upload_alloc_fail(void) {
  return &g_mock_fetch_upload_alloc_fail;
}
int *abstract_http_mock_get_g_mock_fetch_upload_realloc_fail(void) {
  return &g_mock_fetch_upload_realloc_fail;
}
int *abstract_http_mock_get_g_mock_fetch_body_realloc_fail(void) {
  return &g_mock_fetch_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_fetch_loop_wakeup_fail(void) {
  return &g_mock_fetch_loop_wakeup_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_global_init_fail(void) {
  return &g_mock_nghttp3_global_init_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_context_init_fail(void) {
  return &g_mock_nghttp3_context_init_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_config_init_fail(void) {
  return &g_mock_nghttp3_config_init_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_conn_new_fail(void) {
  return &g_mock_nghttp3_conn_new_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_response_alloc_fail(void) {
  return &g_mock_nghttp3_response_alloc_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_response_init_fail(void) {
  return &g_mock_nghttp3_response_init_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_send_fail(void) {
  return &g_mock_nghttp3_send_fail;
}
int *abstract_http_mock_get_g_mock_nghttp3_loop_wakeup_fail(void) {
  return &g_mock_nghttp3_loop_wakeup_fail;
}
int *abstract_http_mock_get_g_mock_libevent_global_init_fail(void) {
  return &g_mock_libevent_global_init_fail;
}
int *abstract_http_mock_get_g_mock_libevent_context_init_fail(void) {
  return &g_mock_libevent_context_init_fail;
}
int *abstract_http_mock_get_g_mock_libevent_config_init_fail(void) {
  return &g_mock_libevent_config_init_fail;
}
int *abstract_http_mock_get_g_mock_libevent_base_new_fail(void) {
  return &g_mock_libevent_base_new_fail;
}
int *abstract_http_mock_get_g_mock_libevent_conn_new_fail(void) {
  return &g_mock_libevent_conn_new_fail;
}
int *abstract_http_mock_get_g_mock_libevent_req_new_fail(void) {
  return &g_mock_libevent_req_new_fail;
}
int *abstract_http_mock_get_g_mock_libevent_make_req_fail(void) {
  return &g_mock_libevent_make_req_fail;
}
int *abstract_http_mock_get_g_mock_libevent_res_alloc_fail(void) {
  return &g_mock_libevent_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libevent_res_init_fail(void) {
  return &g_mock_libevent_res_init_fail;
}
int *abstract_http_mock_get_g_mock_libevent_body_alloc_fail(void) {
  return &g_mock_libevent_body_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libevent_buf_add_fail(void) {
  return &g_mock_libevent_buf_add_fail;
}
int *abstract_http_mock_get_g_mock_libevent_empty_body(void) {
  return &g_mock_libevent_empty_body;
}
int *abstract_http_mock_get_g_mock_libuv_global_init_fail(void) {
  return &g_mock_libuv_global_init_fail;
}
int *abstract_http_mock_get_g_mock_libuv_context_init_fail(void) {
  return &g_mock_libuv_context_init_fail;
}
int *abstract_http_mock_get_g_mock_libuv_config_init_fail(void) {
  return &g_mock_libuv_config_init_fail;
}
int *abstract_http_mock_get_g_mock_libuv_req_buf_alloc_fail(void) {
  return &g_mock_libuv_req_buf_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libuv_res_alloc_fail(void) {
  return &g_mock_libuv_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libuv_res_init_fail(void) {
  return &g_mock_libuv_res_init_fail;
}
int *abstract_http_mock_get_g_mock_libuv_body_alloc_fail(void) {
  return &g_mock_libuv_body_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libuv_body_realloc_fail(void) {
  return &g_mock_libuv_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_libuv_headers_realloc_fail(void) {
  return &g_mock_libuv_headers_realloc_fail;
}
int *abstract_http_mock_get_g_mock_libuv_addrinfo_fail(void) {
  return &g_mock_libuv_addrinfo_fail;
}
int *abstract_http_mock_get_g_mock_libuv_connect_fail(void) {
  return &g_mock_libuv_connect_fail;
}
int *abstract_http_mock_get_g_mock_libuv_write_fail(void) {
  return &g_mock_libuv_write_fail;
}
int *abstract_http_mock_get_g_mock_libuv_read_fail(void) {
  return &g_mock_libuv_read_fail;
}
int *abstract_http_mock_get_g_mock_libuv_alloc_fail(void) {
  return &g_mock_libuv_alloc_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_global_init_fail(void) {
  return &g_mock_lsquic_global_init_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_context_init_fail(void) {
  return &g_mock_lsquic_context_init_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_config_init_fail(void) {
  return &g_mock_lsquic_config_init_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_engine_new_fail(void) {
  return &g_mock_lsquic_engine_new_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_res_alloc_fail(void) {
  return &g_mock_lsquic_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_res_init_fail(void) {
  return &g_mock_lsquic_res_init_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_body_alloc_fail(void) {
  return &g_mock_lsquic_body_alloc_fail;
}
int *abstract_http_mock_get_g_mock_lsquic_read_fail(void) {
  return &g_mock_lsquic_read_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_global_init_fail(void) {
  return &g_mock_libsoup3_global_init_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_context_init_fail(void) {
  return &g_mock_libsoup3_context_init_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_config_init_fail(void) {
  return &g_mock_libsoup3_config_init_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_session_new_fail(void) {
  return &g_mock_libsoup3_session_new_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_msg_new_fail(void) {
  return &g_mock_libsoup3_msg_new_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_send_fail(void) {
  return &g_mock_libsoup3_send_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_res_alloc_fail(void) {
  return &g_mock_libsoup3_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_res_init_fail(void) {
  return &g_mock_libsoup3_res_init_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_body_alloc_fail(void) {
  return &g_mock_libsoup3_body_alloc_fail;
}
int *abstract_http_mock_get_g_mock_libsoup3_uri_parse_fail(void) {
  return &g_mock_libsoup3_uri_parse_fail;
}
int *abstract_http_mock_get_g_mock_android_getenv_detached(void) {
  return &g_mock_android_getenv_detached;
}
int *abstract_http_mock_get_g_mock_android_getenv_fail(void) {
  return &g_mock_android_getenv_fail;
}
int *abstract_http_mock_get_g_mock_android_attach_fail(void) {
  return &g_mock_android_attach_fail;
}
int *abstract_http_mock_get_g_mock_android_new_string_fail(void) {
  return &g_mock_android_new_string_fail;
}
int *abstract_http_mock_get_g_mock_android_find_class_fail(void) {
  return &g_mock_android_find_class_fail;
}
int *abstract_http_mock_get_g_mock_android_get_method_fail(void) {
  return &g_mock_android_get_method_fail;
}
int *abstract_http_mock_get_g_mock_android_new_object_fail(void) {
  return &g_mock_android_new_object_fail;
}
int *abstract_http_mock_get_g_mock_android_call_object_fail(void) {
  return &g_mock_android_call_object_fail;
}
int *abstract_http_mock_get_g_mock_android_res_alloc_fail(void) {
  return &g_mock_android_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_android_res_init_fail(void) {
  return &g_mock_android_res_init_fail;
}
int *abstract_http_mock_get_g_mock_android_input_stream_fail(void) {
  return &g_mock_android_input_stream_fail;
}
int *abstract_http_mock_get_g_mock_android_error_stream_fail(void) {
  return &g_mock_android_error_stream_fail;
}
int *abstract_http_mock_get_g_mock_android_read_exception(void) {
  return &g_mock_android_read_exception;
}
int *abstract_http_mock_get_g_mock_android_body_alloc_fail(void) {
  return &g_mock_android_body_alloc_fail;
}
int *abstract_http_mock_get_g_mock_android_body_realloc_fail(void) {
  return &g_mock_android_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_android_final_body_realloc_fail(void) {
  return &g_mock_android_final_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_android_cleanup_exception(void) {
  return &g_mock_android_cleanup_exception;
}
int *abstract_http_mock_get_g_mock_android_read_chunks(void) {
  return &g_mock_android_read_chunks;
}
int *abstract_http_mock_get_g_mock_android_status_code(void) {
  return &g_mock_android_status_code;
}
int *abstract_http_mock_get_g_mock_android_config_init_fail(void) {
  return &g_mock_android_config_init_fail;
}
int *abstract_http_mock_get_g_mock_msh3_api_open_fail(void) {
  return &g_mock_msh3_api_open_fail;
}
int *abstract_http_mock_get_g_mock_msh3_config_open_fail(void) {
  return &g_mock_msh3_config_open_fail;
}
int *abstract_http_mock_get_g_mock_msh3_conn_open_fail(void) {
  return &g_mock_msh3_conn_open_fail;
}
int *abstract_http_mock_get_g_mock_msh3_request_open_fail(void) {
  return &g_mock_msh3_request_open_fail;
}
int *abstract_http_mock_get_g_mock_msh3_send_fail(void) {
  return &g_mock_msh3_send_fail;
}
int *abstract_http_mock_get_g_mock_msh3_shutdown_error(void) {
  return &g_mock_msh3_shutdown_error;
}
int *abstract_http_mock_get_g_mock_msh3_header_alloc_fail(void) {
  return &g_mock_msh3_header_alloc_fail;
}
int *abstract_http_mock_get_g_mock_msh3_body_realloc_fail(void) {
  return &g_mock_msh3_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_msh3_getaddrinfo_fail(void) {
  return &g_mock_msh3_getaddrinfo_fail;
}
int *abstract_http_mock_get_g_mock_msh3_getaddrinfo_null_result(void) {
  return &g_mock_msh3_getaddrinfo_null_result;
}
int *abstract_http_mock_get_g_mock_msh3_mutex_init_fail(void) {
  return &g_mock_msh3_mutex_init_fail;
}
int *abstract_http_mock_get_g_mock_msh3_config_init_fail(void) {
  return &g_mock_msh3_config_init_fail;
}
int *abstract_http_mock_get_g_mock_msh3_global_lock_fail(void) {
  return &g_mock_msh3_global_lock_fail;
}
int *abstract_http_mock_get_g_mock_msh3_res_alloc_fail(void) {
  return &g_mock_msh3_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_msh3_res_init_fail(void) {
  return &g_mock_msh3_res_init_fail;
}
int *abstract_http_mock_get_g_mock_msh3_cond_init_fail(void) {
  return &g_mock_msh3_cond_init_fail;
}
int *abstract_http_mock_get_g_mock_msh3_send_lock_fail(void) {
  return &g_mock_msh3_send_lock_fail;
}
int *abstract_http_mock_get_g_mock_msh3_cond_wait_fail(void) {
  return &g_mock_msh3_cond_wait_fail;
}
int *abstract_http_mock_get_g_mock_msh3_header_add_fail(void) {
  return &g_mock_msh3_header_add_fail;
}
int *abstract_http_mock_get_g_mock_msh3_cb_mutex_lock_fail(void) {
  return &g_mock_msh3_cb_mutex_lock_fail;
}
int *abstract_http_mock_get_g_mock_msh3_parse_url_alloc_fail(void) {
  return &g_mock_msh3_parse_url_alloc_fail;
}
int *abstract_http_mock_get_g_mock_msh3_extra_events(void) {
  return &g_mock_msh3_extra_events;
}
int *abstract_http_mock_get_g_mock_winhttp_open_fail(void) {
  return &g_mock_winhttp_open_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_connect_fail(void) {
  return &g_mock_winhttp_connect_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_open_request_fail(void) {
  return &g_mock_winhttp_open_request_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_send_request_fail(void) {
  return &g_mock_winhttp_send_request_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_write_data_fail(void) {
  return &g_mock_winhttp_write_data_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_receive_response_fail(void) {
  return &g_mock_winhttp_receive_response_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_query_headers_fail(void) {
  return &g_mock_winhttp_query_headers_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_query_data_available_fail(void) {
  return &g_mock_winhttp_query_data_available_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_read_data_fail(void) {
  return &g_mock_winhttp_read_data_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_set_timeouts_fail(void) {
  return &g_mock_winhttp_set_timeouts_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_set_option_fail(void) {
  return &g_mock_winhttp_set_option_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_crack_url_fail(void) {
  return &g_mock_winhttp_crack_url_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_add_request_headers_fail(void) {
  return &g_mock_winhttp_add_request_headers_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_queue_work_item_fail(void) {
  return &g_mock_winhttp_queue_work_item_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_context_init_fail(void) {
  return &g_mock_winhttp_context_init_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_response_init_fail(void) {
  return &g_mock_winhttp_response_init_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_status_code(void) {
  return &g_mock_winhttp_status_code;
}
int *abstract_http_mock_get_g_mock_winhttp_cookie_count(void) {
  return &g_mock_winhttp_cookie_count;
}
int *abstract_http_mock_get_g_mock_winhttp_read_chunks(void) {
  return &g_mock_winhttp_read_chunks;
}
int *abstract_http_mock_get_g_mock_winhttp_total_body_realloc_fail(void) {
  return &g_mock_winhttp_total_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_read_buf_alloc_fail(void) {
  return &g_mock_winhttp_read_buf_alloc_fail;
}
int *abstract_http_mock_get_g_mock_winhttp_res_alloc_fail(void) {
  return &g_mock_winhttp_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_wininet_open_fail(void) {
  return &g_mock_wininet_open_fail;
}
int *abstract_http_mock_get_g_mock_wininet_connect_fail(void) {
  return &g_mock_wininet_connect_fail;
}
int *abstract_http_mock_get_g_mock_wininet_open_request_fail(void) {
  return &g_mock_wininet_open_request_fail;
}
int *abstract_http_mock_get_g_mock_wininet_send_request_fail(void) {
  return &g_mock_wininet_send_request_fail;
}
int *abstract_http_mock_get_g_mock_wininet_send_request_ex_fail(void) {
  return &g_mock_wininet_send_request_ex_fail;
}
int *abstract_http_mock_get_g_mock_wininet_write_file_fail(void) {
  return &g_mock_wininet_write_file_fail;
}
int *abstract_http_mock_get_g_mock_wininet_end_request_fail(void) {
  return &g_mock_wininet_end_request_fail;
}
int *abstract_http_mock_get_g_mock_wininet_query_info_fail(void) {
  return &g_mock_wininet_query_info_fail;
}
int *abstract_http_mock_get_g_mock_wininet_read_file_fail(void) {
  return &g_mock_wininet_read_file_fail;
}
int *abstract_http_mock_get_g_mock_wininet_set_option_fail(void) {
  return &g_mock_wininet_set_option_fail;
}
int *abstract_http_mock_get_g_mock_wininet_crack_url_fail(void) {
  return &g_mock_wininet_crack_url_fail;
}
int *abstract_http_mock_get_g_mock_wininet_add_headers_fail(void) {
  return &g_mock_wininet_add_headers_fail;
}
int *abstract_http_mock_get_g_mock_wininet_context_init_fail(void) {
  return &g_mock_wininet_context_init_fail;
}
int *abstract_http_mock_get_g_mock_wininet_response_init_fail(void) {
  return &g_mock_wininet_response_init_fail;
}
int *abstract_http_mock_get_g_mock_wininet_status_code(void) {
  return &g_mock_wininet_status_code;
}
int *abstract_http_mock_get_g_mock_wininet_cookie_count(void) {
  return &g_mock_wininet_cookie_count;
}
int *abstract_http_mock_get_g_mock_wininet_read_chunks(void) {
  return &g_mock_wininet_read_chunks;
}
int *abstract_http_mock_get_g_mock_wininet_read_chunk_alloc_fail(void) {
  return &g_mock_wininet_read_chunk_alloc_fail;
}
int *abstract_http_mock_get_g_mock_wininet_body_realloc_fail(void) {
  return &g_mock_wininet_body_realloc_fail;
}
int *abstract_http_mock_get_g_mock_wininet_res_alloc_fail(void) {
  return &g_mock_wininet_res_alloc_fail;
}
int *abstract_http_mock_get_g_mock_accept_fd(void) { return &g_mock_accept_fd; }
int *abstract_http_mock_get_g_mock_server_reading(void) {
  return &g_mock_server_reading;
}
const char **abstract_http_mock_get_g_mock_recv_data(void) {
  return &g_mock_recv_data;
}

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef pthread_key_create
#undef pthread_setspecific
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
#undef pthread_join
#undef pipe
#undef fork
#undef waitpid
#undef select
#undef math_get_current_time_ms
#undef pthread_getspecific

extern enum c_abstract_http_error c_abstract_http_mock_strdup(const char *s,
                                                       char **out);

ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_malloc(size_t size) {
  if (g_mock_alloc_fail) {
    if (g_mock_alloc_count <= 0) {
      return NULL;
    }
    g_mock_alloc_count--;
  }
  return malloc(size);
}

ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_calloc(size_t count, size_t size) {
  if (g_mock_alloc_fail && g_mock_alloc_count-- == 0) {
    return NULL;
  }
  return calloc(count, size);
}

ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_realloc(void *ptr, size_t size) {
  if (g_mock_alloc_fail)
    if (g_mock_alloc_fail)
      if (g_mock_alloc_fail) {
        if (g_mock_alloc_count-- == 0) {
          return NULL;
        }
      }
  return realloc(ptr, size);
}

ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void c_abstract_http_mock_free(void *ptr) {
  free(ptr);
}

#if !defined(_WIN32)
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <errno.h>
#if !defined(_WIN32)
extern int pthread_key_create(pthread_key_t *, void (*)(void *));
extern int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
extern int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
extern int pthread_create(pthread_t *, const pthread_attr_t *,
                          void *(*)(void *), void *);
extern int pthread_join(pthread_t, void **);
extern int pipe(int[2]);
extern pid_t fork(void);
extern pid_t waitpid(pid_t, int *, int);
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern uint64_t real_math_get_current_time_ms(void);
extern int pthread_setspecific(pthread_key_t, const void *);
extern void *pthread_getspecific(pthread_key_t);
#endif

#ifdef _WIN32
int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *errorfds,
                                       const struct timeval *timeout);
#else
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout);
#endif
uint64_t c_abstract_http_mock_math_get_current_time_ms(void);

#if !defined(_WIN32)
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
int c_abstract_http_mock_pipe(int fildes[2]);
pid_t c_abstract_http_mock_fork(void);
pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options);
int c_abstract_http_mock_pthread_setspecific(pthread_key_t key,
                                             const void *value);
void *c_abstract_http_mock_pthread_getspecific(pthread_key_t key);

int c_abstract_http_mock_pthread_key_create(pthread_key_t *key,
                                            void (*destructor)(void *)) {
  if (g_mock_pthread_fail == 1)
    return 1; /* Return non-zero on failure */
  return pthread_key_create(key, destructor);
}

int c_abstract_http_mock_pthread_setspecific(pthread_key_t key,
                                             const void *value) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_setspecific(key, value);
}

void *c_abstract_http_mock_pthread_getspecific(pthread_key_t key) {
  if (g_mock_pthread_fail == 1)
    return NULL;
  return pthread_getspecific(key);
}

int c_abstract_http_mock_pthread_mutex_init(pthread_mutex_t *mutex,
                                            const pthread_mutexattr_t *attr) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_mutex_init(mutex, attr);
}

int c_abstract_http_mock_pthread_cond_init(pthread_cond_t *cond,
                                           const pthread_condattr_t *attr) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_cond_init(cond, attr);
}

int c_abstract_http_mock_pthread_create(pthread_t *thread,
                                        const pthread_attr_t *attr,
                                        void *(*start_routine)(void *),
                                        void *arg) {
  if (g_mock_pthread_fail == 1)
    return 1;
  if (g_mock_pthread_fail == 2 && g_mock_alloc_count-- == 0)
    return 1;
  return pthread_create(thread, attr, start_routine, arg);
}

int c_abstract_http_mock_pthread_join(pthread_t thread, void **value_ptr) {
  if (g_mock_pthread_fail == 3)
    return 1;
  return pthread_join(thread, value_ptr);
}

int c_abstract_http_mock_pipe(int fildes[2]) {
  if (g_mock_pipe_fail) {
    errno = EMFILE;
    return -1;
  }
  return pipe(fildes);
}

#if defined(__EMSCRIPTEN__)
void __gcov_fork(void);
void __gcov_fork(void) {}
#endif

pid_t c_abstract_http_mock_fork(void) {
  if (g_mock_fork_fail) {
    errno = EAGAIN;
    return -1;
  }
  if (g_mock_waitpid_fail == 3) {
    return 0;
  }
#if !defined(__EMSCRIPTEN__)
  return fork();
#else
  errno = ENOSYS;
  return -1;
#endif
}

pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options) {
  if (g_mock_waitpid_fail == 1) {
    errno = ECHILD;
    return -1;
  }
  if (g_mock_waitpid_fail == 2) {
    /* WIFEXITED == false */
    if (stat_loc)
      *stat_loc = 0x007F; /* simulate stopped by signal */
    return pid;
  }
  return waitpid(pid, stat_loc, options);
}
#endif

#ifdef _WIN32
int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *errorfds,
                                       const struct timeval *timeout) {
  static int s_mock_select_win_count = 0;
  if (g_mock_select_fail == 1) {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }
  if (g_mock_select_fail == 2) {
    if (++s_mock_select_win_count >= 2) {
      s_mock_select_win_count = 0;
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }
  }
#else
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout) {
  static int s_mock_select_posix_count = 0;
  if (g_mock_select_fail == 1) {
    errno = C_ABSTRACT_HTTP_ERR_INVAL;
    return -1;
  }
  if (g_mock_select_fail == 2) {
    if (++s_mock_select_posix_count >= 2) {
      s_mock_select_posix_count = 0;
      errno = C_ABSTRACT_HTTP_ERR_INVAL;
      return -1;
    }
  }
#endif
  if (g_mock_select_error_fds == 1) {
    if (readfds)
      FD_ZERO(readfds);
    if (writefds)
      FD_ZERO(writefds);
    if (errorfds) {
      /* Force error flag on all fds */
      unsigned int i;
      for (i = 0; i < (unsigned int)nfds; ++i) {
        FD_SET(i, errorfds);
      }
      return 1;
    } else {
      return 0;
    }
  }
  return select(nfds, readfds, writefds, errorfds, timeout);
}

#undef math_get_current_time_ms
extern uint64_t real_math_get_current_time_ms(void);

uint64_t c_abstract_http_mock_math_get_current_time_ms(void) {
  uint64_t now = real_math_get_current_time_ms();
  if (g_mock_time_jump) {
    if (g_mock_time_jump_count-- <= 0) {
      now += 1000;
    }
  }
  return now;
}

void dummy_cb_thread(void *arg) { (void)arg; }
void *dummy_cb_pthread(void *arg) {
  (void)arg;
  return NULL;
}

enum c_abstract_http_error c_abstract_http_mock_strdup(const char *s,
                                                       char **out) {
  if (g_mock_alloc_fail && g_mock_alloc_count-- == 0) {
    if (out)
      *out = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM; /* C_ABSTRACT_HTTP_ERR_NOMEM */
  }
  if (!s) {
    if (out)
      *out = NULL;
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  {
    size_t len = strlen(s);
    char *d = (char *)c_abstract_http_mock_malloc(len + 1);
    if (!d) {
      if (out)
        *out = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(d, s, len + 1);
    if (out) {
      *out = d;
    } else {
      free(d);
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

#include <stddef.h>
#include <stdio.h>


#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

extern int g_mock_fwrite_fail;
extern int g_mock_fclose_fail;
extern int g_mock_socket_fail;
extern int g_mock_bind_fail;
extern int g_mock_listen_fail;
extern int g_mock_accept_fail;
extern int g_mock_recv_fail;

#undef fwrite
#undef fclose
#undef socket
#undef bind
#undef listen
#undef accept
#undef recv


#ifdef _WIN32
/* extern SOCKET socket(int, int, int); */
/* extern int bind(SOCKET, const struct sockaddr *, int); */
/* extern int listen(SOCKET, int); */
/* extern SOCKET accept(SOCKET, struct sockaddr *, int *); */
/* extern int recv(SOCKET, char *, int, int); */
#else
extern int socket(int, int, int);
extern int bind(int, const struct sockaddr *, socklen_t);
extern int listen(int, int);
extern int accept(int, struct sockaddr *, socklen_t *);
extern ssize_t recv(int, void *, size_t, int);
#endif

size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nitems,
                                   FILE *stream) {
  if (g_mock_fwrite_fail)
    return 0;
  return fwrite(ptr, size, nitems, stream);
}

int c_abstract_http_mock_fclose(FILE *stream) {

  if (g_mock_fclose_fail) {
    fclose(stream);
    return EOF;
  }
  return fclose(stream);
}

#ifdef _WIN32
#include <basetsd.h>
/* clang-format on */
SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type, int protocol) {
  if (g_mock_socket_fail)
    return INVALID_SOCKET;
  return socket(domain, type, protocol);
}

int WSAAPI c_abstract_http_mock_bind(SOCKET socket,
                                     const struct sockaddr *address,
                                     socklen_t address_len) {
  if (g_mock_bind_fail)
    return SOCKET_ERROR;
  return bind(socket, address, address_len);
}

int WSAAPI c_abstract_http_mock_listen(SOCKET socket, int backlog) {
  if (g_mock_listen_fail)
    return SOCKET_ERROR;
  return listen(socket, backlog);
}

SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket,
                                          struct sockaddr *address,
                                          int *address_len) {
  if (g_mock_accept_fail)
    return INVALID_SOCKET;
  if (g_mock_accept_fd >= 0)
    return (SOCKET)g_mock_accept_fd;
  return accept(socket, address, address_len);
}

int WSAAPI c_abstract_http_mock_recv(SOCKET socket, char *buffer, int length,
                                     int flags) {
  if (g_mock_recv_fail)
    return SOCKET_ERROR;
  if (g_mock_recv_data && !g_mock_server_reading) {
    int len;
    if (*g_mock_recv_data == '\0')
      return 0;
    len = (int)strlen(g_mock_recv_data);
    if (len > length)
      len = length;
    memcpy(buffer, g_mock_recv_data, (size_t)len);
    g_mock_recv_data += len;
    return len;
  }
  return recv(socket, buffer, length, flags);
}

int WSAAPI c_abstract_http_mock_getsockname(SOCKET socket,
                                            struct sockaddr *address,
                                            int *address_len) {
  if (g_mock_getsockname_fail)
    return SOCKET_ERROR;
  return getsockname(socket, address, address_len);
}
#else
int c_abstract_http_mock_socket(int domain, int type, int protocol) {
  if (g_mock_socket_fail)
    return -1;
  return socket(domain, type, protocol);
}

int c_abstract_http_mock_bind(int socket, const struct sockaddr *address,
                              socklen_t address_len) {
  if (g_mock_bind_fail)
    return -1;
  return bind(socket, address, address_len);
}

int c_abstract_http_mock_listen(int socket, int backlog) {
  if (g_mock_listen_fail)
    return -1;
  return listen(socket, backlog);
}

int c_abstract_http_mock_accept(int socket, struct sockaddr *address,
                                socklen_t *address_len) {
  if (g_mock_accept_fail)
    return -1;
  if (g_mock_accept_fd >= 0)
    return g_mock_accept_fd;
  return accept(socket, address, address_len);
}

ssize_t c_abstract_http_mock_recv(int socket, void *buffer, size_t length,
                                  int flags) {
  if (g_mock_recv_fail)
    return -1;
  if (g_mock_recv_data && !g_mock_server_reading) {
    size_t len;
    if (*g_mock_recv_data == '\0')
      return 0;
    len = strlen(g_mock_recv_data);
    if (len > length)
      len = length;
    memcpy(buffer, g_mock_recv_data, len);
    g_mock_recv_data += len;
    return (ssize_t)len;
  }
  return recv(socket, buffer, length, flags);
}

int c_abstract_http_mock_getsockname(int socket, struct sockaddr *address,
                                     socklen_t *address_len) {
  if (g_mock_getsockname_fail)
    return -1;
  return getsockname(socket, address, address_len);
}
#endif

#ifdef __APPLE__
int c_abstract_http_mock_select_darwin_extsn(
    int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
    struct timeval *timeout) __asm("_c_abstract_http_mock_select$DARWIN_EXTSN");
int c_abstract_http_mock_select_darwin_extsn(int nfds, fd_set *readfds,
                                             fd_set *writefds, fd_set *errorfds,
                                             struct timeval *timeout) {
  return c_abstract_http_mock_select(nfds, readfds, writefds, errorfds,
                                     timeout);
}
#endif
int g_mock_timer_heap_swap_fail = 0;

#ifdef __MINGW32__
void (*__imp_c_abstract_http_mock_select)(void) =
    (void (*)(void)) & c_abstract_http_mock_select;
void (*__imp_c_abstract_http_mock_socket)(void) =
    (void (*)(void)) & c_abstract_http_mock_socket;
void (*__imp_c_abstract_http_mock_bind)(void) =
    (void (*)(void)) & c_abstract_http_mock_bind;
void (*__imp_c_abstract_http_mock_listen)(void) =
    (void (*)(void)) & c_abstract_http_mock_listen;
void (*__imp_c_abstract_http_mock_accept)(void) =
    (void (*)(void)) & c_abstract_http_mock_accept;
void (*__imp_c_abstract_http_mock_getsockname)(void) =
    (void (*)(void)) & c_abstract_http_mock_getsockname;
void (*__imp_c_abstract_http_mock_recv)(void) =
    (void (*)(void)) & c_abstract_http_mock_recv;
#endif
struct curl_slist;
struct curl_slist *g_mock_curl_cookies = 0;
