#include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
  if (argc != 2) {
    char* error = "Invalid Input\n";
    write(1, error, strlen(error));
    exit(0);
  }

  int time = atoi(argv[1]);
  sleep(time);

  exit(0);
}
