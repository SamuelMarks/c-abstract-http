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

extern int *cdd_mock_get_g_mock_alloc_fail(void);
extern int *cdd_mock_get_g_mock_alloc_count(void);
extern int *cdd_mock_get_g_mock_pthread_fail(void);
extern int *cdd_mock_get_g_mock_pipe_fail(void);
extern int *cdd_mock_get_g_mock_fork_fail(void);
extern int *cdd_mock_get_g_mock_waitpid_fail(void);
extern int *cdd_mock_get_g_mock_select_fail(void);
extern int *cdd_mock_get_g_mock_time_jump(void);
extern int *cdd_mock_get_g_mock_time_jump_count(void);
extern int *cdd_mock_get_g_mock_fwrite_fail(void);
extern int *cdd_mock_get_g_mock_fclose_fail(void);
extern int *cdd_mock_get_g_mock_socket_fail(void);
extern int *cdd_mock_get_g_mock_bind_fail(void);
extern int *cdd_mock_get_g_mock_listen_fail(void);
extern int *cdd_mock_get_g_mock_accept_fail(void);
extern int *cdd_mock_get_g_mock_recv_fail(void);

#define g_mock_alloc_fail (*cdd_mock_get_g_mock_alloc_fail())
#define g_mock_alloc_count (*cdd_mock_get_g_mock_alloc_count())
#define g_mock_pthread_fail (*cdd_mock_get_g_mock_pthread_fail())
#define g_mock_pipe_fail (*cdd_mock_get_g_mock_pipe_fail())
#define g_mock_fork_fail (*cdd_mock_get_g_mock_fork_fail())
#define g_mock_waitpid_fail (*cdd_mock_get_g_mock_waitpid_fail())
#define g_mock_select_fail (*cdd_mock_get_g_mock_select_fail())
#define g_mock_time_jump (*cdd_mock_get_g_mock_time_jump())
#define g_mock_time_jump_count (*cdd_mock_get_g_mock_time_jump_count())
#define g_mock_fwrite_fail (*cdd_mock_get_g_mock_fwrite_fail())
#define g_mock_fclose_fail (*cdd_mock_get_g_mock_fclose_fail())
#define g_mock_socket_fail (*cdd_mock_get_g_mock_socket_fail())
#define g_mock_bind_fail (*cdd_mock_get_g_mock_bind_fail())
#define g_mock_listen_fail (*cdd_mock_get_g_mock_listen_fail())
#define g_mock_accept_fail (*cdd_mock_get_g_mock_accept_fail())
#define g_mock_recv_fail (*cdd_mock_get_g_mock_recv_fail())

/* extern int g_mock_alloc_fail; */
/* extern int g_mock_alloc_count; */
/* extern int g_mock_pthread_fail; */

#ifdef __cplusplus
}
#endif

#endif

/* extern int g_mock_pipe_fail; */
/* extern int g_mock_fork_fail; */
/* extern int g_mock_waitpid_fail; */

/* extern int g_mock_select_fail; */
#ifndef _WIN32
#ifndef _WIN32
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout);
#endif
#endif

/* extern int g_mock_time_jump; */
/* extern int g_mock_time_jump_count; */
long long c_abstract_http_mock_math_get_current_time_ms(void);

void dummy_cb_thread(void *arg);
extern int c_abstract_http_mock_cdd_strdup(const char *s, char **out);
