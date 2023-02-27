#include "kernel/types.h"
#include "user.h"
int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: sleep seconds...\n");
    exit(1);
  }
  int sec = atoi(argv[1]);
  if (sec < 0) {
    fprintf(2, "set a non-negative seconds to sleep\n");
    exit(1);
  }
  sleep(sec);
  exit(0);
}