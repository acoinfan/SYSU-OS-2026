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
    uint32 startAddress, endAddress;   // 左闭右开
    uint32 victim_idx, length;  // [0,length)
public:
    AddressPool() {}

    // 初始化地址池
    void initialize(char *bitmap, const int length, const uint32 startAddress, char* freeBitMap, FreeNode* freeNodes)
    {
        resources.initialize(bitmap, length, freeBitMap, freeNodes);
        this->startAddress = startAddress;
        this->length = length;
        this->victim_idx = 0;
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

    VictimInfo findVictim(uint32 search_length=0, uint32 round=2)
    {
        uint32 victim_pgi_base = PA2PGI(startAddress); 
        uint32 saved_victim_idx = this->victim_idx;
        // Access,Dirty IDX 00
        VictimInfo victims[4] = {{0,0}, {0,0}, {0,0}, {0,0}}; // 这里用0初始化，因为pageinfos[0]必然不受管理
        if (search_length == 0 || search_length > this->length) search_length = this->length;

        for (int r = 0; r < round; r++) {
            this->victim_idx = saved_victim_idx;
            for (int i = 0; i < search_length; i++,
                this->victim_idx = (this->victim_idx + 1) % this->length) {
    
                // 所有的页都已被分配才调用
                ASSERT(resources.isAlloc(this->victim_idx));
                uint32 victim_pgi = victim_pgi_base + this->victim_idx;
                if (memoryManager.pageinfos[victim_pgi].hasFlag(PG_SINGLE) 
                    && memoryManager.pageinfos[victim_pgi].getRef() == 1) {
                    uint32 rmapIdx = memoryManager.pageinfos[victim_pgi].extra;
                    RMapEntry rmapEntry = memoryManager.rmapManager.RMapStart[rmapIdx];
                    uint32* PTEptr = (uint32*)rmapEntry.pte_addr;
                    int access = (!!((*PTEptr) & PTE_ACCESSED)) << 1U;
                    int dirty = (!!((*PTEptr) & PTE_DIRTY));
    
                    // 清除 PTE Access, 并且不属于最后一轮时跳过
                    if (r < round - 1 && access) {
                        (*PTEptr) = (*PTEptr) & (~PTE_ACCESSED);
                        continue;
                    }

                    int priority = access | dirty;
                    if (victims[priority].paddr == 0) {
                        victims[priority].paddr = startAddress + this->victim_idx * PAGE_SIZE;
                        victims[priority].PTEptr = (uint32)PTEptr;
                        if (priority == 0) return victims[0];    // 0,0 shortCut
                    }
                }
            }
        }
        for (int i = 1; i < 4; i++) {
            if (victims[i].paddr != 0 && victims[i].PTEptr != 0) {
                return victims[i];
            }
        }
        // 无效的返回值, 用于判断
        return {0,0};
    }
};


#endif
