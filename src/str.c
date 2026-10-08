/* clang-format off */
#include "str.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

enum c_abstract_http_error
c_abstract_http_str_trim_trailing_whitespace(char *str) {
  size_t len;
  if (!str) {
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  len = strlen(str);
  while (len > 0 && isspace((unsigned char)str[len - 1])) {
    str[len - 1] = '\0';
    len--;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error c_abstract_http_str_starts_with(const char *str,
                                                           const char *prefix,
                                                           int *out_b) {
  if (!str || !prefix || !out_b)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  *out_b = (strncmp(str, prefix, strlen(prefix)) == 0) ? 1 : 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
c_abstract_http_str_equal(const char *a, const char *b, int *out_b) {
  if (!out_b)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (!a && !b) {
    *out_b = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!a || !b) {
    *out_b = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_b = (strcmp(a, b) == 0) ? 1 : 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error math_c_abstract_http_stricmp(const char *a,
                                                        const char *b) {
  if (!a || !b)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return C_ABSTRACT_HTTP_ERR_INVAL;
    }
    a++;
    b++;
  }
  if (*a || *b)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
c_abstract_http_str_iequal(const char *a, const char *b, int *out_b) {
  if (!out_b)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (!a && !b) {
    *out_b = 1;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (!a || !b) {
    *out_b = 0;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  *out_b =
      (math_c_abstract_http_stricmp(a, b) == C_ABSTRACT_HTTP_SUCCESS) ? 1 : 0;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error c_abstract_http_str_after_last(const char *str,
                                                          int delimiter,
                                                          const char **out_s) {
  const char *last;
  if (!str || !out_s)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  last = strrchr(str, delimiter);
  if (last) {
    *out_s = last + 1;
  } else {
    *out_s = str;
  }
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
c_abstract_http_ref_is_type(const char *ref, const char *type, int *out_b) {
  const char *last_slash;
  enum c_abstract_http_error err;
  if (!ref || !type || !out_b)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  err = c_abstract_http_str_after_last(ref, '/', &last_slash);
  if (err != C_ABSTRACT_HTTP_SUCCESS)
    return err;
  return c_abstract_http_str_equal(last_slash, type, out_b);
}

enum c_abstract_http_error c_abstract_http_destringize(const char *quoted,
                                                       char **out_s) {
  size_t len, i, j;
  char *res;
  if (!quoted || !out_s)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  len = strlen(quoted);
  if (len < 2 || quoted[0] != '"' || quoted[len - 1] != '"') {
    return c_abstract_http_strdup(quoted, out_s);
  }
  res = (char *)malloc(len);
  if (!res)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  for (i = 1, j = 0; i < len - 1; i++) {
    if (quoted[i] == '\\' && i + 1 < len - 1) {
      if (quoted[i + 1] == '"' || quoted[i + 1] == '\\') {
        i++;
      }
    }
    res[j++] = quoted[i];
  }
  res[j] = '\0';
  *out_s = res;
  return C_ABSTRACT_HTTP_SUCCESS;
}
