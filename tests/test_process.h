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

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/process.h>
#include "functions/parse/str.h"
/* clang-format on */

TEST test_ipc_pipe_init_free(void) {
  struct CddIpcPipe pipe;
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
  req_in.url =
      (c_cdd_strdup("http://example.com/api", &_ast_strdup_0), _ast_strdup_0);
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

  ASSERT_EQ(EINVAL, cdd_ipc_serialize_request(NULL, &buf, &len));
  ASSERT_EQ(EINVAL, cdd_ipc_serialize_request(&req, NULL, &len));
  ASSERT_EQ(EINVAL, cdd_ipc_serialize_request(&req, &buf, NULL));

  ASSERT_EQ(EINVAL, cdd_ipc_serialize_response(NULL, &buf, &len));
  ASSERT_EQ(EINVAL, cdd_ipc_serialize_response(&res, NULL, &len));
  ASSERT_EQ(EINVAL, cdd_ipc_serialize_response(&res, &buf, NULL));

  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(NULL, 10, &req));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request("buf", 0, &req));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request("buf", 10, NULL));

  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response(NULL, 10, &res));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response("buf", 0, &res));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response("buf", 10, NULL));

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

TEST test_cdd_ipc_rw(void) {
  struct CddIpcPipe pipe;
  char buf[5];

  memset(buf, 0, sizeof(buf));

  ASSERT_EQ(0, cdd_ipc_pipe_init(&pipe));

  ASSERT_EQ(0, cdd_ipc_write(pipe.write_handle, "test", 4));
  ASSERT_EQ(0, cdd_ipc_read(pipe.read_handle, buf, 4));
  ASSERT_STR_EQ("test", buf);

  /* Error cases */
  ASSERT_EQ(EIO, cdd_ipc_write(pipe.read_handle, "fail", 4));

  cdd_ipc_pipe_free(&pipe);
  PASS();
}

TEST test_cdd_process_spawn_errors(void) {
  struct CddIpcPipe rw;
  ASSERT_EQ(EINVAL, cdd_process_spawn(NULL, NULL, NULL));
  ASSERT_EQ(EINVAL, cdd_process_wait_and_free(NULL, NULL));
  if (cdd_ipc_pipe_init(&rw) == 0)
    cdd_ipc_pipe_free(&rw);
  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
extern void cdd_process_test_waitpid_fail(void);
extern void cdd_process_test_waitpid_exit(void);
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
  struct CddIpcPipe p2c, c2p;
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

TEST test_process_fallback_paths(void) {
  struct CddProcess *proc = NULL;
  struct CddIpcPipe pipe;
  struct CddIpcPipe p2c, c2p;
  int rc;

  ASSERT_EQ(EINVAL, cdd_ipc_pipe_init(NULL));
  ASSERT_EQ(0, cdd_ipc_pipe_init(&pipe));

#if !defined(_WIN32)
  g_mock_pipe_fail = 1;
  ASSERT_EQ(EIO, cdd_ipc_pipe_init(&pipe));
  g_mock_pipe_fail = 0;
#endif

  ASSERT_EQ(EINVAL, cdd_process_spawn(NULL, &p2c, &c2p));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  rc = cdd_process_spawn(&proc, &p2c, &c2p);
  ASSERT_EQ(ENOMEM, rc);
  g_mock_alloc_fail = 0;

#if !defined(_WIN32)
  g_mock_fork_fail = 1;
  rc = cdd_process_spawn(&proc, &p2c, &c2p);
  ASSERT_EQ(EIO, rc);
  g_mock_fork_fail = 0;
#endif

  ASSERT_EQ(EINVAL, cdd_process_wait_and_free(NULL, NULL));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  cdd_process_test_waitpid_fail();
  cdd_process_test_waitpid_exit();
#endif

  cdd_ipc_pipe_free(&pipe);
  PASS();
}

TEST test_process_serialize_failures(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  (void)res;
  char *buf = NULL;
  size_t len = 0;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  /* test serialize ENOMEM */
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(ENOMEM, cdd_ipc_serialize_request(&req, &buf, &len));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(ENOMEM, cdd_ipc_serialize_response(&res, &buf, &len));
  g_mock_alloc_fail = 0;

  /* test deserialize EINVAL */
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request("", 0, &req));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response("", 0, &res));

  {
    char dummy[10] = {0};
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(dummy, 1, &req));
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response(dummy, 1, &res));
  }

  ASSERT_EQ(EINVAL, cdd_ipc_serialize_request(NULL, &buf, &len));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(NULL, 0, &req));
  ASSERT_EQ(EINVAL, cdd_ipc_serialize_response(NULL, &buf, &len));
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response(NULL, 0, &res));

  PASS();
}

TEST test_process_deserialization_edge_cases(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  (void)res;
  char *buf = NULL;
  size_t len = 0;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  req.method = HTTP_GET;
  req.url = NULL;
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));
  ASSERT_EQ(0, cdd_ipc_deserialize_request(buf, len, &req));
  free(buf);

  res.status_code = 200;
  res.body = NULL;
  res.body_len = 0;
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  ASSERT_EQ(0, cdd_ipc_deserialize_response(buf, len, &res));
  free(buf);

  req.method = HTTP_GET;
  req.url = "/";
  http_headers_add(&req.headers, "Key", "Value");
  ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 2;
  ASSERT_EQ(ENOMEM, cdd_ipc_deserialize_request(buf, len, &req));
  g_mock_alloc_fail = 0;
  free(buf);

  res.status_code = 200;
  http_headers_add(&res.headers, "Key", "Value");
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 1;
  ASSERT_EQ(ENOMEM, cdd_ipc_deserialize_response(buf, len, &res));
  g_mock_alloc_fail = 0;
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
  ASSERT_EQ(ENOMEM, cdd_ipc_deserialize_request(buf, len, &req));
  g_mock_alloc_fail = 0;
  free(buf);

  http_response_free(&res);
  memset(&res, 0, sizeof(res));
  res.status_code = 200;
  res.body = "data";
  res.body_len = 4;
  ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0; /* 0:body */
  ASSERT_EQ(ENOMEM, cdd_ipc_deserialize_response(buf, len, &res));
  g_mock_alloc_fail = 0;

  {
    size_t fake_len = 1000;
    memcpy(buf + len - 4 - sizeof(size_t), &fake_len, sizeof(size_t));
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response(buf, len, &res));
  }
  free(buf);

  PASS();
}

TEST test_process_more_edge_cases(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  (void)res;
  char buf[100] = {0};

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  /* 429: read_size > end */
  /* Try to read size from an empty buffer (0 bytes) */
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(buf, 0, &req));

  /* 438: read_str -> read_size > end */
  /* Give it enough for method (4 bytes) but not enough for url size */
  ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(buf, sizeof(int) + 1, &req));

  /* 444: p + len > end inside read_str */
  /* Give it valid method, size=100 for url, but buffer is small */
  {
    size_t fake_size = 100;
    memcpy(buf + sizeof(int), &fake_size, sizeof(size_t));
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(
                          buf, sizeof(int) + sizeof(size_t) + 1, &req));
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
      ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(req_buf, req_len, &req));
    }
    free(req_buf);
  }

  PASS();
}

TEST test_process_final_edge_cases(void) {
  struct HttpRequest req;
  struct HttpResponse res;
  (void)res;
  char *buf = NULL;
  size_t len = 0;

  memset(&req, 0, sizeof(req));
  memset(&res, 0, sizeof(res));

  /* 349: waitpid WIFEXITED == false */
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  cdd_process_test_waitpid_exit();
#endif

  /* 390: EIO from read */
  {
    int rc;
    /* read fails */
    g_mock_pipe_fail = 1; /* Wait, does my mock intercept read? */
    /* If not intercepted, reading from an invalid handle or a pipe closed early
     * returns EIO? */
    /* No, I didn't mock `read`. Let's mock it or skip it or use a closed pipe?
     */
    /* A closed pipe returns 0. An invalid fd returns -1. */
    /* I can just pass an invalid handle like (void*)-1 */
    rc = cdd_ipc_read((void *)(size_t)-1, buf, 10);
#if defined(_WIN32)
    ASSERT(rc == EIO || rc == EINVAL);
#else
    ASSERT_EQ(EIO, rc);
#endif
  }

  /* 514: http_request_init failure */
  {
    /* To fail http_request_init, maybe pass NULL? No, it handles NULL. But we
       already passed valid pointer. Does it allocate? `int
       http_request_init(struct HttpRequest *req)` -> `if (!req) return EINVAL;
       memset(req,0); return 0;` Wait, it CANNOT fail unless `req` is NULL! But
       `cdd_ipc_deserialize_request` already checks `if (!req) return EINVAL;`
       at line 510! So `if ((rc = http_request_init(req)) != 0)` is UNREACHABLE!
    */
  }

  /* 524: read_size(&hcount) failure */
  {
    req.method = HTTP_GET;
    req.url = "/";
    ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));

    /* Cut off right before hcount */
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_request(
                          buf, sizeof(int) + sizeof(size_t) + 1, &req));
    free(buf);
  }

  /* 538: read_size(&body_len) failure */
  {
    req.method = HTTP_GET;
    req.url = "/";
    http_headers_add(&req.headers, "A", "B");
    ASSERT_EQ(0, cdd_ipc_serialize_request(&req, &buf, &len));

    /* Cut off right before body_len */
    ASSERT_EQ(EINVAL,
              cdd_ipc_deserialize_request(buf, len - sizeof(size_t), &req));
    free(buf);
  }

  /* Responses too! */
  /* 611: read_int status_code -> already covered or unreachable */
  /* 617: read_size hcount */
  {
    res.status_code = 200;
    ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response(
                          buf, sizeof(int) - 1, &res)); /* status code cutoff */
    ASSERT_EQ(EINVAL, cdd_ipc_deserialize_response(
                          buf, sizeof(int) + sizeof(size_t) - 1,
                          &res)); /* hcount cutoff */
    free(buf);
  }

  /* 631: read_size body_len */
  {
    res.status_code = 200;
    http_headers_add(&res.headers, "A", "B");
    ASSERT_EQ(0, cdd_ipc_serialize_response(&res, &buf, &len));
    ASSERT_EQ(EINVAL,
              cdd_ipc_deserialize_response(buf, len - sizeof(size_t), &res));
    free(buf);
  }

  PASS();
}

SUITE(process_suite) {
  RUN_TEST(test_cdd_process_hooks);
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
