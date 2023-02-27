#include "../kernel/types.h"
#include "./user.h"

int exit_with_err(char *msg) {
  fprintf(1, "error:");
  fprintf(1, msg);
  fprintf(1, "\n");
  return 0;
}
void create_pipe(int *p) {
  int status = -1;
  if ((status = pipe(p)) != 0) {
    exit_with_err("pipe");
  }
}

int write_int(int fd, int val) {
  int status = -1;
  status = write(fd, (const void *)&val, 4);

  if (status == -1) {
    exit_with_err("write");
  }
  return 1;
}

int read_int(int fd, int *val) {
  int status = -1;
  status = read(fd, (void *)val, sizeof(*val));

  if (status != 4) {  // sys err
    exit_with_err("read");
    return 0;
  }
  if (*val == 0) {  // EOF
    return 0;
  }
  return 1;
}

void format_out(int val) { fprintf(1, "prime %d\n", val); }

void call_fork(int *pipe_fd) {
  // init numbers
  int start_num = -1;
  int num = -1;
  int _EOF = 0;

  // sorting fd
  int copy_read_fd = dup(pipe_fd[0]);
  close(pipe_fd[0]);
  close(pipe_fd[1]);

  read_int(copy_read_fd, &num);
  int p[2];  // each process occupy 2 fd
  create_pipe(p);
  start_num = num;
  if (start_num != _EOF) {
    format_out(start_num);
  } else {
    exit(0);
  }

  while (read_int(copy_read_fd, &num)) {  // read until empty pipe
    if (num % start_num != 0) {           // is prime
      write_int(p[1], num);
    }
  }
  write_int(p[1], _EOF);  // denote EOF
  close(copy_read_fd);    // pipe has been empty

  if (fork() == 0) {  // child
    call_fork(p);
  } else {  // parent
    wait(0);
    close(p[0]);
    close(p[1]);
    exit(0);
  }
}

int main() {
  int p[2];
  int _EOF = 0;
  create_pipe(p);

  for (int i = 0; i < 34; i++) {  // send [2,35] to pipe
    write_int(p[1], i + 2);
  }
  write_int(p[1], _EOF);  // denote EOF, otherwise read will block

  if (fork() == 0) {  // child
    call_fork(p);
  } else {  // parent
    wait(0);
    close(p[0]);
    close(p[1]);  // close the write end of pipe until read end finished,
                  // otherwise it will cause read error
    exit(0);
  }
  return 0;
}
