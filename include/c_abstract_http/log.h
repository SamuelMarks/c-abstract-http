#ifndef C_ABSTRACT_HTTP_LOG_H
#define C_ABSTRACT_HTTP_LOG_H

/* clang-format off */
#include <c_abstract_http/http_types.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef LOG_DEBUG
/**
 * @brief Write a debug log message.
 * @param[in] fmt Formatting string.
 * @param[in] ... Variadic arguments.
 * @return C_ABSTRACT_HTTP_SUCCESS on success, error code on failure.
 */
enum c_abstract_http_error c_abstract_http_log_debug(const char *fmt, ...);
#define LOG_DEBUG c_abstract_http_log_debug
#endif /* !LOG_DEBUG */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* C_ABSTRACT_HTTP_LOG_H */
