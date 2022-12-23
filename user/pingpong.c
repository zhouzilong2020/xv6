
#include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  int p[2];
  pipe(p);

  if (fork()) { /* parent */
    char c = 'a';
    write(p[1], &c, 1);
    read(p[0], &c, 1);

    printf("%d: received pong\n", getpid());

    close(p[0]);
    close(p[1]);
  } else { /* child */
    char c;
    read(p[0], &c, 1);

    printf("%d: received ping\n", getpid());

    write(p[1], &c, 1);

    close(p[0]);
    close(p[1]);
  }

  exit(0);
}
