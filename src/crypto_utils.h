#ifndef C_ABSTRACT_HTTP_CRYPTO_UTILS_H
#define C_ABSTRACT_HTTP_CRYPTO_UTILS_H

/**
 * @file crypto_utils.h
 * @brief Internal cryptographic, X.509/SPKI pinning, Base64, and TLS provider
 * utilities.
 */

/* clang-format off */
#include <c_abstract_http/http_raw.h>
#include <c_abstract_http/http_types.h>
#include <c89stringutils_string_extras.h>
#include <stddef.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief SHA-1 Context.
 */
struct sha1_ctx {
  uint32_t state[5];  /**< @brief Running 160-bit digest state */
  uint32_t count[2];  /**< @brief Processed bit length counter */
  uint8_t buffer[64]; /**< @brief Partial 512-bit block buffer */
};

/**
 * @brief SHA-256 Context (FIPS 180-4).
 */
struct sha256_ctx {
  uint32_t state[8];  /**< @brief Running 256-bit digest state */
  uint32_t count[2];  /**< @brief Processed bit length counter (lo, hi) */
  uint8_t buffer[64]; /**< @brief Partial 512-bit block buffer */
};

/** @brief Namespace alias for sha1_init */
#define sha1_init c_abstract_http_sha1_init
/** @brief Namespace alias for sha1_update */
#define sha1_update c_abstract_http_sha1_update
/** @brief Namespace alias for sha1_final */
#define sha1_final c_abstract_http_sha1_final
/** @brief Namespace alias for sha256_init */
#define sha256_init c_abstract_http_sha256_init
/** @brief Namespace alias for sha256_update */
#define sha256_update c_abstract_http_sha256_update
/** @brief Namespace alias for sha256_final */
#define sha256_final c_abstract_http_sha256_final
/** @brief Namespace alias for base64_encode */
#define base64_encode c_abstract_http_base64_encode
/** @brief Namespace alias for base64_decode */
#define base64_decode c_abstract_http_base64_decode
/** @brief Namespace alias for math_is_const_time_streq */
#define math_is_const_time_streq math_c_abstract_http_is_const_time_streq

/**
 * @brief Initialize a SHA-1 context.
 * @param ctx The context to initialize.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
sha1_init(struct sha1_ctx *ctx);

/**
 * @brief Update a SHA-1 context with data.
 * @param ctx The context to update.
 * @param data The data to hash.
 * @param len The length of the data.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
sha1_update(struct sha1_ctx *ctx, const unsigned char *data, size_t len);

/**
 * @brief Finalize a SHA-1 hash.
 * @param ctx The context to finalize.
 * @param out_hash The 20-byte array to store the resulting hash.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
sha1_final(struct sha1_ctx *ctx, unsigned char out_hash[20]);

/**
 * @brief Initialize a SHA-256 context.
 * @param ctx The context to initialize.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
sha256_init(struct sha256_ctx *ctx);

/**
 * @brief Update a SHA-256 context with input bytes.
 * @param ctx The context to update.
 * @param data Input bytes.
 * @param len Byte length of @p data.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
sha256_update(struct sha256_ctx *ctx, const unsigned char *data, size_t len);

/**
 * @brief Finalize a SHA-256 digest into a 32-byte buffer.
 * @param ctx The context to finalize.
 * @param out_hash 32-byte output digest buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
sha256_final(struct sha256_ctx *ctx, unsigned char out_hash[32]);

/**
 * @brief Compute a one-shot SHA-256 digest using the active crypto provider.
 * @param data Input byte buffer (may be NULL if @p len is 0).
 * @param len Byte length of @p data.
 * @param out_hash 32-byte output digest buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_sha256_oneshot(const unsigned char *data, size_t len,
                               unsigned char out_hash[32]);

/**
 * @brief Query the active compile-time cryptographic provider name.
 * @param out_name Output pointer receiving static provider identifier string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_crypto_provider_name(const char **out_name);

/**
 * @brief Verify a DER-encoded SubjectPublicKeyInfo (SPKI) against a SHA-256
 * pin.
 *
 * Accepts pins formatted as `"sha256//<base64>"`, `"<base64>"`, or 64-character
 * lowercase/uppercase hexadecimal strings, comparing in constant time.
 *
 * @param spki_der DER-encoded SubjectPublicKeyInfo bytes.
 * @param spki_len Length of @p spki_der in bytes.
 * @param expected_pin NUL-terminated SHA-256 pin string.
 * @return C_ABSTRACT_HTTP_SUCCESS on match, C_ABSTRACT_HTTP_ERR_ACCES on pin
 * mismatch, or error code on invalid arguments / allocation failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_verify_spki_pin_sha256(const unsigned char *spki_der,
                                       size_t spki_len,
                                       const char *expected_pin);

/**
 * @brief Validate TLS handshake configuration, hostname, X.509 credentials,
 * and optional SHA-256 SPKI certificate pin.
 *
 * @param config Active HTTP/TLS configuration.
 * @param peer_host Target peer hostname (SNI / authority).
 * @param cert_cn Presented peer certificate CommonName / SAN pattern.
 * @param spki_der Presented peer SubjectPublicKeyInfo DER bytes (optional).
 * @param spki_len Length of @p spki_der in bytes.
 * @return C_ABSTRACT_HTTP_SUCCESS when valid, C_ABSTRACT_HTTP_ERR_ACCES on
 * verification/pin failure, or C_ABSTRACT_HTTP_ERR_INVAL on invalid config.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_tls_verify_handshake_config(const struct HttpConfig *config,
                                            const char *peer_host,
                                            const char *cert_cn,
                                            const unsigned char *spki_der,
                                            size_t spki_len);

/**
 * @brief Retrieve the active compile-time CMake TLS socket provider hooks.
 *
 * Returns non-NULL hooks when compiled with a built-in socket TLS provider
 * (`C_ABSTRACT_HTTP_USE_OPENSSL`, `LIBRESSL`, `BORINGSSL`, `WOLFSSL`,
 * `MBEDTLS`, or `GNUTLS`), or NULL when no built-in TLS socket provider is
 * linked.
 *
 * @param out_hooks Output pointer receiving static TLS hook table or NULL.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_get_default_tls_hooks(const struct HttpRawTlsHooks **out_hooks);

/**
 * @brief Fill a buffer with cryptographically unpredictable random bytes.
 *
 * Uses OS entropy (`RtlGenRandom` on Windows, `/dev/urandom` on POSIX) with a
 * non-global xorshift64* fallback on restricted/DOS targets. Never calls
 * `srand()` or `rand()`.
 *
 * @param buf Destination byte buffer (may be NULL only when @p len is 0).
 * @param len Number of random bytes to generate.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_random_bytes(unsigned char *buf, size_t len);

/**
 * @brief Base64 encode data.
 * @param in The data to encode.
 * @param in_len The length of the data.
 * @param out_str Pointer to a string that will be allocated.
 * @param out_len Pointer to store the length of the string.
 * @return 0 on success, ENOMEM on allocation failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t base64_encode(
    const unsigned char *in, size_t in_len, char **out_str, size_t *out_len);

/**
 * @brief Base64 decode a string.
 * @param in The string to decode.
 * @param in_len The length of the string.
 * @param out_data Pointer to an array that will be allocated.
 * @param out_len Pointer to store the length of the data.
 * @return 0 on success, ENOMEM on allocation failure, EINVAL on invalid input.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t base64_decode(
    const char *in, size_t in_len, unsigned char **out_data, size_t *out_len);

/**
 * @brief Compare two strings in constant time.
 * @param a The first string.
 * @param b The second string.
 * @return 1 if equal, 0 if different.
 */
C_ABSTRACT_HTTP_API int math_is_const_time_streq(const char *a, const char *b);

/**
 * @brief Validate and persist HTTP/3 TLS, CA, mTLS, SPKI pin, and redirect
 * settings onto an internal transport `HttpConfig`.
 * @param dst Destination `HttpConfig` owned by the transport context.
 * @param src Caller-supplied `HttpConfig`.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_INVAL,
 * C_ABSTRACT_HTTP_ERR_NOTSUP, or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_h3_apply_tls_config(struct HttpConfig *dst,
                                    const struct HttpConfig *src);

/**
 * @brief Verify HTTP/3 peer TLS configuration, hostname/CN, CA/mTLS, and SPKI
 * SHA-256 certificate pin for `https` and `masque` targets.
 * @param config Active transport `HttpConfig`.
 * @param scheme Parsed URL scheme (`"https"`, `"masque"`, or `"http"`).
 * @param host Target authority hostname.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_ACCES, or
 * C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_h3_verify_peer_tls(const struct HttpConfig *config,
                                   const char *scheme, const char *host);

/**
 * @brief Determine whether an HTTP response triggers an automatic 3xx redirect.
 * @param follow_redirects Non-zero when redirects are enabled.
 * @param res Completed HTTP response.
 * @param out_location Output pointer receiving the `Location` header value.
 * @return 1 if a redirect hop should be taken, 0 otherwise.
 */
C_ABSTRACT_HTTP_API int
math_c_abstract_http_should_redirect(int follow_redirects,
                                     const struct HttpResponse *res,
                                     const char **out_location);

/**
 * @brief Resolve a 3xx `Location` target, release intermediate response state,
 * and rewrite `hop_req` method/body per RFC 9110 Section 15.4.
 * @param hop_req Mutable per-hop `HttpRequest` descriptor.
 * @param io_redir_url In/out owned redirect URL string pointer.
 * @param io_res In/out response pointer (freed and cleared on transition).
 * @param location Raw `Location` header value from `*io_res`.
 * @param hop_index Zero-based hop index (returns `ERR_LOOP` when `>= 10`).
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_LOOP, or error enum.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_h3_advance_redirect(struct HttpRequest *hop_req,
                                    char **io_redir_url,
                                    struct HttpResponse **io_res,
                                    const char *location, int hop_index);

/**
 * @brief Format an `expected_body_len` `content-length` header value for
 * streaming `read_chunk` HTTP/3 uploads when not already present in headers.
 * @param req Outgoing `HttpRequest`.
 * @param out_buf Output 32-byte decimal string buffer.
 * @return 1 if `content-length` was formatted into @p out_buf, 0 otherwise.
 */
C_ABSTRACT_HTTP_API int
math_c_abstract_http_format_clen(const struct HttpRequest *req,
                                 char out_buf[32]);

/**
 * @brief Read an entire certificate/key file from disk into a newly allocated
 * buffer.
 * @param path Filesystem path to load.
 * @param out_buf Output pointer receiving heap-allocated file bytes.
 * @param out_len Output byte length (> 0 on success).
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_INVAL,
 * C_ABSTRACT_HTTP_ERR_ACCES, or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_read_file_bytes(const char *path, unsigned char **out_buf,
                                size_t *out_len);

/**
 * @brief Decode a PEM armor block (`-----BEGIN ...-----`) or raw ASN.1 DER
 * buffer into newly allocated DER bytes.
 * @param in Input PEM or DER byte buffer.
 * @param in_len Byte length of @p in.
 * @param out_der Output pointer receiving heap-allocated DER bytes.
 * @param out_der_len Output byte length of @p *out_der.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_INVAL,
 * C_ABSTRACT_HTTP_ERR_ACCES, or C_ABSTRACT_HTTP_ERR_NOMEM.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_pem_or_der_decode(const unsigned char *in, size_t in_len,
                                  unsigned char **out_der, size_t *out_der_len);

/**
 * @brief Extract the raw DER `SubjectPublicKeyInfo` TLV slice from a DER X.509
 * certificate.
 * @param cert_der Input DER-encoded X.509 certificate buffer.
 * @param cert_len Byte length of @p cert_der.
 * @param out_spki Receives pointer inside @p cert_der to
 * `SubjectPublicKeyInfo`.
 * @param out_spki_len Receives byte length of the `SubjectPublicKeyInfo` TLV.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_INVAL, or
 * C_ABSTRACT_HTTP_ERR_ACCES.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_x509_extract_spki_der(const unsigned char *cert_der,
                                      size_t cert_len,
                                      const unsigned char **out_spki,
                                      size_t *out_spki_len);

/**
 * @brief Unwrap an unencrypted PKCS#8 `PrivateKeyInfo` DER buffer to its inner
 * algorithm-specific `privateKey` OCTET STRING payload (or return @p der
 * unchanged if already PKCS#1 / SEC1).
 * @param der Input DER-encoded private key.
 * @param der_len Byte length of @p der.
 * @param out_key Receives pointer inside @p der to unwrapped key DER bytes.
 * @param out_key_len Receives byte length of @p *out_key.
 * @return C_ABSTRACT_HTTP_SUCCESS, C_ABSTRACT_HTTP_ERR_INVAL, or
 * C_ABSTRACT_HTTP_ERR_ACCES.
 */
NO_DISCARD C_ABSTRACT_HTTP_API c_abstract_http_error_t
c_abstract_http_pkcs8_unwrap_key(const unsigned char *der, size_t der_len,
                                 const unsigned char **out_key,
                                 size_t *out_key_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_CRYPTO_UTILS_H */
