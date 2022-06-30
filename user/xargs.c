// this xargs preforms as the UNIX xargs -n 1
#include "kernel/types.h"
#include "user/user.h"

void exec_command(int argc, char* argv[], char* param) {
  char* real_argv[16];
  memset(real_argv, 0, sizeof(real_argv));

  memcpy(real_argv, argv, sizeof(char*) * argc);
  real_argv[argc] = param;
  exec(real_argv[1], real_argv + 1);
}

int main(int argc, char* argv[]) {
  char buf[256];
  char* p = buf;
  while (read(0, p, 1) > 0) {
    if (*p == '\n') {
      *p = 0;
      int c_pid = fork();
      if (c_pid == 0) {
        exec_command(argc, argv, buf);
      } else {
        int status;
        wait(&status);
      }
      p = buf;
    } else {
      p++;
    }
  }
  exit(0);
}