#include "init.h"
#include "test.h"
#include "syscall.h"

void init(void* arg) {
    write("start init, pid = 1\n");
    pa_dump();
    int count = 0;
    
    // if (fork() == 0) {
    //     execveFunc((uint32)fork_test);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)stack_test);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)test_fd_fork_process);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)test_file_open_close);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)test_file_read_write);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)test_file_append_create_remove_seek);
    //     return;
    // }
    // if (fork() == 0) {
    //     execveFunc((uint32)test_vfs_full);
    //     return;
    // }
    if (fork() == 0) {
        execve("/bin/osh");
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
