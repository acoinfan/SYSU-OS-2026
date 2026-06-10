#ifndef RMAP_H
#define RMAP_H

#include "os_type.h"
#include "pageinfo.h"
#include "bitmap.h"

#define RMAP_PTR_NULL 0x0
struct RMapEntry {
    // 全局唯一标识 
    uint32 pte_paddr;
    // 用于刷新页表
    uint32 pte_vaddr;
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
    // 注意, attach和detach不做任何有关于PTE的维护,也即更改PTE请务必自己手动刷新
    int attach(PageInfo* pi, uint32 pte_paddr, uint32 pte_vaddr, uint16 owner);
    // 注意, attach和detach不做任何有关于PTE的维护,也即更改PTE请务必自己手动刷新
    // 参数pte_vaddr不重要,可以随便传入
    bool detach(PageInfo* pi, uint32 pte_paddr, uint32 pte_vaddr, uint16 owner);
    bool setCOW(PageInfo* pi);  // 将pi内部所有的PTE都设置为COW状态, 如果其中还有ANON,则不处理
    // void detach_by_index(PageInfo* pi, int entry_idx);
private:
    int allocate(uint32 pte_paddr, uint32 pte_vaddr, uint16 owner);
    // 不做清理
    void release(uint32 idx);
};
#endif