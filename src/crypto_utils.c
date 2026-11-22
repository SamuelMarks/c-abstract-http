/**
 * @file crypto_utils.c
 * @brief Internal SHA-1, SHA-256, Base64, X.509/SPKI pinning, and TLS provider
 * utilities.
 */

/* clang-format off */
#include "crypto_utils.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
#if defined(C_ABSTRACT_HTTP_USE_WOLFSSL)
#include <wolfssl/options.h>
#include <wolfssl/openssl/ssl.h>
#include <wolfssl/openssl/x509.h>
#include <wolfssl/openssl/err.h>
#define CAH_HAVE_OPENSSL_COMPAT_TLS 1
#elif defined(C_ABSTRACT_HTTP_USE_OPENSSL) || defined(C_ABSTRACT_HTTP_USE_LIBRESSL) || defined(C_ABSTRACT_HTTP_USE_BORINGSSL)
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#define CAH_HAVE_OPENSSL_COMPAT_TLS 1
#elif defined(C_ABSTRACT_HTTP_USE_MBEDTLS)
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/error.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#define CAH_HAVE_MBEDTLS_TLS 1
#elif defined(C_ABSTRACT_HTTP_USE_GNUTLS)
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/crypto.h>
#define CAH_HAVE_GNUTLS_TLS 1
#elif defined(C_ABSTRACT_HTTP_USE_COMMONCRYPTO) && defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif
#endif
/* clang-format on */

/* SHA-1 implementation based on RFC 3174 */
/**
 * @brief Rotate a 32-bit unsigned word left by the specified bit count.
 * @param bits Number of bits to rotate left.
 * @param word 32-bit unsigned word to rotate.
 * @return Rotated 32-bit value.
 */
#define SHA1_ROTL(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

/**
 * @brief Rotate a 32-bit unsigned word right by the specified bit count.
 * @param bits Number of bits to rotate right.
 * @param word 32-bit unsigned word to rotate.
 * @return Rotated 32-bit value.
 */
#define SHA256_ROTR(bits, word) (((word) >> (bits)) | ((word) << (32 - (bits))))

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
/** @brief Fault-injection counter for SHA-1/SHA-256 failure testing */
extern int g_mock_sha1_fail;
#endif

/**
 * @brief Process one 64-byte SHA-1 message block.
 * @param state Five-word SHA-1 running digest state.
 * @param buffer 64-byte input message block.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error enum on failure.
 */
static enum c_abstract_http_error
math_sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
  uint32_t a, b, c, d, e;
  uint32_t w[80];
  int i;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_sha1_fail < 0) {
    if (++g_mock_sha1_fail == 0)
      return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  for (i = 0; i < 16; i++) {
    w[i] = ((uint32_t)buffer[i * 4] << 24) |
           ((uint32_t)buffer[i * 4 + 1] << 16) |
           ((uint32_t)buffer[i * 4 + 2] << 8) | ((uint32_t)buffer[i * 4 + 3]);
  }
  for (i = 16; i < 80; i++) {
    w[i] = SHA1_ROTL(1, w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
  }

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];

  for (i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | ((~b) & d);
      k = 0x5A827999U;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1U;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCU;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6U;
    }

    {
      uint32_t temp = SHA1_ROTL(5, a) + f + e + k + w[i];
      e = d;
      d = c;
      c = SHA1_ROTL(30, b);
      b = a;
      a = temp;
    }
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize SHA-1 digest context */
enum c_abstract_http_error sha1_init(struct sha1_ctx *ctx) {
  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  ctx->state[0] = 0x67452301U;
  ctx->state[1] = 0xEFCDAB89U;
  ctx->state[2] = 0x98BADCFEU;
  ctx->state[3] = 0x10325476U;
  ctx->state[4] = 0xC3D2E1F0U;
  ctx->count[0] = ctx->count[1] = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Update SHA-1 digest with input bytes */
enum c_abstract_http_error sha1_update(struct sha1_ctx *ctx,
                                       const unsigned char *data, size_t len) {
  enum c_abstract_http_error rc;
  size_t i, j;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_sha1_fail > 0) {
    if (--g_mock_sha1_fail == 0)
      return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  if (!ctx)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (len > 0 && !data)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  j = (ctx->count[0] >> 3) & 63;
  if ((ctx->count[0] += (uint32_t)(len << 3)) < (len << 3)) {
    ctx->count[1]++;
  }
  ctx->count[1] += (uint32_t)(len >> 29);

  if ((j + len) > 63) {
    i = 64 - j;
    memcpy(&ctx->buffer[j], data, i);
    rc = math_sha1_transform(ctx->state, ctx->buffer);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
    for (; i + 63 < len; i += 64) {
      rc = math_sha1_transform(ctx->state, &data[i]);
      if (rc != C_ABSTRACT_HTTP_SUCCESS)
        return rc;
    }
    j = 0;
  } else {
    i = 0;
  }

  memcpy(&ctx->buffer[j], &data[i], len - i);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Finalize SHA-1 digest */
enum c_abstract_http_error sha1_final(struct sha1_ctx *ctx,
                                      unsigned char out_hash[20]) {
  enum c_abstract_http_error rc;
  unsigned char finalcount[8];
  unsigned char c;
  int i;

  if (!ctx || !out_hash)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  for (i = 0; i < 8; i++) {
    finalcount[i] =
        (unsigned char)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) &
                        255);
  }
  c = 0200;
  rc = sha1_update(ctx, &c, 1);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  while ((ctx->count[0] & 504) != 448) {
    c = 0000;
    rc = sha1_update(ctx, &c, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS)
      return rc;
  }
  rc = sha1_update(ctx, finalcount, 8);
  if (rc != C_ABSTRACT_HTTP_SUCCESS)
    return rc;
  for (i = 0; i < 20; i++) {
    out_hash[i] =
        (unsigned char)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
  }
  memset(ctx, 0, sizeof(*ctx));
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief FIPS 180-4 SHA-256 round constants */
static const uint32_t k_sha256[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

/**
 * @brief Process one 64-byte SHA-256 message block.
 * @param state Eight-word SHA-256 running digest state.
 * @param buffer 64-byte input message block.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, or error enum on failure.
 */
static enum c_abstract_http_error
math_sha256_transform(uint32_t state[8], const uint8_t buffer[64]) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, h;
  int i;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_sha1_fail < 0) {
    if (++g_mock_sha1_fail == 0)
      return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  for (i = 0; i < 16; ++i) {
    w[i] = ((uint32_t)buffer[i * 4] << 24) |
           ((uint32_t)buffer[i * 4 + 1] << 16) |
           ((uint32_t)buffer[i * 4 + 2] << 8) | ((uint32_t)buffer[i * 4 + 3]);
  }
  for (i = 16; i < 64; ++i) {
    uint32_t s0 = SHA256_ROTR(7, w[i - 15]) ^ SHA256_ROTR(18, w[i - 15]) ^
                  (w[i - 15] >> 3);
    uint32_t s1 = SHA256_ROTR(17, w[i - 2]) ^ SHA256_ROTR(19, w[i - 2]) ^
                  (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];

  for (i = 0; i < 64; ++i) {
    uint32_t S1 = SHA256_ROTR(6, e) ^ SHA256_ROTR(11, e) ^ SHA256_ROTR(25, e);
    uint32_t ch = (e & f) ^ ((~e) & g);
    uint32_t temp1 = h + S1 + ch + k_sha256[i] + w[i];
    uint32_t S0 = SHA256_ROTR(2, a) ^ SHA256_ROTR(13, a) ^ SHA256_ROTR(22, a);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = S0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Initialize SHA-256 digest context */
enum c_abstract_http_error sha256_init(struct sha256_ctx *ctx) {
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_sha1_fail == -99) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif
  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  ctx->state[0] = 0x6a09e667U;
  ctx->state[1] = 0xbb67ae85U;
  ctx->state[2] = 0x3c6ef372U;
  ctx->state[3] = 0xa54ff53aU;
  ctx->state[4] = 0x510e527fU;
  ctx->state[5] = 0x9b05688cU;
  ctx->state[6] = 0x1f83d9abU;
  ctx->state[7] = 0x5be0cd19U;
  ctx->count[0] = 0;
  ctx->count[1] = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Update SHA-256 digest with input bytes */
enum c_abstract_http_error
sha256_update(struct sha256_ctx *ctx, const unsigned char *data, size_t len) {
  size_t i;
  size_t j;
  enum c_abstract_http_error rc;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  if (g_mock_sha1_fail > 0) {
    if (--g_mock_sha1_fail == 0)
      return C_ABSTRACT_HTTP_ERR_IO;
  }
#endif

  if (!ctx) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (len > 0 && !data) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (len == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }

  j = (ctx->count[0] >> 3) & 63U;
  if ((ctx->count[0] += (uint32_t)(len << 3)) < (uint32_t)(len << 3)) {
    ctx->count[1]++;
  }
  ctx->count[1] += (uint32_t)(len >> 29);

  if ((j + len) > 63U) {
    i = 64U - j;
    memcpy(&ctx->buffer[j], data, i);
    rc = math_sha256_transform(ctx->state, ctx->buffer);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
    for (; i + 63U < len; i += 64U) {
      rc = math_sha256_transform(ctx->state, &data[i]);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        return rc;
      }
    }
    j = 0;
  } else {
    i = 0;
  }

  memcpy(&ctx->buffer[j], &data[i], len - i);
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Finalize SHA-256 digest */
enum c_abstract_http_error sha256_final(struct sha256_ctx *ctx,
                                        unsigned char out_hash[32]) {
  unsigned char finalcount[8];
  unsigned char c;
  int i;
  enum c_abstract_http_error rc;

  if (!ctx || !out_hash) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  for (i = 0; i < 8; ++i) {
    finalcount[i] =
        (unsigned char)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) &
                        255U);
  }
  c = 0x80U;
  rc = sha256_update(ctx, &c, 1);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  c = 0x00U;
  while ((ctx->count[0] & 504U) != 448U) {
    rc = sha256_update(ctx, &c, 1);
    if (rc != C_ABSTRACT_HTTP_SUCCESS) {
      return rc;
    }
  }
  rc = sha256_update(ctx, finalcount, 8);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  for (i = 0; i < 32; ++i) {
    out_hash[i] =
        (unsigned char)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255U);
  }
  memset(ctx, 0, sizeof(*ctx));
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Compute one-shot SHA-256 digest via active provider or FIPS 180-4 */
enum c_abstract_http_error
c_abstract_http_sha256_oneshot(const unsigned char *data, size_t len,
                               unsigned char out_hash[32]) {
  struct sha256_ctx ctx;
  enum c_abstract_http_error rc;

  if (!out_hash || (len > 0 && !data)) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
#if defined(CAH_HAVE_OPENSSL_COMPAT_TLS) &&                                    \
    !defined(C_ABSTRACT_HTTP_USE_WOLFSSL)
  {
    unsigned char *res_ptr =
        SHA256(data ? data : (const unsigned char *)"", len, out_hash);
    if (!res_ptr) {
      return C_ABSTRACT_HTTP_ERR_IO;
    }
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#elif defined(CAH_HAVE_MBEDTLS_TLS)
  if (mbedtls_sha256(data ? data : (const unsigned char *)"", len, out_hash,
                     0) != 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(CAH_HAVE_GNUTLS_TLS)
  if (gnutls_hash_fast(GNUTLS_DIG_SHA256,
                       data ? (const void *)data : (const void *)"", len,
                       out_hash) < 0) {
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
#elif defined(C_ABSTRACT_HTTP_USE_COMMONCRYPTO) && defined(__APPLE__)
  {
    unsigned char *cc_ptr = CC_SHA256(
        data ? (const void *)data : (const void *)"", (CC_LONG)len, out_hash);
    (void)cc_ptr;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif
#endif

  rc = sha256_init(&ctx);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  rc = sha256_update(&ctx, data, len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }
  return sha256_final(&ctx, out_hash);
}

/** @brief Return compile-time cryptographic provider name */
enum c_abstract_http_error
c_abstract_http_crypto_provider_name(const char **out_name) {
  if (!out_name) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(C_ABSTRACT_HTTP_USE_BORINGSSL)
  *out_name = "boringssl";
#elif defined(C_ABSTRACT_HTTP_USE_LIBRESSL)
  *out_name = "libressl";
#elif defined(C_ABSTRACT_HTTP_USE_OPENSSL)
  *out_name = "openssl";
#elif defined(C_ABSTRACT_HTTP_USE_MBEDTLS)
  *out_name = "mbedtls";
#elif defined(C_ABSTRACT_HTTP_USE_WOLFSSL)
  *out_name = "wolfssl";
#elif defined(C_ABSTRACT_HTTP_USE_S2N)
  *out_name = "s2n";
#elif defined(C_ABSTRACT_HTTP_USE_BEARSSL)
  *out_name = "bearssl";
#elif defined(C_ABSTRACT_HTTP_USE_GNUTLS)
  *out_name = "gnutls";
#elif defined(C_ABSTRACT_HTTP_USE_BOTAN)
  *out_name = "botan";
#elif defined(C_ABSTRACT_HTTP_USE_WINCRYPT)
  *out_name = "wincrypt";
#elif defined(C_ABSTRACT_HTTP_USE_COMMONCRYPTO)
  *out_name = "commoncrypto";
#else
  *out_name = "builtin";
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Base64 encoding alphabet */
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/** @brief Base64-encode a byte buffer onto the heap */
enum c_abstract_http_error base64_encode(const unsigned char *in, size_t in_len,
                                         char **out_str, size_t *out_len) {
  size_t len;
  char *out;
  size_t i = 0;
  size_t j = 0;

  if ((!in && in_len > 0) || !out_str || !out_len)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  len = 4 * ((in_len + 2) / 3);
  out = (char *)malloc(len + 1);
  if (!out)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  for (; i < in_len;) {
    uint32_t octet_a = i < in_len ? in[i++] : 0;
    uint32_t octet_b = i < in_len ? in[i++] : 0;
    uint32_t octet_c = i < in_len ? in[i++] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    out[j++] = b64_table[(triple >> 18) & 0x3F];
    out[j++] = b64_table[(triple >> 12) & 0x3F];
    out[j++] = b64_table[(triple >> 6) & 0x3F];
    out[j++] = b64_table[triple & 0x3F];
  }

  if (in_len % 3 == 1) {
    out[len - 1] = '=';
    out[len - 2] = '=';
  } else if (in_len % 3 == 2) {
    out[len - 1] = '=';
  }

  out[len] = '\0';
  *out_str = out;
  *out_len = len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Base64 decoding lookup table */
static const int b64_index[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 0,  0,  0,  0,  63, 0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

/** @brief Decode a Base64 string into a newly allocated byte buffer */
enum c_abstract_http_error base64_decode(const char *in, size_t in_len,
                                         unsigned char **out_data,
                                         size_t *out_len) {
  size_t len;
  unsigned char *out;
  size_t i = 0;
  size_t j = 0;

  if ((!in && in_len > 0) || !out_data || !out_len)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (in_len % 4 != 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  len = in_len / 4 * 3;
  if (in_len > 0 && in[in_len - 1] == '=')
    len--;
  if (in_len > 1 && in[in_len - 2] == '=')
    len--;

  out = (unsigned char *)malloc(len > 0 ? len : 1);
  if (!out)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  for (; i < in_len;) {
    uint32_t a =
        in[i] == '=' ? 0 & i++ : (uint32_t)b64_index[(unsigned char)in[i++]];
    uint32_t b =
        in[i] == '=' ? 0 & i++ : (uint32_t)b64_index[(unsigned char)in[i++]];
    uint32_t c =
        in[i] == '=' ? 0 & i++ : (uint32_t)b64_index[(unsigned char)in[i++]];
    uint32_t d =
        in[i] == '=' ? 0 & i++ : (uint32_t)b64_index[(unsigned char)in[i++]];
    uint32_t triple = (a << 18) + (b << 12) + (c << 6) + d;

    if (j < len)
      out[j++] = (unsigned char)((triple >> 16) & 0xFF);
    if (j < len)
      out[j++] = (unsigned char)((triple >> 8) & 0xFF);
    if (j < len)
      out[j++] = (unsigned char)(triple & 0xFF);
  }

  *out_data = out;
  *out_len = len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

/** @brief Constant-time string equality check */
int math_is_const_time_streq(const char *a, const char *b) {
  size_t len_a = strlen(a);
  size_t len_b = strlen(b);
  size_t i;
  unsigned char result = 0;

  if (len_a != len_b) {
    return 0;
  }

  for (i = 0; i < len_a; i++) {
    result |= (unsigned char)(a[i] ^ b[i]);
  }

  return result == 0;
}

/**
 * @brief Convert an ASCII character to lowercase.
 * @param ch Input byte.
 * @return Lowercase ASCII byte.
 */
static unsigned char math_crypto_tolower(unsigned char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (unsigned char)(ch + ('a' - 'A'));
  }
  return ch;
}

/** @brief Verify DER SPKI bytes against a SHA-256 Base64 or Hex pin */
enum c_abstract_http_error c_abstract_http_verify_spki_pin_sha256(
    const unsigned char *spki_der, size_t spki_len, const char *expected_pin) {
  static const char hex_tbl[] = "0123456789abcdef";
  unsigned char digest[32];
  char hex_digest[65];
  char norm_pin[65];
  char *b64_digest = NULL;
  size_t b64_len = 0;
  const char *pin_body;
  size_t i;
  int matched = 0;
  enum c_abstract_http_error rc;

  if (!spki_der || spki_len == 0 || !expected_pin || expected_pin[0] == '\0') {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  rc = c_abstract_http_sha256_oneshot(spki_der, spki_len, digest);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  pin_body = expected_pin;
  if (strncmp(pin_body, "sha256//", 8) == 0) {
    pin_body += 8;
  } else if (strncmp(pin_body, "sha256/", 7) == 0) {
    pin_body += 7;
  }

  rc = base64_encode(digest, 32, &b64_digest, &b64_len);
  if (rc != C_ABSTRACT_HTTP_SUCCESS) {
    return rc;
  }

  if (math_is_const_time_streq(b64_digest, pin_body)) {
    matched = 1;
  }
  free(b64_digest);

  for (i = 0; i < 32; ++i) {
    hex_digest[i * 2] = hex_tbl[(digest[i] >> 4) & 0x0FU];
    hex_digest[i * 2 + 1] = hex_tbl[digest[i] & 0x0FU];
  }
  hex_digest[64] = '\0';

  if (strlen(pin_body) == 64) {
    for (i = 0; i < 64; ++i) {
      norm_pin[i] = (char)math_crypto_tolower((unsigned char)pin_body[i]);
    }
    norm_pin[64] = '\0';
    if (math_is_const_time_streq(hex_digest, norm_pin)) {
      matched = 1;
    }
  }

  return matched ? C_ABSTRACT_HTTP_SUCCESS : C_ABSTRACT_HTTP_ERR_ACCES;
}

/**
 * @brief Match a peer hostname against an X.509 certificate CN / SAN pattern
 * per RFC 6125 (single leftmost label wildcard `*.example.com`).
 * @param host Target peer hostname.
 * @param pattern Certificate CN or DNS-ID SAN pattern.
 * @return 1 if @p host matches @p pattern, 0 otherwise.
 */
static int math_tls_hostname_matches(const char *host, const char *pattern) {
  size_t hlen;
  size_t plen;
  size_t i;

  if (!host || !pattern || host[0] == '\0' || pattern[0] == '\0') {
    return 0;
  }
  if (pattern[0] == '*' && pattern[1] == '.') {
    const char *suffix = pattern + 1;
    const char *first_dot = strchr(host, '.');
    size_t slen = strlen(suffix);
    if (!first_dot || first_dot == host) {
      return 0;
    }
    if (strlen(first_dot) != slen) {
      return 0;
    }
    for (i = 0; i < slen; ++i) {
      if (math_crypto_tolower((unsigned char)first_dot[i]) !=
          math_crypto_tolower((unsigned char)suffix[i])) {
        return 0;
      }
    }
    return 1;
  }

  hlen = strlen(host);
  plen = strlen(pattern);
  if (hlen != plen) {
    return 0;
  }
  for (i = 0; i < hlen; ++i) {
    if (math_crypto_tolower((unsigned char)host[i]) !=
        math_crypto_tolower((unsigned char)pattern[i])) {
      return 0;
    }
  }
  return 1;
}

/** @brief Validate TLS handshake config, X.509 credentials, and SPKI pin */
enum c_abstract_http_error c_abstract_http_tls_verify_handshake_config(
    const struct HttpConfig *config, const char *peer_host, const char *cert_cn,
    const unsigned char *spki_der, size_t spki_len) {
  if (!config || !peer_host || peer_host[0] == '\0') {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->tls_version_mask != 0 &&
      (config->tls_version_mask & (unsigned int)HTTP_TLS_VERSION_ANY) == 0) {
    return C_ABSTRACT_HTTP_ERR_NOTSUP;
  }
  if ((config->client_cert_path && !config->client_key_path) ||
      (!config->client_cert_path && config->client_key_path)) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->ca_buffer_len > 0 && !config->ca_buffer) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (config->verify_peer) {
    if (!cert_cn || cert_cn[0] == '\0') {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  }
  if (config->verify_host && cert_cn) {
    if (!math_tls_hostname_matches(peer_host, cert_cn)) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
  }
  if (config->cert_pin_sha256) {
    if (!spki_der || spki_len == 0) {
      return C_ABSTRACT_HTTP_ERR_ACCES;
    }
    return c_abstract_http_verify_spki_pin_sha256(spki_der, spki_len,
                                                  config->cert_pin_sha256);
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

#if !defined(C_ABSTRACT_HTTP_TEST_OOM) && defined(CAH_HAVE_OPENSSL_COMPAT_TLS)
/** @brief OpenSSL/LibreSSL/BoringSSL/WolfSSL concrete TLS session state */
struct cah_ossl_tls_session {
  SSL_CTX *ssl_ctx;
  SSL *ssl;
  char *cert_pin;
  int verify_peer;
  int handshake_done;
};

static enum c_abstract_http_error
cah_ossl_tls_open(void *provider_data, int sock_fd, const char *host,
                  const struct HttpConfig *config, void **out_session) {
  struct cah_ossl_tls_session *s;
  enum c_abstract_http_error rc;
  (void)provider_data;

  s = (struct cah_ossl_tls_session *)calloc(1, sizeof(*s));
  if (!s) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  s->ssl_ctx = SSL_CTX_new(TLS_client_method());
  if (!s->ssl_ctx) {
    free(s);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  if (config) {
    s->verify_peer = config->verify_peer;
    SSL_CTX_set_verify(s->ssl_ctx,
                       config->verify_peer ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                       NULL);
    if (config->ca_path) {
      if (SSL_CTX_load_verify_locations(s->ssl_ctx, config->ca_path, NULL) !=
          1) {
        SSL_CTX_free(s->ssl_ctx);
        free(s);
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
    } else if (config->verify_peer) {
      int def_rc = SSL_CTX_set_default_verify_paths(s->ssl_ctx);
      (void)def_rc;
    }
#if !defined(C_ABSTRACT_HTTP_USE_WOLFSSL)
    if (config->ca_buffer && config->ca_buffer_len > 0) {
      BIO *cbio = BIO_new_mem_buf((const void *)config->ca_buffer,
                                  (int)config->ca_buffer_len);
      X509_STORE *store = SSL_CTX_get_cert_store(s->ssl_ctx);
      if (cbio && store) {
        X509 *cacert = PEM_read_bio_X509(cbio, NULL, 0, NULL);
        if (cacert) {
          int add_rc = X509_STORE_add_cert(store, cacert);
          (void)add_rc;
          X509_free(cacert);
        }
      }
      if (cbio) {
        int brc = BIO_free(cbio);
        (void)brc;
      }
    }
#endif
    if (config->client_cert_path && config->client_key_path) {
      if (SSL_CTX_use_certificate_chain_file(s->ssl_ctx,
                                             config->client_cert_path) != 1 ||
          SSL_CTX_use_PrivateKey_file(s->ssl_ctx, config->client_key_path,
                                      SSL_FILETYPE_PEM) != 1) {
        SSL_CTX_free(s->ssl_ctx);
        free(s);
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
    }
    if (config->cert_pin_sha256) {
      rc = c_abstract_http_strdup(config->cert_pin_sha256, &s->cert_pin);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        SSL_CTX_free(s->ssl_ctx);
        free(s);
        return rc;
      }
    }
  }

  s->ssl = SSL_new(s->ssl_ctx);
  if (!s->ssl) {
    free(s->cert_pin);
    SSL_CTX_free(s->ssl_ctx);
    free(s);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (SSL_set_fd(s->ssl, sock_fd) != 1) {
    SSL_free(s->ssl);
    free(s->cert_pin);
    SSL_CTX_free(s->ssl_ctx);
    free(s);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (host) {
    long sni_rc = SSL_set_tlsext_host_name(s->ssl, host);
    (void)sni_rc;
#if !defined(C_ABSTRACT_HTTP_USE_WOLFSSL) && !defined(LIBRESSL_VERSION_NUMBER)
    if (config && config->verify_host) {
      int hrc = SSL_set1_host(s->ssl, host);
      (void)hrc;
    }
#endif
  }
  *out_session = s;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error cah_ossl_tls_handshake(void *session,
                                                         int *out_want) {
  struct cah_ossl_tls_session *s = (struct cah_ossl_tls_session *)session;
  int rv;
  *out_want = 0;
  if (s->handshake_done) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  rv = SSL_connect(s->ssl);
  if (rv == 1) {
    if (s->cert_pin) {
      X509 *peer = SSL_get_peer_certificate(s->ssl);
      unsigned char *spki_der = NULL;
      int spki_len;
      enum c_abstract_http_error prc;
      if (!peer) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
      spki_len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(peer), &spki_der);
      X509_free(peer);
      if (spki_len <= 0 || !spki_der) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
      prc = c_abstract_http_verify_spki_pin_sha256(spki_der, (size_t)spki_len,
                                                   s->cert_pin);
      OPENSSL_free(spki_der);
      if (prc != C_ABSTRACT_HTTP_SUCCESS) {
        return prc;
      }
    }
    s->handshake_done = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  {
    int err = SSL_get_error(s->ssl, rv);
    if (err == SSL_ERROR_WANT_READ) {
      *out_want = 1;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (err == SSL_ERROR_WANT_WRITE) {
      *out_want = 2;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  return s->verify_peer ? C_ABSTRACT_HTTP_ERR_ACCES : C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_ossl_tls_read(void *session, void *buf,
                                                    size_t cap, size_t *out_n,
                                                    int *out_want) {
  struct cah_ossl_tls_session *s = (struct cah_ossl_tls_session *)session;
  int rv = SSL_read(s->ssl, buf, (int)cap);
  *out_want = 0;
  *out_n = 0;
  if (rv > 0) {
    *out_n = (size_t)rv;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  {
    int err = SSL_get_error(s->ssl, rv);
    if (err == SSL_ERROR_WANT_READ) {
      *out_want = 1;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (err == SSL_ERROR_WANT_WRITE) {
      *out_want = 2;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_ossl_tls_write(void *session,
                                                     const void *buf,
                                                     size_t len, size_t *out_n,
                                                     int *out_want) {
  struct cah_ossl_tls_session *s = (struct cah_ossl_tls_session *)session;
  int rv = SSL_write(s->ssl, buf, (int)len);
  *out_want = 0;
  *out_n = 0;
  if (rv > 0) {
    *out_n = (size_t)rv;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  {
    int err = SSL_get_error(s->ssl, rv);
    if (err == SSL_ERROR_WANT_READ) {
      *out_want = 1;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
    if (err == SSL_ERROR_WANT_WRITE) {
      *out_want = 2;
      return C_ABSTRACT_HTTP_SUCCESS;
    }
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_ossl_tls_close(void *session) {
  struct cah_ossl_tls_session *s = (struct cah_ossl_tls_session *)session;
  if (!s) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (s->ssl) {
    int sd = SSL_shutdown(s->ssl);
    (void)sd;
    SSL_free(s->ssl);
  }
  if (s->ssl_ctx) {
    SSL_CTX_free(s->ssl_ctx);
  }
  free(s->cert_pin);
  free(s);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static const struct HttpRawTlsHooks g_cah_default_tls_hooks = {
    NULL,
    cah_ossl_tls_open,
    cah_ossl_tls_handshake,
    cah_ossl_tls_read,
    cah_ossl_tls_write,
    cah_ossl_tls_close};
#define CAH_HAS_DEFAULT_TLS_HOOKS 1
#elif !defined(C_ABSTRACT_HTTP_TEST_OOM) && defined(CAH_HAVE_MBEDTLS_TLS)
/** @brief mbedTLS concrete TLS session state */
struct cah_mbed_tls_session {
  int sock_fd;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cacert;
  mbedtls_x509_crt clicert;
  mbedtls_pk_context clikey;
  char *cert_pin;
  int verify_peer;
  int handshake_done;
};

static int cah_mbed_bio_send(void *ctx, const unsigned char *buf, size_t len) {
  int fd = *(int *)ctx;
  long rv = (long)send(fd, (const char *)buf, (int)len, 0);
  if (rv < 0) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }
  return (int)rv;
}

static int cah_mbed_bio_recv(void *ctx, unsigned char *buf, size_t len) {
  int fd = *(int *)ctx;
  long rv = (long)recv(fd, (char *)buf, (int)len, 0);
  if (rv < 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  return (int)rv;
}

static enum c_abstract_http_error cah_mbed_tls_close(void *session) {
  struct cah_mbed_tls_session *s = (struct cah_mbed_tls_session *)session;
  if (!s) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  mbedtls_ssl_free(&s->ssl);
  mbedtls_ssl_config_free(&s->conf);
  mbedtls_x509_crt_free(&s->cacert);
  mbedtls_x509_crt_free(&s->clicert);
  mbedtls_pk_free(&s->clikey);
  mbedtls_ctr_drbg_free(&s->ctr_drbg);
  mbedtls_entropy_free(&s->entropy);
  free(s->cert_pin);
  free(s);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error
cah_mbed_tls_open(void *provider_data, int sock_fd, const char *host,
                  const struct HttpConfig *config, void **out_session) {
  struct cah_mbed_tls_session *s;
  enum c_abstract_http_error rc;
  (void)provider_data;

  s = (struct cah_mbed_tls_session *)calloc(1, sizeof(*s));
  if (!s) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  s->sock_fd = sock_fd;
  mbedtls_entropy_init(&s->entropy);
  mbedtls_ctr_drbg_init(&s->ctr_drbg);
  mbedtls_ssl_init(&s->ssl);
  mbedtls_ssl_config_init(&s->conf);
  mbedtls_x509_crt_init(&s->cacert);
  mbedtls_x509_crt_init(&s->clicert);
  mbedtls_pk_init(&s->clikey);

  if (mbedtls_ctr_drbg_seed(&s->ctr_drbg, mbedtls_entropy_func, &s->entropy,
                            NULL, 0) != 0 ||
      mbedtls_ssl_config_defaults(&s->conf, MBEDTLS_SSL_IS_CLIENT,
                                  MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    enum c_abstract_http_error cl = cah_mbed_tls_close(s);
    (void)cl;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &s->ctr_drbg);

  if (config) {
    s->verify_peer = config->verify_peer;
    mbedtls_ssl_conf_authmode(&s->conf, config->verify_peer
                                            ? MBEDTLS_SSL_VERIFY_REQUIRED
                                            : MBEDTLS_SSL_VERIFY_NONE);
    if (config->ca_path) {
      if (mbedtls_x509_crt_parse_file(&s->cacert, config->ca_path) != 0) {
        enum c_abstract_http_error cl = cah_mbed_tls_close(s);
        (void)cl;
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
      mbedtls_ssl_conf_ca_chain(&s->conf, &s->cacert, NULL);
    }
    if (config->ca_buffer && config->ca_buffer_len > 0) {
      if (mbedtls_x509_crt_parse(&s->cacert, config->ca_buffer,
                                 config->ca_buffer_len) == 0) {
        mbedtls_ssl_conf_ca_chain(&s->conf, &s->cacert, NULL);
      }
    }
    if (config->cert_pin_sha256) {
      rc = c_abstract_http_strdup(config->cert_pin_sha256, &s->cert_pin);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        enum c_abstract_http_error cl = cah_mbed_tls_close(s);
        (void)cl;
        return rc;
      }
    }
  }

  if (mbedtls_ssl_setup(&s->ssl, &s->conf) != 0) {
    enum c_abstract_http_error cl = cah_mbed_tls_close(s);
    (void)cl;
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  if (host) {
    int hrc = mbedtls_ssl_set_hostname(&s->ssl, host);
    (void)hrc;
  }
  mbedtls_ssl_set_bio(&s->ssl, &s->sock_fd, cah_mbed_bio_send,
                      cah_mbed_bio_recv, NULL);
  *out_session = s;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error cah_mbed_tls_handshake(void *session,
                                                         int *out_want) {
  struct cah_mbed_tls_session *s = (struct cah_mbed_tls_session *)session;
  int rv;
  *out_want = 0;
  if (s->handshake_done) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  rv = mbedtls_ssl_handshake(&s->ssl);
  if (rv == 0) {
    if (s->cert_pin) {
      const mbedtls_x509_crt *peer = mbedtls_ssl_get_peer_cert(&s->ssl);
      unsigned char spki_buf[4096];
      int spki_len;
      if (!peer) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
      spki_len = mbedtls_pk_write_pubkey_der(
          (mbedtls_pk_context *)(size_t)&peer->pk, spki_buf, sizeof(spki_buf));
      if (spki_len <= 0) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
      if (c_abstract_http_verify_spki_pin_sha256(
              spki_buf + (sizeof(spki_buf) - (size_t)spki_len),
              (size_t)spki_len, s->cert_pin) != C_ABSTRACT_HTTP_SUCCESS) {
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
    }
    s->handshake_done = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == MBEDTLS_ERR_SSL_WANT_READ) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == MBEDTLS_ERR_SSL_WANT_WRITE) {
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return s->verify_peer ? C_ABSTRACT_HTTP_ERR_ACCES : C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_mbed_tls_read(void *session, void *buf,
                                                    size_t cap, size_t *out_n,
                                                    int *out_want) {
  struct cah_mbed_tls_session *s = (struct cah_mbed_tls_session *)session;
  int rv = mbedtls_ssl_read(&s->ssl, (unsigned char *)buf, cap);
  *out_want = 0;
  *out_n = 0;
  if (rv > 0) {
    *out_n = (size_t)rv;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == 0 || rv == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == MBEDTLS_ERR_SSL_WANT_READ) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == MBEDTLS_ERR_SSL_WANT_WRITE) {
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_mbed_tls_write(void *session,
                                                     const void *buf,
                                                     size_t len, size_t *out_n,
                                                     int *out_want) {
  struct cah_mbed_tls_session *s = (struct cah_mbed_tls_session *)session;
  int rv = mbedtls_ssl_write(&s->ssl, (const unsigned char *)buf, len);
  *out_want = 0;
  *out_n = 0;
  if (rv > 0) {
    *out_n = (size_t)rv;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == MBEDTLS_ERR_SSL_WANT_READ) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == MBEDTLS_ERR_SSL_WANT_WRITE) {
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

static const struct HttpRawTlsHooks g_cah_default_tls_hooks = {
    NULL,
    cah_mbed_tls_open,
    cah_mbed_tls_handshake,
    cah_mbed_tls_read,
    cah_mbed_tls_write,
    cah_mbed_tls_close};
#define CAH_HAS_DEFAULT_TLS_HOOKS 1
#elif !defined(C_ABSTRACT_HTTP_TEST_OOM) && defined(CAH_HAVE_GNUTLS_TLS)
/** @brief GnuTLS concrete TLS session state */
struct cah_gtls_session {
  gnutls_certificate_credentials_t xcred;
  gnutls_session_t session;
  char *cert_pin;
  int verify_peer;
  int handshake_done;
};

static enum c_abstract_http_error cah_gtls_close(void *session) {
  struct cah_gtls_session *s = (struct cah_gtls_session *)session;
  if (!s) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  gnutls_deinit(s->session);
  gnutls_certificate_free_credentials(s->xcred);
  free(s->cert_pin);
  free(s);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error cah_gtls_open(void *provider_data,
                                                int sock_fd, const char *host,
                                                const struct HttpConfig *config,
                                                void **out_session) {
  struct cah_gtls_session *s;
  enum c_abstract_http_error rc;
  (void)provider_data;

  s = (struct cah_gtls_session *)calloc(1, sizeof(*s));
  if (!s) {
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }
  if (gnutls_certificate_allocate_credentials(&s->xcred) < 0 ||
      gnutls_init(&s->session, GNUTLS_CLIENT) < 0) {
    free(s);
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  gnutls_set_default_priority(s->session);
  if (config) {
    s->verify_peer = config->verify_peer;
    if (config->ca_path) {
      if (gnutls_certificate_set_x509_trust_file(s->xcred, config->ca_path,
                                                 GNUTLS_X509_FMT_PEM) < 0) {
        enum c_abstract_http_error cl = cah_gtls_close(s);
        (void)cl;
        return C_ABSTRACT_HTTP_ERR_ACCES;
      }
    } else if (config->verify_peer) {
      int trc = gnutls_certificate_set_x509_system_trust(s->xcred);
      (void)trc;
    }
    if (config->cert_pin_sha256) {
      rc = c_abstract_http_strdup(config->cert_pin_sha256, &s->cert_pin);
      if (rc != C_ABSTRACT_HTTP_SUCCESS) {
        enum c_abstract_http_error cl = cah_gtls_close(s);
        (void)cl;
        return rc;
      }
    }
  }
  gnutls_credentials_set(s->session, GNUTLS_CRD_CERTIFICATE, s->xcred);
  if (host) {
    int snrc =
        gnutls_server_name_set(s->session, GNUTLS_NAME_DNS, host, strlen(host));
    (void)snrc;
    if (config && config->verify_peer) {
      gnutls_session_set_verify_cert(s->session,
                                     config->verify_host ? host : NULL, 0);
    }
  }
  gnutls_transport_set_int(s->session, sock_fd);
  *out_session = s;
  return C_ABSTRACT_HTTP_SUCCESS;
}

static enum c_abstract_http_error cah_gtls_handshake(void *session,
                                                     int *out_want) {
  struct cah_gtls_session *s = (struct cah_gtls_session *)session;
  int rv;
  *out_want = 0;
  if (s->handshake_done) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  rv = gnutls_handshake(s->session);
  if (rv == GNUTLS_E_SUCCESS) {
    s->handshake_done = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == GNUTLS_E_AGAIN || rv == GNUTLS_E_INTERRUPTED) {
    *out_want = gnutls_record_get_direction(s->session) ? 2 : 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return s->verify_peer ? C_ABSTRACT_HTTP_ERR_ACCES : C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_gtls_read(void *session, void *buf,
                                                size_t cap, size_t *out_n,
                                                int *out_want) {
  struct cah_gtls_session *s = (struct cah_gtls_session *)session;
  long rv = (long)gnutls_record_recv(s->session, buf, cap);
  *out_want = 0;
  *out_n = 0;
  if (rv > 0) {
    *out_n = (size_t)rv;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == 0) {
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == GNUTLS_E_AGAIN || rv == GNUTLS_E_INTERRUPTED) {
    *out_want = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

static enum c_abstract_http_error cah_gtls_write(void *session, const void *buf,
                                                 size_t len, size_t *out_n,
                                                 int *out_want) {
  struct cah_gtls_session *s = (struct cah_gtls_session *)session;
  long rv = (long)gnutls_record_send(s->session, buf, len);
  *out_want = 0;
  *out_n = 0;
  if (rv > 0) {
    *out_n = (size_t)rv;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (rv == GNUTLS_E_AGAIN || rv == GNUTLS_E_INTERRUPTED) {
    *out_want = 2;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  return C_ABSTRACT_HTTP_ERR_IO;
}

static const struct HttpRawTlsHooks g_cah_default_tls_hooks = {
    NULL,          cah_gtls_open,  cah_gtls_handshake,
    cah_gtls_read, cah_gtls_write, cah_gtls_close};
#define CAH_HAS_DEFAULT_TLS_HOOKS 1
#endif

/** @brief Retrieve active compile-time TLS socket provider hooks */
enum c_abstract_http_error c_abstract_http_get_default_tls_hooks(
    const struct HttpRawTlsHooks **out_hooks) {
  if (!out_hooks) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
#if defined(CAH_HAS_DEFAULT_TLS_HOOKS)
  *out_hooks = &g_cah_default_tls_hooks;
#else
  *out_hooks = NULL;
#endif
  return C_ABSTRACT_HTTP_SUCCESS;
}
