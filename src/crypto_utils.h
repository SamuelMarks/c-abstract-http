#ifndef C_ABSTRACT_HTTP_CRYPTO_UTILS_H
#define C_ABSTRACT_HTTP_CRYPTO_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Crypto definitions */

/* clang-format off */
#include <stddef.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int8 uint8_t;
typedef unsigned __int32 uint32_t;
#else
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
#endif
#endif
/* clang-format on */

/**
 * @brief SHA-1 Context.
 */
struct sha1_ctx {
  uint32_t state[5];  /**< @brief Documented */
  uint32_t count[2];  /**< @brief Documented */
  uint8_t buffer[64]; /**< @brief Documented */
};

/**
 * @brief Initialize a SHA-1 context.
 * @param ctx The context to initialize.
 * @return 0 on success.
 */
int sha1_init(struct sha1_ctx *ctx);

/**
 * @brief Update a SHA-1 context with data.
 * @param ctx The context to update.
 * @param data The data to hash.
 * @param len The length of the data.
 * @return 0 on success.
 */
int sha1_update(struct sha1_ctx *ctx, const unsigned char *data, size_t len);

/**
 * @brief Finalize a SHA-1 hash.
 * @param ctx The context to finalize.
 * @param out_hash The 20-byte array to store the resulting hash.
 * @return 0 on success.
 */
int sha1_final(struct sha1_ctx *ctx, unsigned char out_hash[20]);

/**
 * @brief Base64 encode data.
 * @param in The data to encode.
 * @param in_len The length of the data.
 * @param out_str Pointer to a string that will be allocated.
 * @param out_len Pointer to store the length of the string.
 * @return 0 on success, ENOMEM on allocation failure.
 */
int base64_encode(const unsigned char *in, size_t in_len, char **out_str,
                  size_t *out_len);

/**
 * @brief Base64 decode a string.
 * @param in The string to decode.
 * @param in_len The length of the string.
 * @param out_data Pointer to an array that will be allocated.
 * @param out_len Pointer to store the length of the data.
 * @return 0 on success, ENOMEM on allocation failure, EINVAL on invalid input.
 */
int base64_decode(const char *in, size_t in_len, unsigned char **out_data,
                  size_t *out_len);

/**
 * @brief Compare two strings in constant time.
 * @param a The first string.
 * @param b The second string.
 * @return 1 if equal, 0 if different.
 */
int const_time_streq(const char *a, const char *b);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_CRYPTO_UTILS_H */
