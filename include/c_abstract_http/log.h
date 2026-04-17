#ifndef C_ABSTRACT_HTTP_LOG_H
#define C_ABSTRACT_HTTP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LOG_DEBUG
#ifdef DEBUG
void c_abstract_http_log_debug(const char *fmt, ...);
#define LOG_DEBUG c_abstract_http_log_debug
#else
void c_abstract_http_log_debug(const char *fmt, ...);
#define LOG_DEBUG 1 ? (void)0 : c_abstract_http_log_debug
#endif /* DEBUG */
#endif /* !LOG_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* C_ABSTRACT_HTTP_LOG_H */
