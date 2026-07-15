#include "syscall.h"

extern int main(int argc, char** argv);

extern "C" void _start(int argc, char** argv)
{
    int ret = main(argc, argv);
    exit(ret);
    while (1) {
        yield();
    }
}
