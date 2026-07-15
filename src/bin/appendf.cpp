#include "stdio.h"
#include "syscall.h"
#include "stdlib.h"

static int append_args(int fd, int argc, char** argv, int start)
{
    int written = 0;
    for (int i = start; i < argc; i++) {
        if (i > start) {
            int r = fdappend(fd, (void*)" ", 1);
            if (r != 1) return -1;
            written += r;
        }
        int len = strlen(argv[i]);
        int r = fdappend(fd, argv[i], len);
        if (r != len) return -1;
        written += r;
    }
    int r = fdappend(fd, (void*)"\n", 1);
    if (r != 1) return -1;
    return written + 1;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("appendf: usage: appendf path text...\n");
        return 1;
    }

    int fd = open(argv[1], 0);
    if (fd < 0) {
        if (create_file(argv[1], 0) < 0) {
            printf("appendf: cannot create %s\n", argv[1]);
            return 1;
        }
        fd = open(argv[1], 0);
    }
    if (fd < 0) {
        printf("appendf: cannot open %s\n", argv[1]);
        return 1;
    }

    int ret = append_args(fd, argc, argv, 2);
    close(fd);
    sync();
    if (ret < 0) {
        printf("appendf: write failed\n");
        return 1;
    }
    return 0;
}
