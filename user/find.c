#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

void usage_help() {
  printf("useage: find <path of dir> <name of file>");
}

void recurse_find(char* path, char* file_name) {
  int fd;
  struct dirent de;
  struct stat st;
  char buf[512];
  char* p;

  memcpy(buf, path, strlen(path) + 1);

  if ((fd = open(path, 0)) < 0) {
    printf("find: cannot open %s\n", path);
    return;
  }

  p = buf + strlen(buf);
  *p++ = '/';

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) {
      continue;
    }
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      printf("find: path too long\n");
      break;
    }
    memcpy(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    stat(buf, &st);
    switch (st.type) {
      case T_FILE:
        if (strcmp(de.name, file_name) == 0) {
          printf("%s\n", buf);
        }
        break;
      case T_DIR:
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
          break;
        }
        recurse_find(buf, file_name);
        break;
      default:
        continue;
    }
  }
}

void find(char* path, char* file_name) {
  int fd;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    printf("find: cannot open %s\n", path);
    return;
  }
  if (fstat(fd, &st) < 0) {
    printf("find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    printf("find: %s is not a dir\n", path);
    close(fd);
    return;
  }

  recurse_find(path, file_name);
}

int main(int argc, char* argv[]) {
  // here we assert argc == 3, argv[1] is a directory's path, argv[2] is the name of the file
  if (argc != 3) {
    usage_help();
  }
  find(argv[1], argv[2]);

  exit(0);
}