#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("cat: missing operand\n");
        return 1;
    }

    char buf[256];
    int failed = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], 0);
        if (fd < 0) {
            printf("cat: cannot open %s\n", argv[i]);
            failed = 1;
            continue;
        }

        int n = 0;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            write(1, buf, n);
        }
        close(fd);
        if (n < 0) {
            printf("cat: read error on %s\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
