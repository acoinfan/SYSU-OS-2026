#include "address_pool.h"
#include "os_modules.h"
#include "assert.h"
#include "stdlib.h"
#include "syscall.h"
#include "asm_utils.h"

// 初始化地址池
void VAddressPool::initialize(char *bitmap, const int length, const uint32 startAddress,
                                 const uint32 endAddress, const uint32 privileges,
                                 const VPageFlags static_privilege, bool isStatic)
{
    ASSERT(length);
    this->length = length;
    resources.initialize(bitmap, length);
    this->startAddress = startAddress;
    this->endAddress = endAddress;
    this->isStatic = isStatic;
    this->static_privilege = static_privilege;
    ASSERT(length == (endAddress - startAddress + 1) / PAGE_SIZE); // 左闭右闭
    if (isStatic) {
        ASSERT(privileges == 0);
        this->privileges = nullptr;
    } else {
        this->privileges = (VPageFlags *)privileges;
    }
}

void VAddressPool::initialize(const VAddressPoolConfig& config) {
    ASSERT(config.length);
    initialize(config.bitmap, config.length, config.start_addr, config.end_addr,
                config.privilegePtr, config.static_privilege, config.is_static);
}

void VAddressPool::initialize(const VAddressPoolConfigLite& config, char* bitmap, const uint32 privileges) {
    ASSERT(config.length);
    initialize(bitmap, config.length, config.start_addr, config.end_addr,
                privileges, config.static_privilege, config.is_static);
}

void VAddressPool::initialize(const VAddressPool& parent, char *bitmap, const uint32 privileges) {
    this->length = parent.length;
    ASSERT(this->length);
    resources.initialize(bitmap, parent.length);

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

VPageFlags VAddressPool::getVPageFlag(const uint32 vaddr) const
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

inline bool VAddressPool::isValidAddr(const uint32 vaddr) const 
{
    return vaddr >= startAddress && vaddr <= endAddress;
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
                uint32 *PTEptr = (uint32 *)rmapEntry.pte_vaddr;
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

// void UserVAddressPool::initialize(const struct SegBoundary& segBoundary, 
//                         const VAPConfig& heapConf, const VAPConfig& stackConf,
//                         const VAPConfig& mmapConf, const VAPConfig& TLSConf) {
//     ASSERT(!this->isInitialized);
//     this->segBoundary = segBoundary;
//     this->heapPool.initialize(heapConf);
//     this->stackPool.initialize(stackConf);
//     this->mmapPool.initialize(mmapConf);
//     this->TLSPool.initialize(TLSConf);
//     this->isInitialized = true;
//     ASSERT(heapPool.isStatic && stackPool.isStatic);
//     ASSERT(heapPool.getVPageFlag(0) == (VP_RW|VP_USER));
//     ASSERT(stackPool.getVPageFlag(0) == (VP_RW|VP_USER));
// }

bool UserVAddressPool::initialize(const struct SegBoundary& segBoundary, 
                        const VAPConfigLite& heapConf, const VAPConfigLite& stackConf,
                        const VAPConfigLite& mmapConf, const VAPConfigLite& TLSConf) {
    ASSERT(!this->isInitialized);

    uint32 heapBitmapBytes = ceil(heapConf.length, 8);
    uint32 stackBitmapBytes = ceil(stackConf.length, 8);
    uint32 tlsBitmapBytes = ceil(TLSConf.length, 8);
    uint32 mmapBitmapBytes = ceil(mmapConf.length, 8);
    uint32 tlsPrivBytes = TLSConf.length * sizeof(VPageFlags);
    uint32 mmapPrivBytes = mmapConf.length * sizeof(VPageFlags);

    uint32 totalBitmapBytes = ALIGN(PAGE_SIZE, heapBitmapBytes + stackBitmapBytes + tlsBitmapBytes + mmapBitmapBytes);
    uint32 totalPrivBytes = ALIGN(PAGE_SIZE, tlsPrivBytes + mmapPrivBytes);
    
    // try alloc
    uint32 bitmapStart = memoryManager.allocatePages(AddressPoolType::KERNEL, totalBitmapBytes / PAGE_SIZE, VP_RW);
    uint32 privStart = memoryManager.allocatePages(AddressPoolType::KERNEL, totalPrivBytes / PAGE_SIZE, VP_RW);

    if (!bitmapStart || !privStart) {
        memoryManager.releasePages(AddressPoolType::KERNEL, bitmapStart, totalBitmapBytes / PAGE_SIZE);
        memoryManager.releasePages(AddressPoolType::KERNEL, privStart, totalPrivBytes / PAGE_SIZE);
        return false;
    }

    // Clear Space
    memset((void*)bitmapStart, 0, totalBitmapBytes);
    memset((void*)privStart, 0, totalPrivBytes);
    
    // Calculatie Ptr
    uint32 cnt = bitmapStart;
    uint32 heapBitmapStart = cnt;
    cnt += heapBitmapBytes;

    uint32 stackBitmapStart = cnt;
    cnt += stackBitmapBytes;

    uint32 tlsBitmapStart = cnt;
    cnt += tlsBitmapBytes;

    uint32 mmapBitmapStart = cnt;
    cnt += mmapBitmapBytes;

    cnt = privStart;
    uint32 tlsPrivStart = cnt;
    cnt += tlsPrivBytes;

    uint32 mmapPrivStart = cnt;
    cnt += mmapPrivBytes;

    // initialize
    this->segBoundary = segBoundary;
    this->heapPool.initialize(heapConf, (char*)heapBitmapStart, 0U);
    this->stackPool.initialize(stackConf, (char*)stackBitmapStart, 0U);
    this->mmapPool.initialize(mmapConf, (char*)mmapBitmapStart, mmapPrivStart);
    this->TLSPool.initialize(TLSConf, (char*)tlsBitmapStart, tlsPrivStart);
    this->bitmapStart = bitmapStart;
    this->bitmapPage = totalBitmapBytes / PAGE_SIZE;
    this->privStart = privStart;
    this->privPage = totalPrivBytes / PAGE_SIZE;
    this->isInitialized = true;

    ASSERT(heapPool.isStatic && stackPool.isStatic);
    ASSERT(heapPool.getVPageFlag(0) == (VP_RW|VP_USER));
    ASSERT(stackPool.getVPageFlag(0) == (VP_RW|VP_USER));
    return true;
}

bool UserVAddressPool::cloneFrom(const UserVAddressPool& parent) {
    ASSERT(!this->isInitialized);
    if (!parent.isInitialized) return false;
    
    // Calculate Size
    uint32 heapBitmapBytes = ceil(parent.heapPool.length, 8);
    uint32 stackBitmapBytes = ceil(parent.stackPool.length, 8);
    uint32 tlsBitmapBytes  = ceil(parent.TLSPool.length, 8);
    uint32 mmapBitmapBytes = ceil(parent.mmapPool.length, 8);
    uint32 tlsPrivBytes = parent.TLSPool.length * sizeof(VPageFlags);
    uint32 mmapPrivBytes = parent.mmapPool.length * sizeof(VPageFlags);
    
    uint32 totalBitmapBytes = ALIGN(PAGE_SIZE, heapBitmapBytes + stackBitmapBytes + tlsBitmapBytes + mmapBitmapBytes);
    uint32 totalPrivBytes = ALIGN(PAGE_SIZE, tlsPrivBytes + mmapPrivBytes);

    // try alloc
    uint32 bitmapStart = memoryManager.allocatePages(AddressPoolType::KERNEL, totalBitmapBytes / PAGE_SIZE, VP_RW);
    uint32 privStart = memoryManager.allocatePages(AddressPoolType::KERNEL, totalPrivBytes / PAGE_SIZE, VP_RW);

    if (!bitmapStart || !privStart) {
        memoryManager.releasePages(AddressPoolType::KERNEL, bitmapStart, totalBitmapBytes / PAGE_SIZE);
        memoryManager.releasePages(AddressPoolType::KERNEL, privStart, totalPrivBytes / PAGE_SIZE);
        return false;
    }

    // Calculatie Ptr
    uint32 cnt = bitmapStart;
    uint32 heapBitmapStart = cnt;
    cnt += heapBitmapBytes;

    uint32 stackBitmapStart = cnt;
    cnt += stackBitmapBytes;

    uint32 tlsBitmapStart = cnt;
    cnt += tlsBitmapBytes;

    uint32 mmapBitmapStart = cnt;
    cnt += mmapBitmapBytes;

    cnt = privStart;
    uint32 tlsPrivStart = cnt;
    cnt += tlsPrivBytes;

    uint32 mmapPrivStart = cnt;
    cnt += mmapPrivBytes;

    // Copy Parameters
    this->segBoundary = parent.segBoundary;
    this->heapPool.initialize(parent.heapPool, (char*)heapBitmapStart, 0);
    this->stackPool.initialize(parent.stackPool, (char*)stackBitmapStart, 0);
    this->TLSPool.initialize(parent.TLSPool, (char*)tlsBitmapStart, tlsPrivStart);
    this->mmapPool.initialize(parent.mmapPool, (char*)mmapBitmapStart, mmapPrivStart);
    this->bitmapStart = bitmapStart;
    this->bitmapPage = totalBitmapBytes / PAGE_SIZE;
    this->privStart = privStart;
    this->privPage = totalPrivBytes / PAGE_SIZE;
    
    ASSERT(heapPool.isStatic && stackPool.isStatic);
    ASSERT(heapPool.getVPageFlag(0) == (VP_RW|VP_USER));
    ASSERT(stackPool.getVPageFlag(0) == (VP_RW|VP_USER));
    
    ASSERT(this->stackPool.length);
    // DeepCopy BitMap, VPageFlags(Only TLS & mmap)
    uint32 heapLength = this->heapPool.length;
    uint32 stackLength = this->stackPool.length;
    uint32 TLSLength = this->TLSPool.length;
    uint32 mmapLength = this->mmapPool.length;
    dump_pte(heapBitmapStart);

    // Bitmap memcpy
    memcpy((void*)heapBitmapStart, parent.heapPool.resources.bitmap, heapBitmapBytes);
    memcpy((void*)stackBitmapStart, parent.stackPool.resources.bitmap, stackBitmapBytes);
    memcpy((void*)tlsBitmapStart, parent.TLSPool.resources.bitmap, tlsBitmapBytes);
    memcpy((void*)mmapBitmapStart, parent.mmapPool.resources.bitmap, mmapBitmapBytes);

    // Privileges memcpy
    memcpy((void*)tlsPrivStart, parent.TLSPool.privileges, tlsPrivBytes);
    memcpy((void*)mmapPrivStart, parent.mmapPool.privileges, mmapPrivBytes);
    this->isInitialized = true;
    ASSERT(this->stackPool.length);
    return true;
}

void UserVAddressPool::destroy() {
    memoryManager.releasePages(AddressPoolType::KERNEL, bitmapStart, bitmapPage);
    memoryManager.releasePages(AddressPoolType::KERNEL, privStart, privPage);
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

// 成功则返回第一个页的地址，失败则返回-1
// int UserVAddressPool::allocateLazy(UserSegment seg, const uint32 count, VPageFlags privilege, bool reverse) {
//     switch (seg) {
//         case UserSegment::TEXT:
//         case UserSegment::DATA:
//         case UserSegment::BSS:
//             return -1;

//         case UserSegment::HEAP:
//             return heapPool.allocateLazy(count, VP_CLEAR, /*reverse=*/false);   // Privilege不会影响固定池
//         case UserSegment::STACK:
//             return stackPool.allocate(count, VP_CLEAR, /*reverse=*/true);
//         case UserSegment::TLS:
//             return TLSPool.allocate(count, privilege, reverse);
//         case UserSegment::MMAP:
//             return mmapPool.allocate(count, privilege, reverse);
//         default:
//             return -1;
//     }
// }

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

UserSegment UserVAddressPool::vaddr2Seg(const uint32 vaddr) const {
    if (segBoundary.text.isValidAddr(vaddr)) {
        return UserSegment::TEXT;
    } else if (segBoundary.data.isValidAddr(vaddr)) {
        return UserSegment::DATA;
    } else if (segBoundary.bss.isValidAddr(vaddr)) {
        return UserSegment::BSS;
    } else if (heapPool.isValidAddr(vaddr)) {
        return UserSegment::HEAP;
    } else if (stackPool.isValidAddr(vaddr)) {
        return UserSegment::STACK;
    } else if (TLSPool.isValidAddr(vaddr)) {
        return UserSegment::TLS;
    } else if (mmapPool.isValidAddr(vaddr)) {
        return UserSegment::MMAP;
    } else {
        return UserSegment::EMPTY;
    }
}

