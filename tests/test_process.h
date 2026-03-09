#ifndef TEST_PROCESS_H
#define TEST_PROCESS_H

/* clang-format off */
#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>
#include <c_abstract_http/process.h>
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
     but our stub currently exits with code 1 immediately if not matching proper
     binary. */

  ASSERT_EQ(0, cdd_process_wait_and_free(proc, &exit_code));

  cdd_ipc_pipe_free(&parent_to_child);
  cdd_ipc_pipe_free(&child_to_parent);

  PASS();
}

SUITE(process_suite) {
  RUN_TEST(test_ipc_pipe_init_free);
  RUN_TEST(test_serialize_deserialize_request);
  RUN_TEST(test_serialize_deserialize_response);
  RUN_TEST(test_process_spawn_wait);
}

#endif