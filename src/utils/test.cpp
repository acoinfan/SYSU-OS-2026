#include "test.h"
#include "os_modules.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "syscall.h"
#include "stdio.h"

void test_out_of_memory(void* arg) {
    printf("Out Of Memory Begin\n");
    interruptManager.disableTimeInterrupt();
    char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    char* p1 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);

    // OK
    *p0 = 0;
    printf("Part 1 OK\n");
    // FAIL
    *p1 = 0;
    printf("Never Reach Here if Kernel PA = 1\n");
    interruptManager.enableTimeInterrupt(); 
    return;
}
void test_lazy_alloc_thread(void* arg) {
    printf("Lazy Alloc Begin\n");
    interruptManager.disableTimeInterrupt();
    char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    // LAZY ALLOC RELEASE
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);

    // LAZY ALLOC TEST
    p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    *p0 = 0;
    
    // LAZY_ALLOC & FIND_VICTIM
    char* testp = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, 1, VP_RW);
    *testp = 0;

    // VICTIM_RELEASE
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);
    // LAZY ALLOC RELEASE
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)testp, 1);
    printf("Lazy Alloc Done\n");
    interruptManager.enableTimeInterrupt();

    int pid = programManager.executeThread(test_out_of_memory, nullptr, "test_out_of_memory", 1);
    if (pid == -1)
    {
        printf("can not execute thread\n");
        asm_halt();
    }
    return;
}

void COW_writer() {
    volatile char* p = (char*)USER_VADDR_START;
    char c[2] = {};
    c[0] = *p;
    c[1] = '\0';
    uint32 address = (uint32)p;
    dump_pte(address);
    write("[writer] before: ");
    write(&c[0]);    // 打印 'A'

    *p = 'Z';                                      // 写操作触发 COW
    c[0] = *p;
    write("\n[writer] after: ");
    write(&c[0]);

    dump_pte(address);
    asm_halt();
}

void COW_reader() {
    volatile char* p = (char*)USER_VADDR_START;
    char c[2] = {};
    c[0] = *p;
    c[1] = '\0';
    uint32 address = (uint32)p;
    dump_pte(address);
    write("[reader] sees: ");
    write(&c[0]);  
    write("\n");
    asm_halt();
}

void fork_test() {
    int pid = fork();
    write("[fork_test] fork returned\n");
    int cur_pid = programManager.running ? programManager.running->pid : -1;
    char buf[64];
    char pidMsg[64];
    // snprintf 在我们的 mini stdio 中对应 print 类函数
    // sprintf(pidMsg, "[fork_test] running pid=%d fork_ret=%d\n", cur_pid, pid);
    write(pidMsg);

    if (pid == -1)
    {
        write("can not fork\n");
    }
    else
    {
        if (pid)
        {
            char str[30] = "I am father, fork return: ";
            str[27] = (char) (pid + '0');
            str[28] = '\0';
            write(str);
        }
        else
        {
            char str[30] = "I am child, fork return: ";
            str[27] = (char) (pid + '0');
            str[28] - '\n';
            write(str);
            // write("I am child, fork return: %d, my pid: %d\n", pid, 
            //        programManager.running->pid);
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
