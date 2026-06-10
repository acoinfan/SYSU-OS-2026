#include "test.h"
#include "os_constant.h"
#include "user.h"

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
    write("[fork_test] fork returned\n");
    int cur_pid = getpid();

    if (pid == -1)
    {
        write("can not fork\n");
    }
    else
    {
        if (pid)
        {
            char str1[] = "I am father, waiting to rape the child\n";
            char str2[] = "Father Rape his child\n";

            write(str1);
            waitpid(pid, nullptr);
            write(str2);
        }
        else
        {
            char str1[] = "I am child\n";
            write(str1);
            char str3[] = "Child is waiting for his child\n";
            char str4[] = "Child Rape his child\n";
            if (fork() == 0) {
                char str2[] = "I am child of child\n";
                write(str2);
                execveFunc((uint32)test_print_something);
                // NEVER REACH HERE
                write("FAULT!!!!!!!\n");
            } else {
                write(str3);
                wait(nullptr);
                write(str4);
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

void init_process(void* arg) {
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
