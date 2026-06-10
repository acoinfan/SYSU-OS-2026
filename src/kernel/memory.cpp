#include "memory.h"
#include "os_constant.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "screen.h"
#include "program.h"
#include "os_modules.h"
#include "debug.h"

MemoryManager::MemoryManager()
{
    initialize();
}

void MemoryManager::initialize()
{
    LOG_TRACE("CALL\n");
    this->totalMemory = 0;
    this->totalMemory = getTotalMemory();

    // 预留的内存
    // int usedMemory = 256 * PAGE_SIZE + 0x100000;
    int usedMemory = RESERVED_MEMORY;
    if (this->totalMemory < usedMemory)
    {
        LOG_ERROR("memory is too small, halt.\n");
        asm_halt();
    }
    
    // Parameters
    int maxPages = ALIGN(PAGE_SIZE, (this->totalMemory) / 2) / PAGE_SIZE;
    int totalPages = ALIGN(PAGE_SIZE, this->totalMemory) / PAGE_SIZE;
    // PA Para
    int PAfreeNodeListSize = ALIGN(PAGE_SIZE, maxPages * sizeof(FreeNode));
    int PAfreeNodeBitMapSize = ALIGN(PAGE_SIZE, (maxPages+7)/8);
    int PABitMapSize = ALIGN(PAGE_SIZE, (maxPages+7)/8);
    // VA Para
    int VABitMapSize = KERNEL_VA_BITMAP_SIZE;
    int VAPrevilegeSize = ALIGN(PAGE_SIZE, KERNEL_VA_BITMAP_SIZE * 8 * sizeof(VPageFlags));
    // PageInfo Para
    int PageInfoSize = ALIGN(PAGE_SIZE, totalPages * sizeof(PageInfo));
    // RMapManager Para
    int RMapNodeCount = 1.5 * totalPages;
    int RMapNodeListSize = ALIGN(PAGE_SIZE, RMapNodeCount * sizeof(RMapEntry));
    int RMapBitMapSize = ALIGN(PAGE_SIZE, (RMapNodeCount+7)/8);


    // KernelPA Pool
    int kernelPAFreeNodeListStart = usedMemory;
    usedMemory += PAfreeNodeListSize;
    int kernelPAFreeNodeBitMapStart = usedMemory;
    usedMemory += PAfreeNodeBitMapSize;
    int kernelPhysicalBitMapStart = usedMemory;
    usedMemory += PABitMapSize;

    // UserPA Pool
    int userPAFreeNodeListStart = usedMemory;
    usedMemory += PAfreeNodeListSize;
    int userPAFreeNodeBitMapStart = usedMemory;
    usedMemory += PAfreeNodeBitMapSize;
    int userPhysicalBitMapStart = usedMemory;
    usedMemory += PABitMapSize;

    // KernelVA Pool
    int kernelVirtualBitMapStart = usedMemory;
    usedMemory += VABitMapSize;
    int kernelVirtualPrivilegeStart = usedMemory;
    usedMemory += VAPrevilegeSize;

    // PageInfo
    pageinfos = (PageInfo*)usedMemory;
    usedMemory += PageInfoSize;

    // RMapManager
    int RMapNodeListStart = usedMemory;
    usedMemory += RMapNodeListSize;
    int RMapBitMapStart = usedMemory;
    usedMemory += RMapBitMapSize;

    // Page Table Reserved
    int kernelVirtualStartAddress = KERNEL_VIRTUAL_START;

    int PTE_table_start = usedMemory;
    int PTEcount = (PTE_table_start - RESERVED_MEMORY)/ PAGE_SIZE;
    int PDEcount = ceil(PTEcount, (PAGE_SIZE / PTE_SIZE));       // 0-4MB已创建对应的PDE空间,故-1
    usedMemory += (PDEcount * PAGE_SIZE);
    

    // initialize Page Table
    for (int addr = RESERVED_MEMORY; addr < PTE_table_start; 
            addr += PAGE_SIZE, kernelVirtualStartAddress += PAGE_SIZE) {
        int* pde = (int*)toPDE(kernelVirtualStartAddress);
        if(!(*pde & PDE_PRESENT)) {
            int PDEidx = (kernelVirtualStartAddress >> 22) & 0x3ff;
            ASSERT(PDEidx >= 768 && PDEidx < 1024);
            int PTE_table_PAddr = (PTE_table_start + PAGE_SIZE * (PDEidx - 768));
            ASSERT(PTE_table_PAddr % PAGE_SIZE == 0);
            *pde = PTE_table_PAddr | PDE_KERNEL;
            
            // 清零PDE指向的页表
            int *pte_clear = (int*)toPTE(kernelVirtualStartAddress);
            memset((void*)((int)pte_clear & ~0xfff), 0, PAGE_SIZE);
        }
        int* pte = (int*)toPTE(kernelVirtualStartAddress);
        *pte = addr | 0x7;
    }
    ASSERT(!(usedMemory & 0xFFF));

    // 剩余的空闲的内存 
    int freeMemory = this->totalMemory - usedMemory;

    int freePages = freeMemory / PAGE_SIZE;
    int kernelPages = freePages / 2;
    int userPages = freePages - kernelPages;

    int kernelPhysicalStartAddress = usedMemory;
    int userPhysicalStartAddress = usedMemory + kernelPages * PAGE_SIZE;
    
    #define PAStart2VAStart(PAStart) ((PAStart) - (RESERVED_MEMORY) + (KERNEL_VIRTUAL_START))

    // 更新起始点
    // 坑: 不要修改PAStart和VAStart
    kernelPhysicalBitMapStart = PAStart2VAStart(kernelPhysicalBitMapStart);
    kernelPAFreeNodeBitMapStart = PAStart2VAStart(kernelPAFreeNodeBitMapStart);
    kernelPAFreeNodeListStart = PAStart2VAStart(kernelPAFreeNodeListStart);

    userPhysicalBitMapStart = PAStart2VAStart(userPhysicalBitMapStart);
    userPAFreeNodeBitMapStart = PAStart2VAStart(userPAFreeNodeBitMapStart);
    userPAFreeNodeListStart = PAStart2VAStart(userPAFreeNodeListStart);

    kernelVirtualBitMapStart = PAStart2VAStart(kernelVirtualBitMapStart);
    kernelVirtualPrivilegeStart = PAStart2VAStart(kernelVirtualPrivilegeStart);

    RMapNodeListStart = PAStart2VAStart(RMapNodeListStart);
    RMapBitMapStart = PAStart2VAStart(RMapBitMapStart);

    pageinfos = (PageInfo*)PAStart2VAStart((int)pageinfos);

    int kernelVirtualPages = ceil(0xffffffff - kernelVirtualStartAddress, PAGE_SIZE);

    // clean Bitmap, FreeBitMap and FreeNodes
    // clean Bitmap
    memset((void*)kernelPhysicalBitMapStart, 0, PABitMapSize);
    memset((void*)userPhysicalBitMapStart, 0, PABitMapSize);

    memset((void*)kernelVirtualBitMapStart, 0, VABitMapSize);

    // clean FreeBitMap
    memset((void*)kernelPAFreeNodeBitMapStart, 0, PAfreeNodeBitMapSize);
    memset((void*)userPAFreeNodeBitMapStart, 0, PAfreeNodeBitMapSize);

    // clean FreeNodeList
    memset((void*)kernelPAFreeNodeListStart, 0, PAfreeNodeListSize);
    memset((void*)userPAFreeNodeListStart, 0, PAfreeNodeListSize);
    
    // clean Privileges
    memset((void*)kernelVirtualPrivilegeStart, 0, VAPrevilegeSize);

    // clean PageInfo
    memset((void*)pageinfos, 0, PageInfoSize);

    // clean RMap
    memset((void*)RMapNodeListStart, 0, RMapNodeListSize);

    // Set Kernel Pages
    // DEBUG:
    // kernelPages = 1;
    kernelPhysical.initialize(
        (char *)kernelPhysicalBitMapStart,
        kernelPages,
        kernelPhysicalStartAddress,
        (char *)kernelPAFreeNodeBitMapStart,
        (FreeNode*)kernelPAFreeNodeListStart
    );

    userPhysical.initialize(
        (char *)userPhysicalBitMapStart,
        userPages,
        userPhysicalStartAddress,
        (char *)userPAFreeNodeBitMapStart,
        (FreeNode *)userPAFreeNodeListStart
    );
    
    kernelVirtual.initialize(
        (char *)kernelVirtualBitMapStart,
        kernelVirtualPages,
        kernelVirtualStartAddress,
        KERNEL_VIRTUAL_END,
        kernelVirtualPrivilegeStart);
        
    rmapManager.initialize(RMapNodeCount, 
        (RMapEntry*)RMapNodeListStart, 
        (char*)RMapBitMapStart);
    
    // for mapTemp/unmapTemp reservation
    // kernelVirtualStartAddress is used for temp map
    kernelVirtual.resources.set(0, true);

    // Set PageInfo
    int usedPages = usedMemory / PAGE_SIZE;
    for (int i = 0; i < usedPages; i++) {
        // RESERVED, KERNEL, ref = 1
        pageinfos[i].setFlag(PG_RESERVED);
        pageinfos[i].setFlag(PG_KERNEL);

        // 内核保留页不需要RMap
        pageinfos[i].ref = 1;
        pageinfos[i].extra = RMAP_PTR_NULL;
    }

    for (int i = usedPages; i < totalPages; i++) {
        pageinfos[i].setFlag(PG_FREE);
    }

    kprintf("total memory: %d bytes ( %d MB )\n",
        this->totalMemory,
    this->totalMemory / 1024 / 1024);

    kprintf("kernel pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d ( %d MB )\n"
           "    bitmap start address: 0x%x\n",
           kernelPhysicalStartAddress,
           kernelPages, kernelPages * PAGE_SIZE / 1024 / 1024,
           kernelPhysicalBitMapStart);

    kprintf("user pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d ( %d MB )\n"
           "    bit map start address: 0x%x\n",
           userPhysicalStartAddress,
           userPages, userPages * PAGE_SIZE / 1024 / 1024,
           userPhysicalBitMapStart);

    kprintf("kernel virtual pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d  ( %d MB ) \n"
           "    bit map start address: 0x%x\n"
           "    privileges start address: 0x%x\n",
           kernelVirtualStartAddress,
           userPages, kernelVirtualPages * PAGE_SIZE / 1024 / 1024,
           kernelVirtualBitMapStart,
           kernelVirtualPrivilegeStart);
    
    kprintf("Free Node List:\n"
            "   kernel start address: 0x%x\n"
            "   user start address: 0x%x\n",
            kernelPAFreeNodeListStart,
            userPAFreeNodeListStart);

    kprintf("Free Node Bit Map:\n"
            "   kernel start address: 0x%x\n"
            "   user start address: 0x%x\n",
            kernelPAFreeNodeBitMapStart,
            userPAFreeNodeBitMapStart);            
    
    kprintf("RMap\n"
            "   node list start address: 0x%x\n"
            "   total nodes: %d\n"
            "   bit map start address: 0x%x\n",
            RMapNodeListStart,
            RMapNodeListSize,
            RMapBitMapStart);
    
    kprintf("PageInfo\n"
            "   start address: 0x%x\n",
            (int)pageinfos);
}

int MemoryManager::allocatePhysicalPages(enum AddressPoolType type, const int count)
{
    int start = -1;

    if (type == AddressPoolType::KERNEL)
    {
        start = kernelPhysical.allocate(count);

    }
    else if (type == AddressPoolType::USER)
    {
        start = userPhysical.allocate(count);
    }

    if (start == -1) return 0;

    int pgi = PA2PGI(start);
    for (int i = 0; i < count; i++) {
        ASSERT(pageinfos[pgi+i].hasFlag(PG_FREE));
        pageinfos[pgi+i].clear();
        if (type == AddressPoolType::KERNEL)
            pageinfos[pgi+i].setFlag(PG_KERNEL); 
    }
    // 单页,打PG_SINGLE标记
    if (count == 1) {
        ASSERT(!pageinfos[pgi].hasFlag(PG_SINGLE));
        pageinfos[pgi].setFlag(PG_SINGLE);
    }
    return start;
}

void MemoryManager::releasePhysicalPages(enum AddressPoolType type, const int paddr, const int count)
{
    if (type == AddressPoolType::KERNEL)
    {
        kernelPhysical.release(paddr, count);
    }
    else if (type == AddressPoolType::USER)
    {
        userPhysical.release(paddr, count);
    }
    int pgi = PA2PGI(paddr);
    for (int i = 0; i < count; i++) {
        ASSERT(!pageinfos[pgi+i].hasFlag(PG_FREE));
        pageinfos[pgi+i].setFlag(PG_FREE);
    }
    if (count == 1) {
        ASSERT(pageinfos[pgi].hasFlag(PG_SINGLE));
        pageinfos[pgi].clearFlag(PG_SINGLE);
    }
}

int MemoryManager::getTotalMemory()
{

    if (!this->totalMemory)
    {
        int memory = *((int *)MEMORY_SIZE_ADDRESS);
        // ax寄存器保存的内容
        int low = memory & 0xffff;
        // bx寄存器保存的内容
        int high = (memory >> 16) & 0xffff;

        this->totalMemory = low * 1024 + high * 64 * 1024;
    }

    return this->totalMemory;
}

int MemoryManager::allocatePages(enum AddressPoolType type, const int count, const VPageFlags vFlag, UserSegment userSegment, bool reverse)
{
    // 第一步：从虚拟地址池中分配若干虚拟页
    int virtualAddress = allocateVirtualPages(type, count, vFlag, userSegment, reverse);

    if (!virtualAddress)
    {
        return 0;
    }

    bool flag;
    int physicalPageAddress;
    int vaddress = virtualAddress;

    // 依次为每一个虚拟页指定物理页
    for (int i = 0; i < count; ++i, vaddress += PAGE_SIZE)
    {
        flag = false;
        // 第二步：从物理地址池中分配一个物理页
        physicalPageAddress = allocatePhysicalPages(type, 1);
        if (physicalPageAddress)
        {
            // 第三步：为虚拟页建立页目录项和页表项，使虚拟页内的地址经过分页机制变换到物理页内。
            flag = connectPhysicalVirtualPage(vaddress, physicalPageAddress);
        }
        else
        {
            flag = false;
        }

        // 分配失败，释放前面已经分配的虚拟页和物理页表
        if (!flag)
        {
            // 前i个页表已经指定了物理页
            releasePages(type, virtualAddress, i, userSegment);
            // 剩余的页表未指定物理页
            releaseVirtualPages(type, virtualAddress + i * PAGE_SIZE, count - i, userSegment);
            return 0;
        }
    }
    return virtualAddress;
}

int MemoryManager::allocateVirtualPages(enum AddressPoolType type, const int count, const VPageFlags vFlag, UserSegment userSegment, bool reverse)
{
    int start = -1;

    if (type == AddressPoolType::KERNEL)
    {
        ASSERT(userSegment == UserSegment::EMPTY);
        start = kernelVirtual.allocate(count, vFlag);
    } 
    else if (type == AddressPoolType::USER) 
    {
        ASSERT(userSegment != UserSegment::EMPTY);
        start = programManager.running->userVirtual.allocate(userSegment, count, vFlag);
    }

    return (start == -1) ? 0 : start;
}

bool MemoryManager::connectPhysicalVirtualPage(const int virtualAddress, const int physicalPageAddress)
{
    // 计算虚拟地址对应的页目录项和页表项
    int *pde = (int *)toPDE(virtualAddress);
    int *pte = (int *)toPTE(virtualAddress);
    // 页目录项无对应的页表，先分配一个页表
    if(!(*pde & PDE_PRESENT)) 
    {
        // 从内核物理地址空间中分配一个页表
        int page = allocatePageTable();
        if (!page) {
            return false;
        } 

        // 判断地址归属
        AddressPoolType type;
        if (kernelVirtual.isValidAddr(virtualAddress)) {
            type = AddressPoolType::KERNEL;
        } else if (programManager.running->userVirtual.vaddr2Seg(virtualAddress) != UserSegment::EMPTY) {
            type = AddressPoolType::USER;
        } else {
            PANIC("Invalid Virtual Address\n");
        }

        // 使页目录项指向页表, 同时设置权限位
        switch (type) {
            case AddressPoolType::KERNEL:
                *pde = page | PDE_KERNEL;
                break;
            case AddressPoolType::USER:
                *pde = page | PDE_USER;
                break;
            default:
                PANIC("Invalid Type\n");
        }

        // 初始化页表
        char *pagePtr = (char *)(((int)pte) & 0xfffff000);
        memset(pagePtr, 0, PAGE_SIZE);
    }

    // 使页表项指向物理页
    int old = *pte;
    PTEFlags pteFlag;
    UserSegment userSegment;
    // 属于 Kernel VAPool
    if (memoryManager.kernelVirtual.isValidAddr(virtualAddress)) {
        pteFlag = vPageFlags2PTE(memoryManager.kernelVirtual.getVPageFlag(virtualAddress));
    // 属于 User VAPool
    } else if ((userSegment = (programManager.running->userVirtual.vaddr2Seg(virtualAddress))) != UserSegment::EMPTY) {
        pteFlag = vPageFlags2PTE(programManager.running->userVirtual.getVPageFlag(userSegment, virtualAddress));
    // 非法地址
    } else { 
        ASSERT(0);
    }
    // 修改PTE, 增加PDE count
    *pte = physicalPageAddress | pteFlag | PTE_PRESENT;
    PDEinc((uint32)pde);

    // 刷新TLB
    asm_invlpg((void*)virtualAddress);

    // 绑定rmap(注意存的是pte的PA)
    int owner = programManager.running ? programManager.running->pid : 0;
    uint32 pte_pa = toPTEpa(virtualAddress);
    ASSERT(rmapManager.attach(&pageinfos[PA2PGI(physicalPageAddress)], pte_pa, (uint32)pte, owner) != -1);
    // DEBUG:
    LOG_TRACE("bind VA=0x%x to PA=0x%x\n", virtualAddress, physicalPageAddress);
                pageinfos[PA2PGI(physicalPageAddress)].dump();
    return true;
}

// addr = 0x[ffc] : [cff] : [PDE Index * 4]
int MemoryManager::toPDE(const int virtualAddress)
{
    return (0xfffff000 + (((virtualAddress & 0xffc00000) >> 22) * 4));
}

// addr = 0x[ffc] : [PDE Index] : [PTE Index * 4]
int MemoryManager::toPTE(const int virtualAddress)
{
    return (0xffc00000 + ((virtualAddress & 0xffc00000) >> 10) + (((virtualAddress & 0x003ff000) >> 12) * 4));
}

int MemoryManager::toPTEpa(const int vaddr)
{
    uint32 *pde = (uint32 *)toPDE(vaddr);
    if (!(*pde & PTE_PRESENT)) return 0;
    uint32 pte_idx = (vaddr >> 12) & 0x3FF;
    return ((*pde) & PTE_GET_ADDRESS) + (pte_idx << 2);
}

void MemoryManager::releasePages(enum AddressPoolType type, const int virtualAddress, const int count, UserSegment userSegment)
{
    int vaddr = virtualAddress;
    int *pte, *pde;
    for (int i = 0; i < count; ++i, vaddr += PAGE_SIZE)
    {
        // 第一步，对每一个虚拟页，释放为其分配的物理页
        pde = (int *)toPDE(vaddr);
        // 不存在PDE, 跳过
        if (!((*pde)) & PDE_PRESENT) {
            continue;
        }
        pte = (int *)toPTE(vaddr);
        // 若不存在PTE, 跳过释放
        if (!((*pte) & PTE_PRESENT)) {
            *pte = 0;
            // 刷新TLB
            asm_invlpg((void*)vaddr);
            continue;
        } 
   
        int paddr = vaddr2paddr(vaddr);
        int owner = programManager.running ? programManager.running->pid : 0;
        // DEBUG:
        LOG_TRACE("try Free VA=0x%x, PA=0x%x\n", vaddr, paddr);
        pageinfos[PA2PGI(paddr)].dump();
        uint32 pte_pa = toPTEpa(vaddr);
        // 解除rmap绑定
        ASSERT(rmapManager.detach(&pageinfos[PA2PGI(paddr)], pte_pa, 0, owner));

        pageinfos[PA2PGI(paddr)].dump();
        if (pageinfos[PA2PGI(paddr)].getRef() == 0) {
            releasePhysicalPages(type, paddr, 1);
            // DEBUG:
            LOG_TRACE("Free VA=0x%x, PA=0x%x\n", vaddr, paddr);
        }

        // 设置页表项为不存在，防止释放后被再次使用
        *pte = 0;
        // 刷新TLB
        asm_invlpg((void*)vaddr);
        
        // 分析页表是否可以被释放, 如果释放记得减小ref
        uint32 pde = toPDE(vaddr);
        if (PDEdec(pde)) {
            releasePageTable(pde);
            *(uint32*)pde = 0;
            LOG_TRACE("release PageTable\n");
        }
    }

    // 第二步，释放虚拟页
    releaseVirtualPages(type, virtualAddress, count, userSegment);
}

int MemoryManager::vaddr2paddr(int vaddr)
{
    int *pte = (int *)toPTE(vaddr);
    int page = (*pte) & 0xfffff000;
    int offset = vaddr & 0xfff;
    return (page + offset);
}

void MemoryManager::releaseVirtualPages(enum AddressPoolType type, const int vaddr, const int count, UserSegment userSegment)
{
    if (type == AddressPoolType::KERNEL)
    {   
        ASSERT(userSegment == UserSegment::EMPTY);
        kernelVirtual.release(vaddr, count);
    }
    else if (type == AddressPoolType::USER)
    {
        ASSERT(userSegment != UserSegment::EMPTY);
        programManager.running->userVirtual.release(userSegment, vaddr, count);
    }
}

int MemoryManager::allocatePagesLazy(enum AddressPoolType type, const int count, const VPageFlags flag, UserSegment userSegment, bool reverse) {
    int start = allocateVirtualPages(type, count, flag, userSegment, reverse);
    if (!start) return 0;

    int vaddr = start;
    for (int i = 0; i < count; i++, vaddr += PAGE_SIZE) {
        // 计算虚拟地址对应的页目录项和页表项
        int *pde = (int *)toPDE(vaddr);
    
        int *pte = (int *)toPTE(vaddr);
        // 页目录项无对应的页表，先分配一个页表
        if(!(*pde & PDE_PRESENT)) 
        {
            // 从内核物理地址空间中分配一个页表
            int page = allocatePageTable();
            if (!page) {
                // 失败则回滚前面的
                releasePages(type, start, i, userSegment);
                releaseVirtualPages(type, start + i * PAGE_SIZE, count - i, userSegment);
            } 
            // 使页目录项指向页表, 同时设置权限位
            switch (type) {
                case AddressPoolType::KERNEL:
                    *pde = page | PDE_KERNEL;
                    break;
                case AddressPoolType::USER:
                    *pde = page | PDE_USER;
                    break;
                default:
                    PANIC("Invalid Type\n");
            }

            // 初始化页表
            char *pagePtr = (char *)(((int)pte) & 0xfffff000);
            memset(pagePtr, 0, PAGE_SIZE);
        }    
        *pte = PTE_LAZY | PTE_WRITABLE;
        PDEinc((uint32)pde);
    }
    return start;
}

VictimInfo MemoryManager::findVictim(enum AddressPoolType type) {
    VictimInfo victimInfo;
    if (type == AddressPoolType::KERNEL)
    {
        // 搜索全长度,3轮
        victimInfo = memoryManager.kernelPhysical.findVictim(0, 3);
    } else if (type == AddressPoolType::USER) {
        // 搜索一定页数,2轮
        victimInfo = memoryManager.kernelPhysical.findVictim(1000, 2);
    } else {
        victimInfo.paddr = 0;
        victimInfo.PTEptr = 0;
    }
    return victimInfo;
}

uint32 MemoryManager::mapTemp(enum AddressPoolType type, const int paddr) {
    ASSERT(type == AddressPoolType::KERNEL);
    uint32 base = paddr & ~0xFFF;
    uint32 offset = paddr & 0xFFF;

    uint32 vaddr = kernelVirtual.startAddress;
    if (!vaddr) return 0;

    uint32 *pde = (uint32 *)toPDE(vaddr);
    uint32 *pte = (uint32 *)toPTE(vaddr);
    // 页目录项无对应的页表，先分配一个页表
    if(!(*pde & PDE_PRESENT)) 
    {
        // 从内核物理地址空间中分配一个页表
        int page = allocatePageTable();
        if (!page)
            return 0;

        // 使页目录项指向页表, 同时设置权限位
        *pde = page | PDE_KERNEL;
        // 初始化页表
        char *pagePtr = (char *)(((int)pte) & 0xfffff000);
        memset(pagePtr, 0, PAGE_SIZE);
    }
    ASSERT((*pte) % PAGE_SIZE == 0);
    *pte = base | 0x7;
    asm_invlpg((void*)vaddr);
    return vaddr + offset;
}

void MemoryManager::unmapTemp(enum AddressPoolType type) {
    ASSERT(type == AddressPoolType::KERNEL);

    uint32 vaddr = kernelVirtual.startAddress; 
    int *pte = (int *)toPTE(vaddr);

    // 确认是合法的
    ASSERT((*pte) & PTE_PRESENT);
    ASSERT((*pte) & PTE_GET_ADDRESS);
    // 清除原有的TempMap
    *pte = 0x0;
    asm_invlpg((void*)vaddr);
}

bool MemoryManager::setCOW(PageInfo* pi) {
    bool res = rmapManager.setCOW(pi);
    LOG_TRACE("[setCOW] pgi=%u res=%d ref=%u extra=%u flags=0x%x\n",
           (unsigned) (pi - pageinfos), (int)res, pi->getRef(), pi->extra, (unsigned)pi->flags);
    return res;
}

int MemoryManager::allocatePageTable() {
    int page = allocatePhysicalPages(AddressPoolType::KERNEL, 1);
    if (!page) {
        return 0;
    } else {
        pageinfos[PA2PGI(page)].clear();
        pageinfos[PA2PGI(page)].incRef();
        pageinfos[PA2PGI(page)].setFlag(PG_KERNEL | PG_LOCKED);
        return page;
    }
}

int MemoryManager::allocatePageDirTable(uint32 owner) {
    int vaddr = allocatePages(AddressPoolType::KERNEL, 1, VP_RW);
    if (!vaddr) return 0;
    int page = vaddr2paddr(vaddr);
    // 将链接绑在自己的pid
    ASSERT(rmapManager.detach(&pageinfos[PA2PGI(page)], toPTEpa(vaddr), 0, programManager.running->pid));
    rmapManager.attach(&pageinfos[PA2PGI(page)], toPTEpa(vaddr), toPTE(vaddr), owner);
    pageinfos[PA2PGI(page)].setFlag(PG_KERNEL | PG_LOCKED);
    return vaddr;    
}

void MemoryManager::releasePageDirTable(uint32 pageDirAddr) {
    uint32 page = vaddr2paddr(pageDirAddr);
    ASSERT(pageinfos[PA2PGI(page)].hasFlag(PG_KERNEL));
    ASSERT(pageinfos[PA2PGI(page)].hasFlag(PG_LOCKED));
    ASSERT(pageinfos[PA2PGI(page)].getRef() == 1);
    pageinfos[PA2PGI(page)].clearFlag(PG_LOCKED);
    releasePages(AddressPoolType::KERNEL, pageDirAddr, 1);
    return;
}
void MemoryManager::releasePageTable(uint32 PDEptr) {
    uint32 page = (*(uint32*)PDEptr) & PDE_GET_ADDRESS;
    ASSERT(pageinfos[PA2PGI(page)].hasFlag(PG_KERNEL));
    ASSERT(pageinfos[PA2PGI(page)].hasFlag(PG_LOCKED));
    ASSERT(pageinfos[PA2PGI(page)].getRef() == 1);
    pageinfos[PA2PGI(page)].clear();
    pageinfos[PA2PGI(page)].setFlag(PG_FREE);
    return;
}

void MemoryManager::PDEinc(uint32 PDEptr) {
    int count = PDE_GET_COUNT(PDEptr);
    count++;
    if (count > 7) count = 7;
    PDE_SET_COUNT(PDEptr, count);
}

bool MemoryManager::PDEdec(uint32 PDEptr) {
    int count = PDE_GET_COUNT(PDEptr);
    ASSERT(count > 0);
    if (count == 7) return false;
    count--;
    PDE_SET_COUNT(PDEptr, count);
    if (count == 0) return true;
    else return false;
}

void MemoryManager::destroyUserVAPool(UserVAddressPool* userVirtual, uint32 pageDirVAddr) {
    // 释放页表和物理页
    releasePages(AddressPoolType::USER, userVirtual->segBoundary.text.start, 
                (userVirtual->segBoundary.text.end - userVirtual->segBoundary.text.start + 1) / PAGE_SIZE,
                UserSegment::TEXT);
    releasePages(AddressPoolType::USER, userVirtual->segBoundary.data.start, 
                (userVirtual->segBoundary.data.end - userVirtual->segBoundary.data.start + 1) / PAGE_SIZE,
                UserSegment::DATA);
    releasePages(AddressPoolType::USER, userVirtual->segBoundary.bss.start, 
                (userVirtual->segBoundary.bss.end - userVirtual->segBoundary.bss.start + 1) / PAGE_SIZE,
                UserSegment::BSS);      
                                                  
    releasePages(AddressPoolType::USER, userVirtual->heapPool.startAddress, 
                userVirtual->heapPool.length, UserSegment::HEAP);
    releasePages(AddressPoolType::USER, userVirtual->stackPool.startAddress, 
                userVirtual->stackPool.length, UserSegment::STACK);
    releasePages(AddressPoolType::USER, userVirtual->mmapPool.startAddress, 
                userVirtual->mmapPool.length, UserSegment::MMAP);    
    releasePages(AddressPoolType::USER, userVirtual->TLSPool.startAddress, 
                userVirtual->TLSPool.length, UserSegment::TLS);

    for (uint32 vaddr = 0; vaddr <= USER_VADDR_END; vaddr += PAGE_SIZE * PAGE_SIZE) {
        uint32 PDEptr = toPDE(vaddr);
        if (*(uint32*)PDEptr & PDE_PRESENT) {
            releasePageTable(PDEptr);
        }
    }
    // 切回内核页表
    asm_update_cr3(PAGE_DIRECTORY);
    
    // 释放页目录表
    releasePageDirTable(pageDirVAddr);
    
    // 释放BitMap
    userVirtual->destroy();
}