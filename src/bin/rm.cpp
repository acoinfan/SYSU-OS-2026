#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("rm: missing operand\n");
        return 1;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        if (remove_file(argv[i]) < 0) {
            printf("rm: cannot remove %s\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
