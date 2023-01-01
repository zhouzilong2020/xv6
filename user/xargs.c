#include "kernel/param.h"
#include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  char* args[MAXARG];
  char buf[512];

  for (int i = 1; i < argc; i++) {
    int arg_size = strlen(argv[i]) + 1;
    args[i - 1] = (char*)malloc(arg_size);
    memcpy(args[i - 1], argv[i], arg_size);
  }
  args[argc - 1] = buf;

  int i = 0;
  int pid;
  while (read(0, buf + i, 1)) {
    if (buf[i] == '\n') {
      buf[i] = '\0';
      i = 0;
      if ((pid = fork())) { /* parent */
        wait(&pid);
      } else {
        exec(argv[1], args);
      }

    } else {
      i++;
    }
  }

  exit(0);
}
