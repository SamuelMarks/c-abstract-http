#include "../src/crypto_utils.h"
#include "greatest.h"
#include <string.h>

TEST test_sha1_empty_string(void) {
  struct sha1_ctx ctx;
  unsigned char hash[20];
  const unsigned char expected[20] = {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b,
                                      0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60,
                                      0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09};

  sha1_init(&ctx);
  sha1_update(&ctx, (const unsigned char *)"", 0);
  sha1_final(&ctx, hash);

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

  sha1_init(&ctx);
  sha1_update(&ctx, (const unsigned char *)input, strlen(input));
  sha1_final(&ctx, hash);

  ASSERT_MEM_EQ(expected, hash, 20);
  PASS();
}

TEST test_base64_encode_basic(void) {
  const char *input = "hello world";
  char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(0, base64_encode((const unsigned char *)input, strlen(input),
                             &output, &out_len));
  ASSERT_STR_EQ("aGVsbG8gd29ybGQ=", output);
  ASSERT_EQ(16, out_len);
  free(output);
  PASS();
}

TEST test_base64_encode_padded(void) {
  const char *input = "a";
  char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(0, base64_encode((const unsigned char *)input, strlen(input),
                             &output, &out_len));
  ASSERT_STR_EQ("YQ==", output);
  free(output);
  PASS();
}

TEST test_base64_decode_basic(void) {
  const char *input = "aGVsbG8gd29ybGQ=";
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(0, base64_decode(input, strlen(input), &output, &out_len));
  ASSERT_EQ(11, out_len);
  ASSERT_MEM_EQ("hello world", output, 11);
  free(output);
  PASS();
}

TEST test_base64_decode_padded(void) {
  const char *input = "YQ==";
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(0, base64_decode(input, strlen(input), &output, &out_len));
  ASSERT_EQ(1, out_len);
  ASSERT_MEM_EQ("a", output, 1);
  free(output);
  PASS();
}

TEST test_base64_decode_invalid(void) {
  const char *input = "YQ="; /* length 3, not divisible by 4 */
  unsigned char *output = NULL;
  size_t out_len = 0;

  ASSERT_EQ(22, base64_decode(input, strlen(input), &output, &out_len));
  PASS();
}

TEST test_const_time_streq(void) {
  ASSERT_EQ(1, const_time_streq("hello", "hello"));
  ASSERT_EQ(0, const_time_streq("hello", "world"));
  ASSERT_EQ(0, const_time_streq("hello", "hell"));
  ASSERT_EQ(0, const_time_streq("hello", "helloo"));
  PASS();
}

SUITE(crypto_suite) {
  RUN_TEST(test_sha1_empty_string);
  RUN_TEST(test_sha1_fox_string);
  RUN_TEST(test_base64_encode_basic);
  RUN_TEST(test_base64_encode_padded);
  RUN_TEST(test_base64_decode_basic);
  RUN_TEST(test_base64_decode_padded);
  RUN_TEST(test_base64_decode_invalid);
  RUN_TEST(test_const_time_streq);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(crypto_suite);
  GREATEST_MAIN_END();
  return 0;
}
