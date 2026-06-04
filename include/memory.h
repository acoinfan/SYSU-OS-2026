#ifndef MEMORY_H
#define MEMORY_H

#include "address_pool.h"
#include "pageinfo.h"
#include "rmap.h"
#include "enum.h"
// align只能为2的幂
#define ALIGN(align, n) (((n) + ((align) - 1)) & ~((align) - 1))

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
    int allocatePages(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment = UserSegment::EMPTY);

    // 页内存懒分配
    int allocatePagesLazy(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment = UserSegment::EMPTY);
    
    // 虚拟页分配
    int allocateVirtualPages(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment = UserSegment::EMPTY);

    // 建立虚拟页到物理页的联系
    bool connectPhysicalVirtualPage(const int virtualAddress, const int physicalPageAddress);

    // 计算virtualAddress的页目录项的虚拟地址
    int toPDE(const int virtualAddress);

    // 计算virtualAddress的页表项的虚拟地址
    int toPTE(const int virtualAddress);

    // 页内存释放
    void releasePages(enum AddressPoolType type, const int virtualAddress, const int count, UserSegment userSegment = UserSegment::EMPTY);    

    // 找到虚拟地址对应的物理地址
    int vaddr2paddr(int vaddr);

    // 释放虚拟页
    void releaseVirtualPages(enum AddressPoolType type, const int vaddr, const int count, UserSegment userSegment = UserSegment::EMPTY);

    // CLOCK (0,0) is Invalid
    VictimInfo findVictim(enum AddressPoolType type);
};

#endif