#ifndef ENUM_H
#define ENUM_H

#include "os_type.h"

enum ProgramStatus
{
    CREATED,
    RUNNING,
    READY,
    BLOCKED,
    DEAD,
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

enum PDEFlags {
    PDE_PRESENT    =1<<0,
    PDE_WRITABLE   =1<<1,
    PDE_USER_ACCESS=1<<2,
    PDE_ACCESSED   =1<<5,
    PDE_DIRTY      =1<<6,
    PDE_COW        =1<<9,
    PDE_COUNT_MASK =0b111000000000,
    PDE_GET_ADDRESS=0xffffff000,
    PDE_KERNEL     =0xE07,
    PDE_RESERVE    =0xE07,
    PDE_USER       =0x7
};

#define PDE_GET_COUNT(PDEptr) (((*(uint32*)PDEptr) & PDE_COUNT_MASK) >> 9)

#define PDE_SET_COUNT(PDEptr, count) \
    do { \
        (*(uint32*)PDEptr) = ((*(uint32*)PDEptr) & ~PDE_COUNT_MASK) | \
                ((((count) > 7 ? 7 : (count)) & 0x7) << 9); \
    } while (0)

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
    STACK_GROWTH, // 保留字段
    HEAP_GROWTH,  // 保留字段
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

enum UserSegment : unsigned char {
    TEXT = 0,
    DATA,
    BSS,
    // 调为大数值,防止ELF解析出错时不明显
    MMAP = 0xfb,
    TLS = 0xfc,
    HEAP = 0xfd,
    STACK = 0xfe,
    EMPTY = 0xff 
};

enum SyscallType : uint8 {
    SYS_GETPID = 0,
    SYS_GETPPID,

    SYS_FORK,
    SYS_EXIT, 
    SYS_WAIT,
    SYS_EXEC,
    SYS_EXECFUNC,

    SYS_YIELD,
    SYS_SLEEP,

    SYS_OPEN,
    SYS_READ, 
    SYS_WRITE, 
    SYS_CLOSE,
    SYS_FDREAD,
    SYS_FDWRITE,
    SYS_FDAPPEND,
    SYS_CREATE_FILE,
    SYS_REMOVE_FILE,
    SYS_FSEEK,
    SYS_SYNC,
    SYS_MKDIR,
    SYS_RMDIR,
    SYS_FD_DUMP,
    SYS_MOVE_CURSOR,
    SYS_PTE_DUMP,
    SYS_PA_DUMP,
    SYS_EXPANDHEAP
};

enum fs_type {
    FXT12,
    NONE
};

enum class IdeDrive : uint8 {
    PrimaryMaster   = 0,    // hda
    PrimarySlave    = 1,    // hdb
    SecondaryMaster = 2,    // hdc
    SecondarySlave  = 3,    // hdd
    INVALID         = 0xFF  // INVALID
};
#endif
