#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: sleep <time>\n");
        exit(-1);
    }

    int ticks = atoi(argv[1]);

    sleep(ticks);

    printf("wake up from sleep\n");
    exit(0);
}