#include "syscall.h"
#include "stdlib.h"

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            write(" ");
        }
        write(argv[i]);
    }
    write("\n");
    return 0;
}
