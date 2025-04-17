#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdarg.h>

void
fail(const char *fmt, ...)
{
  /* Taken from dmenu */
  va_list ap;
  int saved_errno;

  saved_errno = errno;

  va_start(ap, fmt);
  vfprintf(stderr, "boron: ", ap);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, "%s", strerror(saved_errno));
  fputc('\n', stderr);

  exit(EXIT_FAILURE);
}

void
runcmd(const char* cmd) {
  if (cmd == NULL) {
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    fprintf(stderr, "boron: failed to execute command '%s'.\n", cmd);
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    int32_t status;
    waitpid(pid, &status, 0);
    return;
  } else {
    fprintf(stderr, "boron: failed to execute command '%s'.\n", cmd);
    return;
  }
}

char*
getcmdoutput(const char* command) {
  FILE* fp;
  char* result = NULL;
  size_t result_size = 0;
  char buffer[1024];

  fp = popen(command, "r");
  if (fp == NULL) {
    fprintf(stderr, "boron: failed to execute popen command.\n");
    return NULL;
  }
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t buffer_len = strlen(buffer);
    char* new_result = realloc(result, result_size + buffer_len + 1);
    if (new_result == NULL) {
      fprintf(stderr, "boron: failed to allocate memory to hold command output.\n");
      free(result);
      pclose(fp);
      return NULL;
    }
    result = new_result;
    memcpy(result + result_size, buffer, buffer_len);
    result_size += buffer_len;
    result[result_size] = '\0'; 
  }

  if (pclose(fp) == -1) {
    fprintf(stderr,"boron: failed to close command file pointer.\n");
    free(result);
    return NULL;
  }

  return result;
}

