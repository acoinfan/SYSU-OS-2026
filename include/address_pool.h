#ifndef ADDRESS_POOL_H
#define ADDRESS_POOL_H

#include "bitmap.h"
#include "os_type.h"
#include "buddy.h"
#include "os_constant.h"
#include "pageinfo.h"
#include "assert.h"

struct PA {};
struct VA {};

enum VPageFlags : uint8 {
    VP_RW    = 1 << 0,
    VP_USER  = 1 << 1,
    VP_COW   = 1 << 2,
    VP_FILE  = 1 << 3,
    VP_SWAP  = 1 << 4,
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


template <typename T>
class AddressPool;

template <>
class AddressPool<VA>
{
public:
    BitMap resources;
    uint32 startAddress, endAddress;   // 左闭右闭
    VPageFlags* privileges;
public:
    AddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const uint32 startAddress, 
                    const uint32 endAddress, const uint32 privileges)
    {
        resources.initialize(bitmap, length);
        this->startAddress = startAddress;
        this->endAddress = endAddress;
        this->privileges = (VPageFlags*)privileges;
    }

    // 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
    int allocate(const int count, VPageFlags privilege)
    {
        int start = resources.allocate(count);
        if (start != -1) {
            privileges[start] = privilege;
            return start * PAGE_SIZE + startAddress;
        } else {
            return -1;
        }
    }

    // 释放若干页的空间
    void release(const uint32 address, const int amount)
    {
        resources.release((address - startAddress) / PAGE_SIZE, amount);
    }

    VPageFlags getVPageFlag(const uint32 vaddr)
    {
        ASSERT(isValidAddr(vaddr));
        uint32 idx = ((vaddr & ~0xfff) - startAddress) / PAGE_SIZE;
        ASSERT(resources.get(idx) == 1);
        return privileges[idx];
    }

    inline bool isValidAddr(const uint32 vaddr) {
        return vaddr >= startAddress && vaddr <= endAddress;
    }
};

template <>
class AddressPool<PA>
{
public:
    Buddy resources;
    uint32 startAddress;

public:
    AddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const uint32 startAddress, char* freeBitMap, FreeNode* freeNodes)
    {
        resources.initialize(bitmap, length, freeBitMap, freeNodes);
        this->startAddress = startAddress;
    }

    // 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
    int allocate(const int count)
    {
        int start = resources.allocate(count);
        return (start == -1) ? -1 : (start * PAGE_SIZE + startAddress);
    }

    // 释放若干页的空间
    void release(const uint32 address, const int amount)
    {
        resources.release((address - startAddress) / PAGE_SIZE, amount);
    }
};


#endif
