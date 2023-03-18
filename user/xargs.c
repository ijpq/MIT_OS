#include "kernel/types.h"
#include "user/user.h"
#include "kernel/types.h"

void debug(char *buf) {
    int r= 0;
    do {
        r = read(0, buf, sizeof(buf));
        if (r) {
            printf("read %d\n", r);
            char *p = buf;
            for (; *p != '\0' || (*p == '\0' && *(p+1) != '\0') ;p++) {
                if (*p != '\n') {
                    printf("%c - %d\n",*p, (int)*p);
                } else {
                    printf("newline\n");
                }
            }
            if (*p == '\0') {
                printf("EOF 0\n");
            }
            memset(buf, 0, 512);
        }
    } while(r);
}

int main(int argc, char *argv[]) {
    // parse string into multi group, multi pararms
    // call group times xargs, with each one group params
    char buf[512];
    int r = 0;
    char *new_argv[argc-2+1]; // subtract xargs and grep, plus from left
    // char *exec_cmd = argv[1];

    do {
        r = read(0, buf, 512);
        if (r) {
            if (fork() == 0) { // child
                for (int i =0;i<argc-2;i++) {
                    new_argv[i] = argv[i+2];
                }
                new_argv[argc-2] = buf;
                exec("grep", new_argv);
                exit(0);
            } else {
                wait(0);
                memset(buf, 0, 512);
            }       
        }

    } while (r);

    exit(0);
    return 0;
}
