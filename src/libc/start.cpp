#include "syscall.h"

extern int main();

extern "C" void _start()
{
    int ret = main();
    exit(ret);
    while (1) {
        yield();
    }
}
