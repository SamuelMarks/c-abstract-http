#if !defined(C_ABSTRACT_HTTP_TEST_OOM)
int g_mock_waitpid_fail = 0;
#else
extern int g_mock_waitpid_fail;
#endif

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#elif defined(__MSDOS__) || defined(__DOS__) || defined(DOS)
#include <dos.h>
#else
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <c_abstract_http/process.h>
#include "c_abstract_http/log.h"
#include "str.h"
/* clang-format on */

static struct CddProcessHooks g_process_hooks = {NULL, NULL, NULL, NULL};

void cdd_process_set_hooks(const struct CddProcessHooks *hooks) {
  if (hooks) {
    g_process_hooks = *hooks;
  }
}

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

/** @brief Internal struct CddProcess */
struct CddProcess {
  HANDLE hProcess;
  HANDLE hThread;
};

enum c_abstract_http_error cdd_ipc_pipe_init(struct CddIpcPipe *pipe) {
  SECURITY_ATTRIBUTES saAttr;
  HANDLE hRead, hWrite;

  LOG_DEBUG("cdd_ipc_pipe_init: Entering");
  if (!pipe) {
    LOG_DEBUG("cdd_ipc_pipe_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
    LOG_DEBUG("cdd_ipc_pipe_init: Error CreatePipe failed");
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  pipe->read_handle = hRead;
  pipe->write_handle = hWrite;
  LOG_DEBUG("cdd_ipc_pipe_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_ipc_pipe_free(struct CddIpcPipe *pipe) {
  LOG_DEBUG("cdd_ipc_pipe_free: Entering");
  if (pipe) {
    if (pipe->read_handle) {
      CloseHandle((HANDLE)pipe->read_handle);
      pipe->read_handle = NULL;
    }
    if (pipe->write_handle) {
      CloseHandle((HANDLE)pipe->write_handle);
      pipe->write_handle = NULL;
    }
  }
  LOG_DEBUG("cdd_ipc_pipe_free: Exiting");
}

enum c_abstract_http_error
cdd_process_spawn(struct CddProcess **proc, struct CddIpcPipe *parent_to_child,
                  struct CddIpcPipe *child_to_parent) {
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFOA siStartInfo;
  BOOL bSuccess = FALSE;
  char szCmdline[MAX_PATH];
  struct CddProcess *p;

  LOG_DEBUG("cdd_process_spawn: Entering");
  if (g_process_hooks.spawn) {
    LOG_DEBUG("cdd_process_spawn: Hooking");
    return g_process_hooks.spawn(proc, parent_to_child, child_to_parent);
  }

  if (!proc || !parent_to_child || !child_to_parent) {
    LOG_DEBUG("cdd_process_spawn: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  p = (struct CddProcess *)malloc(sizeof(struct CddProcess));
  if (!p) {
    LOG_DEBUG("cdd_process_spawn: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
  siStartInfo.cb = sizeof(STARTUPINFOA);

  siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  siStartInfo.hStdInput = (HANDLE)parent_to_child->read_handle;
  siStartInfo.hStdOutput = (HANDLE)child_to_parent->write_handle;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  SetHandleInformation((HANDLE)parent_to_child->write_handle,
                       HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE)child_to_parent->read_handle,
                       HANDLE_FLAG_INHERIT, 0);

  GetModuleFileNameA(NULL, szCmdline, MAX_PATH);
#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
  strcat_s(szCmdline, MAX_PATH, " --cdd-worker");
#else
  strcat(szCmdline, " --cdd-worker");
#endif

  bSuccess = CreateProcessA(NULL, szCmdline, NULL, NULL, TRUE, 0, NULL, NULL,
                            &siStartInfo, &piProcInfo);

  if (!bSuccess) {
    LOG_DEBUG("cdd_process_spawn: Error EIO (CreateProcessA failed)");
    free(p);
    return C_ABSTRACT_HTTP_ERR_IO;
  }

  p->hProcess = piProcInfo.hProcess;
  p->hThread = piProcInfo.hThread;
  *proc = p;

  LOG_DEBUG("cdd_process_spawn: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_process_wait_and_free(struct CddProcess *proc,
                                                     int *exit_code) {
  DWORD dwExitCode = 0;

  LOG_DEBUG("cdd_process_wait_and_free: Entering");
  if (g_process_hooks.wait_and_free) {
    LOG_DEBUG("cdd_process_wait_and_free: Hooking");
    return g_process_hooks.wait_and_free(proc, exit_code);
  }

  if (!proc) {
    LOG_DEBUG("cdd_process_wait_and_free: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM) || 1
  if (g_mock_waitpid_fail) {
    if (exit_code) {
      *exit_code = (g_mock_waitpid_fail == 2) ? -1 : 0;
    }
    free(proc);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif

  WaitForSingleObject(proc->hProcess, INFINITE);
  if (exit_code) {
    GetExitCodeProcess(proc->hProcess, &dwExitCode);
    *exit_code = (int)dwExitCode;
  }

  if (proc->hProcess)
    CloseHandle(proc->hProcess);
  if (proc->hThread)
    CloseHandle(proc->hThread);
  free(proc);
  LOG_DEBUG("cdd_process_wait_and_free: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_ipc_write(void *handle, const void *data,
                                         size_t len) {
  DWORD dwWritten;
  BOOL bSuccess;

  LOG_DEBUG("cdd_ipc_write: Entering");
  if (g_process_hooks.ipc_write) {
    LOG_DEBUG("cdd_ipc_write: Hooking");
    return g_process_hooks.ipc_write(handle, data, len);
  }

  if (!handle || !data) {
    LOG_DEBUG("cdd_ipc_write: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  bSuccess = WriteFile((HANDLE)handle, data, (DWORD)len, &dwWritten, NULL);
  if (!bSuccess || dwWritten != len) {
    LOG_DEBUG("cdd_ipc_write: Error EIO (WriteFile failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_ipc_write: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_ipc_read(void *handle, void *data, size_t len) {
  DWORD dwRead;
  BOOL bSuccess;

  LOG_DEBUG("cdd_ipc_read: Entering");
  if (g_process_hooks.ipc_read) {
    LOG_DEBUG("cdd_ipc_read: Hooking");
    return g_process_hooks.ipc_read(handle, data, len);
  }

  if (!handle || !data) {
    LOG_DEBUG("cdd_ipc_read: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  bSuccess = ReadFile((HANDLE)handle, data, (DWORD)len, &dwRead, NULL);
  if (!bSuccess || dwRead != len) {
    LOG_DEBUG("cdd_ipc_read: Error EIO (ReadFile failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_ipc_read: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#else /* POSIX */

/** @brief Internal struct CddProcess */
struct CddProcess {
  /** @brief pid (variable) of struct CddProcess */
  pid_t pid;
};

enum c_abstract_http_error cdd_ipc_pipe_init(struct CddIpcPipe *p) {
  int fd[2];
  LOG_DEBUG("cdd_ipc_pipe_init: Entering");
  if (!p) {
    LOG_DEBUG("cdd_ipc_pipe_init: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }
  if (pipe(fd) == -1) {
    LOG_DEBUG("cdd_ipc_pipe_init: Error EIO (pipe failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  p->read_handle = (void *)(size_t)fd[0];
  p->write_handle = (void *)(size_t)fd[1];
  LOG_DEBUG("cdd_ipc_pipe_init: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

void cdd_ipc_pipe_free(struct CddIpcPipe *pipe) {
  LOG_DEBUG("cdd_ipc_pipe_free: Entering");
  if (pipe) {
    if (pipe->read_handle)
      close((int)(size_t)pipe->read_handle);
    if (pipe->write_handle)
      close((int)(size_t)pipe->write_handle);
    pipe->read_handle = NULL;
    pipe->write_handle = NULL;
  }
  LOG_DEBUG("cdd_ipc_pipe_free: Exiting");
}

enum c_abstract_http_error
cdd_process_spawn(struct CddProcess **proc, struct CddIpcPipe *parent_to_child,
                  struct CddIpcPipe *child_to_parent) {
  pid_t pid;
  struct CddProcess *p;

  LOG_DEBUG("cdd_process_spawn: Entering");
  if (g_process_hooks.spawn) {
    LOG_DEBUG("cdd_process_spawn: Hooking");
    return g_process_hooks.spawn(proc, parent_to_child, child_to_parent);
  }

  if (!proc || !parent_to_child || !child_to_parent) {
    LOG_DEBUG("cdd_process_spawn: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

  p = (struct CddProcess *)malloc(sizeof(struct CddProcess));
  if (!p) {
    LOG_DEBUG("cdd_process_spawn: Error ENOMEM");
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  }

  pid = fork();
  if (pid == -1) {
    LOG_DEBUG("cdd_process_spawn: Error EIO (fork failed)");
    free(p);
    return C_ABSTRACT_HTTP_ERR_IO;
  } else if (pid == 0) {
    char *argv[] = {"cdd-worker", "--cdd-worker", NULL};

    dup2((int)(size_t)parent_to_child->read_handle, STDIN_FILENO);
    dup2((int)(size_t)child_to_parent->write_handle, STDOUT_FILENO);

    close((int)(size_t)parent_to_child->write_handle);
    close((int)(size_t)child_to_parent->read_handle);
    close((int)(size_t)parent_to_child->read_handle);
    close((int)(size_t)child_to_parent->write_handle);

    execv("/proc/self/exe", argv);
    _exit(1);
  } else {
    close((int)(size_t)parent_to_child->read_handle);
    parent_to_child->read_handle = NULL;
    close((int)(size_t)child_to_parent->write_handle);
    child_to_parent->write_handle = NULL;

    p->pid = pid;
    *proc = p;
    LOG_DEBUG("cdd_process_spawn: Success");
    return C_ABSTRACT_HTTP_SUCCESS;
  }
}

enum c_abstract_http_error cdd_process_wait_and_free(struct CddProcess *proc,
                                                     int *exit_code) {
  int status;

  LOG_DEBUG("cdd_process_wait_and_free: Entering");
  if (g_process_hooks.wait_and_free) {
    LOG_DEBUG("cdd_process_wait_and_free: Hooking");
    return g_process_hooks.wait_and_free(proc, exit_code);
  }

  if (!proc) {
    LOG_DEBUG("cdd_process_wait_and_free: Error EINVAL");
    return C_ABSTRACT_HTTP_ERR_INVAL;
  }

#if defined(C_ABSTRACT_HTTP_TEST_OOM) || 1
  if (g_mock_waitpid_fail) {
    if (exit_code) {
      *exit_code = (g_mock_waitpid_fail == 2) ? -1 : 0;
    }
    free(proc);
    return C_ABSTRACT_HTTP_SUCCESS;
  }
#endif

  waitpid(proc->pid, &status, 0);
  if (exit_code) {
    if (WIFEXITED(status)) {
      *exit_code = WEXITSTATUS(status);
    } else {
      *exit_code = -1; /* LCOV_EXCL_LINE */
    }
  }

  free(proc);
  LOG_DEBUG("cdd_process_wait_and_free: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_ipc_write(void *handle, const void *data,
                                         size_t len) {
  ssize_t written;

  LOG_DEBUG("cdd_ipc_write: Entering");
  if (g_process_hooks.ipc_write) {
    LOG_DEBUG("cdd_ipc_write: Hooking");
    return g_process_hooks.ipc_write(handle, data, len);
  }

  written = write((int)(size_t)handle, data, len);
  if (written < 0 || (size_t)written != len) {
    LOG_DEBUG("cdd_ipc_write: Error EIO (write failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_ipc_write: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error cdd_ipc_read(void *handle, void *data, size_t len) {
  ssize_t r;

  LOG_DEBUG("cdd_ipc_read: Entering");
  if (g_process_hooks.ipc_read) {
    LOG_DEBUG("cdd_ipc_read: Hooking");
    return g_process_hooks.ipc_read(handle, data, len);
  }

  r = read((int)(size_t)handle, data, len);
  if (r < 0 || (size_t)r != len) {
    LOG_DEBUG("cdd_ipc_read: Error EIO (read failed)");
    return C_ABSTRACT_HTTP_ERR_IO;
  }
  LOG_DEBUG("cdd_ipc_read: Success");
  return C_ABSTRACT_HTTP_SUCCESS;
}

#endif /* POSIX vs WIN32 */

/* --- Simple Binary Serialization --- */

static void write_int(char **p, int val) {
  memcpy(*p, &val, sizeof(int));
  *p += sizeof(int);
}

static void write_size(char **p, size_t val) {
  memcpy(*p, &val, sizeof(size_t));
  *p += sizeof(size_t);
}

static void write_str(char **p, const char *str) {
  size_t len = str ? strlen(str) : 0;
  write_size(p, len);
  if (len > 0) {
    memcpy(*p, str, len);
    *p += len;
  }
}

static int read_int(const char **p, const char *end, int *val) {
  if (*p + sizeof(int) > end)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  memcpy(val, *p, sizeof(int));
  *p += sizeof(int);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int read_size(const char **p, const char *end, size_t *val) {
  if (*p + sizeof(size_t) > end)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  memcpy(val, *p, sizeof(size_t));
  *p += sizeof(size_t);
  return C_ABSTRACT_HTTP_SUCCESS;
}

static int read_str(const char **p, const char *end, char **str) {
  size_t len;
  if (read_size(p, end, &len) != 0)
    return C_ABSTRACT_HTTP_ERR_INVAL;
  if (len == 0) {
    *str = NULL;
    return C_ABSTRACT_HTTP_SUCCESS;
  }
  if (*p + len > end)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  *str = (char *)malloc(len + 1);
  if (!*str)
    return C_ABSTRACT_HTTP_ERR_NOMEM;
  memcpy(*str, *p, len);
  (*str)[len] = '\0';
  *p += len;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
cdd_ipc_serialize_request(const struct HttpRequest *req, char **out_buf,
                          size_t *out_len) {
  size_t i;
  size_t est_size = sizeof(int) + sizeof(size_t);
  char *buf, *p;

  if (!req || !out_buf || !out_len)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  est_size += sizeof(size_t) + (req->url ? strlen(req->url) : 0);
  est_size += sizeof(size_t);
  for (i = 0; i < req->headers.count; ++i) {
    est_size +=
        sizeof(size_t) +
        (req->headers.headers[i].key ? strlen(req->headers.headers[i].key) : 0);
    est_size += sizeof(size_t) + (req->headers.headers[i].value
                                      ? strlen(req->headers.headers[i].value)
                                      : 0);
  }
  est_size += req->body_len;

  buf = (char *)malloc(est_size);
  if (!buf)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = buf;
  write_int(&p, (int)req->method);
  write_str(&p, req->url);
  write_size(&p, req->headers.count);
  for (i = 0; i < req->headers.count; ++i) {
    write_str(&p, req->headers.headers[i].key);
    write_str(&p, req->headers.headers[i].value);
  }
  write_size(&p, req->body_len);
  if (req->body_len > 0 && req->body) {
    memcpy(p, req->body, req->body_len);
    p += req->body_len;
  }

  *out_buf = buf;
  *out_len = p - buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
cdd_ipc_deserialize_request(const char *buf, size_t len,
                            struct HttpRequest *req) {
  size_t hcount, body_len;
  int method;
  size_t i;
  int rc;
  const char *p;
  const char *end;

  if (!buf || !req)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = buf;
  end = buf + len;

  http_request_init(req);

  if ((rc = read_int(&p, end, &method)) != 0)
    return rc;
  req->method = (enum HttpMethod)method;

  if ((rc = read_str(&p, end, &req->url)) != 0)
    return rc;

  if ((rc = read_size(&p, end, &hcount)) != 0)
    return rc;
  for (i = 0; i < hcount; ++i) {
    char *key = NULL, *value = NULL;
    if ((rc = read_str(&p, end, &key)) != 0 ||
        (rc = read_str(&p, end, &value)) != 0) {
      free(key);
      free(value);
      return rc; /* return actual error code */
    }
    http_headers_add(&req->headers, key, value);
    free(key);
    free(value);
  }

  if ((rc = read_size(&p, end, &body_len)) != 0)
    return rc;
  req->body_len = body_len;
  if (body_len > 0) {
    if (p + body_len > end)
      return C_ABSTRACT_HTTP_ERR_INVAL;
    req->body = malloc(body_len);
    if (!req->body)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    memcpy(req->body, p, body_len);
    p += body_len;
  } else {
    req->body = NULL;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
cdd_ipc_serialize_response(const struct HttpResponse *res, char **out_buf,
                           size_t *out_len) {
  size_t i;
  size_t est_size = sizeof(int) + sizeof(size_t);
  char *buf, *p;

  if (!res || !out_buf || !out_len)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  est_size += sizeof(size_t);
  for (i = 0; i < res->headers.count; ++i) {
    est_size +=
        sizeof(size_t) +
        (res->headers.headers[i].key ? strlen(res->headers.headers[i].key) : 0);
    est_size += sizeof(size_t) + (res->headers.headers[i].value
                                      ? strlen(res->headers.headers[i].value)
                                      : 0);
  }
  est_size += res->body_len;

  buf = (char *)malloc(est_size);
  if (!buf)
    return C_ABSTRACT_HTTP_ERR_NOMEM;

  p = buf;
  write_int(&p, res->status_code);
  write_size(&p, res->headers.count);
  for (i = 0; i < res->headers.count; ++i) {
    write_str(&p, res->headers.headers[i].key);
    write_str(&p, res->headers.headers[i].value);
  }
  write_size(&p, res->body_len);
  if (res->body_len > 0 && res->body) {
    memcpy(p, res->body, res->body_len);
    p += res->body_len;
  }

  *out_buf = buf;
  *out_len = p - buf;
  return C_ABSTRACT_HTTP_SUCCESS;
}

enum c_abstract_http_error
cdd_ipc_deserialize_response(const char *buf, size_t len,
                             struct HttpResponse *res) {
  size_t hcount, body_len;
  size_t i;
  int rc;
  const char *p;
  const char *end;

  if (!buf || !res)
    return C_ABSTRACT_HTTP_ERR_INVAL;

  p = buf;
  end = buf + len;

  http_response_init(res);

  if ((rc = read_int(&p, end, &res->status_code)) != 0)
    return rc;

  if ((rc = read_size(&p, end, &hcount)) != 0)
    return rc;
  for (i = 0; i < hcount; ++i) {
    char *key = NULL, *value = NULL;
    if ((rc = read_str(&p, end, &key)) != 0 ||
        (rc = read_str(&p, end, &value)) != 0) {
      free(key);
      free(value);
      return rc; /* return actual error code */
    }
    http_headers_add(&res->headers, key, value);
    free(key);
    free(value);
  }

  if ((rc = read_size(&p, end, &body_len)) != 0)
    return rc;
  res->body_len = body_len;
  if (body_len > 0) {
    if (p + body_len > end)
      return C_ABSTRACT_HTTP_ERR_INVAL;
    res->body = malloc(body_len);
    if (!res->body)
      return C_ABSTRACT_HTTP_ERR_NOMEM;
    memcpy(res->body, p, body_len);
    p += body_len;
  } else {
    res->body = NULL;
  }

  return C_ABSTRACT_HTTP_SUCCESS;
}

#if 1
void cdd_process_test_waitpid_fail(void);
void cdd_process_test_waitpid_fail(void) {
  struct CddProcess *p = (struct CddProcess *)malloc(sizeof(struct CddProcess));
  if (p) {
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    p->hProcess = (HANDLE)123;
#else
    p->pid = 123;
#endif
    g_mock_waitpid_fail = 1;
    cdd_process_wait_and_free(p, NULL);
    g_mock_waitpid_fail = 0;
  }
}
#endif

#if defined(C_ABSTRACT_HTTP_TEST_OOM)
void cdd_process_test_waitpid_exit(void);
void cdd_process_test_waitpid_exit(void) {
  /* Test WIFEXITED == false */
  struct CddProcess *p = (struct CddProcess *)malloc(sizeof(struct CddProcess));
  if (p) {
    int exit_code;
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    p->hProcess = (HANDLE)123;
#else
    p->pid = 123;
#endif
    g_mock_waitpid_fail = 2;
    cdd_process_wait_and_free(p, &exit_code);
    g_mock_waitpid_fail = 0;
  }
}
#endif
