
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

/* clang-format off */
#ifdef C_ABSTRACT_HTTP_HEADER_ONLY
#define C_ABSTRACT_HTTP_IMPLEMENTATION
#include <c_abstract_http/c_abstract_http.h>
#endif
#include "../src/crypto_utils.h"
#include "greatest.h"
#include <errno.h>
#include <string.h>

#include "mock_alloc.h"
/* clang-format on */

TEST test_sha1_empty_string(void) {
  struct sha1_ctx ctx;
  unsigned char hash[20];
  const unsigned char expected[20] = {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b,
                                      0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60,
                                      0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            sha1_update(&ctx, (const unsigned char *)"", 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_final(&ctx, hash));

  ASSERT_MEM_EQ(expected, hash, 20);
  PASS();
}

TEST test_sha1_fox_string(void) {
  struct sha1_ctx ctx;
  unsigned char hash[20];
  const char *input = "The quick brown fox jumps over the lazy dog";
  const unsigned char expected[20] = {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28,
                                      0xfc, 0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76,
                                      0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            sha1_update(&ctx, (const unsigned char *)input, strlen(input)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_final(&ctx, hash));

  ASSERT_MEM_EQ(expected, hash, 20);
  PASS();
}

TEST test_sha256_vectors_and_spki_pinning(void) {
  struct sha256_ctx ctx;
  unsigned char hash[32];
  unsigned char oneshot[32];
  unsigned char big[140];
  const char *prov = NULL;
  const struct HttpRawTlsHooks *hooks = NULL;
  struct HttpConfig cfg;
  const unsigned char expected_empty[32] = {
      0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
      0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
      0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
  const unsigned char expected_abc[32] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
      0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
      0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha256_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            sha256_update(NULL, (const unsigned char *)"a", 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha256_update(&ctx, NULL, 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_update(&ctx, NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha256_final(NULL, hash));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha256_final(&ctx, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_final(&ctx, hash));
  ASSERT_MEM_EQ(expected_empty, hash, 32);

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_sha256_oneshot(NULL, 1, oneshot));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_sha256_oneshot((const unsigned char *)"a", 1, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_sha256_oneshot(NULL, 0, oneshot));
  ASSERT_MEM_EQ(expected_empty, oneshot, 32);

  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_sha256_oneshot((const unsigned char *)"abc", 3, oneshot));
  ASSERT_MEM_EQ(expected_abc, oneshot, 32);

  /* Multi-block + bitcount rollover */
  memset(big, 'Z', sizeof(big));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_update(&ctx, big, sizeof(big)));
  ctx.count[0] = 0xFFFFFFF8U;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            sha256_update(&ctx, (const unsigned char *)"a", 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_final(&ctx, hash));

  /* Provider name & default TLS hooks */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_crypto_provider_name(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_crypto_provider_name(&prov));
  ASSERT(prov != NULL && prov[0] != '\0');

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_get_default_tls_hooks(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_get_default_tls_hooks(&hooks));
#if !defined(C_ABSTRACT_HTTP_TEST_OOM) &&                                      \
    (defined(__APPLE__) || defined(_WIN32))
  ASSERT(hooks != NULL);
  ASSERT(hooks->open != NULL && hooks->handshake != NULL &&
         hooks->read != NULL && hooks->write != NULL && hooks->close != NULL);
#endif

  /* SPKI SHA-256 pinning (sha256//base64, sha256/base64, raw base64, hex) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_verify_spki_pin_sha256(NULL, 3, "pin"));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_verify_spki_pin_sha256((const unsigned char *)"abc",
                                                   0, "pin"));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_verify_spki_pin_sha256((const unsigned char *)"abc",
                                                   3, NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_verify_spki_pin_sha256((const unsigned char *)"abc",
                                                   3, ""));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_verify_spki_pin_sha256(
                (const unsigned char *)"abc", 3,
                "sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_verify_spki_pin_sha256(
                (const unsigned char *)"abc", 3,
                "sha256/ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_verify_spki_pin_sha256(
                (const unsigned char *)"abc", 3,
                "ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_verify_spki_pin_sha256(
          (const unsigned char *)"abc", 3,
          "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_verify_spki_pin_sha256(
          (const unsigned char *)"abc", 3,
          "0000000000000000000000000000000000000000000000000000000000000000"));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_verify_spki_pin_sha256((const unsigned char *)"abc",
                                                   3, "sha256//wrongpin="));

  /* Handshake & X.509 config verification */
  memset(&cfg, 0, sizeof(cfg));
  cfg.verify_peer = 1;
  cfg.verify_host = 1;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_tls_verify_handshake_config(NULL, "a.com", "a.com",
                                                        NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_tls_verify_handshake_config(&cfg, NULL, "a.com",
                                                        NULL, 0));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_INVAL,
      c_abstract_http_tls_verify_handshake_config(&cfg, "", "a.com", NULL, 0));

  cfg.tls_version_mask = 0xFF00U;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "a.com",
                                                        NULL, 0));
  cfg.tls_version_mask = HTTP_TLS_VERSION_1_2 | HTTP_TLS_VERSION_1_3;

  cfg.client_cert_path = (char *)"client.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "a.com",
                                                        NULL, 0));
  cfg.client_cert_path = NULL;
  cfg.client_key_path = (char *)"client.key";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "a.com",
                                                        NULL, 0));
  cfg.client_cert_path = (char *)"client.pem";

  cfg.ca_buffer_len = 10;
  cfg.ca_buffer = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "a.com",
                                                        NULL, 0));
  cfg.ca_buffer_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", NULL,
                                                        NULL, 0));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "b.com",
                                                        NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com",
                                                        "other.com", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(
                &cfg, "localhost", "*.example.com", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(
                &cfg, ".example.com", "*.example.com", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(
                &cfg, "api.other.com", "*.example.com", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(&cfg, "api.bad.org",
                                                        "*.bad.com", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_tls_verify_handshake_config(
                &cfg, "API.Example.COM", "*.example.com", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_tls_verify_handshake_config(
                &cfg, "API.Example.COM", "api.example.com", NULL, 0));

  cfg.verify_peer = 0;
  cfg.verify_host = 1;
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "", NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", NULL,
                                                        NULL, 0));

  cfg.cert_pin_sha256 =
      (char *)"sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_tls_verify_handshake_config(&cfg, "a.com", "a.com",
                                                        NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_tls_verify_handshake_config(
                &cfg, "a.com", "a.com", (const unsigned char *)"abc", 3));

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
            c_abstract_http_verify_spki_pin_sha256(
                (const unsigned char *)"abc", 3,
                "sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="));
  g_mock_alloc_fail = 0;

  g_mock_sha1_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_verify_spki_pin_sha256(
                (const unsigned char *)"abc", 3,
                "sha256//ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0="));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&ctx));
  g_mock_sha1_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, sha256_final(&ctx, hash));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&ctx));
  g_mock_sha1_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, sha256_final(&ctx, hash));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&ctx));
  g_mock_sha1_fail = 57;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, sha256_final(&ctx, hash));
  g_mock_sha1_fail = 0;
#endif

  PASS();
}

TEST test_base64_encode_basic(void) {
  const char *input = "hello world";
  char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_encode((const unsigned char *)input, strlen(input), &output,
                          &out_len));
  ASSERT_STR_EQ("aGVsbG8gd29ybGQ=", output);
  ASSERT_EQ(16, out_len);
  free(output);
  PASS();
}

TEST test_base64_encode_padded(void) {
  const char *input = "a";
  char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_encode((const unsigned char *)input, strlen(input), &output,
                          &out_len));
  ASSERT_STR_EQ("YQ==", output);
  free(output);
  PASS();
}

TEST test_base64_decode_basic(void) {
  const char *input = "aGVsbG8gd29ybGQ=";
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_decode(input, strlen(input), &output, &out_len));
  ASSERT_EQ(11, out_len);
  ASSERT_MEM_EQ("hello world", output, 11);
  free(output);
  PASS();
}

TEST test_base64_decode_padded(void) {
  const char *input = "YQ==";
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_decode(input, strlen(input), &output, &out_len));
  ASSERT_EQ(1, out_len);
  ASSERT_MEM_EQ("a", output, 1);
  free(output);
  PASS();
}

TEST test_base64_decode_invalid(void) {
  const char *input = "YQ="; /* length 3, not divisible by 4 */
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_decode(input, strlen(input), &output, &out_len));
  PASS();
}

TEST test_base64_decode_1_pad(void) {
  const char *input = "YWI="; /* "ab" */
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_decode(input, strlen(input), &output, &out_len));
  ASSERT_EQ(2, out_len);
  ASSERT_MEM_EQ("ab", output, 2);
  free(output);
  PASS();
}

TEST test_base64_decode_all_pad(void) {
  const char *input = "====";
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_decode(input, strlen(input), &output, &out_len));
  ASSERT_EQ(1, out_len);
  ASSERT_EQ(0, output[0]);
  free(output);
  PASS();
}

TEST test_math_is_const_time_streq(void) {
  ASSERT_EQ(1, math_is_const_time_streq("hello", "hello"));
  ASSERT_EQ(0, math_is_const_time_streq("hello", "world"));
  ASSERT_EQ(0, math_is_const_time_streq("hello", "hell"));
  ASSERT_EQ(0, math_is_const_time_streq("hello", "helloo"));
  PASS();
}

TEST test_crypto_errors(void) {
  struct sha1_ctx ctx;
  unsigned char out[20];
  char *b64_str;
  size_t b64_len;
  unsigned char *dec_data;
  size_t dec_len;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha1_init(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            sha1_update(NULL, (const unsigned char *)"a", 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha1_update(&ctx, NULL, 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha1_final(NULL, out));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, sha1_final(&ctx, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_encode(NULL, 1, &b64_str, &b64_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_encode((const unsigned char *)"a", 1, NULL, &b64_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_encode((const unsigned char *)"a", 1, &b64_str, NULL));

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_decode(NULL, 4, &dec_data, &dec_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_decode("abcd", 4, NULL, &dec_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_decode("abcd", 4, &dec_data, NULL));

  /* Test length 0 */
  ASSERT_EQ(0,
            base64_encode((const unsigned char *)"a", 0, &b64_str, &b64_len));
  if (b64_str)
    free(b64_str);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_decode("abcd", 0, &dec_data, &dec_len));
  if (dec_data)
    free(dec_data);

  /* Invalid length (not multiple of 4) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            base64_decode("abc", 3, &dec_data, &dec_len));

  PASS();
}

TEST test_sha1_large_string(void) {
  struct sha1_ctx ctx;
  unsigned char out[20];
  unsigned char data[128];
  int i;
  for (i = 0; i < 128; i++)
    data[i] = 'a';

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_update(&ctx, data, 128));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_final(&ctx, out));

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_crypto_oom(void) {
  char *b64_str = NULL;
  size_t b64_len = 0;
  unsigned char *dec_data = NULL;
  size_t dec_len = 0;

  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;

  {
    int rc1 = base64_encode((const unsigned char *)"a", 1, &b64_str, &b64_len);
    g_mock_alloc_count = 0;
    {
      int rc2 = base64_decode("abcd", 4, &dec_data, &dec_len);

      g_mock_alloc_fail = 0;
      ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc1, "%d");
      ASSERT_EQ_FMT(C_ABSTRACT_HTTP_ERR_NOMEM, rc2, "%d");
    }
  }
  PASS();
}
#endif

TEST test_sha1_rollover(void) {
  struct sha1_ctx ctx;
  unsigned char out[20];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  /* Simulate that we have hashed almost 2^32 bits */
  ctx.count[0] = 0xFFFFFFF8U; /* 2^32 - 8 bits */
  ctx.count[1] = 0;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            sha1_update(&ctx, (const unsigned char *)"a", 1));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_final(&ctx, out));

  PASS();
}

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
TEST test_sha1_mock_failures(void) {
  struct sha1_ctx ctx;
  struct sha256_ctx s256;
  unsigned char out[20];

  /* Fail 1st sha1_update inside sha1_final */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  g_mock_sha1_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, sha1_final(&ctx, out));

  /* Fail 2nd sha1_update inside sha1_final (the while loop) */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  g_mock_sha1_fail = 2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, sha1_final(&ctx, out));

  /* Fail 3rd sha1_update inside sha1_final */
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
  g_mock_sha1_fail = 57;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, sha1_final(&ctx, out));
  g_mock_sha1_fail = 0;

  /* Fail 1st and 2nd math_sha1_transform calls inside sha1_update */
  {
    unsigned char big_block[140];
    memset(big_block, 'A', sizeof(big_block));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
    g_mock_sha1_fail = -1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              sha1_update(&ctx, big_block, sizeof(big_block)));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha1_init(&ctx));
    g_mock_sha1_fail = -2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              sha1_update(&ctx, big_block, sizeof(big_block)));
    g_mock_sha1_fail = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&s256));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              sha256_update(&s256, big_block, sizeof(big_block)));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&s256));
    g_mock_sha1_fail = -1;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              sha256_update(&s256, big_block, sizeof(big_block)));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, sha256_init(&s256));
    g_mock_sha1_fail = -2;
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
              sha256_update(&s256, big_block, sizeof(big_block)));
    g_mock_sha1_fail = -99;
    {
      unsigned char h32[32];
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
                c_abstract_http_sha256_oneshot(big_block, 4, h32));
    }
    g_mock_sha1_fail = 0;
  }

  PASS();
}
#endif

TEST test_random_bytes(void) {
  unsigned char b1[32];
  unsigned char b2[32];

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_random_bytes(NULL, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_random_bytes(NULL, 8));
  memset(b1, 0, sizeof(b1));
  memset(b2, 0, sizeof(b2));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_random_bytes(b1, sizeof(b1)));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_random_bytes(b2, sizeof(b2)));
  ASSERT(memcmp(b1, b2, sizeof(b1)) != 0);

#if defined(C_ABSTRACT_HTTP_TEST_OOM) && !defined(_WIN32)
  g_mock_sha1_fail = -500;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_random_bytes(b1, sizeof(b1)));
  g_mock_sha1_fail = -501;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_random_bytes(b2, sizeof(b2)));
  g_mock_sha1_fail = -502;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO,
            c_abstract_http_random_bytes(b2, sizeof(b2)));
  g_mock_sha1_fail = 0;
  ASSERT(memcmp(b1, b2, sizeof(b1)) != 0);
#endif
  PASS();
}

NO_DISCARD static c_abstract_http_error_t
crypto_dummy_read_chunk(void *user_data, void *buf, size_t buf_len,
                        size_t *out_read) {
  (void)user_data;
  (void)buf;
  (void)buf_len;
  *out_read = 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

TEST test_h3_shared_helpers(void) {
  struct HttpConfig dst;
  struct HttpConfig src;
  struct HttpRequest req;
  struct HttpResponse *res = NULL;
  char *redir_url = NULL;
  const char *loc = NULL;
  char clen_buf[32];
  int emit = 0;
  static const unsigned char k_ca_bytes[] = {0x30, 0x03, 0x01, 0x01, 0xff};

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&dst));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&src));

  /* 1. c_abstract_http_h3_apply_tls_config validation & persistence */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_apply_tls_config(NULL, &src));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_apply_tls_config(&dst, NULL));

  src.client_cert_path = (char *)"c.pem";
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_apply_tls_config(&dst, &src));
  src.client_cert_path = NULL;
  src.ca_buffer_len = 3;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_apply_tls_config(&dst, &src));
  src.ca_buffer_len = 0;

  src.version_mask = HTTP_VERSION_1_1;
  src.http3_fallback = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            c_abstract_http_h3_apply_tls_config(&dst, &src));
  src.http3_fallback = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_apply_tls_config(&dst, &src));
  src.version_mask = HTTP_VERSION_3;

  src.tls_version_mask = HTTP_TLS_VERSION_1_2;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOTSUP,
            c_abstract_http_h3_apply_tls_config(&dst, &src));
  src.tls_version_mask = HTTP_TLS_VERSION_1_3;

  src.ca_path = (char *)"ca.pem";
  src.ca_buffer = (unsigned char *)k_ca_bytes;
  src.ca_buffer_len = sizeof(k_ca_bytes);
  src.client_cert_path = (char *)"cert.pem";
  src.client_key_path = (char *)"key.pem";
  src.cert_pin_sha256 =
      (char *)"sha256//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  {
    int step;
    for (step = 0; step < 8; ++step) {
      c_abstract_http_error_t orc;
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = step;
      orc = c_abstract_http_h3_apply_tls_config(&dst, &src);
      g_mock_alloc_fail = 0;
      g_mock_alloc_count = 0;
      if (orc == C_ABSTRACT_HTTP_SUCCESS) {
        break;
      }
    }
  }
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_apply_tls_config(&dst, &src));
  src.ca_path = NULL;
  src.ca_buffer = NULL;
  src.ca_buffer_len = 0;
  src.client_cert_path = NULL;
  src.client_key_path = NULL;
  src.cert_pin_sha256 = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_apply_tls_config(&dst, &src));

  /* 2. c_abstract_http_h3_verify_peer_tls */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_verify_peer_tls(NULL, "https", "example.com"));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_verify_peer_tls(&dst, NULL, "example.com"));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_h3_verify_peer_tls(&dst, "https", ""));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_verify_peer_tls(&dst, "http", "example.com"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_verify_peer_tls(&dst, "https", "example.com"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_verify_peer_tls(&dst, "masque", "example.com"));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_h3_verify_peer_tls(&dst, "https", "sub.bad-cn.invalid"));

  /* 3. math_c_abstract_http_should_redirect &
   * c_abstract_http_h3_advance_redirect */
  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT(res != NULL);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 200;
  ASSERT_EQ(0, math_c_abstract_http_should_redirect(0, res, &loc));
  ASSERT_EQ(0, math_c_abstract_http_should_redirect(1, res, &loc));
  res->status_code = 301;
  ASSERT_EQ(0, math_c_abstract_http_should_redirect(1, res, &loc));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&res->headers, "X-Other", "1"));
  free(res->headers.headers[0].key);
  res->headers.headers[0].key = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&res->headers, "X-NonMatch", "1"));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&res->headers, "Location", ""));
  ASSERT_EQ(0, math_c_abstract_http_should_redirect(1, res, &loc));
  free(res->headers.headers[2].value);
  ASSERT_EQ(
      C_ABSTRACT_HTTP_SUCCESS,
      c_abstract_http_strdup("/v2/target", &res->headers.headers[2].value));
  ASSERT_EQ(1, math_c_abstract_http_should_redirect(1, res, &loc));
  ASSERT_STR_EQ("/v2/target", loc);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_init(&req));
  req.method = HTTP_POST;
  req.url = (char *)"https://example.com:8443/v1/start";
  req.body = "abc";
  req.body_len = 3;
  req.read_chunk = crypto_dummy_read_chunk;
  req.expected_body_len = 99;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_h3_advance_redirect(
                                           NULL, &redir_url, &res, loc, 0));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_LOOP, c_abstract_http_h3_advance_redirect(
                                          &req, &redir_url, &res, loc, 10));
  {
    struct HttpRequest bad_req = req;
    bad_req.url = (char *)"noscheme";
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_h3_advance_redirect(&bad_req, &redir_url, &res,
                                                  loc, 0));
  }
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  g_mock_alloc_fail = 1;
  g_mock_alloc_count = 0;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM, c_abstract_http_h3_advance_redirect(
                                           &req, &redir_url, &res, loc, 0));
  g_mock_alloc_fail = 0;
  g_mock_response_free_fail = 1;
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_IO, c_abstract_http_h3_advance_redirect(
                                        &req, &redir_url, &res, loc, 0));
  g_mock_response_free_fail = 0;
  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 301;
#endif
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_advance_redirect(&req, &redir_url, &res,
                                                "/v2/target", 0));
  ASSERT_STR_EQ("https://example.com:8443/v2/target", req.url);
  ASSERT_EQ(HTTP_GET, req.method);
  ASSERT(req.body == NULL);

  /* Scheme-relative, relative with slash, relative without slash, full URL, 307
   */
  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 307;
  req.method = HTTP_POST;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_advance_redirect(&req, &redir_url, &res,
                                                "//cdn.example.com/item", 1));
  ASSERT_STR_EQ("https://cdn.example.com/item", req.url);
  ASSERT_EQ(HTTP_POST, req.method);

  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 303;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_h3_advance_redirect(
                                         &req, &redir_url, &res, "subleaf", 2));
  ASSERT_STR_EQ("https://cdn.example.com/subleaf", req.url);
  ASSERT_EQ(HTTP_GET, req.method);

  free(redir_url);
  redir_url = NULL;
  req.url = (char *)"https://bare.example.com";
  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 302;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_h3_advance_redirect(
                                         &req, &redir_url, &res, "/root", 3));
  ASSERT_STR_EQ("https://bare.example.com/root", req.url);

  free(redir_url);
  redir_url = NULL;
  req.url = (char *)"https://bare.example.com";
  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 308;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_h3_advance_redirect(
                                         &req, &redir_url, &res, "rel", 4));
  ASSERT_STR_EQ("https://bare.example.com/rel", req.url);

  res = (struct HttpResponse *)calloc(1, sizeof(*res));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_response_init(res));
  res->status_code = 308;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_h3_advance_redirect(
                &req, &redir_url, &res, "https://final.example.com/done", 5));
  ASSERT_STR_EQ("https://final.example.com/done", req.url);
  free(redir_url);
  req.url = NULL;

  /* 4. math_c_abstract_http_format_clen */
  (void)emit;
  ASSERT_EQ(0, math_c_abstract_http_format_clen(NULL, clen_buf));
  ASSERT_EQ(0, math_c_abstract_http_format_clen(&req, clen_buf));
  req.read_chunk = crypto_dummy_read_chunk;
  req.expected_body_len = 512;
  ASSERT_EQ(1, math_c_abstract_http_format_clen(&req, clen_buf));
  ASSERT_STR_EQ("512", clen_buf);
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "X-Ignore", "1"));
  free(req.headers.headers[0].key);
  req.headers.headers[0].key = NULL;
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            http_headers_add(&req.headers, "Content-Length", "512"));
  ASSERT_EQ(0, math_c_abstract_http_format_clen(&req, clen_buf));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_request_free(&req));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&dst));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_free(&src));
  PASS();
}

/** @brief Verify PEM/DER decoding, file loading, and X.509 SPKI extraction */
TEST test_x509_pem_der_and_spki_helpers(void) {
  /* Minimal valid X.509 v3 DER certificate with SPKI = {0x30, 0x03,
   * 'a','b','c'} Certificate SEQUENCE (0x30, 0x1A): TBSCertificate SEQUENCE
   * (0x30, 0x15): [0] version: A0 03 02 01 02 serial:      02 01 01 sigAlg: 30
   * 00 issuer:      30 00 validity:    30 00 subject:     30 00 spki:        30
   * 03 61 62 63 sigAlg: 30 00, sigVal: 03 01 00
   */
  static const unsigned char k_v3_cert[] = {
      0x30, 0x1C, 0x30, 0x15, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02,
      0x01, 0x01, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00,
      0x30, 0x03, 0x61, 0x62, 0x63, 0x30, 0x00, 0x03, 0x01, 0x00};
  /* Minimal X.509 v1 DER certificate (no 0xA0 version tag, long-form 0x81 len)
   */
  static const unsigned char k_v1_long_cert[] = {
      0x30, 0x81, 0x17, 0x30, 0x10, 0x02, 0x01, 0x01, 0x30,
      0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x03,
      0x61, 0x62, 0x63, 0x30, 0x00, 0x03, 0x01, 0x00};
  static const unsigned char k_bad_indef[] = {0x30, 0x80, 0x00, 0x00};
  static const unsigned char k_bad_len5[] = {0x30, 0x85, 1, 2, 3, 4, 5};
  static const unsigned char k_bad_over[] = {0x30, 0x40, 0x30, 0x00};
  static const unsigned char k_bad_tbs[] = {0x30, 0x02, 0x02, 0x00};
  static const unsigned char k_bad_empty_tbs[] = {0x30, 0x02, 0x30, 0x00};
  static const unsigned char k_bad_short_fields[] = {0x30, 0x05, 0x30, 0x03,
                                                     0x02, 0x01, 0x01};
  static const unsigned char k_bad_spki_tag[] = {
      0x30, 0x12, 0x30, 0x10, 0x02, 0x01, 0x01, 0x30, 0x00, 0x30,
      0x00, 0x30, 0x00, 0x30, 0x00, 0x02, 0x03, 0x61, 0x62, 0x63};
  char *b64 = NULL;
  size_t b64_len = 0;
  char pem_buf[256];
  unsigned char *der_out = NULL;
  size_t der_len = 0;
  const unsigned char *spki_ptr = NULL;
  size_t spki_len = 0;
  const char *tmp_path = "cah_test_cert_tmp.bin";
  FILE *fp = NULL;

  /* 1. SPKI extraction tests */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL, c_abstract_http_x509_extract_spki_der(
                                           NULL, 0, &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES, c_abstract_http_x509_extract_spki_der(
                                           k_v3_cert, 1, &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(
                k_bad_indef, sizeof(k_bad_indef), &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(
                k_bad_len5, sizeof(k_bad_len5), &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(
                k_bad_over, sizeof(k_bad_over), &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(k_bad_tbs, sizeof(k_bad_tbs),
                                                  &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(k_bad_empty_tbs,
                                                  sizeof(k_bad_empty_tbs),
                                                  &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(k_bad_short_fields,
                                                  sizeof(k_bad_short_fields),
                                                  &spki_ptr, &spki_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_x509_extract_spki_der(
                k_bad_spki_tag, sizeof(k_bad_spki_tag), &spki_ptr, &spki_len));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_x509_extract_spki_der(k_v3_cert, sizeof(k_v3_cert),
                                                  &spki_ptr, &spki_len));
  ASSERT_EQ(5U, spki_len);
  ASSERT_EQ(0, memcmp(spki_ptr,
                      "\x30\x03"
                      "abc",
                      5));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_x509_extract_spki_der(
                k_v1_long_cert, sizeof(k_v1_long_cert), &spki_ptr, &spki_len));
  ASSERT_EQ(5U, spki_len);
  ASSERT_EQ(0, memcmp(spki_ptr,
                      "\x30\x03"
                      "abc",
                      5));

  /* 1b. PKCS#8 unwrap tests */
  {
    static const unsigned char k_p8[] = {0x30, 0x0C, 0x02, 0x01, 0x00,
                                         0x30, 0x02, 0x05, 0x00, 0x04,
                                         0x03, 'K',  'E',  'Y'};
    static const unsigned char k_p1[] = {0x30, 0x06, 0x02, 0x01,
                                         0x00, 0x02, 0x01, 0x07};
    static const unsigned char k_bad_key[] = {0x02, 0x01, 0x00};
    const unsigned char *kptr = NULL;
    size_t klen = 0;

    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
              c_abstract_http_pkcs8_unwrap_key(NULL, 0, &kptr, &klen));
    ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
              c_abstract_http_pkcs8_unwrap_key(k_bad_key, sizeof(k_bad_key),
                                               &kptr, &klen));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_pkcs8_unwrap_key(
                                           k_p8, sizeof(k_p8), &kptr, &klen));
    ASSERT_EQ(3U, klen);
    ASSERT_EQ(0, memcmp(kptr, "KEY", 3));
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_pkcs8_unwrap_key(
                                           k_p1, sizeof(k_p1), &kptr, &klen));
    ASSERT_EQ(sizeof(k_p1), klen);
    ASSERT_EQ(k_p1, kptr);
  }

  /* 2. PEM & DER decode tests */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_pem_or_der_decode(NULL, 0, &der_out, &der_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_pem_or_der_decode((const unsigned char *)"bad", 3,
                                              &der_out, &der_len));
  ASSERT_EQ(
      C_ABSTRACT_HTTP_ERR_ACCES,
      c_abstract_http_pem_or_der_decode(
          (const unsigned char *)"-----BEGIN CERT", 15, &der_out, &der_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_pem_or_der_decode(
                (const unsigned char *)"-----BEGIN CERT-----\nAAAA", 25,
                &der_out, &der_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_pem_or_der_decode(
                (const unsigned char *)"-----BEGIN CERT-----\r\n \t-----END ",
                33, &der_out, &der_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_pem_or_der_decode(
                (const unsigned char *)"-----BEGIN CERT-----\n$$$\n-----END ",
                34, &der_out, &der_len));

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_pem_or_der_decode(k_v3_cert, sizeof(k_v3_cert),
                                              &der_out, &der_len));
  ASSERT_EQ(sizeof(k_v3_cert), der_len);
  free(der_out);
  der_out = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            base64_encode(k_v3_cert, sizeof(k_v3_cert), &b64, &b64_len));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  sprintf_s(pem_buf, sizeof(pem_buf),
            "-----BEGIN CERTIFICATE-----\r\n%s\r\n-----END CERTIFICATE-----\n",
            b64);
#else
  sprintf(pem_buf,
          "-----BEGIN CERTIFICATE-----\r\n%s\r\n-----END CERTIFICATE-----\n",
          b64);
#endif
  free(b64);

  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, c_abstract_http_pem_or_der_decode(
                                         (const unsigned char *)pem_buf,
                                         strlen(pem_buf), &der_out, &der_len));
  ASSERT_EQ(sizeof(k_v3_cert), der_len);
  ASSERT_EQ(0, memcmp(der_out, k_v3_cert, sizeof(k_v3_cert)));
  free(der_out);
  der_out = NULL;

  /* 3. File loading tests (missing, empty, >1024 multi-chunk, OOM) */
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_read_file_bytes("", &der_out, &der_len));
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_read_file_bytes("nonexistent_cah_cert.pem",
                                            &der_out, &der_len));

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  fopen_s(&fp, tmp_path, "wb");
#else
  fp = fopen(tmp_path, "wb");
#endif
  ASSERT(fp != NULL);
  fclose(fp);
  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_ACCES,
            c_abstract_http_read_file_bytes(tmp_path, &der_out, &der_len));

  {
    unsigned char big[1500];
    memset(big, 0x30, sizeof(big));
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    fopen_s(&fp, tmp_path, "wb");
#else
    fp = fopen(tmp_path, "wb");
#endif
    ASSERT(fp != NULL);
    ASSERT_EQ(sizeof(big), fwrite(big, 1, sizeof(big), fp));
    fclose(fp);

    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
              c_abstract_http_read_file_bytes(tmp_path, &der_out, &der_len));
    ASSERT_EQ(sizeof(big), der_len);
    free(der_out);
    der_out = NULL;

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
    {
      int step;
      for (step = 0; step < 4; ++step) {
        c_abstract_http_error_t orc;
        g_mock_alloc_fail = 1;
        g_mock_alloc_count = step;
        orc = c_abstract_http_read_file_bytes(tmp_path, &der_out, &der_len);
        g_mock_alloc_fail = 0;
        if (orc == C_ABSTRACT_HTTP_SUCCESS) {
          free(der_out);
          der_out = NULL;
        }
      }
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                c_abstract_http_pem_or_der_decode(k_v3_cert, sizeof(k_v3_cert),
                                                  &der_out, &der_len));
      g_mock_alloc_fail = 1;
      g_mock_alloc_count = 0;
      ASSERT_EQ(C_ABSTRACT_HTTP_ERR_NOMEM,
                c_abstract_http_pem_or_der_decode(
                    (const unsigned char *)pem_buf, strlen(pem_buf), &der_out,
                    &der_len));
      g_mock_alloc_fail = 0;
    }
#endif
  }
  remove(tmp_path);
  PASS();
}

TEST test_default_tls_hooks(void) {
  const struct HttpRawTlsHooks *hooks = NULL;

  ASSERT_EQ(C_ABSTRACT_HTTP_ERR_INVAL,
            c_abstract_http_get_default_tls_hooks(NULL));
  ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS,
            c_abstract_http_get_default_tls_hooks(&hooks));
  if (hooks) {
    struct HttpConfig cfg;
    void *sess = NULL;
    ASSERT(hooks->open != NULL);
    ASSERT(hooks->handshake != NULL);
    ASSERT(hooks->read != NULL);
    ASSERT(hooks->write != NULL);
    ASSERT(hooks->close != NULL);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, http_config_init(&cfg));
    cfg.ca_path = (char *)"nonexistent_cah_default_tls_ca.pem";
    ASSERT(hooks->open(hooks->provider_data, -1, "localhost", &cfg, &sess) !=
           C_ABSTRACT_HTTP_SUCCESS);
    ASSERT_EQ(C_ABSTRACT_HTTP_SUCCESS, hooks->close(NULL));
  }
  PASS();
}

SUITE(crypto_suite) {
  RUN_TEST(test_random_bytes);
  RUN_TEST(test_default_tls_hooks);
  RUN_TEST(test_h3_shared_helpers);
  RUN_TEST(test_x509_pem_der_and_spki_helpers);
  RUN_TEST(test_sha1_large_string);
  RUN_TEST(test_crypto_errors);
#if defined(C_ABSTRACT_HTTP_TEST_OOM)
  RUN_TEST(test_crypto_oom);
  RUN_TEST(test_sha1_mock_failures);
#endif
  RUN_TEST(test_sha1_rollover);
  RUN_TEST(test_sha1_empty_string);
  RUN_TEST(test_sha1_fox_string);
  RUN_TEST(test_sha256_vectors_and_spki_pinning);
  RUN_TEST(test_base64_encode_basic);
  RUN_TEST(test_base64_encode_padded);
  RUN_TEST(test_base64_decode_basic);
  RUN_TEST(test_base64_decode_padded);
  RUN_TEST(test_base64_decode_invalid);
  RUN_TEST(test_base64_decode_1_pad);
  RUN_TEST(test_base64_decode_all_pad);
  RUN_TEST(test_math_is_const_time_streq);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(crypto_suite);
  GREATEST_MAIN_END();
}
