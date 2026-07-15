#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("mkdir: missing operand\n");
        return 1;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i]) < 0) {
            printf("mkdir: cannot create %s\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
