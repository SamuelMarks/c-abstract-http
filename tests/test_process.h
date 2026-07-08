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

TEST test_ipc_pipe_init_free(void) {
  struct CddIpcPipe pipe = {0};
  ASSERT_EQ(0, cdd_ipc_pipe_init(&pipe));
  ASSERT(pipe.read_handle != NULL);
  ASSERT(pipe.write_handle != NULL);
  cdd_ipc_pipe_free(&pipe);
  ASSERT(pipe.read_handle == NULL);
  ASSERT(pipe.write_handle == NULL);
  PASS();
}

TEST test_serialize_deserialize_request(void) {
  struct HttpRequest req_in, req_out;
  char *buf = NULL;
  size_t len = 0;
  char *_ast_strdup_0 = NULL;

  http_request_init(&req_in);
  req_in.method = HTTP_POST;
  req_in.url = (c_abstract_http_mock_cdd_strdup("http://example.com/api",
                                                &_ast_strdup_0),
                _ast_strdup_0);
  http_headers_add(&req_in.headers, "Content-Type", "application/json");
  http_headers_add(&req_in.headers, "X-Custom", "test_val");

  req_in.body_len = 13;
  req_in.body = malloc(req_in.body_len);
  memcpy(req_in.body, "Hello, World!", req_in.body_len);

  ASSERT_EQ(0, cdd_ipc_serialize_request(&req_in, &buf, &len));
  ASSERT(buf != NULL);
  ASSERT(len > 0);

  ASSERT_EQ(0, cdd_ipc_deserialize_request(buf, len, &req_out));

  ASSERT_EQ(req_in.method, req_out.method);
  ASSERT_STR_EQ(req_in.url, req_out.url);
  ASSERT_EQ(2, req_out.headers.count);
  ASSERT_STR_EQ("Content-Type", req_out.headers.headers[0].key);
  ASSERT_STR_EQ("application/json", req_out.headers.headers[0].value);
  ASSERT_STR_EQ("X-Custom", req_out.headers.headers[1].key);
  ASSERT_STR_EQ("test_val", req_out.headers.headers[1].value);

  ASSERT_EQ(req_in.body_len, req_out.body_len);
  ASSERT_EQ(0, memcmp(req_in.body, req_out.body, req_out.body_len));

  free(buf);
  http_request_free(&req_in);
  http_request_free(&req_out);
  PASS();
}

TEST test_serialize_deserialize_response(void) {
  struct HttpResponse res_in, res_out;
  char *buf = NULL;
  size_t len = 0;

  http_response_init(&res_in);
  res_in.status_code = 404;
  http_headers_add(&res_in.headers, "Server", "mock");

  res_in.body_len = 9;
  res_in.body = malloc(res_in.body_len);
  memcpy(res_in.body, "Not Found", res_in.body_len);

  ASSERT_EQ(0, cdd_ipc_serialize_response(&res_in, &buf, &len));
  ASSERT(buf != NULL);
  ASSERT(len > 0);

  ASSERT_EQ(0, cdd_ipc_deserialize_response(buf, len, &res_out));

  ASSERT_EQ(res_in.status_code, res_out.status_code);
  ASSERT_EQ(1, res_out.headers.count);
  ASSERT_STR_EQ("Server", res_out.headers.headers[0].key);
  ASSERT_STR_EQ("mock", res_out.headers.headers[0].value);

  ASSERT_EQ(res_in.body_len, res_out.body_len);
  ASSERT_EQ(0, memcmp(res_in.body, res_out.body, res_out.body_len));

  free(buf);
  http_response_free(&res_in);
  http_response_free(&res_out);
  PASS();
}

TEST test_process_spawn_wait(void) {
  struct CddIpcPipe parent_to_child, child_to_parent;
  struct CddProcess *proc = NULL;
  int exit_code = 0;
  printf("I AM EXECUTING\n");

  ASSERT_EQ(0, cdd_ipc_pipe_init(&parent_to_child));
  ASSERT_EQ(0, cdd_ipc_pipe_init(&child_to_parent));

  ASSERT_EQ(0, cdd_process_spawn(&proc, &parent_to_child, &child_to_parent));
  ASSERT(proc != NULL);

  /* Write something to unblock it if it waits on stdin,
     but our mock binary currently exits with code 1 immediately if not matching
     proper binary. */

  ASSERT_EQ(0, cdd_process_wait_and_free(proc, &exit_code));

  cdd_ipc_pipe_free(&parent_to_child);
  cdd_ipc_pipe_free(&child_to_parent);

  PASS();
}

TEST test_cdd_serialize_errors(void) {
  char *buf = NULL;
  size_t len = 0;
  struct HttpRequest req;
  struct HttpResponse res;
  (void)res;
  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_request(NULL, &buf, &len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_request(&req, NULL, &len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_request(&req, &buf, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_response(NULL, &buf, &len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_response(&res, NULL, &len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_response(&res, &buf, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request(NULL, 10, &req));
  http_request_free(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request("buf", 0, &req));
  http_request_free(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request("buf", 10, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_response(NULL, 10, &res));
  http_response_free(&res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_response("buf", 0, &res));
  http_response_free(&res);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_response("buf", 10, NULL));

  PASS();
}

TEST test_cdd_process_hooks(void) {
  struct CddProcessHooks hooks;
  memset(&hooks, 0, sizeof(hooks));
  cdd_process_set_hooks(&hooks);
  cdd_process_set_hooks(NULL); /* Should do nothing */

  /* Reset hooks so it doesn't affect other tests */
  cdd_process_set_hooks(&hooks);
  PASS();
}

TEST test_cdd_ipc_short_rw(void) {
  struct CddIpcPipe pipe = {0};
  char buf[4] = {0};
  ASSERT_EQ(0, cdd_ipc_pipe_init(&pipe));

  /* Write 2 bytes */
  ASSERT_EQ(0, cdd_ipc_write(pipe.write_handle, "ab", 2));

  /* Try to read 4 bytes. The pipe only has 2. It will either block or return 2!
     Wait, if it blocks, the test hangs!
     UNIX pipes block by default if less than requested is available and writer
     is still open. If we close the write handle before reading, it returns EOF
     (0) or 2! */
#if defined(_WIN32)
  CloseHandle((HANDLE)pipe.write_handle);
#else
  close((int)(size_t)pipe.write_handle);
#endif
  pipe.write_handle = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, cdd_ipc_read(pipe.read_handle, buf, 4));

  cdd_ipc_pipe_free(&pipe);
  PASS();
}

TEST test_cdd_ipc_rw(void) {
  struct CddIpcPipe pipe = {0};
  char buf[5];

  memset(buf, 0, sizeof(buf));

  ASSERT_EQ(0, cdd_ipc_pipe_init(&pipe));

  ASSERT_EQ(0, cdd_ipc_write(pipe.write_handle, "test", 4));
  ASSERT_EQ(0, cdd_ipc_read(pipe.read_handle, buf, 4));
  ASSERT_STR_EQ("test", buf);

  /* Error cases */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, cdd_ipc_write(pipe.read_handle, "fail", 4));

  cdd_ipc_pipe_free(&pipe);
  PASS();
}

TEST test_cdd_process_spawn_errors(void) {
  struct CddIpcPipe rw = {0};
  struct CddProcess *proc = NULL;

  cdd_ipc_pipe_free(NULL);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_process_spawn(NULL, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_process_spawn(&proc, NULL, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_process_spawn(&proc, &rw, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_process_wait_and_free(NULL, NULL));
  if (cdd_ipc_pipe_init(&rw) == 0)
    cdd_ipc_pipe_free(&rw);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern enum c_abstract_http_error cdd_process_test_waitpid_fail(void);
extern enum c_abstract_http_error cdd_process_test_waitpid_exit(void);
#endif

static int dummy_process_spawn(struct CddProcess **proc, struct CddIpcPipe *p2c,
                               struct CddIpcPipe *c2p) {
  (void)proc;
  (void)p2c;
  (void)c2p;
  return 0;
}
static int dummy_process_wait_and_free(struct CddProcess *proc,
                                       int *exit_code) {
  (void)proc;
  (void)exit_code;
  return 0;
}
static int dummy_ipc_write(void *handle, const void *data, size_t len) {
  (void)handle;
  (void)data;
  (void)len;
  return 0;
}
static int dummy_ipc_read(void *handle, void *data, size_t len) {
  (void)handle;
  (void)data;
  (void)len;
  return 0;
}

TEST test_process_hooks_coverage(void) {
  struct CddProcessHooks hooks;
  struct CddProcess *proc = NULL;
  struct CddIpcPipe p2c = {0}, c2p = {0};
  hooks.spawn = dummy_process_spawn;
  hooks.wait_and_free = dummy_process_wait_and_free;
  hooks.ipc_write = dummy_ipc_write;
  hooks.ipc_read = dummy_ipc_read;
  cdd_process_set_hooks(&hooks);

  ASSERT_EQ(0, cdd_process_spawn(&proc, &p2c, &c2p));
  ASSERT_EQ(0, cdd_process_wait_and_free(proc, NULL));
  ASSERT_EQ(0, cdd_ipc_write(NULL, NULL, 0));
  ASSERT_EQ(0, cdd_ipc_read(NULL, NULL, 0));

  memset(&hooks, 0, sizeof(hooks));
  cdd_process_set_hooks(&hooks);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_process_fallback_paths(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct CddProcess *proc = NULL;
  struct CddIpcPipe pipe = {0};
  struct CddIpcPipe p2c = {0}, c2p = {0};

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_ipc_pipe_init(NULL));
  ASSERT_EQ(0, cdd_ipc_pipe_init(&pipe));

#if !defined(_WIN32)
  g_mock_pipe_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, cdd_ipc_pipe_init(&pipe));
  g_mock_pipe_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_process_spawn(NULL, &p2c, &c2p));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_process_spawn(&proc, &p2c, &c2p);
  {
    int rc_test_tmp = rc;
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

#if !defined(_WIN32)
  g_mock_fork_fail = 1;
  rc = cdd_process_spawn(&proc, &p2c, &c2p);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
  g_mock_fork_fail = 0;
#endif

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, cdd_process_wait_and_free(NULL, NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  rc = cdd_process_test_waitpid_fail();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
  rc = cdd_process_test_waitpid_exit();
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, rc);
#endif

  cdd_ipc_pipe_free(&pipe);
  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_process_serialize_failures(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;
  (void)res;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  /* test serialize C_ABSTRACT_HTTP_ERR_NOMEM */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            cdd_ipc_serialize_request(&req, &buf, &len));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  {
    int rc_test_tmp = cdd_ipc_serialize_response(&res, &buf, &len);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
  }

  /* test deserialize C_ABSTRACT_HTTP_ERR_INVAL */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request("", 0, &req));
  http_request_free(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_response("", 0, &res));
  http_response_free(&res);

  {
    char dummy[10] = {0};
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_request(dummy, 1, &req));
    http_request_free(&req);
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_response(dummy, 1, &res));
    http_response_free(&res);
  }

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_request(NULL, &buf, &len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request(NULL, 0, &req));
  http_request_free(&req);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_serialize_response(NULL, &buf, &len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_response(NULL, 0, &res));
  http_response_free(&res);

  PASS();
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_process_deserialization_edge_cases(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;
  (void)res;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  req.method = HTTP_GET;
  req.url = NULL;
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  ASSERT_EQ(0, cdd_ipc_deserialize_request(buf, len, &req));
  http_request_free(&req);
  free(buf);

  res.status_code = 200;
  res.body = NULL;
  res.body_len = 0;
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  ASSERT_EQ(0, cdd_ipc_deserialize_response(buf, len, &res));
  http_response_free(&res);
  free(buf);

  req.method = HTTP_GET;
  req.url = "/";
  http_headers_add(&req.headers, "Key", "Value");
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  req.url = NULL;
  http_request_free(&req);
  memset(&req, 0, sizeof(req));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2;
  {
    int rc_test_tmp = cdd_ipc_deserialize_request(buf, len, &req);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
    http_request_free(&req);
  }
  free(buf);

  res.status_code = 200;
  http_headers_add(&res.headers, "Key", "Value");
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  http_response_free(&res);
  memset(&res, 0, sizeof(res));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  {
    int rc_test_tmp = cdd_ipc_deserialize_response(buf, len, &res);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
    http_response_free(&res);
  }
  free(buf);

  http_request_free(&req);
  memset(&req, 0, sizeof(req));
  req.method = HTTP_GET;
  req.url = "/";
  req.body = "data";
  req.body_len = 4;
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1; /* 0:url, 1:body */
  {
    int rc_test_tmp = cdd_ipc_deserialize_request(buf, len, &req);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
    http_request_free(&req);
  }
  free(buf);

  http_response_free(&res);
  memset(&res, 0, sizeof(res));
  res.status_code = 200;
  res.body = "data";
  res.body_len = 4;
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0; /* 0:body */
  {
    int rc_test_tmp = cdd_ipc_deserialize_response(buf, len, &res);
    g_mock_alloc_fail = 0;
    ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc_test_tmp, "%d");
    http_response_free(&res);
  }

  {
    size_t fake_len = 1000;
    memcpy(buf + len - 4 - sizeof(size_t), &fake_len, sizeof(size_t));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_response(buf, len, &res));
    http_response_free(&res);
  }
  free(buf);

  PASS();
}
#endif

TEST test_process_more_edge_cases(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char buf[100] = {0};
  (void)res;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  /* 429: read_size > end */
  /* Try to read size from an empty buffer (0 bytes) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request(buf, 0, &req));
  http_request_free(&req);

  /* 438: read_str -> read_size > end */
  /* Give it enough for method (4 bytes) but not enough for url size */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            cdd_ipc_deserialize_request(buf, sizeof(int) + 1, &req));
  http_request_free(&req);

  /* 444: p + len > end inside read_str */
  /* Give it valid method, size=100 for url, but buffer is small */
  {
    size_t fake_size = 100;
    memcpy(buf + sizeof(int), &fake_size, sizeof(size_t));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_request(buf, sizeof(int) + sizeof(size_t) + 1,
                                          &req));
  }

  /* 542: p + body_len > end */
  /* Give it valid request up to body_len */
  {
    char *req_buf = NULL;
    size_t req_len = 0;
    req.method = HTTP_GET;
    req.url = "/";
    req.body = "data";
    req.body_len = 4;
    ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &req_buf, &req_len));

    /* corrupt the body_len to be large */
    {
      size_t fake_len = 1000;
      memcpy(req_buf + req_len - 4 - sizeof(size_t), &fake_len, sizeof(size_t));
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
                cdd_ipc_deserialize_request(req_buf, req_len, &req));
      http_request_free(&req);
    }
    free(req_buf);
  }

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_process_final_edge_cases(void) {
  enum c_abstract_http_error rc = C_ABSTRACT_HTTP_SUCCESS;
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;
  (void)res;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  /* 349: waitpid WIFEXITED == false */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  cdd_process_test_waitpid_exit();
#endif

  /* 390: C_ABSTRACT_HTTP_ERR_IO from read */
  {
    char dummy_buf[10];
    /* read fails */
    g_mock_pipe_fail = 1; /* Wait, does my mock intercept read? */
    /* If not intercepted, reading from an invalid handle or a pipe closed early
     * returns C_ABSTRACT_HTTP_ERR_IO? */
    /* No, I didn't mock `read`. Let's mock it or skip it or use a closed pipe?
     */
    /* A closed pipe returns 0. An invalid fd returns -1. */
    /* I can just pass an invalid handle like (void*)9999 to avoid valgrind
     * warnings on -1 */
    rc = cdd_ipc_read((void *)(size_t)9999, dummy_buf, 10);
#if defined(_WIN32)
    ASSERT(rc == C_ABSTRACT_HTTP_ERR_IO || rc == C_ABSTRACT_HTTP_ERR_INVAL);
#else
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, rc);
#endif
  }

  /* 514: http_request_init failure */
  {
    /* To fail http_request_init, maybe pass NULL? No, it handles NULL. But we
       already passed valid pointer. Does it allocate? `int
       http_request_init(struct HttpRequest *req)` -> `if (!req) return
       C_ABSTRACT_HTTP_ERR_INVAL; memset(req,0); return 0;` Wait, it CANNOT fail
       unless `req` is NULL! But `cdd_ipc_deserialize_request` already checks
       `if (!req) return C_ABSTRACT_HTTP_ERR_INVAL;` at line 510! So `if ((rc =
       http_request_init(req)) != 0)` is UNREACHABLE!
    */
  }

  /* 524: read_size(&hcount) failure */
  {
    req.method = HTTP_GET;
    req.url = "/";
    ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
    req.url = NULL;
    http_request_free(&req);
    memset(&req, 0, sizeof(req));

    /* Cut off right before hcount */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_request(buf, sizeof(int) + sizeof(size_t) + 1,
                                          &req));
    http_request_free(&req);
    free(buf);
  }

  /* 538: read_size(&body_len) failure */
  {
    req.method = HTTP_GET;
    req.url = "/";
    http_headers_add(&req.headers, "A", "B");
    ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
    req.url = NULL;
    http_request_free(&req);
    memset(&req, 0, sizeof(req));

    /* Cut off right before body_len */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_request(buf, len - sizeof(size_t), &req));
    http_request_free(&req);
    free(buf);
  }

  /* Responses too! */
  /* 611: read_int status_code -> already covered or unreachable */
  /* 617: read_size hcount */
  {
    res.status_code = 200;
    ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_response(buf, sizeof(int) - 1,
                                           &res)); /* status code cutoff */
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_response(buf,
                                           sizeof(int) + sizeof(size_t) - 1,
                                           &res)); /* hcount cutoff */
    free(buf);
  }

  /* 631: read_size body_len */
  {
    res.status_code = 200;
    http_headers_add(&res.headers, "A", "B");
    ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
    http_response_free(&res);
    memset(&res, 0, sizeof(res));

    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              cdd_ipc_deserialize_response(buf, len - sizeof(size_t), &res));
    http_response_free(&res);
    free(buf);
  }

  PASS();
}
#endif

TEST test_process_misc_coverage(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;

  http_request_init(&req);
  http_headers_add(&req.headers, NULL, "value");
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  http_request_free(&req);
  free(buf);

  http_response_init(&res);
  http_headers_add(&res.headers, NULL, "value");
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  http_response_free(&res);
  free(buf);

  PASS();
}

TEST test_process_waitpid_fail_2(void) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  void *proc = malloc(1024);
  int exit_code = 0;
  printf("I AM EXECUTING\n");

  g_mock_waitpid_fail = 2;
  ASSERT_EQ(0,
            cdd_process_wait_and_free((struct CddProcess *)proc, &exit_code));
  ASSERT_EQ(-1, exit_code);
  g_mock_waitpid_fail = 0;
#endif
  PASS();
}

TEST test_process_wait_signal(void) {
#if !defined(_WIN32)
  struct CddProcess *proc = NULL;
  struct CddIpcPipe p2c = {0}, c2p = {0};
  int exit_code = 0;
  printf("I AM EXECUTING\n");
  ASSERT_EQ(0, cdd_ipc_pipe_init(&p2c));
  ASSERT_EQ(0, cdd_ipc_pipe_init(&c2p));
  ASSERT_EQ(0, cdd_process_spawn(&proc, &p2c, &c2p));

  cdd_ipc_pipe_free(&p2c);
  cdd_ipc_pipe_free(&c2p);
  ASSERT_EQ(0, cdd_process_wait_and_free(proc, &exit_code));
#endif
  PASS();
}

TEST test_process_null_header_keys(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;

  http_request_init(&req);
  http_headers_add(&req.headers, NULL, "value");
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  http_request_free(&req);
  free(buf);

  memset(&res, 0, sizeof(res));
  res.status_code = 200;
  http_headers_add(&res.headers, NULL, "value");
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  http_response_free(&res);
  free(buf);

  PASS();
}

TEST test_process_serialize_null_key_value(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;

  http_request_init(&req);
  http_headers_add(&req.headers, "k", NULL);
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  http_request_free(&req);
  free(buf);

  memset(&res, 0, sizeof(res));
  res.status_code = 200;
  http_headers_add(&res.headers, "k", NULL);
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  http_response_free(&res);
  free(buf);

  PASS();
}

TEST test_process_serialize_null_key(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  char *buf = NULL;
  size_t len = 0;

  http_request_init(&req);
  http_headers_add(&req.headers, "k", "v");
  free((void *)req.headers.headers[0].key);
  req.headers.headers[0].key = NULL;
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  http_request_free(&req);
  free(buf);

  memset(&res, 0, sizeof(res));
  res.status_code = 200;
  http_headers_init(&res.headers);
  http_headers_add(&res.headers, "k", "v");
  free((void *)res.headers.headers[0].key);
  res.headers.headers[0].key = NULL;
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  http_response_free(&res);
  free(buf);

  PASS();
}

SUITE(process_suite) {

  RUN_TEST(test_process_serialize_null_key);

  RUN_TEST(test_process_serialize_null_key_value);

  RUN_TEST(test_process_null_header_keys);

  RUN_TEST(test_process_wait_signal);
  RUN_TEST(test_process_waitpid_fail_2);
  RUN_TEST(test_process_misc_coverage);
  RUN_TEST(test_cdd_process_hooks);
  RUN_TEST(test_cdd_ipc_short_rw);
  RUN_TEST(test_cdd_ipc_rw);
  RUN_TEST(test_cdd_process_spawn_errors);
  RUN_TEST(test_cdd_serialize_errors);
  RUN_TEST(test_ipc_pipe_init_free);
  RUN_TEST(test_serialize_deserialize_request);
  RUN_TEST(test_serialize_deserialize_response);
  RUN_TEST(test_process_spawn_wait);
  RUN_TEST(test_process_hooks_coverage);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_process_fallback_paths);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_process_serialize_failures);
#endif
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_process_deserialization_edge_cases);
#endif
  RUN_TEST(test_process_more_edge_cases);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_process_final_edge_cases);
#endif
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
