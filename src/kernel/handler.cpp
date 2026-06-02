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
            
            if (paddr == -1) {
                // TODO: Implement CLOCK
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