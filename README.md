//物理内存分配
===== 0x000000 =====
0x0 - 0x100000 内核预留

0x9E000      Kernel VirtualPool PDE[0]
// 0xA0000-0x100000 是只读的 千万别往里面放东西

MAX_TOTAL_NODES 32768     ; buddy.h

// 虚拟内存分配                   
                                     10      10           12
0xfffff000 - 0xffffffff PDE项映射表 [ffc][cff]      [PDE Index * 4]
0xffc00000 - 0xfffeffff PTE项映射表 [ffc][PDE Index][PTE Index * 4]
        10          10        12
VA = [PDE Index][PTE Index][bias]
PA =        [PAGE PA]      [bias]

// syscall

execve: execve->programManager::executeProcess(加入READY,提前分析elf,设置对应信息)
load_process: 真正将_start位置传入,预留ProcessStartStack用于调用asm_start_process

// 进程分配
USER_VADDR_START 0x8048000 
.text   
.data
.bss
heap    1G大小
TLS     MAX_THREAD_AMOUNT(16) * PAGE_SIZE大小
mmap    从TLS末尾直至STACK栈顶
stack   从STACK_TOP = 0xbfff0000 向下延伸 STACK_SIZE = 4MB (1024 PAGE)


// COW思路
对于新COW页，利用临时映射mapTemp()设置它的PTE项 (PTE_PRESENT | PTE_COW ) & ~PTE_RW | PAPageAddress
并且在对应的PA的PageInfo存储的Rmap，添加一个匿名项 (有owner, 无效的pte_addr)
然后对于原本指向它的页, 更新PTE项 PTE = (PTE | PTE_COW) & ~PTE_RW
(记得都要flush TLB)

然后handler触发时，做两轮遍历
第一轮精确匹配，要求PTE_ADDR, OWNER完全匹配，若找到，减小ref(除非ref = 0)，复制这个PA，同时把这个条目删去
第二轮模糊匹配，要求PTE_ADDR = 无效值, OWNER匹配，若找到，减小ref(除非ref = 0), 复制这个PA，同时把这个匿名条目删去
否则，触发error

记得不用函数测试的时候，关掉setCOW里面对PG_KERNEL的检测

/* PDE PTE:
   31-12   11     10     9     8     7     6      5     4     3     2     1     0
   ADDR   LAZY   SWAP   COW    G    PAT  Dirty Access  PCD   PWT   U/S   W/R Present

*/

// 进程初始化
AllocPCB -> initialize Para -> schedule Thread -> switch cr3 ->loadProcess -> Process
        (fork or executeProcess)

// DEBUG: 请找debug.h

// fileSys中断管理
Fat12_FS关中断:
flush, flush_fat_table, read_fat_table, flush_root_dir, destroy_root_dir, destroy_cache_pool
get_cache, put_cache, release_node, lookup

FileManager关中断:
mount, umount, open, close, create, remove, createDir, removeDir, lookup
可能: ls, cd?