#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable : 4273)
#endif
#if defined(_MSC_VER)
#pragma warning(disable : 4273)
#endif
#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef pthread_key_create
#undef pthread_setspecific
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
#undef pipe
#undef fork
#undef waitpid
#undef select
#undef math_get_current_time_ms
#undef pthread_getspecific
#undef fwrite
#undef fclose
#undef socket
#undef bind
#undef listen
#undef accept
#undef recv

/* clang-format off */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_alloc.h"

#if defined(_MSC_VER)
#pragma warning(disable : 4565)
#pragma warning(disable : 4559)
#pragma warning(disable : 4273)
#endif

#ifdef _WIN32
int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, const struct timeval *timeout);
#else
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout);
#endif
size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nitems,
                                   FILE *stream);
int c_abstract_http_mock_fclose(FILE *stream);
#ifdef _WIN32
#include <winsock2.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
typedef int socklen_t;
SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type, int protocol);
int WSAAPI c_abstract_http_mock_bind(SOCKET socket, const struct sockaddr *address, socklen_t address_len);
int WSAAPI c_abstract_http_mock_listen(SOCKET socket, int backlog);
SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket, struct sockaddr *address, int *address_len);
int WSAAPI c_abstract_http_mock_recv(SOCKET socket, char *buffer, int length, int flags);
#else
#include <sys/socket.h>
int c_abstract_http_mock_socket(int domain, int type, int protocol);
int c_abstract_http_mock_bind(int socket, const struct sockaddr *address, socklen_t address_len);
int c_abstract_http_mock_listen(int socket, int backlog);
int c_abstract_http_mock_accept(int socket, struct sockaddr *address, socklen_t *address_len);
ssize_t c_abstract_http_mock_recv(int socket, void *buffer, size_t length, int flags);
#endif

#undef g_mock_alloc_fail
#undef g_mock_alloc_count
#undef g_mock_pthread_fail
#undef g_mock_pipe_fail
#undef g_mock_fork_fail
#undef g_mock_waitpid_fail
#undef g_mock_select_fail
#undef g_mock_select_error_fds
#undef g_mock_time_jump
#undef g_mock_time_jump_count
#undef g_mock_fwrite_fail
#undef g_mock_fclose_fail
#undef g_mock_socket_fail
#undef g_mock_bind_fail
#undef g_mock_listen_fail
#undef g_mock_accept_fail
#undef g_mock_recv_fail
#undef g_mock_alloc_fail
#undef g_mock_alloc_count
#undef g_mock_pthread_fail
#undef g_mock_pipe_fail
#undef g_mock_fork_fail
#undef g_mock_waitpid_fail
#undef g_mock_select_fail
#undef g_mock_select_error_fds
#undef g_mock_time_jump
#undef g_mock_time_jump_count
#undef g_mock_fwrite_fail
#undef g_mock_fclose_fail
#undef g_mock_socket_fail
#undef g_mock_bind_fail
#undef g_mock_listen_fail
#undef g_mock_accept_fail
#undef g_mock_recv_fail
int g_mock_alloc_fail = 0;
int g_mock_alloc_count = 0;
int g_mock_pthread_fail = 0;
int g_mock_pipe_fail = 0;
int g_mock_fork_fail = 0;
int g_mock_waitpid_fail = 0;
int g_mock_select_fail = 0;
int g_mock_select_error_fds = 0;
int g_mock_time_jump = 0;
int g_mock_time_jump_count = 0;
int g_mock_fwrite_fail = 0;
int g_mock_fclose_fail = 0;
int g_mock_socket_fail = 0;
int g_mock_bind_fail = 0;
int g_mock_listen_fail = 0;
int g_mock_accept_fail = 0;
int g_mock_recv_fail = 0;

int *cdd_mock_get_g_mock_alloc_fail(void) { return &g_mock_alloc_fail; }
int *cdd_mock_get_g_mock_alloc_count(void) { return &g_mock_alloc_count; }
int *cdd_mock_get_g_mock_pthread_fail(void) { return &g_mock_pthread_fail; }
int *cdd_mock_get_g_mock_pipe_fail(void) { return &g_mock_pipe_fail; }
int *cdd_mock_get_g_mock_fork_fail(void) { return &g_mock_fork_fail; }
int *cdd_mock_get_g_mock_waitpid_fail(void) { return &g_mock_waitpid_fail; }
int *cdd_mock_get_g_mock_select_fail(void) { return &g_mock_select_fail; }
int *cdd_mock_get_g_mock_select_error_fds(void) { return &g_mock_select_error_fds; }
int *cdd_mock_get_g_mock_time_jump(void) { return &g_mock_time_jump; }
int *cdd_mock_get_g_mock_time_jump_count(void) { return &g_mock_time_jump_count; }
int *cdd_mock_get_g_mock_fwrite_fail(void) { return &g_mock_fwrite_fail; }
int *cdd_mock_get_g_mock_fclose_fail(void) { return &g_mock_fclose_fail; }
int *cdd_mock_get_g_mock_socket_fail(void) { return &g_mock_socket_fail; }
int *cdd_mock_get_g_mock_bind_fail(void) { return &g_mock_bind_fail; }
int *cdd_mock_get_g_mock_listen_fail(void) { return &g_mock_listen_fail; }
int *cdd_mock_get_g_mock_accept_fail(void) { return &g_mock_accept_fail; }
int *cdd_mock_get_g_mock_recv_fail(void) { return &g_mock_recv_fail; }



#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef pthread_key_create
#undef pthread_setspecific
#undef pthread_mutex_init
#undef pthread_cond_init
#undef pthread_create
#undef pipe
#undef fork
#undef waitpid
#undef select
#undef math_get_current_time_ms
#undef pthread_getspecific



CDD_MOCK_ALLOC_RESTRICT CDD_MOCK_ALLOC_NOALIAS void *c_abstract_http_mock_malloc(size_t size);
CDD_MOCK_ALLOC_RESTRICT CDD_MOCK_ALLOC_NOALIAS void *c_abstract_http_mock_calloc(size_t count, size_t size);
CDD_MOCK_ALLOC_RESTRICT CDD_MOCK_ALLOC_NOALIAS void *c_abstract_http_mock_realloc(void *ptr, size_t size);
CDD_MOCK_ALLOC_NOALIAS void c_abstract_http_mock_free(void *ptr);
char *c_abstract_http_mock_strdup(const char *s, char **out);

CDD_MOCK_ALLOC_RESTRICT CDD_MOCK_ALLOC_NOALIAS void *c_abstract_http_mock_malloc(size_t size) {
    if (g_mock_alloc_fail)
    if (g_mock_alloc_fail) { if (g_mock_alloc_count-- == 0) { return NULL; } }
    return malloc(size);
}

CDD_MOCK_ALLOC_RESTRICT CDD_MOCK_ALLOC_NOALIAS void *c_abstract_http_mock_calloc(size_t count, size_t size) {
    if (g_mock_alloc_fail && g_mock_alloc_count-- == 0) {
        return NULL;
    }
    return calloc(count, size);
}

CDD_MOCK_ALLOC_RESTRICT CDD_MOCK_ALLOC_NOALIAS void *c_abstract_http_mock_realloc(void *ptr, size_t size) {
    if (g_mock_alloc_fail)
    if (g_mock_alloc_fail)
    if (g_mock_alloc_fail) { if (g_mock_alloc_count-- == 0) { return NULL; } }
    return realloc(ptr, size);
}

CDD_MOCK_ALLOC_NOALIAS void c_abstract_http_mock_free(void *ptr) {
    free(ptr);
}

char *c_abstract_http_mock_strdup(const char *s, char **out) {
    if (g_mock_alloc_fail && g_mock_alloc_count-- == 0) {
        if (out) *out = NULL;
        return NULL;
    }
    if (!s) {
        if (out) *out = NULL;
        return NULL;
    }
    {
        size_t len = strlen(s);
        char *d = (char*)malloc(len + 1);
        if (!d) {
            if (out) *out = NULL;
            return NULL;
        }
        memcpy(d, s, len + 1);
        if (out) *out = d;
        return d;
    }
}

#if !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif
#include <errno.h>
#if !defined(_WIN32)
extern int pthread_key_create(pthread_key_t *, void (*)(void *));
extern int pthread_mutex_init(pthread_mutex_t *, const pthread_mutexattr_t *);
extern int pthread_cond_init(pthread_cond_t *, const pthread_condattr_t *);
extern int pthread_create(pthread_t *, const pthread_attr_t *,
                          void *(*)(void *), void *);
extern int pipe(int[2]);
extern pid_t fork(void);
extern pid_t waitpid(pid_t, int *, int);
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern uint64_t real_math_get_current_time_ms(void);
extern int pthread_setspecific(pthread_key_t, const void *);
extern void *pthread_getspecific(pthread_key_t);
#endif

#if !defined(_WIN32)
int c_abstract_http_mock_pthread_key_create(pthread_key_t *key,
                                            void (*destructor)(void *));
int c_abstract_http_mock_pthread_mutex_init(pthread_mutex_t *mutex,
                                            const pthread_mutexattr_t *attr);
int c_abstract_http_mock_pthread_cond_init(pthread_cond_t *cond,
                                           const pthread_condattr_t *attr);
int c_abstract_http_mock_pthread_create(pthread_t *thread,
                                        const pthread_attr_t *attr,
                                        void *(*start_routine)(void *),
                                        void *arg);
int c_abstract_http_mock_pipe(int fildes[2]);
pid_t c_abstract_http_mock_fork(void);
pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options);
#ifdef _WIN32
int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *errorfds,
                                       const struct timeval *timeout);
#else
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout);
#endif
uint64_t c_abstract_http_mock_math_get_current_time_ms(void);
int c_abstract_http_mock_pthread_setspecific(pthread_key_t key,
                                             const void *value);
void *c_abstract_http_mock_pthread_getspecific(pthread_key_t key);

int c_abstract_http_mock_pthread_key_create(pthread_key_t *key,
                                            void (*destructor)(void *)) {
  if (g_mock_pthread_fail == 1)
    return 1; /* Return non-zero on failure */
  return pthread_key_create(key, destructor);
}

int c_abstract_http_mock_pthread_setspecific(pthread_key_t key,
                                             const void *value) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_setspecific(key, value);
}

void *c_abstract_http_mock_pthread_getspecific(pthread_key_t key) {
  if (g_mock_pthread_fail == 1)
    return NULL;
  return pthread_getspecific(key);
}

int c_abstract_http_mock_pthread_mutex_init(pthread_mutex_t *mutex,
                                            const pthread_mutexattr_t *attr) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_mutex_init(mutex, attr);
}

int c_abstract_http_mock_pthread_cond_init(pthread_cond_t *cond,
                                           const pthread_condattr_t *attr) {
  if (g_mock_pthread_fail == 1)
    return 1;
  return pthread_cond_init(cond, attr);
}

int c_abstract_http_mock_pthread_create(pthread_t *thread,
                                        const pthread_attr_t *attr,
                                        void *(*start_routine)(void *),
                                        void *arg) {
  if (g_mock_pthread_fail == 1)
    return 1;
  if (g_mock_pthread_fail == 2 && g_mock_alloc_count-- == 0)
    return 1;
  return pthread_create(thread, attr, start_routine, arg);
}

int c_abstract_http_mock_pipe(int fildes[2]) {
  if (g_mock_pipe_fail) {
    errno = EMFILE;
    return -1;
  }
  return pipe(fildes);
}

#if defined(__EMSCRIPTEN__)
void __gcov_fork(void);
void __gcov_fork(void) {}
#endif

pid_t c_abstract_http_mock_fork(void) {
  if (g_mock_fork_fail) {
    errno = EAGAIN;
    return -1;
  }
  if (g_mock_waitpid_fail == 3) {
    return 0;
  }
#if !defined(__EMSCRIPTEN__)
  return fork();
#else
  errno = ENOSYS;
  return -1;
#endif
}

pid_t c_abstract_http_mock_waitpid(pid_t pid, int *stat_loc, int options) {
  if (g_mock_waitpid_fail == 1) {
    errno = ECHILD;
    return -1;
  }
  if (g_mock_waitpid_fail == 2) {
    /* WIFEXITED == false */
    if (stat_loc)
      *stat_loc = 0x007F; /* simulate stopped by signal */
    return pid;
  }
  return waitpid(pid, stat_loc, options);
}
#endif

#ifdef _WIN32
int WSAAPI c_abstract_http_mock_select(int nfds, fd_set *readfds,
                                       fd_set *writefds, fd_set *errorfds,
                                       const struct timeval *timeout) {
#else
int c_abstract_http_mock_select(int nfds, fd_set *readfds, fd_set *writefds,
                                fd_set *errorfds, struct timeval *timeout) {
#endif
  if (g_mock_select_fail == 1) {
    errno = C_ABSTRACT_HTTP_ERR_INVAL;
    return -1;
  }
  if (g_mock_select_error_fds == 1) {
    if (readfds)
      FD_ZERO(readfds);
    if (writefds)
      FD_ZERO(writefds);
    if (errorfds) {
      /* Force error flag on all fds */
      unsigned int i;
      for (i = 0; i < (unsigned int)nfds; ++i) {
        FD_SET(i, errorfds);
      }
      return 1;
    }
  }
  return select(nfds, readfds, writefds, errorfds, timeout);
}

#undef math_get_current_time_ms
extern uint64_t real_math_get_current_time_ms(void);

uint64_t c_abstract_http_mock_math_get_current_time_ms(void) {
  uint64_t now = real_math_get_current_time_ms();
  if (g_mock_time_jump) {
    if (g_mock_time_jump_count-- <= 0) {
      now += 1000;
    }
  }
  return now;
}

void dummy_cb_thread(void *arg) { (void)arg; }

int c_abstract_http_mock_cdd_strdup(const char *s, char **out) {
  if (g_mock_alloc_fail && g_mock_alloc_count-- == 0) {
    if (out)
      *out = NULL;
    return C_ABSTRACT_HTTP_ERR_NOMEM; /* C_ABSTRACT_HTTP_ERR_NOMEM */
  }
  if (!s) {
    if (out)
      *out = NULL;
    return 22; /* C_ABSTRACT_HTTP_ERR_INVAL */
  }
  {
    size_t len = strlen(s);
    char *d = (char *)malloc(len + 1);
    if (!d) {
      if (out)
        *out = NULL;
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    }
    memcpy(d, s, len + 1);
    if (out) {
      *out = d;
    } else {
      c_abstract_http_mock_free(d);
    }
    return 0;
  }
}

#include <stddef.h>
#include <stdio.h>

extern size_t fwrite(const void *, size_t, size_t, FILE *);
extern int fclose(FILE *);

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

extern int g_mock_fwrite_fail;
extern int g_mock_fclose_fail;
extern int g_mock_socket_fail;
extern int g_mock_bind_fail;
extern int g_mock_listen_fail;
extern int g_mock_accept_fail;
extern int g_mock_recv_fail;

#undef fwrite
#undef fclose
#undef socket
#undef bind
#undef listen
#undef accept
#undef recv

extern size_t fwrite(const void *, size_t, size_t, FILE *);
extern int fclose(FILE *);

#ifdef _WIN32
/* extern SOCKET socket(int, int, int); */
/* extern int bind(SOCKET, const struct sockaddr *, int); */
/* extern int listen(SOCKET, int); */
/* extern SOCKET accept(SOCKET, struct sockaddr *, int *); */
/* extern int recv(SOCKET, char *, int, int); */
#else
extern int socket(int, int, int);
extern int bind(int, const struct sockaddr *, socklen_t);
extern int listen(int, int);
extern int accept(int, struct sockaddr *, socklen_t *);
extern ssize_t recv(int, void *, size_t, int);
#endif

size_t c_abstract_http_mock_fwrite(const void *ptr, size_t size, size_t nitems,
                                   FILE *stream) {
  if (g_mock_fwrite_fail)
    return 0;
  return fwrite(ptr, size, nitems, stream);
}

int c_abstract_http_mock_fclose(FILE *stream) {

  if (g_mock_fclose_fail) {
    fclose(stream);
    return EOF;
  }
  return fclose(stream);
}

#ifdef _WIN32
#include <BaseTsd.h>
/* clang-format on */
SOCKET WSAAPI c_abstract_http_mock_socket(int domain, int type, int protocol) {
  if (g_mock_socket_fail)
    return INVALID_SOCKET;
  return socket(domain, type, protocol);
}

int WSAAPI c_abstract_http_mock_bind(SOCKET socket,
                                     const struct sockaddr *address,
                                     socklen_t address_len) {
  if (g_mock_bind_fail)
    return SOCKET_ERROR;
  return bind(socket, address, address_len);
}

int WSAAPI c_abstract_http_mock_listen(SOCKET socket, int backlog) {
  if (g_mock_listen_fail)
    return SOCKET_ERROR;
  return listen(socket, backlog);
}

SOCKET WSAAPI c_abstract_http_mock_accept(SOCKET socket,
                                          struct sockaddr *address,
                                          int *address_len) {
  if (g_mock_accept_fail)
    return INVALID_SOCKET;
  return accept(socket, address, address_len);
}

int WSAAPI c_abstract_http_mock_recv(SOCKET socket, char *buffer, int length,
                                     int flags) {
  if (g_mock_recv_fail)
    return SOCKET_ERROR;
  return recv(socket, buffer, length, flags);
}
#else
int c_abstract_http_mock_socket(int domain, int type, int protocol) {
  if (g_mock_socket_fail)
    return -1;
  return socket(domain, type, protocol);
}

int c_abstract_http_mock_bind(int socket, const struct sockaddr *address,
                              socklen_t address_len) {
  if (g_mock_bind_fail)
    return -1;
  return bind(socket, address, address_len);
}

int c_abstract_http_mock_listen(int socket, int backlog) {
  if (g_mock_listen_fail)
    return -1;
  return listen(socket, backlog);
}

int c_abstract_http_mock_accept(int socket, struct sockaddr *address,
                                socklen_t *address_len) {
  if (g_mock_accept_fail)
    return -1;
  return accept(socket, address, address_len);
}

ssize_t c_abstract_http_mock_recv(int socket, void *buffer, size_t length,
                                  int flags) {
  if (g_mock_recv_fail)
    return -1;
  return recv(socket, buffer, length, flags);
}
#endif

#ifdef __APPLE__
int c_abstract_http_mock_select_darwin_extsn(
    int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds,
    struct timeval *timeout) __asm("_c_abstract_http_mock_select$DARWIN_EXTSN");
int c_abstract_http_mock_select_darwin_extsn(int nfds, fd_set *readfds,
                                             fd_set *writefds, fd_set *errorfds,
                                             struct timeval *timeout) {
  return c_abstract_http_mock_select(nfds, readfds, writefds, errorfds,
                                     timeout);
}
#endif
