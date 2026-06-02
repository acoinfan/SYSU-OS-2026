#ifndef PAGEINFO_H
#define PAGEINFO_H

#include "os_type.h"
#include "enum.h"

#define PAGE_SHIFT 12
#define PA2PGI(PA) (((unsigned) (PA)) >> (PAGE_SHIFT))

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