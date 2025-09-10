#include "kernel/types.h"
#include "user/user.h"

int main() {
    int p2c[2]; // step1
    int c2p[2]; // step2
    int byte = 1;
    int *buf = &byte;

    pipe(p2c);
    pipe(c2p);

    /* parent's pid */
    int ppid = getpid();

    /* kid's pid */
    int kpid = fork();

    if (kpid == 0) {
        /* kid */
        read(p2c[0], buf, 1);
        close(p2c[0]);
        printf("%d: received ping from pid %d\n", getpid(), ppid);
        write(c2p[1], buf, 1);
        close(c2p[1]);
    } else {
        /* parent */
        write(p2c[1], buf, 1);
        close(p2c[1]);
        read(c2p[0], buf, 1);
        close(c2p[0]);
        printf("%d: received pong from pid %d\n", getpid(), kpid);
    }
    exit(0);
}