/* LCOV_EXCL_BR_START */
#ifndef ABSTRACT_HTTP_MOCK_ALLOC_H
#define ABSTRACT_HTTP_MOCK_ALLOC_H

/* clang-format off */
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
#include <pthread.h>
#endif

#include <stdio.h>
#include <c_abstract_http/http_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int *abstract_http_mock_get_g_mock_sha1_fail(void);
extern int *abstract_http_mock_get_g_mock_alloc_fail(void);
extern int *abstract_http_mock_get_g_mock_alloc_count(void);
extern int *abstract_http_mock_get_g_mock_pthread_fail(void);
extern int *abstract_http_mock_get_g_mock_pipe_fail(void);
extern int *abstract_http_mock_get_g_mock_fork_fail(void);
extern int *abstract_http_mock_get_g_mock_waitpid_fail(void);
extern int *abstract_http_mock_get_g_mock_select_fail(void);
extern int *abstract_http_mock_get_g_mock_select_error_fds(void);
extern int *abstract_http_mock_get_g_mock_time_jump(void);
extern int *abstract_http_mock_get_g_mock_time_jump_count(void);
extern int *abstract_http_mock_get_g_mock_fwrite_fail(void);
extern int *abstract_http_mock_get_g_mock_fclose_fail(void);
extern int *abstract_http_mock_get_g_mock_socket_fail(void);
extern int *abstract_http_mock_get_g_mock_bind_fail(void);
extern int *abstract_http_mock_get_g_mock_listen_fail(void);
extern int *abstract_http_mock_get_g_mock_accept_fail(void);
extern int *abstract_http_mock_get_g_mock_recv_fail(void);
extern int *abstract_http_mock_get_g_mock_getsockname_fail(void);

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT __declspec(restrict)
#define ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS __declspec(noalias)
#else
#define ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT
#define ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS
#endif

ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_malloc(size_t size);
ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_calloc(size_t count, size_t size);
ABSTRACT_HTTP_MOCK_ALLOC_RESTRICT ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void *
c_abstract_http_mock_realloc(void *ptr, size_t size);
ABSTRACT_HTTP_MOCK_ALLOC_NOALIAS void c_abstract_http_mock_free(void *ptr);

size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nmemb,
                                   FILE *stream);
int c_abstract_http_mock_fclose(FILE *stream);
#ifndef _WIN32
int c_abstract_http_mock_pipe(int fildes[2]);
pid_t c_abstract_http_mock_fork(void);
pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options);
int c_abstract_http_mock_pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int c_abstract_http_mock_pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int c_abstract_http_mock_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int c_abstract_http_mock_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int c_abstract_http_mock_pthread_join(pthread_t thread, void **value_ptr);
int c_abstract_http_mock_pthread_setspecific(pthread_key_t key, const void *value);
void *c_abstract_http_mock_pthread_getspecific(pthread_key_t key);
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
extern int WSAAPI c_abstract_http_mock_getsockname(SOCKET socket,
                                                   struct sockaddr *address,
                                                   int *address_len);
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
extern int c_abstract_http_mock_getsockname(int socket,
                                            struct sockaddr *address,
                                            socklen_t *address_len);
extern int c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *exceptfds,
                                       struct timeval *timeout);
#endif

#define g_mock_alloc_fail (*abstract_http_mock_get_g_mock_alloc_fail())
#define g_mock_alloc_count (*abstract_http_mock_get_g_mock_alloc_count())
#define g_mock_pthread_fail (*abstract_http_mock_get_g_mock_pthread_fail())
#define g_mock_pipe_fail (*abstract_http_mock_get_g_mock_pipe_fail())
#define g_mock_fork_fail (*abstract_http_mock_get_g_mock_fork_fail())
#define g_mock_waitpid_fail (*abstract_http_mock_get_g_mock_waitpid_fail())
#define g_mock_select_fail (*abstract_http_mock_get_g_mock_select_fail())
#define g_mock_select_error_fds (*abstract_http_mock_get_g_mock_select_error_fds())
#define g_mock_time_jump (*abstract_http_mock_get_g_mock_time_jump())
#define g_mock_time_jump_count (*abstract_http_mock_get_g_mock_time_jump_count())
#define g_mock_fwrite_fail (*abstract_http_mock_get_g_mock_fwrite_fail())
#define g_mock_fclose_fail (*abstract_http_mock_get_g_mock_fclose_fail())
#define g_mock_socket_fail (*abstract_http_mock_get_g_mock_socket_fail())
#define g_mock_bind_fail (*abstract_http_mock_get_g_mock_bind_fail())
#define g_mock_listen_fail (*abstract_http_mock_get_g_mock_listen_fail())
#define g_mock_accept_fail (*abstract_http_mock_get_g_mock_accept_fail())
#define g_mock_recv_fail (*abstract_http_mock_get_g_mock_recv_fail())
#define g_mock_getsockname_fail (*abstract_http_mock_get_g_mock_getsockname_fail())

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
/* clang-format on */

uint64_t c_abstract_http_mock_math_get_current_time_ms(void);

void dummy_cb_thread(void *arg);
void *dummy_cb_pthread(void *arg);
extern enum c_abstract_http_error c_abstract_http_mock_strdup(const char *s,
                                                              char **out);

#ifdef __cplusplus
}
#endif

#endif

/* LCOV_EXCL_BR_STOP */
