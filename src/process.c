/**
 * @file process.c
 * @brief Implementation of cross-platform multiprocessing and serialization.
 * @author Samuel Marks
 */

/* clang-format off */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <c_abstract_http/process.h>
#include <c_abstract_http/str.h>
/* clang-format on */

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)

struct CddProcess {
  HANDLE hProcess;
  HANDLE hThread;
};

int cdd_ipc_pipe_init(struct CddIpcPipe *pipe) {
  SECURITY_ATTRIBUTES saAttr;
  HANDLE hRead, hWrite;

  if (!pipe)
    return EINVAL;

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&hRead, &hWrite, &saAttr, 0)) {
    return EIO;
  }

  pipe->read_handle = hRead;
  pipe->write_handle = hWrite;
  return 0;
}

void cdd_ipc_pipe_free(struct CddIpcPipe *pipe) {
  if (pipe) {
    if (pipe->read_handle)
      CloseHandle((HANDLE)pipe->read_handle);
    if (pipe->write_handle)
      CloseHandle((HANDLE)pipe->write_handle);
    pipe->read_handle = NULL;
    pipe->write_handle = NULL;
  }
}

int cdd_process_spawn(struct CddProcess **proc,
                      struct CddIpcPipe *parent_to_child,
                      struct CddIpcPipe *child_to_parent) {
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFOA siStartInfo;
  BOOL bSuccess = FALSE;
  char szCmdline[MAX_PATH];
  struct CddProcess *p;

  if (!proc || !parent_to_child || !child_to_parent)
    return EINVAL;

  p = (struct CddProcess *)malloc(sizeof(struct CddProcess));
  if (!p)
    return ENOMEM;

  ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
  ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
  siStartInfo.cb = sizeof(STARTUPINFOA);

  /* Set up pipes for stdin/stdout */
  siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  siStartInfo.hStdOutput =
      (HANDLE)parent_to_child
          ->write_handle; /* child writes here? wait, if parent->child it's
                             parent writing to child stdin */

  /* Actually, to properly route, parent writes to parent_to_child.write_handle,
   * child reads from parent_to_child.read_handle */
  /* child writes to child_to_parent.write_handle, parent reads from
   * child_to_parent.read_handle */
  siStartInfo.hStdInput = (HANDLE)parent_to_child->read_handle;
  siStartInfo.hStdOutput = (HANDLE)child_to_parent->write_handle;
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  /* Ensure the write handle to the stdin pipe is not inherited by the child */
  SetHandleInformation((HANDLE)parent_to_child->write_handle,
                       HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE)child_to_parent->read_handle,
                       HANDLE_FLAG_INHERIT, 0);

  GetModuleFileNameA(NULL, szCmdline, MAX_PATH);
  strcat(szCmdline, " --cdd-worker"); /* Custom arg to trigger worker mode */

  bSuccess = CreateProcessA(NULL, szCmdline, /* command line */
                            NULL,            /* process security attributes */
                            NULL, /* primary thread security attributes */
                            TRUE, /* handles are inherited */
                            0,    /* creation flags */
                            NULL, /* use parent's environment */
                            NULL, /* use parent's current directory */
                            &siStartInfo, /* STARTUPINFO pointer */
                            &piProcInfo); /* receives PROCESS_INFORMATION */

  if (!bSuccess) {
    free(p);
    return EIO;
  }

  p->hProcess = piProcInfo.hProcess;
  p->hThread = piProcInfo.hThread;
  *proc = p;

  return 0;
}

int cdd_process_wait_and_free(struct CddProcess *proc, int *exit_code) {
  DWORD dwExitCode = 0;
  if (!proc)
    return EINVAL;

  WaitForSingleObject(proc->hProcess, INFINITE);
  if (exit_code) {
    GetExitCodeProcess(proc->hProcess, &dwExitCode);
    *exit_code = (int)dwExitCode;
  }

  CloseHandle(proc->hProcess);
  CloseHandle(proc->hThread);
  free(proc);
  return 0;
}

int cdd_ipc_write(void *handle, const void *data, size_t len) {
  DWORD dwWritten;
  BOOL bSuccess = WriteFile((HANDLE)handle, data, (DWORD)len, &dwWritten, NULL);
  if (!bSuccess || dwWritten != len)
    return EIO;
  return 0;
}

int cdd_ipc_read(void *handle, void *data, size_t len) {
  DWORD dwRead;
  BOOL bSuccess = ReadFile((HANDLE)handle, data, (DWORD)len, &dwRead, NULL);
  if (!bSuccess || dwRead != len)
    return EIO;
  return 0;
}

#else /* POSIX */

struct CddProcess {
  pid_t pid;
};

int cdd_ipc_pipe_init(struct CddIpcPipe *pipe) {
  int fd[2];
  if (!pipe)
    return EINVAL;
  if (pipe(fd) == -1)
    return EIO;
  /* Store as intptr_t to fit in void* */
  pipe->read_handle = (void *)(size_t)fd[0];
  pipe->write_handle = (void *)(size_t)fd[1];
  return 0;
}

void cdd_ipc_pipe_free(struct CddIpcPipe *pipe) {
  if (pipe) {
    if (pipe->read_handle)
      close((int)(size_t)pipe->read_handle);
    if (pipe->write_handle)
      close((int)(size_t)pipe->write_handle);
    pipe->read_handle = NULL;
    pipe->write_handle = NULL;
  }
}

int cdd_process_spawn(struct CddProcess **proc,
                      struct CddIpcPipe *parent_to_child,
                      struct CddIpcPipe *child_to_parent) {
  pid_t pid;
  struct CddProcess *p;

  if (!proc || !parent_to_child || !child_to_parent)
    return EINVAL;

  p = (struct CddProcess *)malloc(sizeof(struct CddProcess));
  if (!p)
    return ENOMEM;

  pid = fork();
  if (pid == -1) {
    free(p);
    return EIO;
  } else if (pid == 0) {
    /* Child Process */
    char *argv[] = {"cdd-worker", "--cdd-worker", NULL};

    /* Redirect stdin to parent_to_child read end */
    dup2((int)(size_t)parent_to_child->read_handle, STDIN_FILENO);
    /* Redirect stdout to child_to_parent write end */
    dup2((int)(size_t)child_to_parent->write_handle, STDOUT_FILENO);

    /* Close unused pipe ends */
    close((int)(size_t)parent_to_child->write_handle);
    close((int)(size_t)child_to_parent->read_handle);
    close((int)(size_t)parent_to_child->read_handle);
    close((int)(size_t)child_to_parent->write_handle);

    /* Execution - Note: since this is a library, execv is tricky unless we know
       the binary path. For testing/library contexts, we can just return a
       specific exit code to the caller indicating "I am child" or run a
       registered callback. For a true isolated process, we'd exec
       /proc/self/exe on linux. For simplicity in this stub implementation,
       we'll try to exec /proc/self/exe. */
    execv("/proc/self/exe", argv);

    /* If exec fails, exit */
    exit(1);
  } else {
    /* Parent Process */
    /* Close child's ends of the pipes */
    close((int)(size_t)parent_to_child->read_handle);
    parent_to_child->read_handle = NULL;
    close((int)(size_t)child_to_parent->write_handle);
    child_to_parent->write_handle = NULL;

    p->pid = pid;
    *proc = p;
    return 0;
  }
}

int cdd_process_wait_and_free(struct CddProcess *proc, int *exit_code) {
  int status;
  if (!proc)
    return EINVAL;

  waitpid(proc->pid, &status, 0);
  if (exit_code) {
    if (WIFEXITED(status)) {
      *exit_code = WEXITSTATUS(status);
    } else {
      *exit_code = -1;
    }
  }

  free(proc);
  return 0;
}

int cdd_ipc_write(void *handle, const void *data, size_t len) {
  ssize_t written = write((int)(size_t)handle, data, len);
  if (written < 0 || (size_t)written != len)
    return EIO;
  return 0;
}

int cdd_ipc_read(void *handle, void *data, size_t len) {
  ssize_t r = read((int)(size_t)handle, data, len);
  if (r < 0 || (size_t)r != len)
    return EIO;
  return 0;
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
    return EINVAL;
  memcpy(val, *p, sizeof(int));
  *p += sizeof(int);
  return 0;
}

static int read_size(const char **p, const char *end, size_t *val) {
  if (*p + sizeof(size_t) > end)
    return EINVAL;
  memcpy(val, *p, sizeof(size_t));
  *p += sizeof(size_t);
  return 0;
}

static int read_str(const char **p, const char *end, char **str) {
  char *_ast_strdup_x = NULL;
  size_t len;
  if (read_size(p, end, &len) != 0)
    return EINVAL;
  if (len == 0) {
    *str = NULL;
    return 0;
  }
  if (*p + len > end)
    return EINVAL;

  *str = (char *)malloc(len + 1);
  if (!*str)
    return ENOMEM;
  memcpy(*str, *p, len);
  (*str)[len] = '\0';
  *p += len;
  return 0;
}

int cdd_ipc_serialize_request(const struct HttpRequest *req, char **out_buf,
                              size_t *out_len) {
  size_t i;
  size_t est_size = sizeof(int) + sizeof(size_t); /* method + body_len */
  char *buf, *p;

  if (!req || !out_buf || !out_len)
    return EINVAL;

  /* Estimate size */
  est_size += sizeof(size_t) + (req->url ? strlen(req->url) : 0);
  est_size += sizeof(size_t); /* header count */
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
    return ENOMEM;

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
  return 0;
}

int cdd_ipc_deserialize_request(const char *buf, size_t len,
                                struct HttpRequest *req) {
  const char *p = buf;
  const char *end = buf + len;
  size_t hcount, body_len;
  int method;
  size_t i;
  int rc;

  if (!buf || !req)
    return EINVAL;

  if ((rc = http_request_init(req)) != 0)
    return rc;

  if (read_int(&p, end, &method) != 0)
    return EINVAL;
  req->method = (enum HttpMethod)method;

  if (read_str(&p, end, &req->url) != 0)
    return EINVAL;

  if (read_size(&p, end, &hcount) != 0)
    return EINVAL;
  for (i = 0; i < hcount; ++i) {
    char *key = NULL, *value = NULL;
    if (read_str(&p, end, &key) != 0 || read_str(&p, end, &value) != 0) {
      if (key)
        free(key);
      if (value)
        free(value);
      return EINVAL;
    }
    http_headers_add(&req->headers, key, value);
    free(key);
    free(value);
  }

  if (read_size(&p, end, &body_len) != 0)
    return EINVAL;
  req->body_len = body_len;
  if (body_len > 0) {
    if (p + body_len > end)
      return EINVAL;
    req->body = malloc(body_len);
    if (!req->body)
      return ENOMEM;
    memcpy(req->body, p, body_len);
    p += body_len;
  } else {
    req->body = NULL;
  }

  return 0;
}

int cdd_ipc_serialize_response(const struct HttpResponse *res, char **out_buf,
                               size_t *out_len) {
  size_t i;
  size_t est_size = sizeof(int) + sizeof(size_t); /* status + body_len */
  char *buf, *p;

  if (!res || !out_buf || !out_len)
    return EINVAL;

  est_size += sizeof(size_t); /* header count */
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
    return ENOMEM;

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
  return 0;
}

int cdd_ipc_deserialize_response(const char *buf, size_t len,
                                 struct HttpResponse *res) {
  const char *p = buf;
  const char *end = buf + len;
  size_t hcount, body_len;
  size_t i;
  int rc;

  if (!buf || !res)
    return EINVAL;

  if ((rc = http_response_init(res)) != 0)
    return rc;

  if (read_int(&p, end, &res->status_code) != 0)
    return EINVAL;

  if (read_size(&p, end, &hcount) != 0)
    return EINVAL;
  for (i = 0; i < hcount; ++i) {
    char *key = NULL, *value = NULL;
    if (read_str(&p, end, &key) != 0 || read_str(&p, end, &value) != 0) {
      if (key)
        free(key);
      if (value)
        free(value);
      return EINVAL;
    }
    http_headers_add(&res->headers, key, value);
    free(key);
    free(value);
  }

  if (read_size(&p, end, &body_len) != 0)
    return EINVAL;
  res->body_len = body_len;
  if (body_len > 0) {
    if (p + body_len > end)
      return EINVAL;
    res->body = malloc(body_len);
    if (!res->body)
      return ENOMEM;
    memcpy(res->body, p, body_len);
    p += body_len;
  } else {
    res->body = NULL;
  }

  return 0;
}