#include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  int p[2];
  pipe(p);

  int passed_cnt = 0;
  for (int i = 2; i <= 35; i++) {
    int* temp = malloc(sizeof(int));
    *temp = i;
    write(p[1], temp, sizeof(int));
    passed_cnt++;
  }

  while (passed_cnt) {
    int pid;
    if ((pid = fork())) { /* parent */
      close(p[0]);
      close(p[1]);
      wait(&pid);
      exit(0);
    } else {
      int left_r = p[0];
      close(p[1]);

      int first;
      read(left_r, &first, sizeof(int));
      printf("prime %d\n", first);

      pipe(p); /* create a new pipe */
      int right_w = p[1];

      passed_cnt = 0;
      int i;
      while (read(left_r, &i, sizeof(int))) {
        if (i % first == 0) continue;
        int* temp = malloc(sizeof(int));
        *temp = i;

        write(right_w, temp, sizeof(int));
        passed_cnt++;
      }

      close(left_r);
    }
  }
  exit(0);
}
