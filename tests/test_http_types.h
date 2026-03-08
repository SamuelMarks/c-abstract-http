#ifndef TEST_HTTP_TYPES_H
#define TEST_HTTP_TYPES_H

#include <errno.h>
#include <greatest.h>
#include <stdlib.h>
#include <string.h>

#include <c_abstract_http/http_types.h>

TEST test_multipart_lifecycle(void) {
  struct HttpRequest req;
  http_request_init(&req);

  ASSERT_EQ(0, req.parts.count);

  /* Add text part */
  ASSERT_EQ(0, http_request_add_part(&req, "field", NULL, NULL, "value", 5));
  ASSERT_EQ(1, req.parts.count);
  ASSERT_STR_EQ("field", req.parts.parts[0].name);
  ASSERT_EQ(NULL, req.parts.parts[0].filename);

  /* Add file part */
  ASSERT_EQ(0, http_request_add_part(&req, "file", "pic.jpg", "image/jpeg",
                                     "DATA", 4));
  ASSERT_EQ(2, req.parts.count);
  ASSERT_STR_EQ("pic.jpg", req.parts.parts[1].filename);

  http_request_free(&req);
  PASS();
}

TEST test_multipart_flatten(void) {
  struct HttpRequest req;
  char *content;

  http_request_init(&req);
  http_request_add_part(&req, "f1", NULL, NULL, "v1", 2);
  http_request_add_part(&req, "f2", "a.txt", "text/plain", "v2", 2);

  ASSERT_EQ(0, http_request_flatten_parts(&req));
  ASSERT(req.body != NULL);
  ASSERT(req.body_len > 0);

  content = (char *)req.body;
  /* Basic sanity check of content */
  ASSERT(strstr(content, "Content-Disposition: form-data; name=\"f1\""));
  ASSERT(strstr(
      content,
      "Content-Disposition: form-data; name=\"f2\"; filename=\"a.txt\""));
  ASSERT(strstr(content, "Content-Type: text/plain"));
  ASSERT(strstr(content, "v2"));         /* Data */
  ASSERT(strstr(content, "--cddbound")); /* Boundary */

  http_request_free(&req);
  PASS();
}

TEST test_multipart_part_headers(void) {
  struct HttpRequest req;
  char *content;

  http_request_init(&req);
  http_request_add_part(&req, "f1", NULL, NULL, "v1", 2);
  ASSERT_EQ(0, http_request_add_part_header_last(&req, "X-Trace", "abc"));
  ASSERT_EQ(0, http_request_add_part_header_last(&req, "X-Count", "2"));

  ASSERT_EQ(0, http_request_flatten_parts(&req));
  content = (char *)req.body;
  ASSERT(content != NULL);
  ASSERT(strstr(content, "X-Trace: abc"));
  ASSERT(strstr(content, "X-Count: 2"));

  http_request_free(&req);
  PASS();
}

TEST test_auth_basic_header(void) {
  struct HttpRequest req;
  int rc;

  http_request_init(&req);
  rc = http_request_set_auth_basic(&req, "dXNlcjpwYXNz");
  ASSERT_EQ(0, rc);
  ASSERT_EQ(1, req.headers.count);
  ASSERT_STR_EQ("Authorization", req.headers.headers[0].key);
  ASSERT_STR_EQ("Basic dXNlcjpwYXNz", req.headers.headers[0].value);

  http_request_free(&req);
  PASS();
}

TEST test_auth_basic_userpwd(void) {
  struct HttpRequest req;
  int rc;

  http_request_init(&req);
  /* "user:pass" base64 encodes to "dXNlcjpwYXNz" */
  rc = http_request_set_auth_basic_userpwd(&req, "user", "pass");
  ASSERT_EQ(0, rc);
  ASSERT_EQ(1, req.headers.count);
  ASSERT_STR_EQ("Authorization", req.headers.headers[0].key);
  ASSERT_STR_EQ("Basic dXNlcjpwYXNz", req.headers.headers[0].value);

  http_request_free(&req);
  PASS();
}

TEST test_http_config_init_redirects(void) {
  struct HttpConfig config;
  ASSERT_EQ(0, http_config_init(&config));
  ASSERT_EQ(30000, config.timeout_ms);
  ASSERT_EQ(0, config.connect_timeout_ms);
  ASSERT_EQ(0, config.read_timeout_ms);
  ASSERT_EQ(0, config.write_timeout_ms);
  ASSERT_EQ(1, config.verify_peer);
  ASSERT_EQ(1, config.verify_host);
  ASSERT_EQ(1, config.follow_redirects);
  ASSERT_EQ(NULL, config.proxy_url);
  ASSERT_EQ(NULL, config.proxy_username);
  ASSERT_EQ(NULL, config.proxy_password);
  ASSERT_EQ(NULL, config.cookie_jar);

  http_config_free(&config);
  PASS();
}

TEST test_http_request_init_defaults(void) {
  struct HttpRequest req;
  ASSERT_EQ(0, http_request_init(&req));
  ASSERT_EQ(NULL, req.url);
  ASSERT_EQ(HTTP_GET, req.method);
  ASSERT_EQ(NULL, req.body);
  ASSERT_EQ(0, req.body_len);
  ASSERT_EQ(NULL, req.on_chunk);
  ASSERT_EQ(NULL, req.on_chunk_user_data);
  ASSERT_EQ(NULL, req.read_chunk);
  ASSERT_EQ(NULL, req.read_chunk_user_data);
  ASSERT_EQ(0, req.expected_body_len);
  http_request_free(&req);
  PASS();
}

TEST test_http_headers_get_remove(void) {
  struct HttpHeaders headers;
  const char *out;
  ASSERT_EQ(0, http_headers_init(&headers));

  /* Setup */
  ASSERT_EQ(0, http_headers_add(&headers, "Content-Type", "application/json"));
  ASSERT_EQ(0, http_headers_add(&headers, "X-Custom", "123"));
  ASSERT_EQ(0, http_headers_add(&headers, "Set-Cookie", "sid=abc"));

  /* Test Get (Case-insensitive) */
  ASSERT_EQ(0, http_headers_get(&headers, "content-type", &out));
  ASSERT_STR_EQ("application/json", out);
  ASSERT_EQ(0, http_headers_get(&headers, "Content-Type", &out));
  ASSERT_STR_EQ("application/json", out);
  ASSERT_EQ(0, http_headers_get(&headers, "x-custom", &out));
  ASSERT_STR_EQ("123", out);
  ASSERT_EQ(ENOENT, http_headers_get(&headers, "Not-Found", &out));

  /* Test Remove (Middle element) */
  ASSERT_EQ(0, http_headers_remove(&headers, "x-custom"));
  ASSERT_EQ(ENOENT, http_headers_get(&headers, "x-custom", &out));
  ASSERT_EQ(2, headers.count);
  ASSERT_STR_EQ("Content-Type", headers.headers[0].key);
  ASSERT_STR_EQ("Set-Cookie", headers.headers[1].key); /* Shifted left */

  /* Test Remove (Not Found) */
  ASSERT_EQ(ENOENT, http_headers_remove(&headers, "Not-Found"));
  ASSERT_EQ(2, headers.count);

  /* Test Remove (First element) */
  ASSERT_EQ(0, http_headers_remove(&headers, "content-type"));
  ASSERT_EQ(ENOENT, http_headers_get(&headers, "content-type", &out));
  ASSERT_EQ(1, headers.count);
  ASSERT_STR_EQ("Set-Cookie", headers.headers[0].key);

  /* Test Remove (Last element) */
  ASSERT_EQ(0, http_headers_remove(&headers, "set-cookie"));
  ASSERT_EQ(0, headers.count);

  /* Test Multiple Identical Keys (Remove all) */
  ASSERT_EQ(0, http_headers_add(&headers, "X-Dup", "A"));
  ASSERT_EQ(0, http_headers_add(&headers, "X-Dup", "B"));
  ASSERT_EQ(0, http_headers_add(&headers, "Other", "C"));
  ASSERT_EQ(3, headers.count);
  ASSERT_EQ(0, http_headers_remove(&headers, "x-dup"));
  ASSERT_EQ(1, headers.count);
  ASSERT_STR_EQ("Other", headers.headers[0].key);

  http_headers_free(&headers);
  PASS();
}

TEST test_http_cookie_jar(void) {
  struct HttpCookieJar jar;
  const char *out;

  ASSERT_EQ(0, http_cookie_jar_init(&jar));
  ASSERT_EQ(0, jar.count);
  ASSERT_EQ(NULL, jar.cookies);

  /* Set new cookie */
  ASSERT_EQ(0, http_cookie_jar_set(&jar, "session", "abc"));
  ASSERT_EQ(1, jar.count);
  ASSERT_EQ(0, http_cookie_jar_get(&jar, "session", &out));
  ASSERT_STR_EQ("abc", out);

  /* Update existing cookie */
  ASSERT_EQ(0, http_cookie_jar_set(&jar, "session", "def"));
  ASSERT_EQ(1, jar.count);
  ASSERT_EQ(0, http_cookie_jar_get(&jar, "session", &out));
  ASSERT_STR_EQ("def", out);

  /* Add another cookie */
  ASSERT_EQ(0, http_cookie_jar_set(&jar, "theme", "dark"));
  ASSERT_EQ(2, jar.count);
  ASSERT_EQ(0, http_cookie_jar_get(&jar, "theme", &out));
  ASSERT_STR_EQ("dark", out);
  ASSERT_EQ(0, http_cookie_jar_get(&jar, "session", &out));
  ASSERT_STR_EQ("def", out);

  /* Unknown cookie */
  ASSERT_EQ(ENOENT, http_cookie_jar_get(&jar, "unknown", &out));

  http_cookie_jar_free(&jar);
  ASSERT_EQ(0, jar.count);
  ASSERT_EQ(NULL, jar.cookies);

  PASS();
}

SUITE(http_types_suite) {
  RUN_TEST(test_multipart_lifecycle);
  RUN_TEST(test_multipart_flatten);
  RUN_TEST(test_multipart_part_headers);
  RUN_TEST(test_auth_basic_header);
  RUN_TEST(test_auth_basic_userpwd);
  RUN_TEST(test_http_config_init_redirects);
  RUN_TEST(test_http_request_init_defaults);
  RUN_TEST(test_http_headers_get_remove);
  RUN_TEST(test_http_cookie_jar);
}

#endif