#ifndef MEMORY_H
#define MEMORY_H

#include "address_pool.h"
#include "pageinfo.h"
#include "rmap.h"
#include "enum.h"
// align只能为2的幂
#define ALIGN(align, n) (((n) + ((align) - 1)) & ~((align) - 1))

// 仅适用于PTEVaddr -> 受PTE管理的vaddr (用于rmap中反向获取需要刷新的TLB地址)
// 务必保证是正确的页表才可使用
#define PTEVA2VADDR(pte_ptr) \
    ((((uint32)(pte_ptr) - 0xFFC00000) >> 12) << 22 | \
     ((((uint32)(pte_ptr) - 0xFFC00000) >> 2) & 0x3FF) << 12)

class MemoryManager
{
public:
    // 可管理的内存容量
    int totalMemory;
    // 内核物理地址池
    PAddressPool kernelPhysical;
    // 用户物理地址池
    PAddressPool userPhysical;
    // 内核虚拟地址池
    KernelVAddressPool kernelVirtual;
    PageInfo* pageinfos;
    RMapManager rmapManager;
public:
    MemoryManager();

    // 初始化地址池
    void initialize();

    // 从type类型的物理地址池中分配count个连续的页
    // 成功，返回起始地址；失败，返回0
    int allocatePhysicalPages(enum AddressPoolType type, const int count);

    // 释放从paddr开始的count个物理页
    void releasePhysicalPages(enum AddressPoolType type, const int startAddress, const int count);

    // 获取内存总容量
    int getTotalMemory();

    // 页内存分配
    // 成功，返回起始地址；失败，返回0
    int allocatePages(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment = UserSegment::EMPTY, bool reverse = false);

    // 页内存懒分配
    // 成功，返回起始地址；失败，返回0
    int allocatePagesLazy(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment = UserSegment::EMPTY, bool reverse = false);
    
    // 虚拟页分配
    // 成功，返回起始地址；失败，返回0
    int allocateVirtualPages(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment = UserSegment::EMPTY, bool reverse = false);

    // 建立虚拟页到物理页的联系
    bool connectPhysicalVirtualPage(const int virtualAddress, const int physicalPageAddress);

    // 计算virtualAddress的页目录项的虚拟地址
    int toPDE(const int virtualAddress);

    // 计算virtualAddress的页表项的虚拟地址
    int toPTE(const int virtualAddress);

    // 计算virtualAddress的页表项的物理地址
    int toPTEpa(const int virtualAddress);

    // 页内存释放
    void releasePages(enum AddressPoolType type, const int virtualAddress, const int count, UserSegment userSegment = UserSegment::EMPTY);    

    // 找到虚拟地址对应的物理地址
    int vaddr2paddr(int vaddr);

    // 释放虚拟页
    void releaseVirtualPages(enum AddressPoolType type, const int vaddr, const int count, UserSegment userSegment = UserSegment::EMPTY);

    // CLOCK (0,0) is Invalid
    VictimInfo findVictim(enum AddressPoolType type);

    // 请务必将两者成对使用,且务必明确正在做什么
    // 返回的是临时分配页的页首地址 + offset(也就是可以直接使用), 失败是0
    // paddr可以是任何合法地址, 不一定只归属Kernel PA Pool
    uint32 mapTemp(enum AddressPoolType type, const int paddr);
    // 请务必将两者成对使用,且务必明确正在做什么
    void unmapTemp(enum AddressPoolType type);

    bool setCOW(PageInfo* pi);

    int allocatePageTable(void);

    void releasePageTable(uint32 PDEptr);
    void PDEinc(uint32 PDEptr);

    // true if 0
    bool PDEdec(uint32 PDEptr);
};

#endif