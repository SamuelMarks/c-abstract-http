# Usage

This guide covers basic integration, standard HTTP requests, error handling, and memory lifecycle in `c-abstract-http`.

## 1. Initializing the Client

The core object you'll work with is the `HttpClient`. Every networking operation requires an initialized client to function correctly across multiple platforms.

```c
#include <c_abstract_http/c_abstract_http.h>
#include <stdio.h>

int main(void) {
    struct HttpClient client;
    int rc;

    /* 1. Initialize the client context */
    rc = http_client_init(&client);
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize client: %d\n", rc);
        return rc;
    }

    /* Configure base options if necessary */
    client.config.timeout_ms = 5000; /* 5 seconds */
    client.config.follow_redirects = 1;

    /* ... Perform requests here ... */

    /* 2. Free the client context to prevent memory leaks */
    http_client_free(&client);
    return 0;
}
```

## 2. Making a Basic GET Request

Requests and responses are distinct structs. You build an `HttpRequest`, dispatch it via `client.send()`, and receive an `HttpResponse`.

```c
#include <c_abstract_http/c_abstract_http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void perform_get(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    int rc;

    /* Initialize the Request object */
    rc = http_request_init(&req);
    if (rc != 0) return;

    /* Setup properties */
    req.method = HTTP_GET;
    req.url = "https://api.github.com/zen";

    /* Add a custom header */
    http_headers_add(&req.headers, "User-Agent", "c-abstract-http/1.0");

    /* Send the request (this will allocate the Response object on success) */
    rc = client->send(client->transport, &req, &res);
    
    if (rc == 0 && res != NULL) {
        printf("Status Code: %d\n", res->status_code);
        
        if (res->body && res->body_len > 0) {
            /* Safely print the string, bounded by length or null-terminated */
            printf("Response Body:\n%s\n", (char *)res->body);
        }
    } else {
        printf("Request failed with error code: %d\n", rc);
    }

    /* Cleanup */
    if (res) http_response_free(res);
    http_request_free(&req);
}
```

## 3. Reading Headers (Out Pointer Pattern)

Remember the primary architectural rule of this library: **All non-void functions return an integer exit code.** When you want to retrieve a value, pass a pointer to your variable via the `out` parameter.

```c
void extract_content_type(struct HttpResponse *res) {
    const char *out_val = NULL;
    int rc;

    /* We pass &out_val to capture the returned pointer */
    rc = http_headers_get(&res->headers, "Content-Type", &out_val);
    
    if (rc == 0) {
        printf("Content-Type is: %s\n", out_val);
    } else {
        printf("Content-Type header not found (Code: %d)\n", rc);
    }
}
```

## 4. Sending a POST Request with JSON Body

To send data, populate the `body` and `body_len` fields on the `HttpRequest`.

```c
void perform_post(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    const char *payload = "{\"name\": \"c-abstract-http\", \"version\": 1}";
    int rc;

    http_request_init(&req);

    req.method = HTTP_POST;
    req.url = "https://httpbin.org/post";
    
    /* Attach the body payload */
    req.body = (void *)payload;
    req.body_len = strlen(payload);

    /* Required Headers */
    http_headers_add(&req.headers, "Content-Type", "application/json");
    http_headers_add(&req.headers, "User-Agent", "c-abstract-http");

    /* Dispatch */
    rc = client->send(client->transport, &req, &res);
    
    if (rc == 0 && res != NULL) {
        printf("POST Success! Status: %d\n", res->status_code);
    }

    /* Cleanup */
    if (res) http_response_free(res);
    http_request_free(&req);
}
```

## 5. Working with Cookies

A shared `HttpCookieJar` can be assigned to the `HttpConfig` to automatically persist cookies across multiple requests.

```c
#include <c_abstract_http/c_abstract_http.h>
#include <stdio.h>

void test_cookies(void) {
    struct HttpClient client;
    struct HttpCookieJar jar;
    const char *cookie_val = NULL;
    int rc;

    http_client_init(&client);
    http_cookie_jar_init(&jar);

    /* Bind the jar to the client configuration */
    client.config.cookie_jar = &jar;

    /* Manually inject a cookie into the jar */
    http_cookie_jar_set(&jar, "session_id", "xyz123");

    /* Requests sent via `client` will now automatically append the session_id cookie! */
    /* And Set-Cookie response headers will automatically populate the `jar`. */

    /* Retrieve a cookie */
    rc = http_cookie_jar_get(&jar, "session_id", &cookie_val);
    if (rc == 0) {
        printf("Stored Cookie: %s\n", cookie_val);
    }

    /* Cleanup both */
    http_cookie_jar_free(&jar);
    http_client_free(&client);
}
```

## 6. Multipart Form Data (File Uploads)

If you need to simulate an HTML `<form>` upload, use the `http_request_add_part` function. The library natively flattens or forwards the parts directly to the OS backend.

```c
void upload_file(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    const char *img_data = "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A..."; /* Pretend it's a PNG */
    int rc;

    http_request_init(&req);
    req.method = HTTP_POST;
    req.url = "https://httpbin.org/post";

    /* Add a text field */
    http_request_add_part(&req, "username", NULL, NULL, "samuel", 6);

    /* Add a file part */
    http_request_add_part(&req, "avatar", "profile.png", "image/png", img_data, 8);

    /* Send */
    rc = client->send(client->transport, &req, &res);

    /* Cleanup */
    if (res) http_response_free(res);
    http_request_free(&req);
}
```