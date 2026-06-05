#include "syscall.h"
#include "interrupt.h"
#include "stdlib.h"
#include "asm_utils.h"
#include "os_modules.h"
#include "os_constant.h"
#include "stdio.h"
#include "thread.h"

int system_call_table[MAX_SYSTEM_CALL];

void SystemService::initialize()
{
    memset((char *)system_call_table, 0, sizeof(int) * MAX_SYSTEM_CALL);
    // 代码段选择子默认是DPL=0的平坦模式代码段选择子，DPL=3，否则用户态程序无法使用该中断描述符
    interruptManager.setInterruptDescriptor(0x80, (uint32)asm_system_call_handler, 3);
    setSystemCall(0, (int)syscall_0);
    setSystemCall(1, (int)sys_write);
    setSystemCall(2, (int)sys_dump_pte);
}

bool SystemService::setSystemCall(int index, int function)
{
    system_call_table[index] = function;
    return true;
}


int sys_write(int fd, const char* buf, int len)
{
    (void)fd; // 目前只支持 stdout，忽略 fd

    if (!buf || len <= 0) return -1;

    for (int i = 0; i < len; ++i) {
        stdio.print(buf[i]);  // 直接复用内核的输出
    }
    return len;
}

int syscall_0(int first, int second, int third, int forth, int fifth)
{
    printf("systerm call 0: %d, %d, %d, %d, %d\n",
           first, second, third, forth, fifth);
    return first + second + third + forth + fifth;
}

int sys_dump_pte(uint32 vaddr)
{
    PCB* cur = programManager.running;
    if (!cur) return -1;

    // 计算 PDE 指针（通过内核映射）
    uint32* pde = (uint32*)memoryManager.toPDE(vaddr);
    uint32 pde_val = *pde;

    printf("[sys_dump_pte] pid=%d vaddr=0x%x PDE=0x%x\n",
           cur->pid, vaddr, pde_val);

    if (!(pde_val & PTE_PRESENT)) {
        printf("  PDE not present.\n");
        return 0;
    }

    // 透过 PDE 得到页表物理地址
    uint32 pte_pa_base = pde_val & PTE_GET_ADDRESS;
    // 临时映射这张页表
    uint32 tmp = memoryManager.mapTemp(AddressPoolType::KERNEL, pte_pa_base);
    if (!tmp) {
        printf("  mapTemp failed.\n");
        return -1;
    }
    uint32 pte_idx = (vaddr >> 12) & 0x3FF;
    uint32* pte = (uint32*)(tmp + (pte_idx << 2));

    printf("  PTE[%u]=0x%x\n", pte_idx, *pte);
    uint32 paddr = *pte & PTE_GET_ADDRESS;
    memoryManager.pageinfos[PA2PGI(paddr)].dump();
    memoryManager.unmapTemp(AddressPoolType::KERNEL);
    return 0;
}