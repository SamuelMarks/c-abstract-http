# Usage

This guide covers basic integration, standard HTTP requests, error handling, OAuth2 support, and memory lifecycle in `c-abstract-http`.

## 1. Initializing the Client

```c
#include <c_abstract_http/c_abstract_http.h>
#include <stdio.h>

int main(void) {
    struct HttpClient client;
    int rc = http_client_init(&client);
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize HTTP client: %d\n", rc);
        return rc;
    }

    client.config.timeout_ms = 5000;
    client.config.follow_redirects = 1;

    /* Request HTTP/3 with a fallback to HTTP/2 and HTTP/1.1 */
    client.config.version_mask = HTTP_VERSION_3 | HTTP_VERSION_2 | HTTP_VERSION_1_1;
    client.config.http3_fallback = 1;

    /* Restrict TLS versions to 1.2 and 1.3 for security */
    client.config.tls_version_mask = HTTP_TLS_VERSION_1_3 | HTTP_TLS_VERSION_1_2;

    http_client_free(&client);
    return 0;
}
```

## 2. Simple HTTP GET Request

```c
#include <c_abstract_http/c_abstract_http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fetch_and_print(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    int rc = 0;
    
    http_request_init(&req);
    
    req.url = malloc(strlen("https://example.com") + 1);
    if (!req.url) {
        fprintf(stderr, "Failed to allocate memory for URL\n");
        rc = -1;
        goto cleanup;
    }
    strcpy(req.url, "https://example.com");

    rc = client->send(client->transport, &req, &res);
    if (rc != 0) {
        fprintf(stderr, "Failed to send HTTP request: %d\n", rc);
        goto cleanup;
    }

    if (res) {
        printf("Response Status: %d\n", res->status_code);
        if (res->body) {
            printf("Body:\n%.*s\n", (int)res->body_len, (char*)res->body);
        }
    }

cleanup:
    if (res) {
        http_response_free(res);
    }
    http_request_free(&req);
    return rc;
}
```

## 3. Downloading Multiple Files

```c
#include <c_abstract_http/c_abstract_http.h>
#include <cfs/cfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int download_files(struct HttpClient *client, const char *dest_dir) {
    const char *urls[] = {
        "https://example.com/file1.txt",
        "https://example.com/file2.txt"
    };
    size_t num_urls = sizeof(urls) / sizeof(urls[0]);
    struct HttpRequest reqs[2];
    struct HttpRequest *req_ptrs[2];
    struct HttpFuture f1, f2;
    struct HttpFuture *futures[2] = { &f1, &f2 };
    size_t i;
    int rc = 0;
    cfs_path dir_path;
    cfs_error_code ec = 0;
    
    for (i = 0; i < num_urls; ++i) {
        http_request_init(&reqs[i]);
        http_future_init(futures[i]);
    }

    /* Ensure destination directory exists using c-fs */
    cfs_path_init_str(&dir_path, (const cfs_char_t*)dest_dir);
    cfs_create_directories(&dir_path, &ec);
    if (ec != 0) {
        fprintf(stderr, "Failed to create destination directories\n");
        rc = (int)ec;
        goto cleanup_dir;
    }

    for (i = 0; i < num_urls; ++i) {
        reqs[i].url = malloc(strlen(urls[i]) + 1);
        if (!reqs[i].url) {
            fprintf(stderr, "Failed to allocate memory for URL %zu\n", i);
            rc = -1;
            goto cleanup;
        }
        strcpy(reqs[i].url, urls[i]);
        req_ptrs[i] = &reqs[i];
    }

    /* Send requests concurrently */
    rc = http_client_send_multi(client, req_ptrs, num_urls, futures, NULL, NULL, 0);
    if (rc != 0) {
        fprintf(stderr, "Failed to send concurrent requests: %d\n", rc);
        goto cleanup;
    }

    for (i = 0; i < num_urls; ++i) {
        if (futures[i]->is_ready && futures[i]->error_code == 0 && futures[i]->response) {
            struct HttpResponse *res = futures[i]->response;
            cfs_path file_path;
            const char *filename;
            const cfs_char_t *path_str = NULL;
            FILE *fp;
            
            cfs_path_init(&file_path);
            cfs_path_clone(&file_path, &dir_path);
            
            /* Extract filename from URL */
            filename = strrchr(urls[i], '/');
            if (filename) filename++;
            else filename = "downloaded_file.txt";
            
            cfs_path_append(&file_path, (const cfs_char_t*)filename);
            cfs_path_c_str(&file_path, &path_str);
            
            if (!path_str || !res->body) {
                fprintf(stderr, "Failed to resolve path or missing body for %s\n", urls[i]);
                cfs_path_destroy(&file_path);
                continue;
            }
            
            fp = fopen((const char*)path_str, "wb");
            if (!fp) {
                fprintf(stderr, "Failed to open file for writing: %s\n", (const char*)path_str);
                cfs_path_destroy(&file_path);
                continue;
            }
            
            if (fwrite(res->body, 1, res->body_len, fp) != res->body_len) {
                fprintf(stderr, "Failed to completely write body to file: %s\n", (const char*)path_str);
            } else {
                printf("Saved: %s\n", (const char*)path_str);
            }
            
            fclose(fp);
            cfs_path_destroy(&file_path);
            http_response_free(res);
            futures[i]->response = NULL;
        } else {
            fprintf(stderr, "Request %zu failed with error code %d\n", i, futures[i]->error_code);
            rc = -1; /* Mark overall function as failed if any request failed */
        }
    }
    
cleanup:
    for (i = 0; i < num_urls; ++i) {
        http_request_free(&reqs[i]);
        http_future_free(futures[i]);
    }
cleanup_dir:
    cfs_path_destroy(&dir_path);
    return rc;
}
```

## 4. OAuth 2.0 Client Support

`c-abstract-http` natively handles OAuth 2.0 flows, making authentication against modern APIs simple and secure.

### Password Grant

```c
int get_token(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    int rc = 0;
    
    http_request_init(&req);
    
    rc = http_request_init_oauth2_password_grant(&req, 
        "https://auth.server/token",
        "user@example.com",
        "super_secret_password",
        "client_id_123",
        NULL, 
        "read write"
    );
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize password grant: %d\n", rc);
        goto cleanup;
    }

    rc = client->send(client->transport, &req, &res);
    if (rc != 0) {
        fprintf(stderr, "Failed to send password grant request: %d\n", rc);
        goto cleanup;
    }

    if (res) {
        printf("Response: %s\n", (char*)res->body);
    }

cleanup:
    if (res) {
        http_response_free(res);
    }
    http_request_free(&req);
    return rc;
}
```

### Client Credentials Grant

```c
int service_token(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    int rc = 0;

    http_request_init(&req);
    
    rc = http_request_init_oauth2_client_credentials_grant(&req, 
        "https://auth.server/token",
        "my_client_id",
        "my_client_secret",
        "admin_scope"
    );
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize client credentials grant: %d\n", rc);
        goto cleanup;
    }

    rc = client->send(client->transport, &req, &res);
    if (rc != 0) {
        fprintf(stderr, "Failed to send client credentials request: %d\n", rc);
        goto cleanup;
    }
    
    if (res) {
        printf("Response: %s\n", (char*)res->body);
    }

cleanup:
    if (res) {
        http_response_free(res);
    }
    http_request_free(&req);
    return rc;
}
```

### Device Authorization Flow

For smart TVs and headless CLIs:

```c
int device_flow(struct HttpClient *client) {
    struct HttpRequest req;
    struct HttpResponse *res = NULL;
    int rc = 0;
    
    http_request_init(&req);

    /* 1. Get the device code */
    rc = http_request_init_oauth2_device_authorization_request(&req, 
        "https://auth.server/device",
        "my_client_id",
        NULL
    );
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize device authorization request: %d\n", rc);
        goto cleanup;
    }
    
    rc = client->send(client->transport, &req, &res);
    if (rc != 0) {
        fprintf(stderr, "Failed to send device authorization request: %d\n", rc);
        goto cleanup;
    }
    
    if (res) {
        printf("Device Auth Response: %s\n", (char*)res->body);
        http_response_free(res);
        res = NULL;
    }
    http_request_free(&req);

    /* 2. Poll for the access token using the device code */
    http_request_init(&req);
    rc = http_request_init_oauth2_device_access_token_request(&req, 
        "https://auth.server/token",
        "my_client_id",
        "device_code_123"
    );
    if (rc != 0) {
        fprintf(stderr, "Failed to initialize device access token request: %d\n", rc);
        goto cleanup;
    }
    
    rc = client->send(client->transport, &req, &res);
    if (rc != 0) {
        fprintf(stderr, "Failed to send device access token request: %d\n", rc);
        goto cleanup;
    }
    
    if (res) {
        printf("Token Response: %s\n", (char*)res->body);
    }

cleanup:
    if (res) {
        http_response_free(res);
    }
    http_request_free(&req);
    return rc;
}
```

### Localhost Intercept Server

To handle the OAuth 2.0 loopback flow for desktop applications:

```c
int loopback_login(void) {
    char *code = NULL;
    char *state = NULL;
    char *err = NULL;
    char *err_desc = NULL;
    int rc = 0;

    /* Block until the user completes the flow in their browser and is redirected to http://127.0.0.1:8080 */
    rc = http_oauth2_localhost_intercept(
        8080, 
        "HTTP/1.1 200 OK\r\n\r\n<html><body>Successfully logged in! You can close this tab.</body></html>", 
        &code, &state, &err, &err_desc
    );

    if (rc != 0) {
        fprintf(stderr, "Failed to intercept localhost loopback: %d\n", rc);
        goto cleanup;
    }

    if (err) {
        fprintf(stderr, "OAuth2 Error: %s - %s\n", err, err_desc ? err_desc : "No description");
        rc = -1;
        goto cleanup;
    }

    if (code) {
        printf("Received Auth Code: %s\n", code);
    }

cleanup:
    if (code) free(code);
    if (state) free(state);
    if (err) free(err);
    if (err_desc) free(err_desc);
    
    return rc;
}
```

## 5. Supported Crypto Libraries

By default, `c-abstract-http` uses the native TLS backend of your network library (e.g. Schannel for WinHTTP, SecureTransport for CFNetwork, GnuTLS/OpenSSL for libcurl). You can force a specific crypto library via CMake when building:

```bash
cmake -B build -S . \
    -DC_ABSTRACT_HTTP_USE_MBEDTLS=ON \
    -DC_ABSTRACT_HTTP_USE_OPENSSL=OFF
```

## 6. DOS and Raw Sockets Fallback

If you are compiling for DOS (e.g. using OpenWatcom) or building for deeply embedded bare-metal systems, `c-abstract-http` automatically switches to its manual **Raw Sockets** implementation (`http_raw.c`). This backend relies solely on `select`, `read`, and `write` POSIX-like methods.

In such environments, you will usually want to pair the network stack (like Watt-32 or mTCP) with a lightweight cryptography library, like MbedTLS or wolfSSL. 

You can force this fallback layer on any POSIX system by passing the raw sockets flag:

```bash
cmake -B build -S . \
    -DCMAKE_SYSTEM_NAME=DOS \
    -DC_ABSTRACT_HTTP_USE_MBEDTLS=ON 
```

Available configurations include `C_ABSTRACT_HTTP_USE_OPENSSL`, `C_ABSTRACT_HTTP_USE_MBEDTLS`, `C_ABSTRACT_HTTP_USE_LIBRESSL`, `C_ABSTRACT_HTTP_USE_BORINGSSL`, `C_ABSTRACT_HTTP_USE_WOLFSSL`, `C_ABSTRACT_HTTP_USE_S2N`, `C_ABSTRACT_HTTP_USE_BEARSSL`, `C_ABSTRACT_HTTP_USE_SCHANNEL`, `C_ABSTRACT_HTTP_USE_GNUTLS`, `C_ABSTRACT_HTTP_USE_BOTAN`, `C_ABSTRACT_HTTP_USE_COMMONCRYPTO`, and `C_ABSTRACT_HTTP_USE_WINCRYPT`.