#ifndef ADDRESS_POOL_H
#define ADDRESS_POOL_H

#include "bitmap.h"
#include "os_type.h"
#include "buddy.h"
#include "os_constant.h"
#include "pageinfo.h"
#include "assert.h"
#include "rmap.h"
#include "enum.h"

struct PA {};
struct VA {};

struct VictimInfo {
    uint32 PTEptr;
    uint32 paddr;
};

inline PTEFlags vPageFlags2PTE(VPageFlags flags)
{
    uint32 pteFlags = 0;

    if (flags & VP_COW)
    {
        pteFlags |= PTE_COW;
    }
    else if (flags & VP_RW)
    {
        pteFlags |= PTE_WRITABLE;
    }

    if (flags & VP_USER)
    {
        pteFlags |= PTE_USER_ACCESS;
    }

    if (flags & VP_SWAP)
    {
        pteFlags |= PTE_SWAP;
    }

    return (PTEFlags)pteFlags;
}



class VAddressPool
{
public:
    BitMap resources;
    uint32 startAddress, endAddress;   // 左闭右闭
    VPageFlags* privileges;
public:
    VAddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const uint32 startAddress, 
                    const uint32 endAddress, const uint32 privileges);

    // 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
    int allocate(const int count, VPageFlags privilege);

    // 释放若干页的空间
    void release(const uint32 address, const int amount);

    VPageFlags getVPageFlag(const uint32 vaddr);

    inline bool isValidAddr(const uint32 vaddr) {
        return vaddr >= startAddress && vaddr <= endAddress;
    }
};


class PAddressPool
{
public:
    Buddy resources;
    uint32 startAddress, endAddress;   // 左闭右开
    uint32 victim_idx, length;  // [0,length)
public:
    PAddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const uint32 startAddress, char* freeBitMap, FreeNode* freeNodes);

    // 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
    int allocate(const int count);

    // 释放若干页的空间
    void release(const uint32 address, const int amount);

    VictimInfo findVictim(uint32 search_length=0, uint32 round=2);
};

#endif
