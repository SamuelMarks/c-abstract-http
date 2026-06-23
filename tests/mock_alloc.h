#ifndef CDD_MOCK_ALLOC_H
#define CDD_MOCK_ALLOC_H

#ifdef _WIN32
#pragma push_macro("socket")
#pragma push_macro("bind")
#pragma push_macro("listen")
#pragma push_macro("accept")
#pragma push_macro("recv")
#pragma push_macro("select")
#undef socket
#undef bind
#undef listen
#undef accept
#undef recv
#undef select
/* clang-format off */
#include <winsock2.h>
#pragma pop_macro("select")
#pragma pop_macro("recv")
#pragma pop_macro("accept")
#pragma pop_macro("listen")
#pragma pop_macro("bind")
#pragma pop_macro("socket")
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <c_abstract_http/http_types.h>
/* clang-format on */

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
extern int *cdd_mock_get_g_mock_select_error_fds(void);
extern int *cdd_mock_get_g_mock_time_jump(void);
extern int *cdd_mock_get_g_mock_time_jump_count(void);
extern int *cdd_mock_get_g_mock_fwrite_fail(void);
extern int *cdd_mock_get_g_mock_fclose_fail(void);
extern int *cdd_mock_get_g_mock_socket_fail(void);
extern int *cdd_mock_get_g_mock_bind_fail(void);
extern int *cdd_mock_get_g_mock_listen_fail(void);
extern int *cdd_mock_get_g_mock_accept_fail(void);
extern int *cdd_mock_get_g_mock_recv_fail(void);

void *c_abstract_http_mock_malloc(size_t size);
void *c_abstract_http_mock_calloc(size_t count, size_t size);
void *c_abstract_http_mock_realloc(void *ptr, size_t size);
void c_abstract_http_mock_free(void *ptr);
size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nmemb,
                                   FILE *stream);
int c_abstract_http_mock_fclose(FILE *stream);
#ifndef _WIN32
int c_abstract_http_mock_pipe(int fildes[2]);
pid_t c_abstract_http_mock_fork(void);
pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options);
#endif

#if defined(_WIN32)
extern SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type,
                                                 int protocol);
extern int WSAAPI c_abstract_http_mock_bind(SOCKET socket,
                                            const struct sockaddr *address,
                                            int address_len);
extern int WSAAPI c_abstract_http_mock_listen(SOCKET socket, int backlog);
extern SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket,
                                                 struct sockaddr *address,
                                                 int *address_len);
extern int WSAAPI c_abstract_http_mock_recv(SOCKET socket, char *buffer,
                                            int length, int flags);
extern int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                              fd_set *writefds,
                                              fd_set *exceptfds,
                                              const struct timeval *timeout);
#else
extern int c_abstract_http_mock_socket(int domain, int type, int protocol);
extern int c_abstract_http_mock_bind(int socket, const struct sockaddr *address,
                                     socklen_t address_len);
extern int c_abstract_http_mock_listen(int socket, int backlog);
extern int c_abstract_http_mock_accept(int socket, struct sockaddr *address,
                                       socklen_t *address_len);
extern ssize_t c_abstract_http_mock_recv(int socket, void *buffer,
                                         size_t length, int flags);
extern int c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *exceptfds,
                                       struct timeval *timeout);
#endif

#define g_mock_alloc_fail (*cdd_mock_get_g_mock_alloc_fail())
#define g_mock_alloc_count (*cdd_mock_get_g_mock_alloc_count())
#define g_mock_pthread_fail (*cdd_mock_get_g_mock_pthread_fail())
#define g_mock_pipe_fail (*cdd_mock_get_g_mock_pipe_fail())
#define g_mock_fork_fail (*cdd_mock_get_g_mock_fork_fail())
#define g_mock_waitpid_fail (*cdd_mock_get_g_mock_waitpid_fail())
#define g_mock_select_fail (*cdd_mock_get_g_mock_select_fail())
#define g_mock_select_error_fds (*cdd_mock_get_g_mock_select_error_fds())
#define g_mock_time_jump (*cdd_mock_get_g_mock_time_jump())
#define g_mock_time_jump_count (*cdd_mock_get_g_mock_time_jump_count())
#define g_mock_fwrite_fail (*cdd_mock_get_g_mock_fwrite_fail())
#define g_mock_fclose_fail (*cdd_mock_get_g_mock_fclose_fail())
#define g_mock_socket_fail (*cdd_mock_get_g_mock_socket_fail())
#define g_mock_bind_fail (*cdd_mock_get_g_mock_bind_fail())
#define g_mock_listen_fail (*cdd_mock_get_g_mock_listen_fail())
#define g_mock_accept_fail (*cdd_mock_get_g_mock_accept_fail())
#define g_mock_recv_fail (*cdd_mock_get_g_mock_recv_fail())

uint64_t c_abstract_http_mock_math_get_current_time_ms(void);

void dummy_cb_thread(void *arg);
extern int c_abstract_http_mock_cdd_strdup(const char *s, char **out);

#ifdef __cplusplus
}
#endif

#endif
