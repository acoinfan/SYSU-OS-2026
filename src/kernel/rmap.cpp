#include "rmap.h"
#include "debug.h"
#include "stdlib.h"
#include "os_constant.h"
#include "asm_utils.h"
#include "os_modules.h"

void RMapManager::initialize(int _length, RMapEntry* _RMapStart, char* _bitmap) {
    memset(_bitmap, 0, (_length + 7) / 8);
    bitmap.initialize(_bitmap, _length);
    RMapStart = _RMapStart;
    length = _length;
    bitmap.set(0, true);
}

int RMapManager::attach(PageInfo* pi, uint32 pte_paddr, uint32 pte_vaddr, uint16 owner) {
    int idx = this->allocate(pte_paddr, pte_vaddr, owner);
    if (idx == -1) return -1;

    // 设置
    if (pi->extra == RMAP_PTR_NULL) {
        // Page映射为空
        pi->extra = idx;
    } else {
        // Page映射不为空
        RMapStart[idx].next = pi->extra;
        pi->extra = idx;
    }
    pi->incRef();
    return idx;
}

bool RMapManager::detach(PageInfo* pi, uint32 pte_paddr, uint32 pte_vaddr, uint16 owner) {
    ASSERT(pi->ref > 0);
    int idx = pi->extra;

    ASSERT(idx != 0);
    if (RMapStart[idx].pte_paddr == pte_paddr && RMapStart[idx].owner == owner) {
        pi->extra = RMapStart[idx].next;
        this->release(idx);
        pi->decRef();
        return true;
    }
    idx = RMapStart[idx].next;
    int prev = pi->extra;
    while (idx != 0) {
        if (RMapStart[idx].pte_paddr == pte_paddr && RMapStart[idx].owner == owner) {
            RMapStart[prev].next = RMapStart[idx].next;
            this->release(idx);
            pi->decRef();
            return true;
        }    
        prev = idx;
        idx = RMapStart[idx].next;   
    }
    return false;
}

int RMapManager::allocate(uint32 pte_paddr, uint32 pte_vaddr, uint16 owner) {
    int idx = bitmap.allocate(1);
    if (idx == -1) return -1;

    RMapStart[idx].pte_paddr = pte_paddr;
    RMapStart[idx].pte_vaddr = pte_vaddr;
    RMapStart[idx].owner = owner;
    RMapStart[idx].next = RMAP_PTR_NULL;
    return idx;
}

void RMapManager::release(uint32 idx) {
    ASSERT(idx < length);
    bitmap.release(idx, 1);
    memset(&RMapStart[idx], 0, sizeof(RMapEntry));
}

bool RMapManager::setCOW(PageInfo* pi) {
    // 无法找到起始点或为未加载物理页
    if (pi->hasFlag(PG_FREE)) return false;
    if (pi->getRef() == 0) return false;
    // 临时用于函数复制, 正常只能允许ELF加载
    if (pi->hasFlag(PG_KERNEL) && pi->hasFlag(PG_RESERVED)) {
        ;
    } else {
        if (pi->extra == RMAP_PTR_NULL) return false;
    }
    // 遍历设置COW
    int cnt = pi->extra;
    while (cnt != RMAP_PTR_NULL) {
        uint32 pte_paddr = RMapStart[cnt].pte_paddr;
        uint16 owner = RMapStart[cnt].owner;
        ASSERT(pte_paddr != 0);

        uint32 tmp = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_paddr);
        if (!tmp) {
            // 严重错误, 无法恢复
            ASSERT(0);
        }
        *((uint32*)tmp) = (*((uint32*)tmp) | PTE_COW) & (~PTE_WRITABLE); 
        if (programManager.running && programManager.running->pid == owner && RMapStart[cnt].pte_vaddr != PTE_ANON_VADDR) {
            // 相同进程页表项变更, 有必要刷新
            asm_invlpg((void*)(PTEVA2VADDR(RMapStart[cnt].pte_vaddr)));
        }
        memoryManager.unmapTemp(AddressPoolType::KERNEL);
        cnt = RMapStart[cnt].next;
    }
    return true;
}