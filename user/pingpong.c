
#include "kernel/types.h"
#include "user.h"
int main() {
  int p[2];
  pipe(p);
  char *buf = "";

  if (fork() == 0) { // child
    read(p[0], buf, sizeof(buf));
    close(p[0]);

    int pid = getpid();
    fprintf(1, "%d: received ping\n", pid);
    write(p[1], buf, sizeof(buf));
    close(p[1]);
    exit(0);

  } else { // parent

    char *str = "MESSAGES0";
    write(p[1], str, sizeof(str));
    read(p[0], buf, sizeof(buf));
    close(p[0]);
    int pid = getpid();
    fprintf(1, "%d: received pong\n", pid);
    close(p[1]);
    exit(0);
  }
  return 0;
}
