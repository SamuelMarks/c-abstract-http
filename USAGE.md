# Usage

This guide covers basic integration, standard HTTP requests, error handling, OAuth2 support, and memory lifecycle in `c-abstract-http`.

## 1. Initializing the Client

```c
#include <c_abstract_http/c_abstract_http.h>
#include <stdio.h>

int main(void) {
    struct HttpClient client;
    int rc = http_client_init(&client);
    if (rc != 0) return rc;

    client.config.timeout_ms = 5000;
    client.config.follow_redirects = 1;

    http_client_free(&client);
    return 0;
}
```

## 2. OAuth 2.0 Client Support

`c-abstract-http` natively handles OAuth 2.0 flows, making authentication against modern APIs simple and secure.

### Password Grant

```c
void get_token(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    
    http_request_init(&req);
    
    http_request_init_oauth2_password_grant(&req, 
        "https://auth.server/token",
        "user@example.com",
        "super_secret_password",
        "client_id_123",
        NULL, 
        "read write"
    );

    client->send(client->transport, &req, &res);

    if (res) {
        printf("Response: %s\n", (char*)res->body);
        http_response_free(res);
    }
    http_request_free(&req);
}
```

### Client Credentials Grant

```c
void service_token(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;

    http_request_init(&req);
    http_request_init_oauth2_client_credentials_grant(&req, 
        "https://auth.server/token",
        "my_client_id",
        "my_client_secret",
        "admin_scope"
    );

    client->send(client->transport, &req, &res);
    
    if (res) http_response_free(res);
    http_request_free(&req);
}
```

### Device Authorization Flow

For smart TVs and headless CLIs:

```c
void device_flow(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    http_request_init(&req);

    /* 1. Get the device code */
    http_request_init_oauth2_device_authorization_request(&req, 
        "https://auth.server/device",
        "my_client_id",
        NULL
    );
    client->send(client->transport, &req, &res);
    if (res) http_response_free(res);
    http_request_free(&req);

    /* 2. Poll for the access token using the device code */
    http_request_init(&req);
    http_request_init_oauth2_device_access_token_request(&req, 
        "https://auth.server/token",
        "my_client_id",
        "device_code_123"
    );
    client->send(client->transport, &req, &res);
    if (res) http_response_free(res);
    http_request_free(&req);
}
```

### Localhost Intercept Server

To handle the OAuth 2.0 loopback flow for desktop applications:

```c
void loopback_login() {
    char *code = NULL;
    char *state = NULL;
    char *err = NULL;
    char *err_desc = NULL;

    /* Block until the user completes the flow in their browser and is redirected to http://127.0.0.1:8080 */
    int rc = http_oauth2_localhost_intercept(
        8080, 
        "HTTP/1.1 200 OK\r\n\r\n<html><body>Successfully logged in! You can close this tab.</body></html>", 
        &code, &state, &err, &err_desc
    );

    if (rc == 0 && code) {
        printf("Received Auth Code: %s\n", code);
    }

    if (code) free(code);
    if (state) free(state);
    if (err) free(err);
    if (err_desc) free(err_desc);
}
```

## 3. Supported Crypto Libraries

By default, `c-abstract-http` uses the native TLS backend of your network library (e.g. Schannel for WinHTTP, SecureTransport for CFNetwork, GnuTLS/OpenSSL for libcurl). You can force a specific crypto library via CMake when building:

```bash
cmake -B build -S . \
    -DC_ABSTRACT_HTTP_USE_MBEDTLS=ON \
    -DC_ABSTRACT_HTTP_USE_OPENSSL=OFF
```

Available configurations include `C_ABSTRACT_HTTP_USE_OPENSSL`, `C_ABSTRACT_HTTP_USE_MBEDTLS`, `C_ABSTRACT_HTTP_USE_LIBRESSL`, `C_ABSTRACT_HTTP_USE_BORINGSSL`, `C_ABSTRACT_HTTP_USE_WOLFSSL`, `C_ABSTRACT_HTTP_USE_S2N`, `C_ABSTRACT_HTTP_USE_BEARSSL`, `C_ABSTRACT_HTTP_USE_SCHANNEL`, `C_ABSTRACT_HTTP_USE_GNUTLS`, `C_ABSTRACT_HTTP_USE_BOTAN`, `C_ABSTRACT_HTTP_USE_COMMONCRYPTO`, and `C_ABSTRACT_HTTP_USE_WINCRYPT`.