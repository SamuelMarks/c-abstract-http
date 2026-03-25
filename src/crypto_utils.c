/* clang-format off */
#include "crypto_utils.h"
#include <stdlib.h>
#include <string.h>
/* clang-format on */

/* SHA-1 implementation based on RFC 3174 */
#define SHA1_ROTL(bits, word) (((word) << (bits)) | ((word) >> (32 - (bits))))

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
  uint32_t a, b, c, d, e;
  uint32_t w[80];
  int i;

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
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
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
}

int sha1_init(struct sha1_ctx *ctx) {
  if (!ctx)
    return -1;
  ctx->count[0] = ctx->count[1] = 0;
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->state[4] = 0xC3D2E1F0;
  return 0;
}

int sha1_update(struct sha1_ctx *ctx, const unsigned char *data, size_t len) {
  uint32_t i;
  uint32_t j;

  if (!ctx || (!data && len > 0))
    return -1;

  j = (ctx->count[0] >> 3) & 63;
  if ((ctx->count[0] += (uint32_t)(len << 3)) < (len << 3)) {
    ctx->count[1]++;
  }
  ctx->count[1] += (uint32_t)(len >> 29);

  if ((j + len) > 63) {
    i = 64 - j;
    memcpy(&ctx->buffer[j], data, i);
    sha1_transform(ctx->state, ctx->buffer);
    for (; i + 63 < len; i += 64) {
      sha1_transform(ctx->state, &data[i]);
    }
    j = 0;
  } else {
    i = 0;
  }

  memcpy(&ctx->buffer[j], &data[i], len - i);
  return 0;
}

int sha1_final(struct sha1_ctx *ctx, unsigned char out_hash[20]) {
  unsigned char finalcount[8];
  unsigned char c;
  int i;

  if (!ctx || !out_hash)
    return -1;

  for (i = 0; i < 8; i++) {
    finalcount[i] =
        (unsigned char)((ctx->count[(i >= 4 ? 0 : 1)] >> ((3 - (i & 3)) * 8)) &
                        255);
  }
  c = 0200;
  sha1_update(ctx, &c, 1);
  while ((ctx->count[0] & 504) != 448) {
    c = 0000;
    sha1_update(ctx, &c, 1);
  }
  sha1_update(ctx, finalcount, 8);
  for (i = 0; i < 20; i++) {
    out_hash[i] =
        (unsigned char)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
  }

  memset(ctx, 0, sizeof(*ctx));
  return 0;
}

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(const unsigned char *in, size_t in_len, char **out_str,
                  size_t *out_len) {
  size_t len;
  size_t i;
  size_t j;
  char *out;

  if (!out_str || !out_len || (!in && in_len > 0))
    return -1;

  len = 4 * ((in_len + 2) / 3);
  out = (char *)malloc(len + 1);
  if (!out)
    return 12; /* ENOMEM fallback */

  for (i = 0, j = 0; i < in_len;) {
    uint32_t octet_a, octet_b, octet_c, triple;
    octet_a = i < in_len ? (unsigned char)in[i++] : 0;
    octet_b = i < in_len ? (unsigned char)in[i++] : 0;
    octet_c = i < in_len ? (unsigned char)in[i++] : 0;
    triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    out[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
    out[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
    out[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
    out[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
  }

  for (i = 0; i < (3 - in_len % 3) % 3; i++) {
    out[len - 1 - i] = '=';
  }

  out[len] = '\0';
  *out_str = out;
  *out_len = len;

  return 0;
}

static const unsigned char base64_decode_table[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64,
    64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64};

int base64_decode(const char *in, size_t in_len, unsigned char **out_data,
                  size_t *out_len) {
  size_t i;
  size_t j;
  size_t out_size;
  unsigned char *out;

  if (!out_data || !out_len || (!in && in_len > 0))
    return -1;
  if (in_len % 4 != 0)
    return 22; /* EINVAL fallback */

  out_size = in_len / 4 * 3;
  if (in_len > 0 && in[in_len - 1] == '=')
    out_size--;
  if (in_len > 1 && in[in_len - 2] == '=')
    out_size--;

  out = (unsigned char *)malloc(out_size);
  if (!out)
    return 12; /* ENOMEM fallback */

  for (i = 0, j = 0; i < in_len;) {
    uint32_t sextet_a, sextet_b, sextet_c, sextet_d, triple;
    sextet_a =
        in[i] == '=' ? 0 : base64_decode_table[(unsigned char)in[i]];
    i++;
    sextet_b =
        in[i] == '=' ? 0 : base64_decode_table[(unsigned char)in[i]];
    i++;
    sextet_c =
        in[i] == '=' ? 0 : base64_decode_table[(unsigned char)in[i]];
    i++;
    sextet_d =
        in[i] == '=' ? 0 : base64_decode_table[(unsigned char)in[i]];
    i++;

    triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) +
                      (sextet_c << 1 * 6) + (sextet_d << 0 * 6);

    if (j < out_size)
      out[j++] = (triple >> 2 * 8) & 0xFF;
    if (j < out_size)
      out[j++] = (triple >> 1 * 8) & 0xFF;
    if (j < out_size)
      out[j++] = (triple >> 0 * 8) & 0xFF;
  }

  *out_data = out;
  *out_len = out_size;

  return 0;
}

int const_time_streq(const char *a, const char *b) {
  size_t len_a = strlen(a);
  size_t len_b = strlen(b);
  size_t i;
  int diff = 0;

  if (len_a != len_b)
    return 0;

  for (i = 0; i < len_a; i++) {
    diff |= (a[i] ^ b[i]);
  }

  return diff == 0;
}
