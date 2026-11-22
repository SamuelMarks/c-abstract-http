
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

SUITE(crypto_suite) {
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
