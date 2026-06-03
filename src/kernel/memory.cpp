#include "memory.h"
#include "os_constant.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "stdio.h"
#include "program.h"
#include "os_modules.h"
#include "assert.h"

MemoryManager::MemoryManager()
{
    initialize();
}

void MemoryManager::initialize()
{
    // DEBUG:
    printf("CALL\n");
    this->totalMemory = 0;
    this->totalMemory = getTotalMemory();

    // 预留的内存
    // int usedMemory = 256 * PAGE_SIZE + 0x100000;
    int usedMemory = RESERVED_MEMORY;
    if (this->totalMemory < usedMemory)
    {
        printf("memory is too small, halt.\n");
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
        if(!(*pde & 0x00000001)) {
            int PDEidx = (kernelVirtualStartAddress >> 22) & 0x3ff;
            ASSERT(PDEidx >= 768 && PDEidx < 1024);
            int PTE_table_PAddr = (PTE_table_start + PAGE_SIZE * (PDEidx - 768));
            ASSERT(PTE_table_PAddr % PAGE_SIZE == 0);
            *pde = PTE_table_PAddr | 0x7;
            
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
    kernelPages = 1;
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

    printf("total memory: %d bytes ( %d MB )\n",
        this->totalMemory,
    this->totalMemory / 1024 / 1024);

    printf("kernel pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d ( %d MB )\n"
           "    bitmap start address: 0x%x\n",
           kernelPhysicalStartAddress,
           kernelPages, kernelPages * PAGE_SIZE / 1024 / 1024,
           kernelPhysicalBitMapStart);

    printf("user pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d ( %d MB )\n"
           "    bit map start address: 0x%x\n",
           userPhysicalStartAddress,
           userPages, userPages * PAGE_SIZE / 1024 / 1024,
           userPhysicalBitMapStart);

    printf("kernel virtual pool\n"
           "    start address: 0x%x\n"
           "    total pages: %d  ( %d MB ) \n"
           "    bit map start address: 0x%x\n"
           "    privileges start address: 0x%x\n",
           kernelVirtualStartAddress,
           userPages, kernelVirtualPages * PAGE_SIZE / 1024 / 1024,
           kernelVirtualBitMapStart,
           kernelVirtualPrivilegeStart);
    
    printf("Free Node List:\n"
            "   kernel start address: 0x%x\n"
            "   user start address: 0x%x\n",
            kernelPAFreeNodeListStart,
            userPAFreeNodeListStart);

    printf("Free Node Bit Map:\n"
            "   kernel start address: 0x%x\n"
            "   user start address: 0x%x\n",
            kernelPAFreeNodeBitMapStart,
            userPAFreeNodeBitMapStart);            
    
    printf("RMap\n"
            "   node list start address: 0x%x\n"
            "   total nodes: %d\n"
            "   bit map start address: 0x%x\n",
            RMapNodeListStart,
            RMapNodeListSize,
            RMapBitMapStart);
    
    printf("PageInfo\n"
            "   start address: 0x%x\n",
            (int)pageinfos);
    // printf("0x%x\n", vaddr2paddr(0xC02C0000));
    // printf("0x100000 %x ;", *(int*) toPDE(0x100000));
    // // 获取0x100000对应的PTE信息,储存在0x101000 + 4 * 256
    // printf("0x101400 %x\n", *(int*) toPTE(0x100000));

    // printf("Try to access 0x101000\n");
    // int value1 = *(int*) 0x101000;
    // printf("value = %d\n", value1);
    // *(int*) 0x101000 = 114514;
    // printf("after value = %d\n", *(int*) 0x101000);
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

    // printf("start: %d\n", start);
    if (start == -1) return 0;

    int pgi = PA2PGI(start);
    // printf("pgi: %d\n", pgi);
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

int MemoryManager::allocatePages(enum AddressPoolType type, const int count, const VPageFlags vFlag)
{
    // 第一步：从虚拟地址池中分配若干虚拟页
    int virtualAddress = allocateVirtualPages(type, count, vFlag);

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
            //printf("allocate physical page 0x%x\n", physicalPageAddress);

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
            releasePages(type, virtualAddress, i);
            // 剩余的页表未指定物理页
            releaseVirtualPages(type, virtualAddress + i * PAGE_SIZE, count - i);
            return 0;
        }
    }
    return virtualAddress;
}

int MemoryManager::allocateVirtualPages(enum AddressPoolType type, const int count, const VPageFlags vFlag)
{
    int start = -1;

    if (type == AddressPoolType::KERNEL)
    {
        start = kernelVirtual.allocate(count, vFlag);
    }

    return (start == -1) ? 0 : start;
}

bool MemoryManager::connectPhysicalVirtualPage(const int virtualAddress, const int physicalPageAddress)
{
    // 计算虚拟地址对应的页目录项和页表项
    int *pde = (int *)toPDE(virtualAddress);
    // if (virtualAddress >= 0x100000 && virtualAddress < 0x200000) {
    //     printf("[MAP-BEFORE] v=0x%x pde@0x%x=0x%x pte@0x%x=0x%x new_pa=0x%x\n",
    //            virtualAddress, pde, *pde, pte, *pte, physicalPageAddress);
    // }
    int *pte = (int *)toPTE(virtualAddress);
    // 页目录项无对应的页表，先分配一个页表
    if(!(*pde & 0x00000001)) 
    {
        // 从内核物理地址空间中分配一个页表
        int page = allocatePhysicalPages(AddressPoolType::KERNEL, 1);
        if (!page)
            return false;
        else {
            pageinfos[PA2PGI(page)].clear();
            pageinfos[PA2PGI(page)].incRef();
            pageinfos[PA2PGI(page)].setFlag(PG_KERNEL | PG_LOCKED);
        }
        // 使页目录项指向页表, 同时设置权限位

        *pde = page | 0x7;
        // 初始化页表
        char *pagePtr = (char *)(((int)pte) & 0xfffff000);
        memset(pagePtr, 0, PAGE_SIZE);
    }

    // 使页表项指向物理页
    int old = *pte;
    PTEFlags pteFlag;
    // 属于 Kernel VAPool
    if (memoryManager.kernelVirtual.isValidAddr(virtualAddress)) {
        pteFlag = vPageFlags2PTE(memoryManager.kernelVirtual.getVPageFlag(virtualAddress));
    // TODO: User VAPool
    } else if (1) {
        asm_halt();
    // 非法地址
    } else { 
        ASSERT(0);
    }
    *pte = physicalPageAddress | pteFlag | PTE_PRESENT;

    // if (virtualAddress >= 0x100000 && virtualAddress < 0x200000) {
    //     printf("[MAP-AFTER ] v=0x%x pte old=0x%x new=0x%x\n", virtualAddress, old, *pte);
    // }

    // 绑定rmap
    int owner = programManager.running ? programManager.running->pid : 0;
    rmapManager.attach(&pageinfos[PA2PGI(physicalPageAddress)], (uint32)pte, owner);
    // DEBUG:
    printf("bind VA=0x%x to PA=0x%x\n", virtualAddress, physicalPageAddress);
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

void MemoryManager::releasePages(enum AddressPoolType type, const int virtualAddress, const int count)
{
    int vaddr = virtualAddress;
    int *pte;
    for (int i = 0; i < count; ++i, vaddr += PAGE_SIZE)
    {
        // 第一步，对每一个虚拟页，释放为其分配的物理页
        pte = (int *)toPTE(vaddr);
        // 若不存在PTE, 跳过释放
        if (!((*pte) & PTE_PRESENT)) {
            *pte = 0;
            continue;
        } 
   
        int paddr = vaddr2paddr(vaddr);
        int owner = programManager.running ? programManager.running->pid : 0;
        // DEBUG:
        printf("try Free VA=0x%x, PA=0x%x\n", vaddr, paddr);
        pageinfos[PA2PGI(paddr)].dump();
        rmapManager.detach(&pageinfos[PA2PGI(paddr)], (uint32)pte, owner);
        pageinfos[PA2PGI(paddr)].dump();
        if (pageinfos[PA2PGI(paddr)].getRef() == 0) {
            releasePhysicalPages(type, paddr, 1);
            // DEBUG:
            printf("Free VA=0x%x, PA=0x%x\n", vaddr, paddr);
        }

        // 设置页表项为不存在，防止释放后被再次使用
        *pte = 0;

        // TODO: 分析页表是否可以被释放, 如果释放记得减小ref
    }

    // 第二步，释放虚拟页
    releaseVirtualPages(type, virtualAddress, count);
}

int MemoryManager::vaddr2paddr(int vaddr)
{
    int *pte = (int *)toPTE(vaddr);
    int page = (*pte) & 0xfffff000;
    int offset = vaddr & 0xfff;
    return (page + offset);
}

void MemoryManager::releaseVirtualPages(enum AddressPoolType type, const int vaddr, const int count)
{
    if (type == AddressPoolType::KERNEL)
    {
        kernelVirtual.release(vaddr, count);
    }
}

int MemoryManager::allocatePagesLazy(enum AddressPoolType type, const VPageFlags flag) {
    int vaddr = allocateVirtualPages(type, 1, flag);
    if (vaddr == -1) return -1;

    // 计算虚拟地址对应的页目录项和页表项
    int *pde = (int *)toPDE(vaddr);

    int *pte = (int *)toPTE(vaddr);
    // 页目录项无对应的页表，先分配一个页表
    if(!(*pde & 0x00000001)) 
    {
        // 从内核物理地址空间中分配一个页表
        int page = allocatePhysicalPages(AddressPoolType::KERNEL, 1);
        if (!page)
            return false;
        else {
            pageinfos[PA2PGI(page)].clear();
            pageinfos[PA2PGI(page)].incRef();
            pageinfos[PA2PGI(page)].setFlag(PG_KERNEL | PG_LOCKED);
        }
        // 使页目录项指向页表, 同时设置权限位

        *pde = page | 0x7;
        // 初始化页表
        char *pagePtr = (char *)(((int)pte) & 0xfffff000);
        memset(pagePtr, 0, PAGE_SIZE);
    }    
    *pte = PTE_LAZY | PTE_WRITABLE;
    return vaddr;
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