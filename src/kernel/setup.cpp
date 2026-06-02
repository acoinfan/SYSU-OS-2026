#include "asm_utils.h"
#include "interrupt.h"
#include "stdio.h"
#include "program.h"
#include "thread.h"
#include "sync.h"
#include "memory.h"
#include "address_pool.h"

// 屏幕IO处理器
STDIO stdio;
// 中断管理器
InterruptManager interruptManager;
// 程序管理器
ProgramManager programManager;
// 内存管理器
MemoryManager memoryManager;

void first_thread(void *arg)
{
    // 第1个线程不可以返回
    // stdio.moveCursor(0);·
    // for (int i = 0; i < 25 * 80; ++i)
    // {
    //     stdio.print(' ');
    // }
    // stdio.moveCursor(0);
    interruptManager.disableTimeInterrupt();
    char* p0 = (char *)memoryManager.allocatePagesLazy(AddressPoolType::KERNEL, VP_RW);
    *p0 = 0;
    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p0, 1);
    char *p1 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100, VP_RW);
    char *p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 10, VP_RW);
    char *p3 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100, VP_RW);

    printf("%x %x %x\n", p1, p2, p3);
    *p3 = 0xFF;

    memoryManager.releasePages(AddressPoolType::KERNEL, (int)p2, 10);
    p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 100, VP_RW);

    printf("%x\n", p2);

    p2 = (char *)memoryManager.allocatePages(AddressPoolType::KERNEL, 10, VP_RW);
    
    printf("%x\n", p2);
    printf("0x100000 %x ;", *(int*)0x100000);
    // 获取0x100000对应的PTE信息,储存在0x101000 + 4 * 256
    printf("0x101400 %x\n", *(int*)0x101400);

    printf("Try to access 0x101000\n");
    int value0 = *(int*) 0x101000;
    printf("value = %d\n", value0);
    *(int*) 0x101000 = value0;
    printf("after value = %d\n", *(int*) 0x101000);

    printf("Try to access 0xC0101000\n");
    int value1 = *(int*) 0xC0101000;
    printf("value = %d\n", value1);
    *(int*) 0xC0101000 = 114514;
    printf("after value = %d\n", *(int*) 0xC0101000);

    printf("Try to access 0x40000000\n");
    int value2 = *(int*) 0x40000000;
    printf("value = %d\n", value2);
    *(int*) 0x40000000 = 114514;
    asm_halt();
}

void idle_thread(void* arg) {
    int pid = programManager.executeThread(first_thread, nullptr, "first thread", 1);
    if (pid == -1)
    {
        printf("can not execute thread\n");
        asm_halt();
    }    
    asm_halt();
}

extern "C" void setup_kernel()
{

    // 中断管理器
    interruptManager.initialize();
    interruptManager.setTimeInterrupt((void *)asm_time_interrupt_handler);

    // 输出管理器
    stdio.initialize();

    // 进程/线程管理器
    programManager.initialize(SchedulerType::RR);

    // 内存管理器
    memoryManager.openPageMechanism();
    memoryManager.initialize();


    // 创建第一个线程
    int pid = programManager.executeThread(idle_thread, nullptr, "idle thread", 1);
    if (pid == -1)
    {
        printf("can not execute thread\n");
        asm_halt();
    }

    PCB* firstThread;
    PCB rubbish;
    
    switch (programManager.sType) {
        case SchedulerType::RR:
            firstThread = programManager.rrScheduler.pickNext();
            break;
        case SchedulerType::FIFS:
            firstThread = programManager.fifsScheduler.pickNext();
            break;
    }
    firstThread->status = ProgramStatus::RUNNING;
    programManager.running = firstThread;

    // 第一次切换 pid=0
    interruptManager.enableTimeInterrupt();
    asm_switch_thread(&rubbish, firstThread);
    asm_halt();
}
