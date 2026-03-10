/**
 * @file http_types.h
 * @brief Abstract Network Interface (ANI) definitions.
 *
 * Defines core structures for HTTP communication, configuration (retries),
 * and Multipart/Form-Data support.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_HTTP_TYPES_H
#define C_CDD_HTTP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stddef.h>

#ifndef NUM_FORMAT
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#define NUM_FORMAT "%Iu"
#else
#define NUM_FORMAT "%zu"
#endif
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1600)
typedef __int64 cdd_int64_t;
typedef unsigned __int64 cdd_uint64_t;
#define CDD_INT64_FMT "%I64d"
#else
#include <stdint.h>
typedef int64_t cdd_int64_t;
typedef uint64_t cdd_uint64_t;
#define CDD_INT64_FMT "%lld"
#endif

/**
 * @brief HTTP Method verbs.
 */
/* clang-format on */
enum HttpMethod {
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_PATCH,
  HTTP_HEAD,
  HTTP_OPTIONS,
  HTTP_TRACE,
  HTTP_QUERY,
  HTTP_CONNECT
};

/**
 * @brief Retry Policy Enum.
 */
enum HttpRetryPolicy {
  HTTP_RETRY_NONE,       /**< No retries (default) */
  HTTP_RETRY_LINEAR,     /**< Fixed delay between retries */
  HTTP_RETRY_EXPONENTIAL /**< Exponential backoff */
};

/**
 * @brief Represents a single HTTP header.
 */
struct HttpHeader {
  char *key;   /**< Header name (allocated) */
  char *value; /**< Header value (allocated) */
};

/**
 * @brief Container for HTTP headers.
 */
struct HttpHeaders {
  struct HttpHeader *headers; /**< Dynamic array of headers */
  size_t count;               /**< Number of headers used */
  size_t capacity;            /**< Current capacity */
};

/**
 * @brief Represents a single part in a Multipart request.
 */
struct HttpPart {
  char *name;         /**< Form field name */
  char *filename;     /**< Filename (optional, implies file upload) */
  char *content_type; /**< Content-Type of the part (e.g. "application/json"),
                         optional */
  struct HttpHeaders headers; /**< Optional per-part headers */
  void *data;                 /**< Pointer to data buffer */
  size_t data_len;            /**< Length of data buffer */
};

/**
 * @brief Container for Multipart parts.
 */
struct HttpParts {
  struct HttpPart *parts; /**< Dynamic array of parts */
  size_t count;           /**< Number of parts used */
  size_t capacity;        /**< Current capacity */
};

/**
 * @brief Callback function signature for receiving response body chunks.
 * @param user_data User provided context pointer.
 * @param chunk Pointer to the received data chunk.
 * @param chunk_len Length of the received data chunk.
 * @return 0 to continue reading, non-zero to abort the request.
 */
typedef int (*http_on_chunk_fn)(void *user_data, const void *chunk,
                                size_t chunk_len);

/**
 * @brief Callback function signature for providing request body chunks
 * (Streaming Uploads).
 * @param user_data User provided context pointer.
 * @param buf Buffer to fill with data.
 * @param buf_len Maximum number of bytes to write to buf.
 * @param out_read Pointer to store the number of bytes actually written. Set to
 * 0 to indicate EOF.
 * @return 0 on success, non-zero to abort the request.
 */
typedef int (*http_read_chunk_fn)(void *user_data, void *buf, size_t buf_len,
                                  size_t *out_read);

/**
 * @brief Represents an outgoing HTTP request.
 */
struct HttpRequest {
  char *url;                  /**< Full destination URL */
  enum HttpMethod method;     /**< HTTP Verb */
  struct HttpHeaders headers; /**< Request headers */
  void *body; /**< Raw body payload (mutually exclusive with parts generally,
                 but flattened parts end up here) */
  size_t body_len;        /**< Length of raw body */
  struct HttpParts parts; /**< Multipart segments (if any) */
  http_on_chunk_fn
      on_chunk; /**< Optional callback for streaming response bodies */
  void *on_chunk_user_data; /**< User data passed to the on_chunk callback */

  http_read_chunk_fn
      read_chunk; /**< Optional callback for streaming request bodies */
  void *read_chunk_user_data; /**< User data for the read_chunk callback */
  size_t expected_body_len;   /**< Expected total upload size (set to 0 for
                                 unknown/chunked) */
};

/**
 * @brief Represents an incoming HTTP response.
 */
struct HttpResponse {
  int status_code;            /**< HTTP Status Code (e.g. 200, 404) */
  struct HttpHeaders headers; /**< Response Headers */
  void *body;                 /**< Response Body Payload */
  size_t body_len;            /**< Length of Response Body */
};

/**
 * @brief Represents a single HTTP Cookie.
 */
struct HttpCookie {
  char *name;    /**< Cookie name (allocated) */
  char *value;   /**< Cookie value (allocated) */
  char *domain;  /**< Domain attribute (allocated, optional) */
  char *path;    /**< Path attribute (allocated, optional) */
  long expires;  /**< Expiration timestamp (unix epoch), 0 if session cookie */
  int secure;    /**< Secure flag (1 = true, 0 = false) */
  int http_only; /**< HttpOnly flag (1 = true, 0 = false) */
};

/**
 * @brief Container for HTTP Cookies (Cookie Jar).
 */
struct HttpCookieJar {
  struct HttpCookie *cookies; /**< Dynamic array of cookies */
  size_t count;               /**< Number of cookies stored */
  size_t capacity;            /**< Current array capacity */
};

/**
 * @brief Execution Modality Enum.
 */
enum ExecutionModality {
  MODALITY_SYNC,
  MODALITY_ASYNC,
  MODALITY_THREAD_POOL,
  MODALITY_MULTIPROCESS,
  MODALITY_GREENTHREAD,
  MODALITY_MESSAGE_PASSING
};

/**
 * @brief Runtime context for the chosen execution modality.
 */
struct ModalityContext {
  enum ExecutionModality modality;
  void *internal_ctx; /**< Modality-specific internal context */
};

/**
 * @brief Represents a deferred HTTP response for non-sync modalities.
 */
struct HttpFuture {
  int is_ready;                  /**< 1 if completed, 0 otherwise */
  int error_code;                /**< Error code if request failed */
  struct HttpResponse *response; /**< The resulting response, if successful */
  void *internal_state;          /**< Modality-specific state */
};

/**
 * @brief Aggregate for multiple HttpRequest objects.
 */
struct HttpMultiRequest {
  struct HttpRequest **requests; /**< Array of pointers to requests */
  size_t count;                  /**< Number of requests */
  size_t capacity;               /**< Current capacity */
};

/**
 * @brief Configuration settings for the HTTP Client.
 */
struct HttpConfig {
  long timeout_ms;         /**< General timeout in milliseconds */
  long connect_timeout_ms; /**< Connect timeout in milliseconds (0 to use
                              timeout_ms) */
  long read_timeout_ms;    /**< Receive/Read timeout in milliseconds (0 to use
                              timeout_ms) */
  long write_timeout_ms;   /**< Send/Write timeout in milliseconds (0 to use
                              timeout_ms) */
  int verify_peer;         /**< 1 to verify SSL peer, 0 to ignore */
  int verify_host;         /**< 1 to verify SSL host, 0 to ignore */
  int follow_redirects;    /**< 1 to automatically follow 3xx redirects, 0 to
                              disable */
  char *user_agent;        /**< Custom User-Agent string */
  char *proxy_url;         /**< Proxy URL (e.g. "http://10.0.0.1:8080") */
  char *proxy_username;    /**< Proxy basic auth username (optional) */
  char *proxy_password;    /**< Proxy basic auth password (optional) */
  int retry_count;         /**< Max retries on failure (default 0) */
  enum HttpRetryPolicy retry_policy; /**< Backoff strategy */
  struct HttpCookieJar *cookie_jar;  /**< Optional shared cookie jar for
                                        persistence across requests */
  enum ExecutionModality modality;   /**< Execution paradigm */
  size_t min_threads; /**< Min threads for thread pool modality */
  size_t max_threads; /**< Max threads for thread pool modality */
};

struct HttpTransportContext;
struct ModalityEventLoop;

/**
 * @brief Function pointer signature for the transport send method.
 */
typedef int (*http_send_fn)(struct HttpTransportContext *ctx,
                            const struct HttpRequest *req,
                            struct HttpResponse **res);

/**
 * @brief Function pointer signature for the transport asynchronous multi-send
 * method.
 */
typedef int (*http_send_multi_fn)(struct HttpTransportContext *ctx,
                                  struct ModalityEventLoop *loop,
                                  const struct HttpMultiRequest *multi,
                                  struct HttpFuture **futures);

/**
 * @brief High-level client context.
 */
struct HttpClient {
  struct HttpTransportContext
      *transport;    /**< Backend-specific context (CURL*, HINTERNET, etc.) */
  http_send_fn send; /**< Function pointer to execute requests */
  http_send_multi_fn
      send_multi;           /**< Function pointer for multi/async requests */
  char *base_url;           /**< Base URL for API calls */
  struct HttpConfig config; /**< Client configuration */

  /* Runtime State */
  struct ModalityEventLoop *loop;    /**< Event loop for async mode */
  struct CddThreadPool *thread_pool; /**< Thread pool for multi-threaded mode */
};

/**
 * @brief Progress callback for multi-request downloads.
 * @param[in] current_bytes Bytes transferred so far across all requests.
 * @param[in] total_bytes Total expected bytes (0 if unknown).
 * @param[in] user_data User context.
 * @return 0 to continue, non-zero to abort all transfers.
 */
typedef int (*http_multi_progress_cb)(size_t current_bytes, size_t total_bytes,
                                      void *user_data);

/**
 * @brief Dispatch multiple requests concurrently based on the client's
 * modality.
 * @param[in] client The HTTP client.
 * @param[in] requests The array of requests to execute.
 * @param[in] num_requests Number of requests.
 * @param[out] futures Array of futures (must be pre-allocated to size
 * `num_requests`).
 * @param[in] progress_cb Optional progress callback.
 * @param[in] user_data User data for the progress callback.
 * @param[in] fail_fast 1 to abort all remaining requests if one fails, 0 to
 * continue.
 * @return 0 on successful dispatch (futures may still report individual
 * errors), error code otherwise.
 */
extern int http_client_send_multi(struct HttpClient *client,
                                  struct HttpRequest *const *requests,
                                  size_t num_requests,
                                  struct HttpFuture **futures,
                                  http_multi_progress_cb progress_cb,
                                  void *user_data, int fail_fast);

/* --- Lifecycle Management --- */

/** @brief http_headers_init definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_headers_init(struct HttpHeaders *headers);
/** @brief http_headers_free definition */
extern /**
        * @brief Autogenerated docstring
        */
    void
    http_headers_free(struct HttpHeaders *headers);
/** @brief http_headers_add definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_headers_add(struct HttpHeaders *headers, const char *key,
                     const char *value);

/**
 * @brief Retrieves the value for a specific header key.
 * @param headers The headers container.
 * @param key The header key to search for (case-insensitive).
 * @param out Pointer to store the found header value.
 * @return 0 on success, ENOENT if not found, EINVAL on bad input.
 */
extern int http_headers_get(const struct HttpHeaders *headers, const char *key,
                            const char **out);

/**
 * @brief Removes a header by key.
 * @param headers The headers container.
 * @param key The header key to remove (case-insensitive).
 * @return 0 on success, ENOENT if not found, EINVAL on bad input.
 */
extern int http_headers_remove(struct HttpHeaders *headers, const char *key);

/**
 * @brief Initialize a cookie jar.
 * @param jar The cookie jar to initialize.
 * @return 0 on success, EINVAL if jar is NULL.
 */
extern int http_cookie_jar_init(struct HttpCookieJar *jar);

/**
 * @brief Free resources held by a cookie jar.
 * @param jar The cookie jar to free.
 */
extern void http_cookie_jar_free(struct HttpCookieJar *jar);

/**
 * @brief Add or update a cookie in the jar.
 * @param jar The cookie jar.
 * @param name Cookie name.
 * @param value Cookie value.
 * @return 0 on success, ENOMEM on failure.
 */
extern int http_cookie_jar_set(struct HttpCookieJar *jar, const char *name,
                               const char *value);

/**
 * @brief Get a cookie value from the jar.
 * @param jar The cookie jar.
 * @param name Cookie name.
 * @param out Pointer to store the found cookie value string.
 * @return 0 on success, ENOENT if not found, EINVAL on bad input.
 */
extern int http_cookie_jar_get(const struct HttpCookieJar *jar,
                               const char *name, const char **out);

/** @brief http_config_init definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_config_init(struct HttpConfig *config);
/** @brief http_config_free definition */
extern /**
        * @brief Autogenerated docstring
        */
    void
    http_config_free(struct HttpConfig *config);

/** @brief http_client_init definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_client_init(struct HttpClient *client);
/** @brief http_client_free definition */
extern /**
        * @brief Autogenerated docstring
        */
    void
    http_client_free(struct HttpClient *client);

/** @brief http_request_init definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_request_init(struct HttpRequest *req);
/** @brief http_request_free definition */
extern /**
        * @brief Autogenerated docstring
        */
    void
    http_request_free(struct HttpRequest *req);

/**
 * @brief Initialize a ModalityContext.
 * @param ctx Context to initialize.
 * @return 0 on success.
 */
extern int http_modality_context_init(struct ModalityContext *ctx);

/**
 * @brief Free a ModalityContext.
 * @param ctx Context to free.
 */
extern void http_modality_context_free(struct ModalityContext *ctx);

/**
 * @brief Initialize a HttpFuture.
 * @param future Future to initialize.
 * @return 0 on success.
 */
extern int http_future_init(struct HttpFuture *future);

/**
 * @brief Free a HttpFuture.
 * @param future Future to free.
 */
extern void http_future_free(struct HttpFuture *future);

/**
 * @brief Initialize an HttpMultiRequest.
 * @param multi Multi-request to initialize.
 * @return 0 on success.
 */
extern int http_multi_request_init(struct HttpMultiRequest *multi);

/**
 * @brief Free an HttpMultiRequest.
 * @param multi Multi-request to free.
 */
extern void http_multi_request_free(struct HttpMultiRequest *multi);

/**
 * @brief Add a request to a multi-request struct.
 * @param multi Multi-request struct.
 * @param req Request to add.
 * @return 0 on success, ENOMEM on allocation failure.
 */
extern int http_multi_request_add(struct HttpMultiRequest *multi,
                                  struct HttpRequest *req);

/** @brief http_request_set_auth_bearer definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_request_set_auth_bearer(struct HttpRequest *req, const char *token);
/**
 * @brief Set HTTP Basic Authorization header with a pre-encoded token.
 *
 * The token must be the base64-encoded "username:password" string.
 *
 * @param[in,out] req The request to modify.
 * @param[in] token Base64-encoded "username:password".
 * @return 0 on success, error code otherwise.
 */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_request_set_auth_basic(struct HttpRequest *req, const char *token);

/**
 * @brief Set HTTP Basic Authorization header by encoding username and password.
 *
 * Base64 encodes the "username:password" and applies the Authorization header.
 *
 * @param[in,out] req The request to modify.
 * @param[in] username The username.
 * @param[in] password The password.
 * @return 0 on success, error code otherwise.
 */
extern int http_request_set_auth_basic_userpwd(struct HttpRequest *req,
                                               const char *username,
                                               const char *password);

/** @brief http_response_init definition */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_response_init(struct HttpResponse *res);
/** @brief http_response_free definition */
extern /**
        * @brief Autogenerated docstring
        */
    void
    http_response_free(struct HttpResponse *res);
extern int
/** @brief http_response_save_to_file definition */
http_response_save_to_file(const struct HttpResponse *res, const char *path);

/* --- Multipart Management --- */

/**
 * @brief Initialize parts container.
 * @param[out] parts Container to init.
 * @return 0 on success.
 */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_parts_init(struct HttpParts *parts);

/**
 * @brief Free parts container contents.
 * @param[in] parts Container to clean.
 */
extern /**
        * @brief Autogenerated docstring
        */
    void
    http_parts_free(struct HttpParts *parts);

/**
 * @brief Add a part to the request.
 *
 * @param[in,out] req The request object.
 * @param[in] name Field name.
 * @param[in] filename Optional filename (NULL for text fields).
 * @param[in] content_type Optional MIME type (NULL autodetects/defaults).
 * @param[in] data Pointer to payload.
 * @param[in] data_len Length of payload.
 * @return 0 on success, ENOMEM on failure.
 */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_request_add_part(struct HttpRequest *req, const char *name,
                          const char *filename, const char *content_type,
                          const void *data, size_t data_len);

/**
 * @brief Add a header to the most recently added multipart part.
 *
 * @param[in,out] req The request object.
 * @param[in] key Header name.
 * @param[in] value Header value.
 * @return 0 on success, EINVAL if no part exists or inputs are invalid.
 */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_request_add_part_header_last(struct HttpRequest *req, const char *key,
                                      const char *value);

/**
 * @brief Flatten parts into a single multipart/form-data body buffer.
 *
 * Generates the boundary, headers, and payload for all parts, concatenates them
 * into `req->body`, and sets the `Content-Type` header on `req`.
 * Used by transport layers that lack native multipart support or for
 * consistency.
 *
 * @param[in,out] req The request structure.
 * @return 0 on success, ENOMEM on allocation failure.
 */
extern /**
        * @brief Autogenerated docstring
        */
    int
    http_request_flatten_parts(struct HttpRequest *req);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_CDD_HTTP_TYPES_H */
