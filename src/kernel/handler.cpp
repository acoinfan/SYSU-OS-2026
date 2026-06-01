#include "handler.h"
#include "interrupt.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "program.h"
#include "pageinfo.h"

void handle_kernel_page_fault(const PageFaultInfo& info) {
    switch (info.faultType) {
        case FaultType::COPY_ON_WRITE: {
            break;
        }
        case FaultType::DEMAND_ZERO: {
            if (memoryManager.kernelVirtual.startAddress > info.addr) {
                printf("Page Fault: DEMAND_ZERO but INVALID_ADDRESS, halt\n");
                asm_halt();
            }
            int paddr = memoryManager.kernelPhysical.allocate(1);
            
            if (paddr == -1) {
                // TODO: Implement CLOCK
                printf("Page Fault: DEMAND_ZERO but OUT_OF_MEMORY, halt\n");
                asm_halt();                
            } 
            *info.PTEptr = paddr | 0x7;
            if (memoryManager.pageinfos[PA2PGI(paddr)].hasFlag(PG_ZERO)) {
                memoryManager.pageinfos[PA2PGI(paddr)].clearFlag(PG_ZERO);
            } else {
                uint32 vaddr = info.addr & ~0xfff;
                memset((void*)vaddr, 0, PAGE_SIZE);
            }
            break;
        }
    }
}

bool handle_user_page_fault(const PageFaultInfo& info) {
    asm_halt();
    return false;
}