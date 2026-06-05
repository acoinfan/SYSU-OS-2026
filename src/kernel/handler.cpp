#include "handler.h"
#include "interrupt.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "stdio.h"
#include "os_modules.h"
#include "program.h"
#include "pageinfo.h"

void handle_kernel_page_fault(const PageFaultInfo& info) {
    printf("KERNEL FAULT: faultType = %d\n", (int)info.faultType);
    printf("Page Fault Handler: Hit at address 0x%x\n", info.addr);
    switch (info.faultType) {
        case FaultType::COPY_ON_WRITE: {
            printf("Page Fault: Kernel Cannot COPY_ON_WRITE, halt\n");
            ASSERT(0);
            asm_halt();
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
            memoryManager.pageinfos[PA2PGI(paddr)].setFlag(PG_KERNEL);
            return;
        }
        default: {
            asm_halt();

        }
    }
    // TODO: flush TLB

}

bool handle_user_page_fault(const PageFaultInfo& info) {
    printf("USER FAULT: faultType = %d\n", (int)info.faultType);
    printf("Page Fault Handler: Hit at address 0x%x\n", info.addr);

    if (!programManager.running) return false;
    UserVAddressPool& userVirtual = programManager.running->userVirtual;
    uint32 owner = programManager.running->pid;

    switch (info.faultType) {
        case FaultType::COPY_ON_WRITE: {
            printf("Page Fault: COPY_ON_WRITE start\n");
            UserSegment userSegment = userVirtual.vaddr2Seg(info.addr);
            if (userSegment == UserSegment::EMPTY) {
                printf("Page Fault: COPY_ON_WRITE but INVALID_ADDRESS, return false\n");
                return false;
            }

            uint32 vaddr = info.addr & ~0xfff;
            uint32* PTEptr = (uint32*)memoryManager.toPTE(info.addr);
            uint32 paddr = (*PTEptr) & PTE_GET_ADDRESS;
            uint32 ref = memoryManager.pageinfos[PA2PGI(paddr)].getRef();
            if (ref == 0) {
                printf("Page Fault: COPY_ON_WRITE but invalid pageinfo ref, return false\n");
                return false;
            } else if (ref == 1) {
                // 填入对应的vaddr, 清除PTE_COW, 改为可写, 刷新TLB
                *PTEptr = (*PTEptr & ~PTE_COW) | PTE_WRITABLE;
                uint32 idx = memoryManager.pageinfos[PA2PGI(paddr)].extra;
                memoryManager.rmapManager.RMapStart[idx].pte_vaddr = (uint32)PTEptr;
                asm_invlpg((void*)vaddr);
            } else {
                int new_paddr = memoryManager.allocatePhysicalPages(AddressPoolType::USER, 1);
                
                // Try Clock
                if (new_paddr == 0 && (new_paddr = out_of_memory(AddressPoolType::USER, 1)) == 0) {
                    printf("Page Fault: COPY_ON_WRITE but OUT_OF_MEMORY, return false\n");
                    return false;                
                } 

                // 更新PTE(权限,地址)
                uint32 PTE_paddr = memoryManager.toPTEpa(vaddr);
                *PTEptr = (*PTEptr & ~PTE_COW) | PTE_WRITABLE;
                *PTEptr = (*PTEptr & ~PTE_GET_ADDRESS) | new_paddr;
                asm_invlpg((void*)vaddr);

                // 解绑rmap并绑定新rmap
                memoryManager.rmapManager.detach(&memoryManager.pageinfos[PA2PGI(paddr)], 
                                                PTE_paddr, 0, owner);
                memoryManager.rmapManager.attach(&memoryManager.pageinfos[PA2PGI(new_paddr)], 
                                                PTE_paddr, (uint32)PTEptr, owner);                
                // 利用临时绑定复制数据
                void* src = (void*)memoryManager.mapTemp(AddressPoolType::KERNEL, paddr);
                void* dst = (void*)vaddr;
                if (src == 0) {
                    printf("Page Fault: COPY_ON_WRITE but mapTemp fail, return false\n");
                    return false;                         
                }
                memcpy(dst, src, PAGE_SIZE);
                memoryManager.unmapTemp(AddressPoolType::KERNEL);

                return true;
            }
        }
        case FaultType::DEMAND_ZERO: {
            UserSegment userSegment = userVirtual.vaddr2Seg(info.addr);
            if (userSegment == UserSegment::EMPTY) {
                printf("Page Fault: DEMAND_ZERO but INVALID_ADDRESS, return false\n");
                return false;
            }
            int paddr = memoryManager.allocatePhysicalPages(AddressPoolType::USER, 1);
            
            // Try Clock
            if (paddr == 0 && (paddr = out_of_memory(AddressPoolType::USER, 1)) == 0) {
                printf("Page Fault: DEMAND_ZERO but OUT_OF_MEMORY, return false\n");
                return false;                
            } 
            uint32 vaddr = info.addr & ~0xfff;
            memoryManager.connectPhysicalVirtualPage(vaddr, paddr);
            if (memoryManager.pageinfos[PA2PGI(paddr)].hasFlag(PG_ZERO)) {
                memoryManager.pageinfos[PA2PGI(paddr)].clearFlag(PG_ZERO);
            } else {
                memset((void*)vaddr, 0, PAGE_SIZE);
            }
            return true;
        } 
        case FaultType::STACK_GROWTH: {
            // uint32 addr = memoryManager.allocatePagesLazy(AddressPoolType::USER, 1, (VPageFlags)(VP_RW | VP_USER), UserSegment::STACK, true);
            // if (!addr) return false;
            // return true;
        }
        default: {
            printf("fail addr: 0x%x, pid: %d\n", info.addr, programManager.running->pid);
            ASSERT(0);
            asm_halt();

        }
    }
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