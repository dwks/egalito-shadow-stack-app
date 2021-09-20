#include <stdio.h>
#include <stdlib.h>

void attacker_target(void) {
    printf("successful exploit! congratulations.\n");
    exit(1);
}

void read_input(void) {
    char buf[64];
    printf("buf is at %p, target is at %p\n", buf, attacker_target);
    fflush(stdout);
    fgets(buf, 1024, stdin);
    printf("\nbuf: [%s]\n", buf);
}

int main(int argc, char *argv[]) {
    read_input();
    printf("exit main()\n");
    return 0;
}
