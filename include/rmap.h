#ifndef RMAP_H
#define RMAP_H

#include "os_type.h"
#include "pageinfo.h"
#include "bitmap.h"

#define RMAP_PTR_NULL 0x0
struct RMapEntry {
    uint32 pte_addr;
    uint32 next;
    uint16 owner;
};

class RMapManager {
public:
    // RMapStart预留0不可被分配
    RMapEntry* RMapStart;
    BitMap bitmap;
    int length;
public:
    void initialize(int _length, RMapEntry* _RMapStart, char* _bitmap);
    int attach(PageInfo* pi, uint32 pte_addr, uint16 owner);
    void detach(PageInfo* pi, uint32 pte_addr, uint16 owner);
    // void detach_by_index(PageInfo* pi, int entry_idx);
private:
    int allocate(uint32 pte_addr, uint16 owner);
    // 不做清理
    void release(uint32 idx);

};
#endif