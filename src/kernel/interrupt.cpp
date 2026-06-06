#include "interrupt.h"
#include "os_type.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "program.h"
#include "handler.h"
#include "pageinfo.h"
#include "debug.h"

int times = 0;
extern "C" void c_page_fault_handler(uint32 error_code)
{
    bool from_user = (error_code & (1u << 2)) != 0;
    FaultType faultType = FaultType::UNKNOWN;
    uint32 addr = (uint32)asm_get_page_error_addr();
    uint32 PDE = *(uint32*)memoryManager.toPDE(addr);
    uint32 PTE = 0;
    if (!(PDE & PTE_PRESENT)) {
        faultType = FaultType::PAGE_TABLE_BROKEN;
        PANIC("Page Fault: PAGE_TABLE_BROKEN, halt\n"
              "broken addr: 0x%x, pid: %d\n", addr, programManager.running->pid);
        asm_halt();
    } else {
        PTE = *(uint32*)memoryManager.toPTE(addr);

        if (!(PTE & PTE_PRESENT)) {
            // Demand Zero
            if ((PTE & PTE_WRITABLE) && (PTE & PTE_LAZY)) {
                faultType = FaultType::DEMAND_ZERO;
            // File Backed (TO DO)
            } else if (memoryManager.pageinfos[PA2PGI(PTE & PTE_GET_ADDRESS)].hasFlag(PG_FILE)) {
                faultType = FaultType::FILE_BACKED;
            // Swap In
            } else if (PTE & PTE_SWAP) {
                faultType = FaultType::SWAP_IN;
            } else if (from_user) {
                UserSegment userSeg = programManager.running->userVirtual.vaddr2Seg(addr);
                switch (userSeg) {
                    case UserSegment::HEAP:
                        faultType = FaultType::HEAP_GROWTH;
                        break;
                    case UserSegment::STACK:
                        faultType = FaultType::STACK_GROWTH;
                        break;
                    default:
                        faultType = FaultType::INVALID_ADDRESS;
                }
            } else {
                faultType = FaultType::INVALID_ADDRESS;
            }
        } else {
            uint32 paddr = PTE & PTE_GET_ADDRESS;
            // Kernel Reserved (测试COW-Func时请务必关掉它)
            if (0 && memoryManager.pageinfos[PA2PGI(paddr)].hasFlag(PG_KERNEL) && memoryManager.pageinfos[PA2PGI(paddr)].hasFlag(PG_RESERVED)) {
                faultType = FaultType::KERNEL_RESERVED;
            // Copy On Write
            } else if ((!(PTE & PTE_WRITABLE)) && (PTE & PTE_COW)) {
                faultType = FaultType::COPY_ON_WRITE;
            // Permission Violation
            } else if (((!(PTE & PTE_WRITABLE)) || (from_user && !(PTE & PTE_USER_ACCESS))) && (!(PTE & PTE_COW))) {
                faultType = FaultType::PERMISSION_VIOLATION;
            }
        }
    }

    PageFaultInfo info = {faultType, addr, (uint32*)memoryManager.toPDE(addr), 
                            (uint32*)memoryManager.toPTE(addr)};
    if (from_user) {
        bool res = handle_user_page_fault(info);
    } else {
        handle_kernel_page_fault(info);
    }
    return;
}


// 中断处理函数 
extern "C" void c_time_interrupt_handler()
{
    PCB *cur = programManager.running;
    bool cond = false;
    switch (programManager.sType) {
        case SchedulerType::RR:
            cond = programManager.rrScheduler.onTick(cur);
            break;
        case SchedulerType::FIFS:
            cond = programManager.fifsScheduler.onTick(cur);
            break;
    } 
    if (cond) {
        programManager.schedule();
    }
}


InterruptManager::InterruptManager()
{
    initialize();
}

void InterruptManager::initialize()
{
    // 初始化中断计数变量
    times = 0;
    
    // 初始化IDT
    IDT = (uint32 *)IDT_START_ADDRESS;
    asm_lidt(IDT_START_ADDRESS, 256 * 8 - 1);

    for (uint i = 0; i < 256; ++i)
    {
        setInterruptDescriptor(i, (uint32)asm_unhandled_interrupt, 0);
    }

    setInterruptDescriptor(14, (uint32)asm_page_fault_handler, 0);

    // 初始化8259A芯片
    initialize8259A();
}

void InterruptManager::setInterruptDescriptor(uint32 index, uint32 address, byte DPL)
{
    IDT[index * 2] = (CODE_SELECTOR << 16) | (address & 0xffff);
    IDT[index * 2 + 1] = (address & 0xffff0000) | (0x1 << 15) | (DPL << 13) | (0xe << 8);
}

void InterruptManager::initialize8259A()
{
    // ICW 1
    asm_out_port(0x20, 0x11);
    asm_out_port(0xa0, 0x11);
    // ICW 2
    IRQ0_8259A_MASTER = 0x20;
    IRQ0_8259A_SLAVE = 0x28;
    asm_out_port(0x21, IRQ0_8259A_MASTER);
    asm_out_port(0xa1, IRQ0_8259A_SLAVE);
    // ICW 3
    asm_out_port(0x21, 4);
    asm_out_port(0xa1, 2);
    // ICW 4
    asm_out_port(0x21, 1);
    asm_out_port(0xa1, 1);

    // OCW 1 屏蔽主片所有中断，但主片的IRQ2需要开启
    asm_out_port(0x21, 0xfb);
    // OCW 1 屏蔽从片所有中断
    asm_out_port(0xa1, 0xff);
}

void InterruptManager::enableTimeInterrupt()
{
    uint8 value;
    // 读入主片OCW
    asm_in_port(0x21, &value);
    // 开启主片时钟中断，置0开启
    value = value & 0xfe;
    asm_out_port(0x21, value);
}

void InterruptManager::disableTimeInterrupt()
{
    uint8 value;
    asm_in_port(0x21, &value);
    // 关闭时钟中断，置1关闭
    value = value | 0x01;
    asm_out_port(0x21, value);
}

void InterruptManager::setTimeInterrupt(void *handler)
{
    setInterruptDescriptor(IRQ0_8259A_MASTER, (uint32)handler, 0);
}

void InterruptManager::enableInterrupt()
{
    asm_enable_interrupt();
}

void InterruptManager::disableInterrupt()
{
    asm_disable_interrupt();
}

bool InterruptManager::getInterruptStatus()
{
    return asm_interrupt_status() ? true : false;
}

// 设置中断状态
void InterruptManager::setInterruptStatus(bool status)
{
    if (status)
    {
        enableInterrupt();
    }
    else
    {
        disableInterrupt();
    }
}

