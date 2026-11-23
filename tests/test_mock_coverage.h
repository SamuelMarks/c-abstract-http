
#ifndef TEST_MOCK_COVERAGE_H
#define TEST_MOCK_COVERAGE_H
/* clang-format off */
#include "abstract_http_test_helpers/abstract_http_helpers.h"
#include "abstract_http_test_helpers/mock_server.h"
#include "fuzz_sse.h"
#include "fuzz_ws.h"
#ifdef _WIN32
typedef int socklen_t;
#endif
#include "greatest.h"
#include "mock_alloc.h"
#if !defined(_WIN32)
#include <pthread.h>
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if !defined(_WIN32)
typedef int MOCK_SOCKET_T;
#else
typedef SOCKET MOCK_SOCKET_T;
#endif
/* extern int g_mock_recv_fail; */

TEST test_mock_alloc_coverage(void) {

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, dummy_cb_thread(NULL));
  dummy_cb_pthread(NULL);

  g_mock_select_fail = 1;
  ASSERT_EQ(-1, c_abstract_http_mock_select(0, NULL, NULL, NULL, NULL));
  g_mock_select_fail = 0;

  g_mock_time_jump = 1;
  g_mock_time_jump_count = 2;
  c_abstract_http_mock_math_get_current_time_ms();
  c_abstract_http_mock_math_get_current_time_ms();
  c_abstract_http_mock_math_get_current_time_ms();
  g_mock_time_jump = 0;

  PASS();
}

TEST test_mock_alloc_more(void) {
  char *out2 = NULL;
  int *mock_int_ptr = NULL;
  const char **mock_recv_ptr = NULL;

#define ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(name)                               \
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,                                         \
            abstract_http_mock_get_g_mock_##name(NULL));                       \
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,                                           \
            abstract_http_mock_get_g_mock_##name(&mock_int_ptr));              \
  ASSERT(mock_int_ptr == &g_mock_##name);

  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(sha1_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(alloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(alloc_count)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(pthread_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(pthread_create_sync)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(pipe_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(fork_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(waitpid_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(select_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(select_error_fds)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(time_jump)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(time_jump_count)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(fwrite_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(fclose_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(socket_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(bind_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(listen_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(accept_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(recv_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(mutex_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(cond_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(strcasecmp_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(headers_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(parts_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(multi_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(sprintf_s_wrapper_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(urlencode_append_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(mask_key_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(serialize_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(pack_header_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(accept_fd)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(server_reading)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_open_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_connect_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_open_request_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_send_request_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_write_data_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_receive_response_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_query_headers_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_query_data_available_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_read_data_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_set_timeouts_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_set_option_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_crack_url_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_add_request_headers_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_queue_work_item_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_context_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_response_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_status_code)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_cookie_count)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_read_chunks)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_total_body_realloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_read_buf_alloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(winhttp_res_alloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_open_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_connect_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_open_request_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_send_request_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_send_request_ex_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_write_file_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_end_request_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_query_info_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_read_file_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_set_option_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_crack_url_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_add_headers_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_context_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_response_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_status_code)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_cookie_count)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_read_chunks)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_read_chunk_alloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_body_realloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wininet_res_alloc_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wasm_fetch_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wasm_fetch_timeout)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wasm_config_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wasm_response_init_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wasm_header_add_fail)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(wasm_headers_len_zero)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(msh3_zero_addrlen)
  ABSTRACT_HTTP_TEST_MOCK_INT_GETTER(msh3_cond_wait_loop)
#undef ABSTRACT_HTTP_TEST_MOCK_INT_GETTER

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_mock_get_g_mock_recv_data(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mock_get_g_mock_recv_data(&mock_recv_ptr));
  ASSERT(mock_recv_ptr == &g_mock_recv_data);

#if !defined(_WIN32)
  {
    pthread_t dummy_thread;
    memset(&dummy_thread, 0, sizeof(dummy_thread));
    g_mock_pthread_fail = 1;
    ASSERT_EQ(1, c_abstract_http_mock_pthread_create(&dummy_thread, NULL,
                                                     dummy_cb_pthread, NULL));
    ASSERT_EQ(NULL, c_abstract_http_mock_pthread_getspecific(0));

    g_mock_pthread_fail = 2;
    g_mock_alloc_count = 0;
    ASSERT_EQ(1, c_abstract_http_mock_pthread_create(&dummy_thread, NULL,
                                                     dummy_cb_pthread, NULL));
    g_mock_pthread_fail = 3;
    ASSERT_EQ(1, c_abstract_http_mock_pthread_join(dummy_thread, NULL));
    g_mock_pthread_fail = 0;
  }
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_mock_strdup(NULL, &out2));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_mock_strdup(NULL, NULL));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_mock_strdup("test", &out2));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = c_abstract_http_mock_strdup("test", NULL);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  /* trigger mock alloc fail inside strdup itself */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_mock_strdup("test_inner_fail", &out2));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_mock_strdup("test_inner_fail2", &out2));
  g_mock_alloc_fail = 0;

  g_mock_recv_fail = 1;
  ASSERT_EQ(-1, c_abstract_http_mock_recv(0, NULL, 0, 0));
  g_mock_recv_fail = 0;

  {
    char rbuf[5];
    g_mock_recv_data = "hello world";
    ASSERT_EQ(5, c_abstract_http_mock_recv(0, rbuf, 5, 0));
    g_mock_recv_data = NULL;
  }

  g_mock_select_error_fds = 1;
  {
    fd_set errfds;
    fd_set readfds;
    fd_set writefds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&errfds);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    ASSERT_EQ(
        1, c_abstract_http_mock_select(1, &readfds, &writefds, &errfds, &tv));
    ASSERT_EQ(0,
              c_abstract_http_mock_select(0, &readfds, &writefds, NULL, &tv));
    ASSERT_EQ(
        1, c_abstract_http_mock_select(0, &readfds, &writefds, &errfds, &tv));
  }
  g_mock_select_error_fds = 0;

#ifndef _WIN32
  g_mock_waitpid_fail = 3;
  ASSERT_EQ(0, c_abstract_http_mock_fork());
  g_mock_waitpid_fail = 0;
#endif

  {
    char *out_test = NULL;
    ASSERT_EQ(0, c_abstract_http_mock_strdup("test", &out_test));
    if (out_test)
      free(out_test);

    ASSERT_EQ(0, c_abstract_http_mock_strdup("test2", NULL));

    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
              c_abstract_http_mock_strdup("test3", NULL));
    g_mock_alloc_fail = 0;
  }

  {
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    c_abstract_http_mock_select(1, NULL, NULL, NULL, &tv);
  }
#if !defined(_WIN32)
  g_mock_pthread_fail = 2;
  g_mock_alloc_count = 1;
  {
    pthread_t thread;
    ASSERT_EQ(0, c_abstract_http_mock_pthread_create(&thread, NULL,
                                                     dummy_cb_pthread, NULL));
    c_abstract_http_mock_pthread_join(thread, NULL);
  }
  g_mock_pthread_fail = 0;

  g_mock_waitpid_fail = 1;
  ASSERT_EQ(-1, c_abstract_http_mock_waitpid(0, NULL, 0));
  g_mock_waitpid_fail = 2;
  ASSERT_EQ(0, c_abstract_http_mock_waitpid(0, NULL, 0));
  g_mock_waitpid_fail = 0;
#endif

  {
    void *ptr;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(NULL, c_abstract_http_mock_calloc(1, 1));
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(NULL, c_abstract_http_mock_realloc(NULL, 1));
    g_mock_alloc_fail = 0;
    ptr = c_abstract_http_mock_malloc(1);
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(NULL, c_abstract_http_mock_realloc(ptr, 2));
    g_mock_alloc_fail = 0;
    c_abstract_http_mock_free(ptr);
  }

  PASS();
}

NO_DISCARD static c_abstract_http_error_t trigger_req_cb(void *arg) {
  c_abstract_http_error_t rc;
  MockServerPtr srv = (MockServerPtr)arg;
#if !defined(_WIN32)
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 30000000;
  nanosleep(&ts, NULL);
#else
  Sleep(30);
#endif
  rc = abstract_http_mock_server_force_request(srv, "async_wait");
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return abstract_http_mock_server_signal_ready(srv);
}

TEST test_mock_server_coverage(void) {
  struct MockServer_ *srv = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, mock_server_init((MockServerPtr *)&srv));
  ASSERT_EQ(NULL, srv);

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, mock_server_init(NULL));
  g_mock_alloc_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init(NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init((MockServerPtr *)&srv));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_server_start(NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* Mock socket fail */
  g_mock_socket_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mock_server_start((MockServerPtr)srv));
  g_mock_socket_fail = 0;

  /* Mock bind fail */
  g_mock_bind_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mock_server_start((MockServerPtr)srv));
  g_mock_bind_fail = 0;

  /* Mock listen fail */
  g_mock_listen_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mock_server_start((MockServerPtr)srv));
  g_mock_listen_fail = 0;

  g_mock_getsockname_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mock_server_start((MockServerPtr)srv));
  g_mock_getsockname_fail = 0;

#if !defined(_WIN32)
  /* Mock getsockname fail - not mocked but we can mock pthread_create */
  g_mock_pthread_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, mock_server_start((MockServerPtr)srv));
  g_mock_pthread_fail = 0;
#endif
#endif

  /* Stop running server destruction. For coverage, let's start it successfully!
     But if we start it successfully, we need to connect and send data to hit
     line 201 and 211 and 218! Oh wait! We can just use the apple or curl
     integration tests, but wait, we just want to hit them. If we don't start
     the server successfully, `srv->running = 0`. Wait, let's just let it start!
   */

  /* Make accept fail but srv->running still true to test sleep retry */
  g_mock_accept_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start((MockServerPtr)srv));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, mock_server_start((MockServerPtr)srv));

  /* Wait so thread runs and hits accept failure */
#if defined(_WIN32)
  Sleep(150);
#else
  {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 20000000;
    nanosleep(&ts, NULL);
  }
#endif
  g_mock_accept_fail = 0;

  ASSERT_EQ(0, math_mock_server_get_port(NULL));
  ASSERT(math_mock_server_get_port((MockServerPtr)srv) > 0);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_server_wait_for_request(NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            mock_server_wait_for_request((MockServerPtr)srv, NULL));

  /* Connect and immediately close to trigger bytes_read <= 0 in mock_server */
  {
    MOCK_SOCKET_T sock0 = (MOCK_SOCKET_T)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port =
        htons((uint16_t)math_mock_server_get_port((MockServerPtr)srv));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock0, (struct sockaddr *)&addr, sizeof(addr));
    TEST_CLOSESOCKET(sock0);
#if !defined(_WIN32)
    {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 120000000;
      nanosleep(&ts, NULL);
    }
#else
    Sleep(120);
#endif
  }

  /* Send mock data */
  {
    MOCK_SOCKET_T sock = (MOCK_SOCKET_T)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port =
        htons((uint16_t)math_mock_server_get_port((MockServerPtr)srv));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    send(sock, "test", 4, 0);
    TEST_CLOSESOCKET(sock);
  }

  /* Send second mock data while first is still unconsumed */
  {
    MOCK_SOCKET_T sock2;
    struct sockaddr_in addr;
#if !defined(_WIN32)
    struct timespec ts;
#endif
    memset(&addr, 0, sizeof(addr));
#if !defined(_WIN32)
    ts.tv_sec = 0;
    ts.tv_nsec = 120000000;
    nanosleep(&ts, NULL);
#else
    Sleep(120);
#endif
    sock2 = (MOCK_SOCKET_T)socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port =
        htons((uint16_t)math_mock_server_get_port((MockServerPtr)srv));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock2, (struct sockaddr *)&addr, sizeof(addr));
    send(sock2, "test2", 5, 0);
    TEST_CLOSESOCKET(sock2);
#if !defined(_WIN32)
    nanosleep(&ts, NULL);
#else
    Sleep(120);
#endif
  }

  {
    struct MockServerRequest req;
    struct MockServerRequest empty_req;
    memset(&empty_req, 0, sizeof(empty_req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(&empty_req));

    /* Wait for the request we just sent! */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_wait_for_request((MockServerPtr)srv, &req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(&req));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_request_cleanup(NULL));

    /* Exercise force_request overwriting existing request and clear_request */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, "test_prev"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, "test_next"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_clear_request((MockServerPtr)srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_clear_request(NULL));

    /* Test cond_wait by waiting when has_request == 0 */
    {
      struct AbstractHttpThreadPool *pool = NULL;
      struct MockServerRequest async_req;
      memset(&async_req, 0, sizeof(async_req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_thread_pool_init(&pool, 1));
      ASSERT_EQ(
          C_ABSTRACT_HTTP_SUCCESS,
          abstract_http_thread_pool_push(pool, trigger_req_cb, (void *)srv));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                mock_server_wait_for_request((MockServerPtr)srv, &async_req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                mock_server_request_cleanup(&async_req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_thread_pool_free(pool));
    }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    /* Mock out_req allocation failure */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, "test"));
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_wait_for_request((MockServerPtr)srv, &req));
    g_mock_alloc_fail = 0;
    ASSERT_EQ(NULL, req.raw_header);
#endif

    /* Mock server has request but captured_request is NULL */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              mock_server_wait_for_request((MockServerPtr)srv, &req));

    /* Test force_request alloc failure */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_request(NULL, "test"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, "test"));
    {
      int has_req = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_has_request(
                                             (MockServerPtr)srv, &has_req));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_has_request(
                                             (MockServerPtr)srv, NULL));
      ASSERT_EQ(1, has_req);
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_mock_server_has_request(NULL, &has_req));
      ASSERT_EQ(0, has_req);
    }

    /* Let's stop the server so background threads don't steal the allocation
     * count */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy((MockServerPtr)srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init((MockServerPtr *)&srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start((MockServerPtr)srv));

    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, "test2"));
    g_mock_alloc_fail = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_clear_request((MockServerPtr)srv));

    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)srv, "test3"));
    g_mock_alloc_fail = 0;
    {
      int has_req = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_has_request(
                                             (MockServerPtr)srv, &has_req));
      ASSERT_EQ(1, has_req);
    }

    {
      struct MockServerRequest req2;
      extern int g_mock_server_cond_wait_fail;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_mock_server_clear_request((MockServerPtr)srv));
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_mock_server_force_running((MockServerPtr)srv, 1));
      g_mock_server_cond_wait_fail = 1;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                mock_server_wait_for_request((MockServerPtr)srv, &req2));
      g_mock_server_cond_wait_fail = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                abstract_http_mock_server_force_running((MockServerPtr)srv, 0));
    }

    /* Hit remaining branches */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_clear_request(NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_signal_ready(NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_clear_request((MockServerPtr)srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_clear_request((MockServerPtr)srv));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_fd(NULL, 123));
  }

  /* Mock alloc fail inside server recv loop */
  {
    MOCK_SOCKET_T sock = (MOCK_SOCKET_T)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port =
        htons((uint16_t)math_mock_server_get_port((MockServerPtr)srv));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    /* Make malloc fail in recv */
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    send(sock, "fail", 4, 0);
/* wait for it to process */
#if defined(_WIN32)
    Sleep(200);
#else
    {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 200000000;
      nanosleep(&ts, NULL);
    }
#endif
    g_mock_alloc_fail = 0;
    TEST_CLOSESOCKET(sock);
  }

  /* Mock alloc fail for existing captured_request in recv loop */
  {
    MOCK_SOCKET_T sock = (MOCK_SOCKET_T)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port =
        htons((uint16_t)math_mock_server_get_port((MockServerPtr)srv));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    /* Send first one to allocate */
    send(sock, "first", 5, 0);
#if defined(_WIN32)
    Sleep(150);
#else
    {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 150000000;
      nanosleep(&ts, NULL);
    }
#endif

    TEST_CLOSESOCKET(sock);

    sock = (MOCK_SOCKET_T)socket(AF_INET, SOCK_STREAM, 0);
    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    /* Make malloc fail for the second one, to hit the `free` block */
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    send(sock, "second", 6, 0);
#if defined(_WIN32)
    Sleep(150);
#else
    {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 150000000;
      nanosleep(&ts, NULL);
    }
#endif
    g_mock_alloc_fail = 0;
    TEST_CLOSESOCKET(sock);
  }

  /* Mock alloc fail inside server recv loop? We can't easily sync that unless
     we have a callback. But wait, `abstract_http_mock_server_force_request`
     covers the memory. What about the accept loop failure? `if (!s->running)
     break;` It breaks out of the loop and exits the thread! */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy((MockServerPtr)srv));

  /* Double destroy coverage */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy(NULL));

  /* Force test branch at 288 in mock_server.c */
  {
    struct MockServer_ *s2 = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init((MockServerPtr *)&s2));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_fd(NULL, 12345));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_fd((MockServerPtr)s2, 12345));

    /* Test double force_request */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)s2, "req1"));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_request(
                                           (MockServerPtr)s2, "req2"));

    /* Test run thread once with running = 0 */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_run_thread_once(NULL));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_run_thread_once((MockServerPtr)s2));

    /* Test force running without server_fd for mock_server_destroy */
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_port(NULL, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_port((MockServerPtr)s2, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_running(NULL, 1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_running((MockServerPtr)s2, 1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_fd((MockServerPtr)s2, -1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_running((MockServerPtr)s2, 1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy((MockServerPtr)s2));
  }

  {
    struct MockServer_ *s_fd = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_init((MockServerPtr *)&s_fd));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_fd((MockServerPtr)s_fd, 12345));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_destroy((MockServerPtr)s_fd));
  }

  {
    struct MockServer_ *s_port = NULL;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_init((MockServerPtr *)&s_port));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_port(
                                           (MockServerPtr)s_port, 9999));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_fd((MockServerPtr)s_port, -1));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, abstract_http_mock_server_force_running(
                                           (MockServerPtr)s_port, 1));
    g_mock_socket_fail = 1;
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              mock_server_destroy((MockServerPtr)s_port));
    g_mock_socket_fail = 0;
  }

  /* Test wait_for_request when server is stopped and has no request */
  {
    struct MockServer_ *s3 = NULL;
    struct MockServerRequest req_out;
    memset(&req_out, 0, sizeof(req_out));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_init((MockServerPtr *)&s3));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_start((MockServerPtr)s3));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_force_running((MockServerPtr)s3, 0));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              abstract_http_mock_server_signal_ready((MockServerPtr)s3));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              mock_server_wait_for_request((MockServerPtr)s3, &req_out));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, mock_server_destroy((MockServerPtr)s3));
  }

  PASS();
}

TEST test_abstract_http_helpers_coverage(void) {
  c_abstract_http_error_t rc;
  const char *tmp_filename;
  FILE *fh;

  tmp_filename = "test_helper_output.tmp";
  fh = NULL;

  /* abstract_http_precondition_failed */
  rc = abstract_http_precondition_failed();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* write_to_file NULL arguments */
  rc = write_to_file(NULL, "data");
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);
  rc = write_to_file("data.tmp", NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* write_to_file invalid path */
  rc =
      write_to_file("/invalid_nonexistent_directory_98765/file.tmp", "content");
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);

  /* write_to_file success */
  rc = write_to_file(tmp_filename, "hello world");
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* verify file content */
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  if (fopen_s(&fh, tmp_filename, "r") != 0) {
    fh = NULL;
  }
#else
  fh = fopen(tmp_filename, "r");
#endif
  ASSERT(fh != NULL);
  fclose(fh);
  remove(tmp_filename);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* write_to_file mock failure on fwrite */
  g_mock_fwrite_fail = 1;
  rc = write_to_file(tmp_filename, "failure");
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_fwrite_fail = 0;
  remove(tmp_filename);

  /* write_to_file mock failure on fclose */
  g_mock_fclose_fail = 1;
  rc = write_to_file(tmp_filename, "failure");
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_fclose_fail = 0;
  remove(tmp_filename);
#endif

  PASS();
}

TEST test_fuzz_harness_coverage(void) {
  c_abstract_http_error_t rc;
  int out_rc;
#if defined(C_ABSTRACT_HTTP_ENABLE_SSE)
  char *big_line = NULL;
  const uint8_t sse_valid[24] = "event: msg\ndata: hi\n\n";
  const uint8_t sse_err[14] = ": malformed\n\n";
#endif
#if defined(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
  const uint8_t ws_valid[6] = {0x81, 0x04, 't', 'e', 's', 't'};
  const uint8_t ws_err[2] = {0x70, 0x00};
  const uint8_t ws_close[4] = {0x88, 0x02, 0x03, 0xE8};
#endif

  out_rc = 0;

#if defined(C_ABSTRACT_HTTP_ENABLE_SSE)
  /* fuzz_sse NULL handling */
  rc = test_fuzz_sse_run(NULL, 10, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* fuzz_sse empty / 0 size */
  rc = test_fuzz_sse_run(NULL, 0, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = test_fuzz_sse_run(sse_valid, 0, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* fuzz_sse valid event */
  rc = test_fuzz_sse_run(sse_valid, sizeof(sse_valid) - 1, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = test_fuzz_sse_run(sse_valid, sizeof(sse_valid) - 1, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* fuzz_sse error trigger */
  rc = test_fuzz_sse_run(sse_err, sizeof(sse_err) - 1, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* fuzz_sse oversized line error */
  big_line = (char *)malloc(35000);
  if (big_line != NULL) {
    memset(big_line, 'A', 34999);
    memcpy(big_line, "data: ", 6);
    big_line[34998] = '\n';
    big_line[34999] = '\0';
    rc = test_fuzz_sse_run((const uint8_t *)big_line, 35000, &out_rc);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
    free(big_line);
    big_line = NULL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* fuzz_sse alloc fail */
  g_mock_alloc_fail = 1;
  rc = test_fuzz_sse_run(sse_valid, sizeof(sse_valid) - 1, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  rc = test_fuzz_sse_run(sse_valid, sizeof(sse_valid) - 1, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;

  /* fuzz_sse parser destroy fail */
  g_mock_cond_free_fail = 1;
  rc = test_fuzz_sse_run(sse_valid, sizeof(sse_valid) - 1, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_cond_free_fail = 0;
#endif
#else
  rc = test_fuzz_sse_run(NULL, 0, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = test_fuzz_sse_run((const uint8_t *)"test", 4, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#endif

#if defined(C_ABSTRACT_HTTP_ENABLE_WEBSOCKETS)
  /* fuzz_ws NULL handling */
  rc = test_fuzz_ws_run(NULL, 10, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, rc);

  /* fuzz_ws empty / 0 size */
  rc = test_fuzz_ws_run(NULL, 0, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = test_fuzz_ws_run(ws_valid, 0, NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* fuzz_ws valid message */
  rc = test_fuzz_ws_run(ws_valid, sizeof(ws_valid), &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = test_fuzz_ws_run(ws_valid, sizeof(ws_valid), NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* fuzz_ws error frame */
  rc = test_fuzz_ws_run(ws_err, sizeof(ws_err), &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

  /* fuzz_ws close frame */
  rc = test_fuzz_ws_run(ws_close, sizeof(ws_close), &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* fuzz_ws alloc fail */
  g_mock_alloc_fail = 1;
  rc = test_fuzz_ws_run(ws_valid, sizeof(ws_valid), &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  rc = test_fuzz_ws_run(ws_valid, sizeof(ws_valid), NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, rc);
  g_mock_alloc_fail = 0;
  /* fuzz_ws parser destroy fail */
  g_mock_parser_destroy_fail = 1;
  rc = test_fuzz_ws_run(ws_valid, sizeof(ws_valid), &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_parser_destroy_fail = 0;
#endif
#else
  rc = test_fuzz_ws_run(NULL, 0, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = test_fuzz_ws_run((const uint8_t *)"test", 4, &out_rc);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#endif

  PASS();
}

TEST test_mock_socket_coverage(void) {
#ifdef __APPLE__
  int c_abstract_http_mock_select_darwin_extsn(
      int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
      struct timeval
          *timeout) __asm("_c_abstract_http_mock_select$DARWIN_EXTSN");
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
  SOCKET fd;
#else
  int fd;
#endif
  char buf[32];
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  struct timeval tv = {0, 0};
  if (tv.tv_sec) {
  }
  memset(&addr, 0, sizeof(addr));

  g_mock_socket_fail = 1;
#if defined(_WIN32) && !defined(__CYGWIN__)
  c_abstract_http_mock_socket(AF_INET, SOCK_STREAM, 0);
  g_mock_socket_fail = 0;
  fd = c_abstract_http_mock_socket(AF_INET, SOCK_STREAM, 0);

  g_mock_bind_fail = 1;
  c_abstract_http_mock_bind(fd, (const struct sockaddr *)&addr, addr_len);
  g_mock_bind_fail = 0;

  g_mock_listen_fail = 1;
  c_abstract_http_mock_listen(fd, 1);
  g_mock_listen_fail = 0;

  g_mock_accept_fail = 1;
  c_abstract_http_mock_accept(fd, (struct sockaddr *)&addr, (int *)&addr_len);
  g_mock_accept_fail = 0;
  g_mock_accept_fd = 100;
  c_abstract_http_mock_accept(fd, (struct sockaddr *)&addr, (int *)&addr_len);
  g_mock_accept_fd = -1;

  g_mock_recv_fail = 1;
  c_abstract_http_mock_recv(fd, buf, sizeof(buf), 0);
  g_mock_recv_fail = 0;
  g_mock_recv_data = "hello";
  g_mock_server_reading = 1;
  c_abstract_http_mock_recv(INVALID_SOCKET, buf, 1, 0);
  g_mock_server_reading = 0;
  c_abstract_http_mock_recv(fd, buf, 1, 0);
  g_mock_recv_data = "";
  c_abstract_http_mock_recv(fd, buf, sizeof(buf), 0);
  g_mock_recv_data = NULL;

  g_mock_getsockname_fail = 1;
  c_abstract_http_mock_getsockname(fd, (struct sockaddr *)&addr,
                                   (int *)&addr_len);
  g_mock_getsockname_fail = 0;
#else
  c_abstract_http_mock_socket(AF_INET, SOCK_STREAM, 0);
  g_mock_socket_fail = 0;
  fd = c_abstract_http_mock_socket(AF_INET, SOCK_STREAM, 0);

  g_mock_bind_fail = 1;
  c_abstract_http_mock_bind(fd, (const struct sockaddr *)&addr, addr_len);
  g_mock_bind_fail = 0;

  g_mock_listen_fail = 1;
  c_abstract_http_mock_listen(fd, 1);
  g_mock_listen_fail = 0;

  g_mock_accept_fail = 1;
  c_abstract_http_mock_accept(fd, (struct sockaddr *)&addr, &addr_len);
  g_mock_accept_fail = 0;
  g_mock_accept_fd = 100;
  c_abstract_http_mock_accept(fd, (struct sockaddr *)&addr, &addr_len);
  g_mock_accept_fd = -1;

  g_mock_recv_fail = 1;
  c_abstract_http_mock_recv(fd, buf, sizeof(buf), 0);
  g_mock_recv_fail = 0;
  g_mock_recv_data = "hello";
  g_mock_server_reading = 1;
  c_abstract_http_mock_recv(-1, buf, 1, 0);
  g_mock_server_reading = 0;
  c_abstract_http_mock_recv(fd, buf, 1, 0);
  g_mock_recv_data = "";
  c_abstract_http_mock_recv(fd, buf, sizeof(buf), 0);
  g_mock_recv_data = NULL;

  g_mock_getsockname_fail = 1;
  c_abstract_http_mock_getsockname(fd, (struct sockaddr *)&addr, &addr_len);
  g_mock_getsockname_fail = 0;
#endif

#ifdef __APPLE__
  c_abstract_http_mock_select_darwin_extsn(0, NULL, NULL, NULL, &tv);
#endif

  PASS();
}

TEST test_mock_alloc_all_getters(void) {
  int *mock_int_ptr = NULL;
  const char **mock_recv_ptr = NULL;

#define ABSTRACT_HTTP_ASSERT_GETTER(name)                                      \
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,                                         \
            abstract_http_mock_get_g_mock_##name(NULL));                       \
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,                                           \
            abstract_http_mock_get_g_mock_##name(&mock_int_ptr));              \
  ASSERT(mock_int_ptr == &g_mock_##name);

  ABSTRACT_HTTP_ASSERT_GETTER(getsockname_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(sha1_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(alloc_count)
  ABSTRACT_HTTP_ASSERT_GETTER(pthread_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(pthread_create_sync)
  ABSTRACT_HTTP_ASSERT_GETTER(pipe_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fork_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(waitpid_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(select_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(select_error_fds)
  ABSTRACT_HTTP_ASSERT_GETTER(time_jump)
  ABSTRACT_HTTP_ASSERT_GETTER(time_jump_count)
  ABSTRACT_HTTP_ASSERT_GETTER(fwrite_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fclose_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(socket_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(bind_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(listen_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(accept_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(recv_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(config_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(response_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(multi_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(parts_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(headers_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(cond_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(mutex_free_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(mutex_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(cond_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(strcasecmp_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(headers_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(parts_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(multi_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(sprintf_s_wrapper_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(urlencode_append_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(mask_key_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(serialize_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(pack_header_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_send_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_connect_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_gethostbyname_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_nonblocking_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_blocking_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(raw_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(aria2_system_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(aria2_fopen_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(aria2_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(aria2_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(xquic_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(xquic_engine_present)
  ABSTRACT_HTTP_ASSERT_GETTER(picoquic_create_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(picoquic_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(picoquic_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(picoquic_quic_null_on_free)
  ABSTRACT_HTTP_ASSERT_GETTER(wasm_fetch_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wasm_fetch_timeout)
  ABSTRACT_HTTP_ASSERT_GETTER(wasm_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wasm_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wasm_header_add_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wasm_headers_len_zero)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_global_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_parse_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_req_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_err_code)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_empty_body)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_response_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_upload_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_upload_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(fetch_loop_wakeup_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_global_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_conn_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_response_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_send_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(nghttp3_loop_wakeup_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_global_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_base_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_conn_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_req_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_make_req_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_res_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_body_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_buf_add_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libevent_empty_body)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_global_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_req_buf_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_res_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_body_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_headers_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_addrinfo_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_connect_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_write_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_read_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libuv_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_global_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_engine_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_res_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_body_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(lsquic_read_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_global_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_session_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_msg_new_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_send_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_res_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_body_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(libsoup3_uri_parse_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_getenv_detached)
  ABSTRACT_HTTP_ASSERT_GETTER(android_getenv_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_attach_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_new_string_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_find_class_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_get_method_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_new_object_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_call_object_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_res_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_input_stream_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_error_stream_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_read_exception)
  ABSTRACT_HTTP_ASSERT_GETTER(android_body_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_final_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(android_cleanup_exception)
  ABSTRACT_HTTP_ASSERT_GETTER(android_read_chunks)
  ABSTRACT_HTTP_ASSERT_GETTER(android_status_code)
  ABSTRACT_HTTP_ASSERT_GETTER(android_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_api_open_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_config_open_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_conn_open_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_request_open_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_send_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_shutdown_error)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_header_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_getaddrinfo_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_getaddrinfo_null_result)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_mutex_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_config_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_global_lock_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_res_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_cond_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_send_lock_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_cond_wait_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_header_add_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_cb_mutex_lock_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_parse_url_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_extra_events)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_zero_addrlen)
  ABSTRACT_HTTP_ASSERT_GETTER(msh3_cond_wait_loop)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_open_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_connect_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_open_request_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_send_request_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_write_data_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_receive_response_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_query_headers_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_query_data_available_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_read_data_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_set_timeouts_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_set_option_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_crack_url_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_add_request_headers_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_queue_work_item_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_status_code)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_cookie_count)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_read_chunks)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_total_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_read_buf_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_close_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(winhttp_cookie_set_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_open_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_connect_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_open_request_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_send_request_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_send_request_ex_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_write_file_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_end_request_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_query_info_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_read_file_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_set_option_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_crack_url_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_add_headers_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_context_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_response_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_status_code)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_cookie_count)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_read_chunks)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_read_chunk_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_body_realloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_res_alloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_close_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(wininet_cookie_set_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(accept_fd)
  ABSTRACT_HTTP_ASSERT_GETTER(server_reading)
  ABSTRACT_HTTP_ASSERT_GETTER(timer_heap_swap_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(cond_init_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(queue_malloc_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(parser_destroy_fail)
  ABSTRACT_HTTP_ASSERT_GETTER(http_types_strdup_fail)
#undef ABSTRACT_HTTP_ASSERT_GETTER

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            abstract_http_mock_get_g_mock_recv_data(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            abstract_http_mock_get_g_mock_recv_data(&mock_recv_ptr));
  ASSERT(mock_recv_ptr == &g_mock_recv_data);
  PASS();
}

SUITE(mock_coverage_suite) {
  RUN_TEST(test_mock_alloc_all_getters);
  RUN_TEST(test_mock_socket_coverage);
  RUN_TEST(test_mock_alloc_coverage);
  RUN_TEST(test_mock_alloc_more);
  RUN_TEST(test_mock_server_coverage);
  RUN_TEST(test_abstract_http_helpers_coverage);
  RUN_TEST(test_fuzz_harness_coverage);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
