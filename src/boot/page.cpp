#include "os_constant.h"
#include "stdlib.h"
#include "asm_utils.h"

extern "C" void open_page_mechanism()
{
    // 页目录表指针
    int *directory = (int *)PAGE_DIRECTORY;
    // 线性地址0~4MB对应的页表
    // page = 0x101000, 0x101000开始至0x102000
    int *page = (int *)(PAGE_DIRECTORY + PAGE_SIZE);
    // int *low_page = (int *)KERNEL_VA_POOL_PDE0;

    // 初始化页目录表
    memset(directory, 0, PAGE_SIZE);
    // 初始化线性地址0~4MB对应的页表
    memset(page, 0, PAGE_SIZE);
    // 初始化PDE[0]
    // memset(low_page, 0, PAGE_SIZE);
    int address = 0;
    // 将线性地址0~1MB恒等映射到物理地址0~1MB
    for (int i = 0; i < 256; ++i)
    {
        // U/S = 1, R/W = 1, P = 1
        page[i] = address | 0x7;
        // low_page[i] = address | 0x7;
        address += PAGE_SIZE;
    }

    // 1~2MB 分别处理
    // address = 0x100000;
    // for (int i = 256; i < 512; ++i)
    // {
    //     // U/S = 1, R/W = 0, P = 1
    //     low_page[i] = address | 0b111;   // 我们认为直接映射可以读写修改
    //     page[i] = address | 0b101;       // 高空间映射只读
    //     address += PAGE_SIZE;
    // }

    // 将线性地址2~4MB恒等映射到物理地址2~4MB
    // address = 0x200000;
    // for (int i = 512; i < 1024; ++i)
    // {
    //     // U/S = 1, R/W = 1, P = 1
    //     low_page[i] = address | 0x7;
    //     page[i] = address | 0x7;
    //     address += PAGE_SIZE;
    // }
    // 初始化页目录项

    // 0~4MB
    directory[0] = ((int)page) | 0x07;
    // 3GB的内核空间
    directory[768] = ((int)page) | 0x07;
    // 最后一个页目录项指向页目录表
    directory[1023] = ((int)directory) | 0x7;

    // 769~1022项
    for (int i = 1; i < 255; i++) {
        int* pde = (int*)(PAGE_DIRECTORY + PAGE_SIZE + i * PAGE_SIZE);
        directory[768 + i] = (int)pde | 0x7; 
        memset(pde, 0, PAGE_SIZE);  
    }
}