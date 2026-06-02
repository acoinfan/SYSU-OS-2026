#ifndef PAGEINFO_H
#define PAGEINFO_H

#include "os_type.h"

#define PAGE_SHIFT 12
#define PA2PGI(PA) (((unsigned) (PA)) >> (PAGE_SHIFT))

enum PageFlags : uint16 {
    PG_RESERVED =1<<0,   // 系统保留

    PG_FREE     =1<<1,   // buddy空闲页

    PG_FILE     =1<<2,   // 文件映射页

    PG_LOCKED   =1<<3,   // 禁止淘汰

    PG_ZERO     =1<<4,   // 零页

    PG_KERNEL   =1<<5,   // 内核页

    PG_SINGLE   =1<<6,   // Buddy中order=0的Page, 用于CLOCK算法
    
    PG_ALL      =0xFFFF
};
/*
Kernel保留: PG_RESERVED | PG_KERNEL
Kernel VA映射: PG_KERNEL
User VA映射: 无
空页: PG_ZERO
空闲页：保留 PG_FREE | PG_ZERO（二者独立：空闲但未清零时只保留 PG_FREE）。
用户页在使用：清掉 PG_FREE/PG_ZERO/PG_KERNEL，无需额外标志。若来自文件，可叠加 PG_FILE。
内核保留结构：PG_RESERVED | PG_KERNEL 阻止调页。
页表页或其他锁定页：再叠加 PG_LOCKED，保证 CLOCK 不会淘汰。
*/

enum PTEFlags {
    PTE_PRESENT    =1<<0,
    PTE_WRITABLE   =1<<1,
    PTE_USER_ACCESS=1<<2,
    PTE_ACCESSED   =1<<5,
    PTE_DIRTY      =1<<6,
    PTE_COW        =1<<9,
    PTE_SWAP       =1<<10,
    PTE_LAZY       =1<<11,
    PTE_GET_ADDRESS=0xfffff000
};

struct PageInfo {
    uint16 ref;
    uint16 flags;
    uint32 extra;

    // 只允许被rmap类调用,或者在新建/删除页表的时候调用
    uint16 incRef(void);
    // 只允许被rmap类调用,或者在新建/删除页表的时候调用
    uint16 decRef(void);
    uint16 getRef(void) const;
    void setFlag(PageFlags flag);
    void setFlag(uint16 mask);
    bool hasFlag(PageFlags flag) const;
    void clearFlag(PageFlags flag);
    void clearFlag(uint16 mask);
    void clear();
    void dump(void) const;
};


/* PDE PTE:
   31-12   11     10     9     8     7     6      5     4     3     2     1     0
   ADDR   LAZY   SWAP   COW    G    PAT  Dirty Access  PCD   PWT   U/S   W/R Present

*/
#endif