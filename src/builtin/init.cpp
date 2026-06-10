#include "init.h"
#include "test.h"
#include "syscall.h"

void init(void* arg) {
    write("start init, pid = 1\n");
    pa_dump();
    int count = 0;

    if (fork() == 0) {
        execveFunc((uint32)fork_test);
        return;
    }
    while (true)
    {
        if (wait(nullptr) == -1)
        {
            if (count == 0) {
                write("init try to rape~~~\n");
            }
            count++;
            if (count == 50) {
                count = 0;
            }
            yield();
            continue;
        }
        ;
    }
}