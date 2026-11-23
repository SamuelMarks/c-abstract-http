/**
 * @file mock_alloc.c
 * @brief OOM and syscall fault-injection mock implementations.
 */

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef c_abstract_http_strdup
#undef pthread_key_create
#undef pthread_setspecific
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
#undef pthread_join
#undef pipe
#undef fork
#undef execv
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
#undef recv
#undef getsockname

/* clang-format off */
#include "mock_alloc.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <basetsd.h>
#include <winsock2.h>
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
/* clang-format on */

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef c_abstract_http_strdup
#undef pthread_key_create
#undef pthread_setspecific
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
#undef pthread_join
#undef pipe
#undef fork
#undef execv
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
#undef recv
#undef getsockname

#ifdef _WIN32
#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
typedef SSIZE_T ssize_t;
#endif
typedef int socklen_t;

static void mock_fd_set_win(SOCKET fd, fd_set *set) {
  u_int i;
  for (i = 0; i < set->fd_count; i++) {
    if (set->fd_array[i] == fd) {
      break;
    }
  }
  if (i == set->fd_count) {
    if (set->fd_count < FD_SETSIZE) {
      set->fd_array[i] = fd;
      set->fd_count++;
    }
  }
}
#endif

#define ABSTRACT_HTTP_DEF_MOCK_INT_VAR(name, init_val)                         \
  int g_mock_##name = (init_val);                                              \
  NO_DISCARD c_abstract_http_error_t abstract_http_mock_get_g_mock_##name(     \
      int **out_ptr) {                                                         \
    if (!out_ptr) {                                                            \
      return C_ABSTRACT_HTTP_ERR_INVAL;                                        \
    }                                                                          \
    *out_ptr = &g_mock_##name;                                                 \
    return C_ABSTRACT_HTTP_SUCCESS;                                            \
  }

ABSTRACT_HTTP_DEF_MOCK_INT_VAR(sha1_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(alloc_count, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(pthread_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(pthread_create_sync, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(pipe_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fork_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(waitpid_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(select_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(select_error_fds, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(time_jump, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(time_jump_count, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fwrite_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fclose_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(socket_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(bind_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(listen_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(accept_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(recv_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(config_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(response_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(multi_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(parts_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(headers_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(cond_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(mutex_free_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(getsockname_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(mutex_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(cond_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(strcasecmp_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(headers_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(parts_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(multi_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(sprintf_s_wrapper_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(urlencode_append_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(mask_key_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(serialize_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(pack_header_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_send_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_connect_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_gethostbyname_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_nonblocking_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_blocking_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(raw_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(aria2_system_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(aria2_fopen_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(aria2_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(aria2_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(xquic_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(xquic_engine_present, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(picoquic_create_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(picoquic_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(picoquic_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(picoquic_quic_null_on_free, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wasm_fetch_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wasm_fetch_timeout, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wasm_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wasm_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wasm_header_add_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wasm_headers_len_zero, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_global_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_parse_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_req_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_err_code, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_empty_body, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_response_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_upload_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_upload_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(fetch_loop_wakeup_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_global_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_conn_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_response_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_send_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(nghttp3_loop_wakeup_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_global_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_base_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_conn_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_req_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_make_req_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_res_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_body_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_buf_add_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libevent_empty_body, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_global_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_req_buf_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_res_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_body_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_headers_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_addrinfo_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_connect_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_write_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_read_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libuv_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_global_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_engine_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_res_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_body_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(lsquic_read_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_global_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_session_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_msg_new_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_send_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_res_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_body_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(libsoup3_uri_parse_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_getenv_detached, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_getenv_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_attach_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_new_string_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_find_class_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_get_method_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_new_object_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_call_object_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_res_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_input_stream_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_error_stream_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_read_exception, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_body_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_final_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_cleanup_exception, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_read_chunks, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_status_code, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(android_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_api_open_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_config_open_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_conn_open_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_request_open_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_send_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_shutdown_error, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_header_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_getaddrinfo_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_getaddrinfo_null_result, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_mutex_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_config_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_global_lock_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_res_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_cond_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_send_lock_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_cond_wait_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_header_add_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_cb_mutex_lock_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_parse_url_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_extra_events, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_zero_addrlen, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(msh3_cond_wait_loop, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_open_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_connect_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_open_request_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_send_request_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_write_data_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_receive_response_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_query_headers_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_query_data_available_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_read_data_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_set_timeouts_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_set_option_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_crack_url_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_add_request_headers_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_queue_work_item_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_status_code, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_cookie_count, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_read_chunks, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_total_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_read_buf_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_close_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(winhttp_cookie_set_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_open_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_connect_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_open_request_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_send_request_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_send_request_ex_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_write_file_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_end_request_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_query_info_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_read_file_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_set_option_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_crack_url_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_add_headers_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_context_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_response_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_status_code, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_cookie_count, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_read_chunks, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_read_chunk_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_body_realloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_res_alloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_close_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(wininet_cookie_set_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(accept_fd, -1)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(server_reading, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(timer_heap_swap_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(cond_init_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(queue_malloc_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(parser_destroy_fail, 0)
ABSTRACT_HTTP_DEF_MOCK_INT_VAR(http_types_strdup_fail, 0)

const char *g_mock_recv_data = NULL;

NO_DISCARD c_abstract_http_error_t
abstract_http_mock_get_g_mock_recv_data(const char ***out_ptr) {
  if (!out_ptr) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  *out_ptr = &g_mock_recv_data;
  return C_ABSTRACT_HTTP_SUCCESS;
}

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

#if !defined(_WIN32) && !defined(__MSDOS__) && !defined(__DOS__) &&            \
    !defined(DOS)
int c_abstract_http_mock_pthread_key_create(pthread_key_t *key,
                                            void (*destructor)(void *)) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_key_create(key, destructor);
}

int c_abstract_http_mock_pthread_setspecific(pthread_key_t key,
                                             const void *value) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_setspecific(key, value);
}

int c_abstract_http_mock_execv(const char *path, char *const argv[]) {
  (void)path;
  (void)argv;
  return -1;
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
  if (g_mock_pthread_fail == 6) {
    if (g_mock_alloc_count-- == 0) {
      return 1;
    }
    if (thread) {
      *thread = (pthread_t)(size_t)12345;
    }
    return 0;
  }
  if (g_mock_pthread_fail == 2 && g_mock_alloc_count-- == 0)
    return 1;
  return pthread_create(thread, attr, start_routine, arg);
}

int c_abstract_http_mock_pthread_join(pthread_t thread, void **value_ptr) {
  if (g_mock_pthread_fail == 3) {
    if (thread != (pthread_t)0 && thread != (pthread_t)(size_t)12345) {
      pthread_join(thread, value_ptr);
    }
    return 1;
  }
  if (g_mock_pthread_fail == 6) {
    return 1;
  }
  if (thread != (pthread_t)0 && thread != (pthread_t)(size_t)12345) {
    return pthread_join(thread, value_ptr);
  }
  return 0;
}

int c_abstract_http_mock_pipe(int fildes[2]) {
  if (g_mock_pipe_fail > 0) {
    if (--g_mock_pipe_fail == 0) {
      errno = EMFILE;
      return -1;
    }
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
    if (stat_loc)
      *stat_loc = 0x007F;
    return pid;
  }
  return waitpid(pid, stat_loc, options);
}
#endif

#if !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
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
      int i;
      for (i = 0; i < nfds; ++i) {
#ifdef _WIN32
        mock_fd_set_win((SOCKET)i, errorfds);
#else
        FD_SET(i, errorfds);
#endif
      }
      return 1;
    } else {
      return 0;
    }
  }
  return select(nfds, readfds, writefds, errorfds, timeout);
}
#endif

abstract_http_int64_t c_abstract_http_mock_math_get_current_time_ms(void) {
  abstract_http_int64_t now = real_math_get_current_time_ms();
  if (g_mock_time_jump) {
    if (g_mock_time_jump_count-- <= 0) {
      now += 1000;
    }
  }
  return now;
}

NO_DISCARD c_abstract_http_error_t dummy_cb_thread(void *arg) {
  (void)arg;
  return C_ABSTRACT_HTTP_SUCCESS;
}

void *dummy_cb_pthread(void *arg) {
  (void)arg;
  return NULL;
}

NO_DISCARD c_abstract_http_error_t c_abstract_http_mock_strdup(const char *s,
                                                               char **out) {
  if (g_mock_alloc_fail && g_mock_alloc_count-- == 0) {
    if (out)
      *out = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM;
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
SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type, int protocol) {
  if (g_mock_socket_fail)
    return INVALID_SOCKET;
  return socket(domain, type, protocol);
}

int WSAAPI c_abstract_http_mock_bind(SOCKET socket_fd,
                                     const struct sockaddr *address,
                                     socklen_t address_len) {
  if (g_mock_bind_fail)
    return SOCKET_ERROR;
  return bind(socket_fd, address, address_len);
}

int WSAAPI c_abstract_http_mock_listen(SOCKET socket_fd, int backlog) {
  if (g_mock_listen_fail)
    return SOCKET_ERROR;
  return listen(socket_fd, backlog);
}

SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket_fd,
                                          struct sockaddr *address,
                                          int *address_len) {
  if (g_mock_accept_fail)
    return INVALID_SOCKET;
  if (g_mock_accept_fd >= 0)
    return (SOCKET)g_mock_accept_fd;
  return accept(socket_fd, address, address_len);
}

int WSAAPI c_abstract_http_mock_recv(SOCKET socket_fd, char *buffer, int length,
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
  return recv(socket_fd, buffer, length, flags);
}

int WSAAPI c_abstract_http_mock_getsockname(SOCKET socket_fd,
                                            struct sockaddr *address,
                                            int *address_len) {
  if (g_mock_getsockname_fail)
    return SOCKET_ERROR;
  return getsockname(socket_fd, address, address_len);
}
#elif !defined(__MSDOS__) && !defined(__DOS__) && !defined(DOS)
int c_abstract_http_mock_socket(int domain, int type, int protocol) {
  if (g_mock_socket_fail)
    return -1;
  return socket(domain, type, protocol);
}

int c_abstract_http_mock_bind(int socket_fd, const struct sockaddr *address,
                              socklen_t address_len) {
  if (g_mock_bind_fail)
    return -1;
  return bind(socket_fd, address, address_len);
}

int c_abstract_http_mock_listen(int socket_fd, int backlog) {
  if (g_mock_listen_fail)
    return -1;
  return listen(socket_fd, backlog);
}

int c_abstract_http_mock_accept(int socket_fd, struct sockaddr *address,
                                socklen_t *address_len) {
  if (g_mock_accept_fail)
    return -1;
  if (g_mock_accept_fd >= 0)
    return g_mock_accept_fd;
  return accept(socket_fd, address, address_len);
}

ssize_t c_abstract_http_mock_recv(int socket_fd, void *buffer, size_t length,
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
  return recv(socket_fd, buffer, length, flags);
}

int c_abstract_http_mock_getsockname(int socket_fd, struct sockaddr *address,
                                     socklen_t *address_len) {
  if (g_mock_getsockname_fail)
    return -1;
  return getsockname(socket_fd, address, address_len);
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

#ifdef __MINGW32__
typedef void (*mock_fn_ptr_t)(void);
void (*__imp_c_abstract_http_mock_select)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_select;
void (*__imp_c_abstract_http_mock_socket)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_socket;
void (*__imp_c_abstract_http_mock_bind)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_bind;
void (*__imp_c_abstract_http_mock_listen)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_listen;
void (*__imp_c_abstract_http_mock_accept)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_accept;
void (*__imp_c_abstract_http_mock_getsockname)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_getsockname;
void (*__imp_c_abstract_http_mock_recv)(void) =
    (mock_fn_ptr_t)&c_abstract_http_mock_recv;
#endif

struct curl_slist;
struct curl_slist *g_mock_curl_cookies = 0;
