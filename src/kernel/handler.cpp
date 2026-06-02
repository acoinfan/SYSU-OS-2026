#include "handler.h"
#include "interrupt.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "program.h"
#include "pageinfo.h"

void handle_kernel_page_fault(const PageFaultInfo& info) {
    printf("faultType = %d\n", (int)info.faultType);
    printf("Page Fault Handler: Hit at address 0x%x\n", info.addr);
    switch (info.faultType) {
        case FaultType::COPY_ON_WRITE: {
            break;
        }
        case FaultType::DEMAND_ZERO: {
            if (!memoryManager.kernelVirtual.isValidAddr(info.addr)) {
                printf("Page Fault: DEMAND_ZERO but INVALID_ADDRESS, halt\n");
                asm_halt();
            }
            int paddr = memoryManager.allocatePhysicalPages(AddressPoolType::KERNEL, 1);
            
            // Try Clock
            if (paddr == 0 && (paddr = out_of_memory(AddressPoolType::KERNEL, 1)) == 0) {
                printf("Page Fault: DEMAND_ZERO but OUT_OF_MEMORY, halt\n");
                asm_halt();                
            } 
            uint32 vaddr = info.addr & ~0xfff;
            memoryManager.connectPhysicalVirtualPage(vaddr, paddr);
            if (memoryManager.pageinfos[PA2PGI(paddr)].hasFlag(PG_ZERO)) {
                memoryManager.pageinfos[PA2PGI(paddr)].clearFlag(PG_ZERO);
            } else {
                memset((void*)vaddr, 0, PAGE_SIZE);
            }
            return;
        }
        default: {
            asm_halt();

        }
    }
    // TODO: flush TLB

}

bool handle_user_page_fault(const PageFaultInfo& info) {
    asm_halt();
    return false;
}

// 清除Victim块的信息，同时返回给Handler
// return 0 as invalid addr
int out_of_memory(enum AddressPoolType type, const int count) {
    // 拒绝大小不等于1的分配
    if (count != 1) return 0;

    VictimInfo victimInfo = memoryManager.findVictim(type);
    if (victimInfo.paddr == 0 && victimInfo.PTEptr == 0) return 0;

    // TODO: 清除原对应PTE信息, 这里暂时直接把原PTE清空
    *(uint32*)victimInfo.PTEptr = 0;
    uint32 PTE = *(uint32*)victimInfo.PTEptr;

    // TODO: 脏则写回
    if (PTE & PTE_DIRTY) {
        // TODO: 写入SWAP
        if (1) {
            ;
        // TODO: 写回DISK
        } else if (0){
            ;
        }
    }
    
    // 设置合适的PageInfo
    uint32 pgi = PA2PGI(victimInfo.paddr);
    memoryManager.pageinfos[pgi].clear();

    if (type == AddressPoolType::KERNEL) {
        memoryManager.pageinfos[pgi].setFlag(PG_KERNEL); 
    }
    memoryManager.pageinfos[pgi].setFlag(PG_SINGLE);
    return victimInfo.paddr;
}