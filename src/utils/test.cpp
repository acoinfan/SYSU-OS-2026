#include "test.h"
#include "os_modules.h"
#include "os_constant.h"
#include "asm_utils.h"

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

#define SYS_WRITE 1
#define SYS_DUMP_PTE 2

void COW_writer() {
    volatile char* p = (char*)USER_VADDR_START;
    uint32 address = (uint32)p;
    asm_system_call(SYS_DUMP_PTE, address);
    asm_system_call(SYS_WRITE, 0, (int)"[writer] before: ", 15);
    asm_system_call(SYS_WRITE, 0, (int)p, 1);      // 打印 'A'

    *p = 'Z';                                      // 写操作触发 COW

    asm_system_call(SYS_WRITE, 0, (int)"\n[writer] after: ", 17);
    asm_system_call(SYS_WRITE, 0, (int)p, 1);      // 预期打印 'Z'

    asm_system_call(SYS_DUMP_PTE, address);
    asm_halt();
}

void COW_reader() {
    volatile char* p = (char*)USER_VADDR_START;
    uint32 address = (uint32)p;
    asm_system_call(SYS_DUMP_PTE, address);
    asm_system_call(SYS_WRITE, 0, (int)"[reader] sees: ", 15);
    asm_system_call(SYS_WRITE, 0, (int)p, 1);      // 预期一直是 'A'
    asm_system_call(SYS_WRITE, 0, (int)"\n", 1);

    asm_halt();
}