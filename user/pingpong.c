#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  int pipe_fd[2];
  char buf;
  if (pipe(pipe_fd) < 0) {
    printf("[-] create pipe failed\n");
    exit(-1);
  }

  int cpid = fork();
  if (cpid < 0) {
    printf("[-] fork failed\n");
    exit(-1);
  }


  if (cpid == 0) {
    // child
    if (read(pipe_fd[0], &buf, 1) != 1) {
      printf("[-] error when recv from pipe\n");
      exit(-1); 
    }
    int pid = getpid();
    printf("%d: received ping\n", pid);
    write(pipe_fd[1], "\xBB", 1);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    exit(0);
  } else {
    // parent
    write(pipe_fd[1], "\xAA", 1);
    if (read(pipe_fd[0], &buf, 1) != 1) {
      printf("[-] error when recv from pipe\n");
      exit(-1); 
    }
    int pid = getpid();
    printf("%d: received pong\n", pid);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    exit(0);
  }
}