
#include "kernel/types.h"
#include "user.h"
int main() {
  int p[2];
  pipe(p);
  const int buffer_size = 512;
  char buf[buffer_size];

  if (fork() == 0) { // child
    read(p[0], buf, buffer_size);
    close(p[0]);

    int pid = getpid();
    fprintf(1, "%d: received ping\n", pid);
    write(p[1], buf, buffer_size);
    close(p[1]);
    exit(0);

  } else { // parent

    char *str = "MESSAGES0";
    write(p[1], str, strlen(str));
    read(p[0], buf, buffer_size);
    close(p[0]);
    int pid = getpid();
    fprintf(1, "%d: received pong\n", pid);
    close(p[1]);
    exit(0);
  }
  return 0;
}
