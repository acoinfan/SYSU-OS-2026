#include "rmap.h"
#include "assert.h"
#include "stdlib.h"

void RMapManager::initialize(int _length, RMapEntry* _RMapStart, char* _bitmap) {
    memset(_bitmap, 0, (length + 7) / 8);
    bitmap.initialize(_bitmap, _length);
    RMapStart = _RMapStart;
    length = _length;
    bitmap.set(0, true);
}

int RMapManager::attach(PageInfo* pi, uint32 pte_addr, uint16 owner) {
    int idx = this->allocate(pte_addr, owner);
    if (idx == -1) return -1;

    // 设置
    if (pi->extra == RMAP_PTR_NULL) {
        // Page映射为空
        pi->extra = idx;
    } else {
        // Page映射不为空
        int cnt = pi->extra;
        while (RMapStart[cnt].next != RMAP_PTR_NULL) {
            cnt = RMapStart[cnt].next;
        }
        RMapStart[cnt].next = idx;
    }
    pi->incRef();
    return idx;
}

void RMapManager::detach(PageInfo* pi, uint32 pte_addr, uint16 owner) {
    ASSERT(pi->ref > 0);
    int idx = pi->extra;

    ASSERT(idx != 0);
    if (RMapStart[idx].pte_addr == pte_addr && RMapStart[idx].owner == owner) {
        pi->extra = RMapStart[idx].next;
        this->release(idx);
        return;
    }
    idx = RMapStart[idx].next;
    int prev = pi->extra;
    while (idx != 0) {
        if (RMapStart[idx].pte_addr == pte_addr && RMapStart[idx].owner == owner) {
            RMapStart[prev].next = RMapStart[idx].next;
            this->release(idx);
            return;
        }    
        prev = idx;
        idx = RMapStart[idx].next;   
    }
    pi->decRef();
    ASSERT(0);
    return;
}

int RMapManager::allocate(uint32 pte_addr, uint16 owner) {
    int idx = bitmap.allocate(1);
    if (idx == -1) return -1;

    RMapStart[idx].pte_addr = pte_addr;
    RMapStart[idx].owner = owner;
    RMapStart[idx].next = RMAP_PTR_NULL;
    return idx;
}

void RMapManager::release(uint32 idx) {
    ASSERT(idx < length);
    bitmap.release(idx, 1);
    memset(&RMapStart[idx], 0, sizeof(RMapEntry));
}