#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: sleep seconds\n");
    exit(1);
  }
  int seconds = atoi(argv[1]);
  if (sleep(seconds) == 0) {
    exit(0);
  } else {
    exit(1);
  }
}