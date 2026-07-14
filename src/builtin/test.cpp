#include "test.h"
#include "syscall.h"
#include "stdio.h"

#define USER_VADDR_START 0x8048000
#define PAGE_SIZE        4096
void test_print_something(void* arg) {
    write("Call Print something\n");
    write("Welcome to use SYSU-OS-2026\n");
    write("Author: A_coin_fan\n");
    write("Hello from 2026/6/9\n");
}

// void test_out_of_memory(void* arg) {
//     write("Out Of Memory Begin\n");
//     interruptManager.disableTimeInterrupt();
//     char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
//     char* p1 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);

//     // OK
//     *p0 = 0;
//     write("Part 1 OK\n");
//     // FAIL
//     *p1 = 0;
//     write("Never Reach Here if Kernel PA = 1\n");
//     return;
// }
// void test_lazy_alloc_thread(void* arg) {
//     write("Lazy Alloc Begin\n");
//     interruptManager.disableTimeInterrupt();
//     char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
//     // LAZY ALLOC RELEASE
//     memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);

//     // LAZY ALLOC TEST
//     p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
//     *p0 = 0;
    
//     // LAZY_ALLOC & FIND_VICTIM
//     char* testp = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
//     *testp = 0;

//     // VICTIM_RELEASE
//     memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);
//     // LAZY ALLOC RELEASE
//     memoryManager.releasePages(AddressPoolType::KERNEL, (int)testp, 1);
//     write("Lazy Alloc Done\n");
//     interruptManager.enableTimeInterrupt();

//     int pid = programManager.executeThread(test_out_of_memory, nullptr, "test_out_of_memory", 1, true);
//     if (pid == -1)
//     {
//         write("can not execute thread\n");
//         asm_halt();
//     }
//     return;
// }

void COW_writer() {
    volatile char* p = (char*)USER_VADDR_START;
    char c[2] = {};
    c[0] = *p;
    c[1] = '\0';
    uint32 address = (uint32)p;
    pte_dump(address);
    write("[writer] before: ");
    write(&c[0]);    // 打印 'A'

    *p = 'Z';                                      // 写操作触发 COW
    c[0] = *p;
    write("\n[writer] after: ");
    write(&c[0]);

    pte_dump(address);;
}

void COW_reader() {
    volatile char* p = (char*)USER_VADDR_START;
    char c[2] = {};
    c[0] = *p;
    c[1] = '\0';
    uint32 address = (uint32)p;
    pte_dump(address);
    write("[reader] sees: ");
    write(&c[0]);  
    write("\n");
}

void fork_test() {
    int pid = fork();
    printf("[fork_test] call Fork_test\n");
    int cur_pid = getpid();

    if (pid == -1)
    {
        write("can not fork\n");
    }
    else
    {
        if (pid)
        {
            printf("I am father, pid = %d, waiting to rape the child\n", getpid());
            int sonPid = waitpid(pid, nullptr);
            printf("Father Rape his child (pid = %d)\n", sonPid);
        }
        else
        {
            printf("I am child, pid = %d\n", getpid());
            char str3[] = "Child is waiting for his child\n";
            char str4[] = "Child Rape his child\n";
            if (fork() == 0) {
                printf("I am child of child, pid = %d\n", getpid());
                execveFunc((uint32)test_print_something);
                // NEVER REACH HERE
                write("FAULT!!!!!!!\n");
            } else {
                printf("Child is waiting for his child\n");
                int sonPid = wait(nullptr);
                printf("Child Rape his child (pid = %d)\n", sonPid);
            }
        }
    }
    return;
}

void stack_test() {
    char buf[2 * PAGE_SIZE];

    for (int i = 0; i < sizeof(buf); i++)
        buf[i] = i % 26;

    write("done\n");
    return;
}

void test_file_open_close(void* arg) {
    write("[file_test] begin\n");

    int fd0 = open("/dir1/testf", 0);
    printf("[file_test] open /dir1/testf -> %d\n", fd0);

    int fd1 = open("/dir1/testf", 0);
    printf("[file_test] open same file again -> %d\n", fd1);

    int fd_bad = open("/dir1/not_exist", 0);
    printf("[file_test] open missing file -> %d\n", fd_bad);

    int close0 = close(fd0);
    printf("[file_test] close fd0 -> %d\n", close0);

    int close1 = close(fd1);
    printf("[file_test] close fd1 -> %d\n", close1);

    int close_again = close(fd1);
    printf("[file_test] close fd1 again -> %d\n", close_again);

    int fd2 = open("/dir1/testf", 0);
    printf("[file_test] reopen after close -> %d\n", fd2);

    int close2 = close(fd2);
    printf("[file_test] close fd2 -> %d\n", close2);

    write("[file_test] end\n");
}
