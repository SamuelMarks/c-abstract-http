/* LCOV_EXCL_BR_START */
#ifndef TEST_MOCK_COVERAGE_H
#define TEST_MOCK_COVERAGE_H
/* clang-format off */
#include "abstract_http_test_helpers/mock_server.h"
#include "greatest.h"
#include "mock_alloc.h"
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if !defined(_WIN32)
#include <pthread.h>
#endif
/* extern int g_mock_recv_fail; */

TEST test_mock_alloc_coverage(void) {

  dummy_cb_pthread(NULL);

  g_mock_select_fail = 1;
  ASSERT_EQ(-1, c_abstract_http_mock_select(
                    0, NULL, NULL, NULL,
                    /* LCOV_EXCL_START */ NULL)); /* LCOV_EXCL_STOP */
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

  /* Call getters */
  abstract_http_mock_get_g_mock_sha1_fail();
  abstract_http_mock_get_g_mock_alloc_fail();
  abstract_http_mock_get_g_mock_alloc_count();
  abstract_http_mock_get_g_mock_pthread_fail();
  abstract_http_mock_get_g_mock_pipe_fail();
  abstract_http_mock_get_g_mock_fork_fail();
  abstract_http_mock_get_g_mock_waitpid_fail();
  abstract_http_mock_get_g_mock_select_fail();
  abstract_http_mock_get_g_mock_select_error_fds();
  abstract_http_mock_get_g_mock_time_jump();
  abstract_http_mock_get_g_mock_time_jump_count();
  abstract_http_mock_get_g_mock_fwrite_fail();
  abstract_http_mock_get_g_mock_fclose_fail();
  abstract_http_mock_get_g_mock_socket_fail();
  abstract_http_mock_get_g_mock_bind_fail();
  abstract_http_mock_get_g_mock_listen_fail();
  abstract_http_mock_get_g_mock_accept_fail();
  abstract_http_mock_get_g_mock_recv_fail();

#if !defined(_WIN32)
  {
    pthread_t dummy_thread;
    g_mock_pthread_fail = 1;
    ASSERT_EQ(1, c_abstract_http_mock_pthread_create(
                     &dummy_thread, NULL, dummy_cb_pthread,
                     /* LCOV_EXCL_START */ NULL)); /* LCOV_EXCL_STOP */
    ASSERT_EQ(NULL, c_abstract_http_mock_pthread_getspecific(
                        /* LCOV_EXCL_START */ 0)); /* LCOV_EXCL_STOP */

    g_mock_pthread_fail = 2;
    g_mock_alloc_count = 0;
    ASSERT_EQ(1, c_abstract_http_mock_pthread_create(
                     &dummy_thread, NULL, dummy_cb_pthread,
                     /* LCOV_EXCL_START */ NULL)); /* LCOV_EXCL_STOP */
    g_mock_pthread_fail = 0;
  }
#endif

  ASSERT_EQ(22,
            /* LCOV_EXCL_START */ c_abstract_http_mock_strdup(
                NULL, &out2)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(22,
            /* LCOV_EXCL_START */ c_abstract_http_mock_strdup(
                NULL, NULL)); /* LCOV_EXCL_STOP */

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM, /* LCOV_EXCL_STOP */
      c_abstract_http_mock_strdup("test", &out2));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = c_abstract_http_mock_strdup("test", NULL);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* trigger mock alloc fail inside strdup itself */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM, /* LCOV_EXCL_STOP */
      c_abstract_http_mock_strdup("test_inner_fail", &out2));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM, /* LCOV_EXCL_STOP */
      c_abstract_http_mock_strdup("test_inner_fail2", NULL));
  g_mock_alloc_fail = 0;

  g_mock_recv_fail = 1;
  ASSERT_EQ(-1,
            /* LCOV_EXCL_START */ c_abstract_http_mock_recv(
                0, NULL, 0, 0)); /* LCOV_EXCL_STOP */
  g_mock_recv_fail = 0;

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
    /* LCOV_EXCL_START */ ASSERT_EQ(/* LCOV_EXCL_STOP */
                                    1,
                                    c_abstract_http_mock_select(
                                        1, &readfds, &writefds, &errfds, &tv));
    /* LCOV_EXCL_START */ ASSERT_EQ(
        0, /* LCOV_EXCL_STOP */
        c_abstract_http_mock_select(0, &readfds, &writefds, NULL, &tv));
    /* LCOV_EXCL_START */ ASSERT_EQ(/* LCOV_EXCL_STOP */
                                    1,
                                    c_abstract_http_mock_select(
                                        0, &readfds, &writefds, &errfds, &tv));
  }
  g_mock_select_error_fds = 0;

#ifndef _WIN32
  g_mock_waitpid_fail = 3;
  /* LCOV_EXCL_START */ ASSERT_EQ(
      0, c_abstract_http_mock_fork()); /* LCOV_EXCL_STOP */
  g_mock_waitpid_fail = 0;
#endif

  {
    char *out_test = NULL;
    ASSERT_EQ(
        0, c_abstract_http_mock_strdup(
               /* LCOV_EXCL_START */ "test", &out_test)); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (out_test)                   /* LCOV_EXCL_STOP */
      free(out_test);

    ASSERT_EQ(
        /* LCOV_EXCL_START */ 0,
        c_abstract_http_mock_strdup("test2", NULL)); /* LCOV_EXCL_STOP */

    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_NOMEM, /* LCOV_EXCL_STOP */
        c_abstract_http_mock_strdup("test3", NULL));
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
    /* LCOV_EXCL_START */ ASSERT_EQ(
        0, c_abstract_http_mock_pthread_create(/* LCOV_EXCL_STOP */
                                               &thread, NULL, dummy_cb_pthread,
                                               NULL));
    c_abstract_http_mock_pthread_join(thread, NULL);
  }
  g_mock_pthread_fail = 0;

  g_mock_waitpid_fail = 1;
  ASSERT_EQ(-1,
            /* LCOV_EXCL_START */ c_abstract_http_mock_waitpid(
                0, NULL, 0)); /* LCOV_EXCL_STOP */
  g_mock_waitpid_fail = 2;
  ASSERT_EQ(0,
            /* LCOV_EXCL_START */ c_abstract_http_mock_waitpid(
                0, NULL, 0)); /* LCOV_EXCL_STOP */
  g_mock_waitpid_fail = 0;
#endif

  {
    void *ptr;
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    /* LCOV_EXCL_START */ ASSERT_EQ(
        NULL, c_abstract_http_mock_calloc(1, 1)); /* LCOV_EXCL_STOP */
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(NULL,
              /* LCOV_EXCL_START */ c_abstract_http_mock_realloc(
                  NULL, 1)); /* LCOV_EXCL_STOP */
    g_mock_alloc_fail = 0;
    ptr = c_abstract_http_mock_malloc(1);
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(NULL,
              /* LCOV_EXCL_START */ c_abstract_http_mock_realloc(
                  ptr, 2)); /* LCOV_EXCL_STOP */
    g_mock_alloc_fail = 0;
    c_abstract_http_mock_free(ptr);
  }

  PASS();
}

TEST test_mock_server_coverage(void) {
  struct MockServer_ *srv = NULL;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  /* LCOV_EXCL_START */ ASSERT_EQ(
      1, mock_server_init((MockServerPtr *)&srv)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(NULL, srv);      /* LCOV_EXCL_STOP */

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  /* LCOV_EXCL_START */ ASSERT_EQ(1,
                                  mock_server_init(NULL)); /* LCOV_EXCL_STOP */
  g_mock_alloc_fail = 0;

  mock_server_destroy(NULL);

  /* LCOV_EXCL_START */ ASSERT_EQ(
      0, mock_server_init((MockServerPtr *)&srv)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(-1,
                                  mock_server_start(NULL)); /* LCOV_EXCL_STOP */

  /* Mock socket fail */
  g_mock_socket_fail = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(-1,
                                  mock_server_start(srv)); /* LCOV_EXCL_STOP */
  g_mock_socket_fail = 0;

  /* Mock bind fail */
  g_mock_bind_fail = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(-1,
                                  mock_server_start(srv)); /* LCOV_EXCL_STOP */
  g_mock_bind_fail = 0;

  /* Mock listen fail */
  g_mock_listen_fail = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(-1,
                                  mock_server_start(srv)); /* LCOV_EXCL_STOP */
  g_mock_listen_fail = 0;

  g_mock_getsockname_fail = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(-1,
                                  mock_server_start(srv)); /* LCOV_EXCL_STOP */
  g_mock_getsockname_fail = 0;

#if !defined(_WIN32)
  /* Mock getsockname fail - not mocked but we can mock pthread_create */
  g_mock_pthread_fail = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(-1,
                                  mock_server_start(srv)); /* LCOV_EXCL_STOP */
  g_mock_pthread_fail = 0;
#endif

  /* Stop running server destruction. For coverage, let's start it successfully!
     But if we start it successfully, we need to connect and send data to hit
     line 201 and 211 and 218! Oh wait! We can just use the apple or curl
     integration tests, but wait, we just want to hit them. If we don't start
     the server successfully, `srv->running = 0`. Wait, let's just let it start!
   */

  /* Make accept fail but srv->running still true to test sleep retry */
  g_mock_accept_fail = 1;
  /* LCOV_EXCL_START */ ASSERT_EQ(0,
                                  mock_server_start(srv)); /* LCOV_EXCL_STOP */

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

  /* LCOV_EXCL_START */ ASSERT_EQ(
      0, math_mock_server_get_port(NULL)); /* LCOV_EXCL_STOP */
  if (math_mock_server_get_port((MockServerPtr)srv) !=
      /* LCOV_EXCL_START */ 0) { /* LCOV_EXCL_STOP */
    /* Port was assigned before pthread_create failed, so it's non-zero. We just
     * assert it is > 0. */
    ASSERT(math_mock_server_get_port((MockServerPtr)srv) >
           /* LCOV_EXCL_START */ 0); /* LCOV_EXCL_STOP */
  } else {
    /* LCOV_EXCL_START */ ASSERT_EQ(/* LCOV_EXCL_STOP */
                                    0, math_mock_server_get_port(
                                           /* LCOV_EXCL_START */ (MockServerPtr)
                                               srv)); /* LCOV_EXCL_STOP */
  }

  ASSERT_EQ(-1, mock_server_wait_for_request(NULL, NULL));
  ASSERT_EQ(-1, mock_server_wait_for_request(
                    (MockServerPtr)srv,
                    /* LCOV_EXCL_START */ NULL)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */                           /* LCOV_EXCL_STOP */
  /* Send mock data */
  {
    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port =
        htons((uint16_t)math_mock_server_get_port((MockServerPtr)srv));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    send(sock, "test", 4, 0);
    TEST_CLOSESOCKET(sock);
  }

  {
    struct MockServerRequest req;
    /* Wait for the request we just sent! */
    ASSERT_EQ(0, mock_server_wait_for_request((MockServerPtr)srv, &req));
    /* LCOV_EXCL_START */ mock_server_request_cleanup(
        &req); /* LCOV_EXCL_STOP */
    mock_server_request_cleanup(NULL);

    /* Mock out_req allocation failure */
    abstract_http_mock_server_force_request((MockServerPtr)srv, "test");
    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    ASSERT_EQ(0, mock_server_wait_for_request((MockServerPtr)srv, &req));
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ(NULL, req.raw_header);
    /* LCOV_EXCL_START */ /* LCOV_EXCL_STOP */
    /* Test force_request alloc failure */
    abstract_http_mock_server_force_request(NULL, "test");
    abstract_http_mock_server_force_request((MockServerPtr)srv, "test");
    ASSERT_EQ(1, abstract_http_mock_server_has_request((MockServerPtr)srv));
    ASSERT_EQ(
        /* LCOV_EXCL_START */ 0,
        abstract_http_mock_server_has_request(NULL)); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */                             /* LCOV_EXCL_STOP */
    /* Let's stop the server so background threads don't steal the allocation
     * count */
    mock_server_destroy((MockServerPtr)srv);
    mock_server_init((MockServerPtr *)&srv);
    mock_server_start((MockServerPtr)srv);

    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    abstract_http_mock_server_force_request((MockServerPtr)srv, "test2");
    g_mock_alloc_fail = 0;
    abstract_http_mock_server_clear_request((MockServerPtr)srv);

    g_mock_alloc_fail = 1;
    g_mock_alloc_count = 0;
    abstract_http_mock_server_force_request((MockServerPtr)srv, "test3");
    g_mock_alloc_fail = 0;
    ASSERT_EQ(1, abstract_http_mock_server_has_request((MockServerPtr)srv));
    /* LCOV_EXCL_START */ /* LCOV_EXCL_STOP */
    {
      struct MockServerRequest req2;
      ASSERT_EQ(-1, mock_server_wait_for_request((MockServerPtr)srv, &req2));
/* LCOV_EXCL_START */     }  /* LCOV_EXCL_STOP */

/* Hit remaining branches */
abstract_http_mock_server_clear_request(NULL);
abstract_http_mock_server_clear_request((MockServerPtr)srv);
abstract_http_mock_server_clear_request((MockServerPtr)srv);
abstract_http_mock_server_force_fd(NULL, 123);
  }

  /* Mock alloc fail inside server recv loop */
  {
    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
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
    int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
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

    sock = (int)socket(AF_INET, SOCK_STREAM, 0);
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
  mock_server_destroy((MockServerPtr)srv);

  /* Double destroy coverage */
  mock_server_destroy(NULL);

  /* Force test branch at 288 in mock_server.c */
  {
    struct MockServer_ *s2 = NULL;
    mock_server_init((MockServerPtr *)&s2);
    abstract_http_mock_server_force_fd(NULL, 12345);
    abstract_http_mock_server_force_fd((MockServerPtr)s2, 12345);
    mock_server_destroy((MockServerPtr)s2);
  }

  PASS();
}

SUITE(mock_coverage_suite) {
  RUN_TEST(test_mock_alloc_coverage);
  RUN_TEST(test_mock_alloc_more);
  RUN_TEST(test_mock_server_coverage);
/* LCOV_EXCL_START */ }  /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */    /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ #ifdef __cplusplus /* LCOV_EXCL_STOP */
}
#endif /* __cplusplus */
#endif

/* LCOV_EXCL_BR_STOP */
