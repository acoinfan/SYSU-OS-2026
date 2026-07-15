#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("rmdir: missing operand\n");
        return 1;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            printf("rmdir: cannot remove %s\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
