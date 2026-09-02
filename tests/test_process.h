/* LCOV_EXCL_BR_START */
#ifndef TEST_PROCESS_H
#define TEST_PROCESS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/process.h>
/* clang-format on */

/* LCOV_EXCL_START */ TEST test_ipc_pipe_init_free(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe pipe = {
      0}; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &pipe));                                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(pipe.read_handle != NULL);   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(pipe.write_handle != NULL);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&pipe); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(pipe.read_handle == NULL);   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(pipe.write_handle == NULL);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                             /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                                     /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_serialize_deserialize_request(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req_in, req_out;
  /* LCOV_EXCL_START */ char *buf = NULL;           /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ char *_ast_strdup_0 = NULL; /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req_in);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ }                          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req_in.method = HTTP_POST; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req_in.url =               /* LCOV_EXCL_STOP */
      (c_abstract_http_mock_strdup(
           "http://example.com/api",
           /* LCOV_EXCL_START */ &_ast_strdup_0), /* LCOV_EXCL_STOP */
       /* LCOV_EXCL_START */ _ast_strdup_0);      /* LCOV_EXCL_STOP */
  (void)!http_headers_add(
      &req_in.headers, "Content-Type",
      /* LCOV_EXCL_START */ "application/json"); /* LCOV_EXCL_STOP */
  (void)!http_headers_add(
      &req_in.headers, "X-Custom",
      /* LCOV_EXCL_START */ "test_val"); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ req_in.body_len = 13; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req_in.body =
      malloc(req_in.body_len); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(req_in.body, "Hello, World!",
                               req_in.body_len); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req_in, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT(buf != NULL); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(len > 0);     /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request(buf, len, &req_out));

  /* LCOV_EXCL_START */ ASSERT_EQ(req_in.method,
                                  req_out.method); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_STR_EQ(req_in.url,
                                      req_out.url); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(2,
                                  req_out.headers.count); /* LCOV_EXCL_STOP */
  ASSERT_STR_EQ("Content-Type",
                /* LCOV_EXCL_START */ req_out.headers.headers[0]
                    .key); /* LCOV_EXCL_STOP */
  ASSERT_STR_EQ("application/json",
                /* LCOV_EXCL_START */ req_out.headers.headers[0]
                    .value); /* LCOV_EXCL_STOP */
  ASSERT_STR_EQ("X-Custom",
                /* LCOV_EXCL_START */ req_out.headers.headers[1]
                    .key); /* LCOV_EXCL_STOP */
  ASSERT_STR_EQ("test_val",
                /* LCOV_EXCL_START */ req_out.headers.headers[1]
                    .value); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(req_in.body_len,
                                  req_out.body_len); /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      0, memcmp(req_in.body, req_out.body,
                /* LCOV_EXCL_START */ req_out.body_len)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ free(buf);                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req_in);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req_out); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                      /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                              /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_serialize_deserialize_response(void) { /* LCOV_EXCL_STOP */
  struct HttpResponse res_in, res_out;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_response_init(&res_in);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ }                         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res_in.status_code = 404; /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&res_in.headers, "Server", "mock");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ res_in.body_len = 9; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res_in.body =
      malloc(res_in.body_len); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(res_in.body, "Not Found",
                               res_in.body_len); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res_in, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT(buf != NULL); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT(len > 0);     /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response(buf, len, &res_out));

  /* LCOV_EXCL_START */ ASSERT_EQ(res_in.status_code,
                                  res_out.status_code); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(1,
                                  res_out.headers.count); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_STR_EQ(
      "Server", res_out.headers.headers[0].key); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_STR_EQ(
      "mock", res_out.headers.headers[0].value); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(res_in.body_len,
                                  res_out.body_len); /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      0, memcmp(res_in.body, res_out.body,
                /* LCOV_EXCL_START */ res_out.body_len)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ free(buf);                    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_response_free(&res_in);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_response_free(&res_out); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                       /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                               /* LCOV_EXCL_STOP */

#ifndef __EMSCRIPTEN__
/* LCOV_EXCL_START */ TEST test_process_spawn_wait(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpIpcPipe parent_to_child, child_to_parent;
  /* LCOV_EXCL_START */ struct AbstractHttpProcess *proc =
      NULL;                                         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int exit_code = 0;          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ printf("I AM EXECUTING\n"); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_pipe_init(&parent_to_child));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_pipe_init(&child_to_parent));

  /* LCOV_EXCL_START */ ASSERT_EQ(/* LCOV_EXCL_STOP */
                                  C_ABSTRACT_HTTP_SUCCESS,
                                  abstract_http_process_spawn(
                                      &proc, &parent_to_child,
                                      &child_to_parent));
  /* LCOV_EXCL_START */ ASSERT(proc != NULL); /* LCOV_EXCL_STOP */

  /* Write something to unblock it if it waits on stdin,
     but our mock binary currently exits with code 1 immediately if not matching
     proper binary. */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_process_wait_and_free(proc, &exit_code));

  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(
      &parent_to_child); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(
      &child_to_parent); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST
test_abstract_http_serialize_errors(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ char *buf = NULL;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;     /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  (void)res;
  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(NULL, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, NULL, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, NULL));

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(NULL, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, NULL, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, NULL));

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request(NULL, 10, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request("buf", 0, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request("buf", 10, NULL));

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response(NULL, 10, &res));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response("buf", 0, &res));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response("buf", 10, NULL));

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_abstract_http_process_hooks(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpProcessHooks hooks;
  /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        abstract_http_process_set_hooks(&hooks);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = abstract_http_process_set_hooks(NULL);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  }
  /* LCOV_EXCL_START */ /* Should do nothing */ /* LCOV_EXCL_STOP */

  /* Reset hooks so it doesn't affect other tests */
  {
    enum c_abstract_http_error rc_test =
        abstract_http_process_set_hooks(&hooks);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ }       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ TEST
test_abstract_http_ipc_short_rw(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe pipe = {
      0};                                  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ char buf[4] = {0}; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &pipe)); /* LCOV_EXCL_STOP */

  /* Write 2 bytes */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_write(pipe.write_handle, "ab", 2));

  /* Try to read 4 bytes. The pipe only has 2. It will either block or return 2!
     Wait, if it blocks, the test hangs!
     UNIX pipes block by default if less than requested is available and writer
     is still open. If we close the write handle before reading, it returns EOF
     (0) or 2! */
#if defined(_WIN32)
  CloseHandle((HANDLE)pipe.write_handle);
#else
  /* LCOV_EXCL_START */ close(
      (int)(size_t)pipe.write_handle); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ pipe.write_handle = NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO, /* LCOV_EXCL_STOP */
      abstract_http_ipc_read(pipe.read_handle, buf, 4));

  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&pipe); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                             /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                                     /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_abstract_http_ipc_rw(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe pipe = {
      0}; /* LCOV_EXCL_STOP */
  char buf[5];

  /* LCOV_EXCL_START */ memset(buf, 0, sizeof(buf)); /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &pipe)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_write(pipe.write_handle, "test", 4));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_read(pipe.read_handle, buf, 4));
  /* LCOV_EXCL_START */ ASSERT_STR_EQ("test", buf); /* LCOV_EXCL_STOP */

  /* Error cases */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO, /* LCOV_EXCL_STOP */
      abstract_http_ipc_write(pipe.read_handle, "fail", 4));

  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&pipe); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                             /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                                     /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_abstract_http_process_spawn_errors(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe rw = {
      0}; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpProcess *proc =
      NULL; /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(NULL); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_process_spawn(NULL, NULL, NULL));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_process_spawn(&proc, NULL, NULL));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_process_spawn(&proc, &rw, NULL));

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_process_wait_and_free(NULL, NULL));
  /* LCOV_EXCL_START */ if (abstract_http_ipc_pipe_init(&rw) ==
                            0)                              /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&rw); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                             /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                                     /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error abstract_http_process_test_waitpid_fail(void);
extern enum c_abstract_http_error abstract_http_process_test_waitpid_exit(void);
#endif

static int
/* LCOV_EXCL_START */
dummy_process_spawn(struct AbstractHttpProcess **proc, /* LCOV_EXCL_STOP */
                    struct AbstractHttpIpcPipe *p2c,
                    struct AbstractHttpIpcPipe *c2p) {
  /* LCOV_EXCL_START */ (void)proc; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)p2c;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)c2p;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;   /* LCOV_EXCL_STOP */
}
static int dummy_process_wait_and_free(
    /* LCOV_EXCL_START */ struct AbstractHttpProcess *proc, /* LCOV_EXCL_STOP */
    int *exit_code) {
  /* LCOV_EXCL_START */ (void)proc;      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)exit_code; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;        /* LCOV_EXCL_STOP */
}
static int
dummy_ipc_write(void *handle, const void *data,
                /* LCOV_EXCL_START */ size_t len) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)handle;               /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)data;                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)len;                  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;                   /* LCOV_EXCL_STOP */
}
static int
dummy_ipc_read(void *handle, void *data,
               /* LCOV_EXCL_START */ size_t len) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)handle;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)data;                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ (void)len;                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ return 0;                  /* LCOV_EXCL_STOP */
}

/* LCOV_EXCL_START */ TEST
test_process_hooks_coverage(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpProcessHooks hooks;
  /* LCOV_EXCL_START */ struct AbstractHttpProcess *proc =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe p2c = {0},
                                                   c2p = {
                                                       0}; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.spawn = dummy_process_spawn; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.wait_and_free =
      dummy_process_wait_and_free;                         /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.ipc_write = dummy_ipc_write; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ hooks.ipc_read = dummy_ipc_read;   /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        abstract_http_process_set_hooks(&hooks);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_process_spawn(&proc, &p2c, &c2p));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_process_wait_and_free(proc, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_write(
                NULL, NULL, 0)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_read(
                NULL, NULL, 0)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&hooks, 0, sizeof(hooks)); /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        abstract_http_process_set_hooks(&hooks);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ }       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_process_fallback_paths(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpProcess *proc =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe pipe = {
      0}; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe p2c = {0},
                                                   c2p = {
                                                       0}; /* LCOV_EXCL_STOP */

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                NULL)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &pipe)); /* LCOV_EXCL_STOP */

#if !defined(_WIN32)
  /* LCOV_EXCL_START */ g_mock_pipe_fail = 1; /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &pipe));                      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_pipe_fail = 0; /* LCOV_EXCL_STOP */
#endif

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_process_spawn(NULL, &p2c, &c2p));

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_process_spawn(&proc, &p2c, &c2p); /* LCOV_EXCL_STOP */
  {
    /* LCOV_EXCL_START */ int rc_test_tmp = rc;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

#if !defined(_WIN32)
  /* LCOV_EXCL_START */ g_mock_fork_fail = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_process_spawn(&proc, &p2c, &c2p); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                  rc);        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_fork_fail = 0; /* LCOV_EXCL_STOP */
#endif

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_process_wait_and_free(NULL, NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ rc =
      abstract_http_process_test_waitpid_fail(); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc);          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_process_test_waitpid_fail(); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ rc =
      abstract_http_process_test_waitpid_exit(); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc);          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ rc =
      abstract_http_process_test_waitpid_exit(); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
                                  rc); /* LCOV_EXCL_STOP */
#endif

  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&pipe); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();                             /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                                     /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_process_serialize_failures(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */
  (void)res;

  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

  /* test serialize C_ABSTRACT_HTTP_ERR_NOMEM */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_NOMEM, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  {
    int rc_test_tmp = abstract_http_ipc_serialize_response(
        /* LCOV_EXCL_START */ &res, &buf, &len); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d"); /* LCOV_EXCL_STOP */
  }

  /* test deserialize C_ABSTRACT_HTTP_ERR_INVAL */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request("", 0, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response("", 0, &res));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */

  {
    /* LCOV_EXCL_START */ char dummy[10] = {0}; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
        abstract_http_ipc_deserialize_request(dummy, 1, &req));
    /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
        abstract_http_ipc_deserialize_response(dummy, 1, &res));
    /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(NULL, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request(NULL, 0, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(NULL, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response(NULL, 0, &res));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_process_deserialization_edge_cases(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */
  (void)res;

  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url = NULL;        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request(buf, len, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ res.status_code = 200; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.body = NULL;       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.body_len = 0;      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_response(buf, len, &res));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url = "/";         /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&req.headers, "Key", "Value");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ req.url = NULL;               /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ http_request_free(&req);      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 2; /* LCOV_EXCL_STOP */
  {
    int rc_test_tmp = abstract_http_ipc_deserialize_request(
        /* LCOV_EXCL_START */ buf, len, &req);   /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d");     /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ free(buf); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ res.status_code = 200; /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&res.headers, "Key", "Value");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ http_response_free(&res);     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1; /* LCOV_EXCL_STOP */
  {
    int rc_test_tmp = abstract_http_ipc_deserialize_response(
        /* LCOV_EXCL_START */ buf, len, &res);   /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d");      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ free(buf); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_request_free(&req);      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.method = HTTP_GET;        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.url = "/";                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body = "data";            /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body_len = 4;             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1;
  /* 0:url, 1:body */ /* LCOV_EXCL_STOP */
  {
    int rc_test_tmp = abstract_http_ipc_deserialize_request(
        /* LCOV_EXCL_START */ buf, len, &req);   /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d");     /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ free(buf); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ http_response_free(&res);     /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.status_code = 200;        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.body = "data";            /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.body_len = 4;             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0;
  /* 0:body */ /* LCOV_EXCL_STOP */
  {
    int rc_test_tmp = abstract_http_ipc_deserialize_response(
        /* LCOV_EXCL_START */ buf, len, &res);   /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp,
                  /* LCOV_EXCL_START */ "%d");      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  }

  {
    /* LCOV_EXCL_START */ size_t fake_len = 1000; /* LCOV_EXCL_STOP */
    memcpy(buf + len - 4 - sizeof(size_t), &fake_len,
           /* LCOV_EXCL_START */ sizeof(size_t)); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
        abstract_http_ipc_deserialize_response(buf, len, &res));
    /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  }
  /* LCOV_EXCL_START */ free(buf); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST
test_process_more_edge_cases(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char buf[100] = {0}; /* LCOV_EXCL_STOP */
  (void)res;

  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

  /* 429: read_size > end */
  /* Try to read size from an empty buffer (0 bytes) */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request(buf, 0, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */

  /* 438: read_str -> read_size > end */
  /* Give it enough for method (4 bytes) but not enough for url size */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
      abstract_http_ipc_deserialize_request(buf, sizeof(int) + 1, &req));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */

  /* 444: p + len > end inside read_str */
  /* Give it valid method, size=100 for url, but buffer is small */
  {
    /* LCOV_EXCL_START */ size_t fake_size = 100; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ memcpy(buf + sizeof(int), &fake_size,
                                 sizeof(size_t)); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
        abstract_http_ipc_deserialize_request(
            buf, sizeof(int) + sizeof(size_t) + 1, &req));
  }

  /* 542: p + body_len > end */
  /* Give it valid request up to body_len */
  {
    /* LCOV_EXCL_START */ char *req_buf = NULL;  /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ size_t req_len = 0;    /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.url = "/";         /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.body = "data";     /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.body_len = 4;      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
        abstract_http_ipc_serialize_request(&req, &req_buf, &req_len));

    /* corrupt the body_len to be large */
    {
      /* LCOV_EXCL_START */ size_t fake_len = 1000; /* LCOV_EXCL_STOP */
      memcpy(req_buf + req_len - 4 - sizeof(size_t), &fake_len,
             /* LCOV_EXCL_START */ sizeof(size_t)); /* LCOV_EXCL_STOP */
      /* LCOV_EXCL_START */ ASSERT_EQ(
          C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
          abstract_http_ipc_deserialize_request(req_buf, req_len, &req));
      /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
    }
    /* LCOV_EXCL_START */ free(req_buf); /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_process_final_edge_cases(void) { /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ enum c_abstract_http_error rc =
      C_ABSTRACT_HTTP_SUCCESS; /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */
  (void)res;

  /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

  /* 349: waitpid WIFEXITED == false */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */
            abstract_http_process_test_waitpid_exit()); /* LCOV_EXCL_STOP
                                                         */
#endif

  /* 390: C_ABSTRACT_HTTP_ERR_IO from read */
  {
    char dummy_buf[10];
    /* read fails */
    g_mock_pipe_fail = 1;
    /* LCOV_EXCL_START */
    /* Wait, does my mock intercept read? */ /* LCOV_EXCL_STOP
                                              */
    /* If not intercepted, reading from an invalid handle or a pipe closed early
     * returns C_ABSTRACT_HTTP_ERR_IO? */
    /* No, I didn't mock `read`. Let's mock it or skip it or use a closed pipe?
     */
    /* A closed pipe returns 0. An invalid fd returns -1. */
    /* I can just pass an invalid handle like (void*)9999 to avoid valgrind
     * warnings on -1 */
    rc = abstract_http_ipc_read((void *)(size_t)9999, dummy_buf,
                                /* LCOV_EXCL_START */ 10); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ g_mock_pipe_fail = 0;            /* LCOV_EXCL_STOP */
#if defined(_WIN32)
    ASSERT(rc == C_ABSTRACT_HTTP_ERR_IO || rc == C_ABSTRACT_HTTP_ERR_INVAL);
#else
    /* LCOV_EXCL_START */ ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                                    rc); /* LCOV_EXCL_STOP */
#endif
  }

  /* 514: http_request_init failure */
  {
    /* LCOV_EXCL_START */ struct AbstractHttpProcess *my_proc =
        NULL; /* LCOV_EXCL_STOP */
    struct AbstractHttpIpcPipe p1, p2;
    {
      enum c_abstract_http_error rc_test = abstract_http_ipc_pipe_init(&p1);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test = abstract_http_ipc_pipe_init(&p2);
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ if (abstract_http_process_spawn(
                                  &my_proc, &p1, &p2) == /* LCOV_EXCL_STOP */
                              C_ABSTRACT_HTTP_SUCCESS) {
      /* LCOV_EXCL_START */ if (my_proc)                    /* LCOV_EXCL_STOP */
        /* LCOV_EXCL_START */ free(my_proc);                /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ }                                 /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&p1); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&p2); /* LCOV_EXCL_STOP */
  }
  {
    /* To fail http_request_init, maybe pass NULL? No, it handles NULL. But we
       already passed valid pointer. Does it allocate? `int
       http_request_init(struct HttpRequest *req)` -> `if (!req) return
       C_ABSTRACT_HTTP_ERR_INVAL; memset(req,0); return 0;` Wait, it CANNOT fail
       unless `req` is NULL! But `abstract_http_ipc_deserialize_request` already
       checks `if (!req) return C_ABSTRACT_HTTP_ERR_INVAL;` at line 510! So `if
       ((rc = http_request_init(req)) != 0)` is UNREACHABLE!
    */
  }

  /* 524: read_size(&hcount) failure */
  {
    /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.url = "/";         /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
        abstract_http_ipc_serialize_request(&req, &buf, &len));
    /* LCOV_EXCL_START */ req.url = NULL;               /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_request_free(&req);      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */

    /* Cut off right before hcount */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
        abstract_http_ipc_deserialize_request(
            buf, sizeof(int) + sizeof(size_t) + 1, &req));
    /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */
  }

  /* 538: read_size(&body_len) failure */
  {
    /* LCOV_EXCL_START */ req.method = HTTP_GET; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ req.url = "/";         /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test =
          http_headers_add(&req.headers, "A", "B");
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
        abstract_http_ipc_serialize_request(&req, &buf, &len));
    /* LCOV_EXCL_START */ req.url = NULL;               /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ http_request_free(&req);      /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ memset(&req, 0, sizeof(req)); /* LCOV_EXCL_STOP */

    /* Cut off right before body_len */
    ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL,
        /* LCOV_EXCL_START */
        abstract_http_ipc_deserialize_request(/* LCOV_EXCL_STOP
                                               */
                                              buf, len - sizeof(size_t), &req));
    /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */
  }

  /* Responses too! */
  /* 611: read_int status_code -> already covered or unreachable */
  /* 617: read_size hcount */
  {
    /* LCOV_EXCL_START */ res.status_code = 200; /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
        abstract_http_ipc_serialize_response(&res, &buf, &len));
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_ERR_INVAL, /* LCOV_EXCL_STOP */
        abstract_http_ipc_deserialize_response(buf, sizeof(int) - 1,
                                               &res)); /* status code cutoff */
    /* LCOV_EXCL_START */ ASSERT_EQ(                   /* LCOV_EXCL_STOP */
                                    C_ABSTRACT_HTTP_ERR_INVAL,
                                    abstract_http_ipc_deserialize_response(
                                        buf, sizeof(int) + sizeof(size_t) - 1,
                                        &res)); /* hcount cutoff */
    /* LCOV_EXCL_START */ free(buf);            /* LCOV_EXCL_STOP */
  }

  /* 631: read_size body_len */
  {
    /* LCOV_EXCL_START */ res.status_code = 200; /* LCOV_EXCL_STOP */
    {
      enum c_abstract_http_error rc_test =
          http_headers_add(&res.headers, "A", "B");
      if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
        printf("Error: %d\n", (int)rc_test);
      }
    /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ ASSERT_EQ(
        C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
        abstract_http_ipc_serialize_response(&res, &buf, &len));
    /* LCOV_EXCL_START */ http_response_free(&res);     /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */

    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              /* LCOV_EXCL_START */
              abstract_http_ipc_deserialize_response(/* LCOV_EXCL_STOP */
                                                     buf, len - sizeof(size_t),
                                                     &res));
    /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
    /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */
  }

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST
test_process_misc_coverage(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&req.headers, NULL, "value");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_response_init(&res);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&res.headers, NULL, "value");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_process_waitpid_fail_2(void) { /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ void *proc = malloc(1024);  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int exit_code = 0;          /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ printf("I AM EXECUTING\n"); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_waitpid_fail = 2; /* LCOV_EXCL_STOP */
  ASSERT_EQ(
      /* LCOV_EXCL_START */ 0,
      abstract_http_process_wait_and_free(/* LCOV_EXCL_STOP */
                                          (struct AbstractHttpProcess *)proc,
                                          &exit_code));
  /* LCOV_EXCL_START */ ASSERT_EQ(-1, exit_code); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_waitpid_fail = 0;  /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

#ifndef __EMSCRIPTEN__
/* LCOV_EXCL_START */ TEST test_process_wait_signal(void) { /* LCOV_EXCL_STOP */
#if !defined(_WIN32)
  /* LCOV_EXCL_START */ struct AbstractHttpProcess *proc =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ struct AbstractHttpIpcPipe p2c = {0},
                                                   c2p = {
                                                       0}; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int exit_code = 0;                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ printf("I AM EXECUTING\n");        /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &p2c)); /* LCOV_EXCL_STOP */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &c2p)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_process_spawn(&proc, &p2c, &c2p));

  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&p2c); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(&c2p); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_process_wait_and_free(proc, &exit_code));
#endif
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ TEST
test_process_null_header_keys(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&req.headers, NULL, "value");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.status_code = 200;        /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&res.headers, NULL, "value");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_process_serialize_null_key_value(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&req.headers, "k", "v");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(
      (void *)req.headers.headers[0].value); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.headers.headers[0].value =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.status_code = 200;        /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&res.headers, "k", "v");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(
      (void *)res.headers.headers[0].value); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.headers.headers[0].value =
      NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_process_serialize_null_key(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&req.headers, "k", "v");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(
      (void *)req.headers.headers[0].key);                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.headers.headers[0].key = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.status_code = 200;        /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test = http_headers_init(&res.headers);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  {
    enum c_abstract_http_error rc_test =
        http_headers_add(&res.headers, "k", "v");
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(
      (void *)res.headers.headers[0].key);                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.headers.headers[0].key = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

/* LCOV_EXCL_START */ TEST
test_process_serialize_body_len_no_body(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  /* LCOV_EXCL_START */ char *buf = NULL; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t len = 0;   /* LCOV_EXCL_STOP */

  {
    enum c_abstract_http_error rc_test = http_request_init(&req);
    if (rc_test != C_ABSTRACT_HTTP_SUCCESS) {
      printf("Error: %d\n", (int)rc_test);
    }
  /* LCOV_EXCL_START */ }                   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body_len = 100; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ req.body = NULL;    /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_request(&req, &buf, &len));
  /* LCOV_EXCL_START */ http_request_free(&req); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);               /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ memset(&res, 0, sizeof(res)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.status_code = 200;        /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.body_len = 100;           /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ res.body = NULL;              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS, /* LCOV_EXCL_STOP */
      abstract_http_ipc_serialize_response(&res, &buf, &len));
  /* LCOV_EXCL_START */ http_response_free(&res); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ free(buf);                /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern int g_mock_write_partial;
/* LCOV_EXCL_START */ TEST
test_process_write_partial(void) { /* LCOV_EXCL_STOP */
  struct AbstractHttpIpcPipe my_pipe;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            /* LCOV_EXCL_START */ abstract_http_ipc_pipe_init(
                &my_pipe)); /* LCOV_EXCL_STOP */
#ifndef _WIN32
  /* LCOV_EXCL_START */ g_mock_write_partial = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO, /* LCOV_EXCL_STOP */
      abstract_http_ipc_write(my_pipe.write_handle, "test", 4));
  /* LCOV_EXCL_START */ g_mock_write_partial = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_IO, /* LCOV_EXCL_STOP */
      abstract_http_ipc_write(my_pipe.write_handle, "t", 1));
#endif
  /* LCOV_EXCL_START */ abstract_http_ipc_pipe_free(
      &my_pipe);                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS(); /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }         /* LCOV_EXCL_STOP */
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/* LCOV_EXCL_START */ TEST
test_process_deserialize_oom(void) { /* LCOV_EXCL_STOP */
  struct HttpRequest req;
  struct HttpResponse res;
  enum c_abstract_http_error rc;
  char req_buf[100];
  char res_buf[100];
  char *p;
  /* LCOV_EXCL_START */ int method = 0;   /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ int status = 200; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t zero = 0;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ size_t one = 1;   /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ p = req_buf;                      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &method, sizeof(int));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(int);                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &zero, sizeof(size_t)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &one, sizeof(size_t));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &one, sizeof(size_t));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ *p++ = 'A';                       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &zero, sizeof(size_t)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &zero, sizeof(size_t)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ p = res_buf;                      /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &status, sizeof(int));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(int);                 /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &one, sizeof(size_t));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &one, sizeof(size_t));  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ *p++ = 'A';                       /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &zero, sizeof(size_t)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ memcpy(p, &zero, sizeof(size_t)); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ p += sizeof(size_t);              /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;            /* LCOV_EXCL_STOP */
  g_mock_alloc_count = 0;
  /* LCOV_EXCL_START */
  /* first parse_str alloc (key) fails */ /* LCOV_EXCL_STOP
                                           */
  rc = abstract_http_ipc_deserialize_request(
      req_buf, 45,
      /* LCOV_EXCL_START */ &req);             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc,
                                      "%d"); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1;
  /* headers alloc fails */ /* LCOV_EXCL_STOP */
  rc = abstract_http_ipc_deserialize_request(
      req_buf, 45,
      /* LCOV_EXCL_START */ &req);             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_INVAL, rc,
                                      "%d"); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 0; /* LCOV_EXCL_STOP */
  rc = abstract_http_ipc_deserialize_response(
      res_buf, 41,
      /* LCOV_EXCL_START */ &res);             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc,
                                      "%d"); /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ g_mock_alloc_fail = 1;  /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_count = 1; /* LCOV_EXCL_STOP */
  rc = abstract_http_ipc_deserialize_response(
      res_buf, 41,
      /* LCOV_EXCL_START */ &res);             /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ g_mock_alloc_fail = 0; /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_INVAL, rc,
                                      "%d"); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ PASS();              /* LCOV_EXCL_STOP */
/* LCOV_EXCL_START */ }                      /* LCOV_EXCL_STOP */
#endif

/* LCOV_EXCL_START */ SUITE(process_suite) { /* LCOV_EXCL_STOP */

  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_serialize_null_key); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_serialize_null_key_value); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_serialize_body_len_no_body); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_write_partial); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_null_header_keys); /* LCOV_EXCL_STOP */

#ifndef __EMSCRIPTEN__
  /* LCOV_EXCL_START */ RUN_TEST(test_process_wait_signal); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_waitpid_fail_2); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_misc_coverage); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_abstract_http_process_hooks); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_abstract_http_ipc_short_rw); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_abstract_http_ipc_rw); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_abstract_http_process_spawn_errors); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_abstract_http_serialize_errors);                /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(test_ipc_pipe_init_free); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_serialize_deserialize_request); /* LCOV_EXCL_STOP */
  /* LCOV_EXCL_START */ RUN_TEST(
      test_serialize_deserialize_response); /* LCOV_EXCL_STOP */
#ifndef __EMSCRIPTEN__
  /* LCOV_EXCL_START */ RUN_TEST(test_process_spawn_wait); /* LCOV_EXCL_STOP */
#endif
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_hooks_coverage); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_fallback_paths); /* LCOV_EXCL_STOP */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_serialize_failures); /* LCOV_EXCL_STOP */
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_deserialization_edge_cases); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_deserialize_oom); /* LCOV_EXCL_STOP */
#endif

#endif
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_more_edge_cases); /* LCOV_EXCL_STOP */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  /* LCOV_EXCL_START */ RUN_TEST(
      test_process_final_edge_cases); /* LCOV_EXCL_STOP */
#endif
/* LCOV_EXCL_START */ } /* LCOV_EXCL_STOP */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

/* LCOV_EXCL_BR_STOP */
