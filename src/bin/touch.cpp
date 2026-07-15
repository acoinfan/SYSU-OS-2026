#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("touch: missing operand\n");
        return 1;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], 0);
        if (fd >= 0) {
            close(fd);
            continue;
        }
        if (create_file(argv[i], 0) < 0) {
            printf("touch: cannot touch %s\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
