#ifndef CDD_MOCK_ALLOC_H
#define CDD_MOCK_ALLOC_H

#ifdef _WIN32
/* clang-format off */
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
/* clang-format on */
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int g_mock_alloc_fail;
extern int g_mock_alloc_count;
extern int g_mock_pthread_fail;

#ifdef __cplusplus
}
#endif

#endif

extern int g_mock_pipe_fail;
extern int g_mock_fork_fail;
extern int g_mock_waitpid_fail;

extern int g_mock_select_fail;
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);

extern int g_mock_time_jump;
extern int g_mock_time_jump_count;
long long c_abstract_http_mock_math_get_current_time_ms(void);

void dummy_cb_thread(void *arg);
extern int c_abstract_http_mock_cdd_strdup(const char *s, char **out);
