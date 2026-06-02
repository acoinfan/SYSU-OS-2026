#ifndef ENUM_H
#define ENUM_H

#include "os_type.h"

enum ProgramStatus
{
    CREATED,
    RUNNING,
    READY,
    BLOCKED,
    DEAD
};

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
/* PDE PTE:
   31-12   11     10     9     8     7     6      5     4     3     2     1     0
   ADDR   LAZY   SWAP   COW    G    PAT  Dirty Access  PCD   PWT   U/S   W/R Present

*/

enum VPageFlags : uint8 {
    VP_CLEAR = 0,
    VP_RW    = 1 << 0,
    VP_USER  = 1 << 1,
    VP_COW   = 1 << 2,
    VP_FILE  = 1 << 3,
    VP_SWAP  = 1 << 4,
    VP_ALL   = 0xff
};

enum struct SchedulerType{
   RR,
   FIFS
};

enum AddressPoolType
{
    USER,
    KERNEL
};

enum struct FaultType : uint8 {
    DEMAND_ZERO = 0,
    STACK_GROWTH,
    HEAP_GROWTH,
    SWAP_IN,
    COPY_ON_WRITE,
    FILE_BACKED,
    PERMISSION_VIOLATION,
    KERNEL_RESERVED,
    INVALID_ADDRESS,
    OUT_OF_MEMORY,
    PAGE_TABLE_BROKEN,
    UNKNOWN
};

enum UserSegment {
    HEAP,
    STACK,
    MMAP,
    TLS,
    TEXT,
    DATA,
    BSS
};


#endif