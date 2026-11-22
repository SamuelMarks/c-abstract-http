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

/**
 * @brief Initialize a SHA-1 context.
 * @param ctx The context to initialize.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
sha1_init(struct sha1_ctx *ctx);

/**
 * @brief Update a SHA-1 context with data.
 * @param ctx The context to update.
 * @param data The data to hash.
 * @param len The length of the data.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
sha1_update(struct sha1_ctx *ctx, const unsigned char *data, size_t len);

/**
 * @brief Finalize a SHA-1 hash.
 * @param ctx The context to finalize.
 * @param out_hash The 20-byte array to store the resulting hash.
 * @return 0 on success.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
sha1_final(struct sha1_ctx *ctx, unsigned char out_hash[20]);

/**
 * @brief Initialize a SHA-256 context.
 * @param ctx The context to initialize.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
sha256_init(struct sha256_ctx *ctx);

/**
 * @brief Update a SHA-256 context with input bytes.
 * @param ctx The context to update.
 * @param data Input bytes.
 * @param len Byte length of @p data.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
sha256_update(struct sha256_ctx *ctx, const unsigned char *data, size_t len);

/**
 * @brief Finalize a SHA-256 digest into a 32-byte buffer.
 * @param ctx The context to finalize.
 * @param out_hash 32-byte output digest buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
sha256_final(struct sha256_ctx *ctx, unsigned char out_hash[32]);

/**
 * @brief Compute a one-shot SHA-256 digest using the active crypto provider.
 * @param data Input byte buffer (may be NULL if @p len is 0).
 * @param len Byte length of @p data.
 * @param out_hash 32-byte output digest buffer.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error code.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_sha256_oneshot(const unsigned char *data, size_t len,
                               unsigned char out_hash[32]);

/**
 * @brief Query the active compile-time cryptographic provider name.
 * @param out_name Output pointer receiving static provider identifier string.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
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
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
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
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_tls_verify_handshake_config(const struct HttpConfig *config,
                                            const char *peer_host,
                                            const char *cert_cn,
                                            const unsigned char *spki_der,
                                            size_t spki_len);

/**
 * @brief Retrieve the active compile-time CMake TLS socket provider hooks.
 *
 * Returns non-NULL hooks when compiled with an external TLS provider
 * (`C_ABSTRACT_HTTP_USE_OPENSSL`, `LIBRESSL`, `BORINGSSL`, `WOLFSSL`,
 * `MBEDTLS`, `GNUTLS`, `S2N`, `BEARSSL`, `BOTAN`, `WINCRYPT`, `COMMONCRYPTO`),
 * or NULL when no built-in TLS socket provider is linked.
 *
 * @param out_hooks Output pointer receiving static TLS hook table or NULL.
 * @return C_ABSTRACT_HTTP_SUCCESS or C_ABSTRACT_HTTP_ERR_INVAL.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
c_abstract_http_get_default_tls_hooks(const struct HttpRawTlsHooks **out_hooks);

/**
 * @brief Base64 encode data.
 * @param in The data to encode.
 * @param in_len The length of the data.
 * @param out_str Pointer to a string that will be allocated.
 * @param out_len Pointer to store the length of the string.
 * @return 0 on success, ENOMEM on allocation failure.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
base64_encode(const unsigned char *in, size_t in_len, char **out_str,
              size_t *out_len);

/**
 * @brief Base64 decode a string.
 * @param in The string to decode.
 * @param in_len The length of the string.
 * @param out_data Pointer to an array that will be allocated.
 * @param out_len Pointer to store the length of the data.
 * @return 0 on success, ENOMEM on allocation failure, EINVAL on invalid input.
 */
NO_DISCARD C_ABSTRACT_HTTP_API enum c_abstract_http_error
base64_decode(const char *in, size_t in_len, unsigned char **out_data,
              size_t *out_len);

/**
 * @brief Compare two strings in constant time.
 * @param a The first string.
 * @param b The second string.
 * @return 1 if equal, 0 if different.
 */
C_ABSTRACT_HTTP_API int math_is_const_time_streq(const char *a, const char *b);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_CRYPTO_UTILS_H */
