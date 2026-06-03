#include "address_pool.h"
#include "os_modules.h"
#include "assert.h"
#include "stdlib.h"

// 初始化地址池
void VAddressPool::initialize(char *bitmap, const int length, const uint32 startAddress,
                                 const uint32 endAddress, const uint32 privileges,
                                 const VPageFlags static_privilege, bool isStatic)
{
    resources.initialize(bitmap, length);
    this->startAddress = startAddress;
    this->endAddress = endAddress;
    this->isStatic = isStatic;
    this->static_privilege = static_privilege;
    ASSERT(length == endAddress - startAddress + 1); // 左闭右闭
    if (isStatic) {
        ASSERT(privileges == 0);
        this->privileges = nullptr;
    } else {
        this->privileges = (VPageFlags *)privileges;
    }
}

void VAddressPool::initialize(const VAddressPoolConfig& config) {
    initialize(config.bitmap, config.length, config.start_addr, config.end_addr,
                config.privilegePtr, config.static_privilege, config.is_static);
}

void VAddressPool::initialize(const VAddressPool& parent, char *bitmap, const uint32 privileges) {
    uint32 length = parent.endAddress - parent.startAddress + 1;
    resources.initialize(bitmap, length);

    this->startAddress = parent.startAddress;
    this->endAddress = parent.endAddress;
    this->isStatic = parent.isStatic;
    this->static_privilege = parent.static_privilege;

    if (this->isStatic) {
        ASSERT(parent.privileges == 0);
        ASSERT(privileges == 0);
        this->privileges = nullptr;
    } else {
        this->privileges = (VPageFlags *)privileges;
    }
}

// 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
// 若isStatic, 传入的privilege会被忽略
int VAddressPool::allocate(const int count, VPageFlags privilege, bool reverse)
{
    int start = resources.allocate(count, reverse);
    if (start != -1)
    {   
        if (!isStatic) {
            for (int i = 0; i < count; i++) {
                privileges[start+i] = privilege;
            }

        }
        return start * PAGE_SIZE + startAddress;
    }
    else
    {
        return -1;
    }
}

// 释放若干页的空间
void VAddressPool::release(const uint32 address, const int count)
{
    int start = (address - startAddress) / PAGE_SIZE;
    resources.release(start, count);
    if (!isStatic) {
        for (int i = 0; i < count; i++) {
            privileges[start+i] = VP_CLEAR;
        }

    }
}

VPageFlags VAddressPool::getVPageFlag(const uint32 vaddr)
{
    if (isStatic) {
        return this->static_privilege;
    }
    // 对于Static池, 不做检测
    ASSERT(isValidAddr(vaddr));
    uint32 idx = ((vaddr & ~0xfff) - startAddress) / PAGE_SIZE;
    ASSERT(resources.get(idx) == 1);
    return privileges[idx];
}

// 初始化地址池
void PAddressPool::initialize(char *bitmap, const int length, const uint32 startAddress, char *freeBitMap, FreeNode *freeNodes)
{
    resources.initialize(bitmap, length, freeBitMap, freeNodes);
    this->startAddress = startAddress;
    this->length = length;
    this->victim_idx = 0;
}

// 从地址池中分配count个连续页，成功则返回第一个页的地址，失败则返回-1
int PAddressPool::allocate(const int count)
{
    int start = resources.allocate(count);
    return (start == -1) ? -1 : (start * PAGE_SIZE + startAddress);
}

// 释放若干页的空间
void PAddressPool::release(const uint32 address, const int amount)
{
    resources.release((address - startAddress) / PAGE_SIZE, amount);
}

VictimInfo PAddressPool::findVictim(uint32 search_length, uint32 round)
{
    uint32 victim_pgi_base = PA2PGI(startAddress);
    uint32 saved_victim_idx = this->victim_idx;
    // Access,Dirty IDX 00
    VictimInfo victims[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}; // 这里用0初始化，因为pageinfos[0]必然不受管理
    if (search_length == 0 || search_length > this->length)
        search_length = this->length;

    for (int r = 0; r < round; r++)
    {
        this->victim_idx = saved_victim_idx;
        for (int i = 0; i < search_length; i++,
                 this->victim_idx = (this->victim_idx + 1) % this->length)
        {

            // 所有的页都已被分配才调用
            ASSERT(resources.isAlloc(this->victim_idx));
            uint32 victim_pgi = victim_pgi_base + this->victim_idx;
            if (memoryManager.pageinfos[victim_pgi].hasFlag(PG_SINGLE) && memoryManager.pageinfos[victim_pgi].getRef() == 1)
            {
                uint32 rmapIdx = memoryManager.pageinfos[victim_pgi].extra;
                RMapEntry rmapEntry = memoryManager.rmapManager.RMapStart[rmapIdx];
                uint32 *PTEptr = (uint32 *)rmapEntry.pte_addr;
                int access = (!!((*PTEptr) & PTE_ACCESSED)) << 1U;
                int dirty = (!!((*PTEptr) & PTE_DIRTY));

                // 清除 PTE Access, 并且不属于最后一轮时跳过
                if (r < round - 1 && access)
                {
                    (*PTEptr) = (*PTEptr) & (~PTE_ACCESSED);
                    continue;
                }

                int priority = access | dirty;
                if (victims[priority].paddr == 0)
                {
                    victims[priority].paddr = startAddress + this->victim_idx * PAGE_SIZE;
                    victims[priority].PTEptr = (uint32)PTEptr;
                    if (priority == 0)
                        return victims[0]; // 0,0 shortCut
                }
            }
        }
    }
    for (int i = 1; i < 4; i++)
    {
        if (victims[i].paddr != 0 && victims[i].PTEptr != 0)
        {
            return victims[i];
        }
    }
    // 无效的返回值, 用于判断
    return {0, 0};
}

void UserVAddressPool::initialize(const struct SegBoundary& segBoundary, 
                        const VAPConfig& heapConf, const VAPConfig& stackConf,
                        const VAPConfig& mmapConf, const VAPConfig& TLSConf) {
    ASSERT(!this->isInitialized);
    this->segBoundary = segBoundary;
    this->heapPool.initialize(heapConf);
    this->stackPool.initialize(stackConf);
    this->mmapPool.initialize(mmapConf);
    this->TLSPool.initialize(TLSConf);
    this->isInitialized = true;
    ASSERT(heapPool.isStatic && stackPool.isStatic);
    ASSERT(heapPool.getVPageFlag(0) == (VP_RW|VP_USER));
    ASSERT(stackPool.getVPageFlag(0) == (VP_RW|VP_USER));
}

bool UserVAddressPool::cloneFrom(const UserVAddressPool& parent, const VPoolBuffers& vpoolBuf) {
    ASSERT(!this->isInitialized);
    if (!parent.isInitialized) return false;

    // Copy Parameters
    this->segBoundary = parent.segBoundary;
    this->heapPool.initialize(parent.heapPool, (char*)vpoolBuf.heapBitmap, 0);
    this->stackPool.initialize(parent.stackPool, (char*)vpoolBuf.stackBitmap, 0);
    this->TLSPool.initialize(parent.TLSPool, (char*)vpoolBuf.TLSBitmap, vpoolBuf.TLSPrivileges);
    this->mmapPool.initialize(parent.mmapPool, (char*)vpoolBuf.mmapBitmap, vpoolBuf.mmapPrivileges);
    ASSERT(heapPool.isStatic && stackPool.isStatic);
    ASSERT(heapPool.getVPageFlag(0) == (VP_RW|VP_USER));
    ASSERT(stackPool.getVPageFlag(0) == (VP_RW|VP_USER));
    
    // DeepCopy BitMap, VPageFlags(Only TLS & mmap)
    uint32 heapLength = this->heapPool.endAddress - this->heapPool.startAddress + 1;
    uint32 stackLength = this->stackPool.endAddress - this->stackPool.startAddress + 1;
    uint32 TLSLength = this->TLSPool.endAddress - this->TLSPool.startAddress + 1;
    uint32 mmapLength = this->mmapPool.endAddress - this->mmapPool.startAddress + 1;

    // Bitmap memcpy
    memcpy((void*)vpoolBuf.heapBitmap, parent.heapPool.resources.bitmap, ceil(heapLength, 8));
    memcpy((void*)vpoolBuf.stackBitmap, parent.stackPool.resources.bitmap, ceil(stackLength, 8));
    memcpy((void*)vpoolBuf.TLSBitmap, parent.TLSPool.resources.bitmap, ceil(TLSLength, 8));
    memcpy((void*)vpoolBuf.mmapBitmap, parent.mmapPool.resources.bitmap, ceil(mmapLength, 8));

    // Privileges memcpy
    memcpy((void*)vpoolBuf.TLSPrivileges, parent.TLSPool.privileges, TLSLength * sizeof(VPageFlags));
    memcpy((void*)vpoolBuf.mmapPrivileges, parent.mmapPool.privileges, mmapLength * sizeof(VPageFlags));
    return true;
}

// 成功则返回第一个页的地址，失败则返回-1
int UserVAddressPool::allocate(UserSegment seg, const uint32 count, VPageFlags privilege, bool reverse) {
    switch (seg) {
        case UserSegment::TEXT:
        case UserSegment::DATA:
        case UserSegment::BSS:
            return -1;

        case UserSegment::HEAP:
            return heapPool.allocate(count, VP_CLEAR, /*reverse=*/false);   // Privilege不会影响固定池
        case UserSegment::STACK:
            return stackPool.allocate(count, VP_CLEAR, /*reverse=*/true);
        case UserSegment::TLS:
            return TLSPool.allocate(count, privilege, reverse);
        case UserSegment::MMAP:
            return mmapPool.allocate(count, privilege, reverse);
        default:
            return -1;
    }
}

void UserVAddressPool::release(UserSegment seg, const uint32 vaddr, const uint32 count) {
    switch (seg) {
        case UserSegment::TEXT:
        case UserSegment::DATA:
        case UserSegment::BSS:
            return;

        case UserSegment::HEAP:
            return heapPool.release(vaddr, count);  
        case UserSegment::STACK:
            return stackPool.release(vaddr, count);
        case UserSegment::TLS:
            return TLSPool.release(vaddr, count);
        case UserSegment::MMAP:
            return mmapPool.release(vaddr, count);
        default:
            return;
    }
}

VPageFlags UserVAddressPool::getVPageFlag(UserSegment seg, const uint32 vaddr) {
    switch (seg) {
        // 固定
        case UserSegment::TEXT:
            return VP_USER;
        case UserSegment::DATA:
            return (VPageFlags)(VP_RW|VP_USER);
        case UserSegment::BSS:
            return (VPageFlags)(VP_RW|VP_USER);

        // 理论均为 VP_RW | VP_USER
        case UserSegment::HEAP:
            return heapPool.getVPageFlag(vaddr);   
        case UserSegment::STACK:
            return stackPool.getVPageFlag(vaddr);
        
        // 动态
        case UserSegment::TLS:
            return TLSPool.getVPageFlag(vaddr);
        case UserSegment::MMAP:
            return mmapPool.getVPageFlag(vaddr);
        default:
            ASSERT(0);
    }
}

Boundary UserVAddressPool::getBoundary(UserSegment seg) const {
    switch (seg) {
        // 固定
        case UserSegment::TEXT:
            return segBoundary.text;
        case UserSegment::DATA:
            return segBoundary.data;
        case UserSegment::BSS:
            return segBoundary.bss;

        // 理论均为 VP_RW | VP_USER
        case UserSegment::HEAP:
            return {heapPool.startAddress, heapPool.endAddress};  
        case UserSegment::STACK:
            return {stackPool.startAddress, stackPool.endAddress};
        
        // 动态
        case UserSegment::TLS:
            return {TLSPool.startAddress, TLSPool.endAddress};
        case UserSegment::MMAP:
            return {mmapPool.startAddress, mmapPool.endAddress};
        default:
            ASSERT(0);
    }
}

bool UserVAddressPool::isValidAddr(const uint32 vaddr) const {
    return false;// TODO
}

