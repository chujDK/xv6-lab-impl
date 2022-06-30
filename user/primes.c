#include "kernel/types.h"
#include "user/user.h"

void pipeline(int p_fd[2]) {
  int p, n;
  int c_status;
  close(p_fd[1]);
  if (read(p_fd[0], &p, sizeof(int)) == 0) {
    exit(0);
  }
  printf("prime %d\n", p);

  int fd[2];
  if (pipe(fd) < 0) {
    exit(-1);
  }
  int c_pid = fork();
  if (c_pid < 0) {
    exit(-1);
  } else if (c_pid == 0) {
    close(p_fd[0]);
    pipeline(fd);
  } else {
    close(fd[0]);
    while (1) {
      if (read(p_fd[0], &n, sizeof(int)) <= 0) {
        close(fd[1]);
        close(p_fd[0]);
        wait(&c_status);
        exit(c_status);
      }
      if (n % p != 0) {
        write(fd[1], &n, sizeof(int));
      }
    }
  }
}

int main(int argc, char* argv[]) {
  int p_fd[2];
  if (pipe(p_fd) < 0) {
    exit(0);
  }

  int c_pid = fork();
  int c_status;
  if (c_pid < 0) {
    exit(-1);
  } else if (c_pid == 0) {
    pipeline(p_fd);
  } else {
    close(p_fd[0]);
    // generate all numbers
    for (int i = 2; i <= 35; i++) {
      write(p_fd[1], &i, sizeof(int));
    }
    close(p_fd[1]);
    wait(&c_status);
  }
  exit(c_status);
}