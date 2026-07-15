#include "init.h"
#include "test.h"
#include "syscall.h"
#include "malloctest.h"

void init(void* arg) {
    write("start init, pid = 1\n");
    pa_dump();
    int count = 0;

    // if (fork() == 0) {
    //     execveFunc((uint32)fork_test);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)test_fd_fork_process);
    //     return;
    // }
    // if (fork() == 0) {
    //     execve("/elf_test");
    //     return;
    // }
    // if (fork() == 0) {
    //     execve("/elf_test");
    //     return;
    // }
    // if (fork() == 0) {
    //     execve("/elf_test");
    //     return;
    // }
    if (fork() == 0) {
        execve("/complex");
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
        } else {
            write("init rape 1 process\n");
        }
        ;
    }
}