//物理内存分配
===== 0x000000 =====
0x0 - 0x100000 内核预留

0x9E000      Kernel VirtualPool PDE[0]
// 0xA0000-0x100000 是只读的 千万别往里面放东西

```c
    int startaddr = 0x9FFFC;
    // 0 -1 -1
    printf("%d %d %d\n", *(int*)(startaddr+0), *(int*)(startaddr+4), *(int*)(startaddr+8));
    *((int*) (startaddr + 0)) = 114514;
    *((int*) (startaddr + 4)) = 114514;
    *((int*) (startaddr + 8)) = 114514;
    // 114514 -1 -1
    printf("%d %d %d\n", *(int*)(startaddr+0), *(int*)(startaddr+4), *(int*)(startaddr+8));
```
===== 0x100000 =====

0x100000 - 0x200000 PTE PDE
其中包括:
0x100000 - 0x101000 PTE表 
0x101000 - 0x200000 PDE idx768至1022项 (PTE 1023项指向0x100000 PTE表起始点)

===== 0x200000 =====

0x200000 - 0x2C0000 FreeNodeList for Kernel PhysicalPool (大小为768KB)
最大可管理物理池大小: 768KB / sizeof(Node) * 4KB = 48K Nodes * 4 KB = 192MB
                             ↑ 16B
0x2C0000 - 0x2C8000 Kernel PhysicalPool Bitmap
最大可管理物理内存: 32KB / 1bit * 4KB = 1GB

0x2C8000 - 0x2CA000 Kernel PhysicalPool Free Bitmap
最大可管理节点总数: 8KB / 1bit = 64K Nodes > 48K

0x2CA000 - 0x2CC000 User PhysicalPool Free Bitmap
最大可管理节点总数: 8KB / 1bit = 64K Nodes > 48K

0x2D0000 - 0x2D8000 User PhysicalPool Bitmap
最大可管理物理内存: 32KB / 1bit * 4KB = 1GB        (256K Pages)

0x2E0000 - 0x300000 Kernel VirtualPool Bitmap
最大可管理虚拟内存: 128KB / 1bit * 4KB = 4GB

===== 0x300000 =====

0x300000 - 0x3C0000 FreeNodeList for User PhysicalPool (大小为768KB)
最大可管理物理池大小: 768KB / sizeof(Node) * 4KB = 48K Nodes * 4 KB = 192MB                            

0x3C0000 - 0x400000 Page Info (256KB)
最大可管理总页面: 256KB / sizeof(PageInfo) = 512K Pages?
                            ↑ 4B

MAX_TOTAL_NODES 32768     ; buddy.h

// 虚拟内存分配                   
                                     10      10           12
0xfffff000 - 0xffffffff PDE项映射表 [ffc][cff]      [PDE Index * 4]
0xffc00000 - 0xfffeffff PTE项映射表 [ffc][PDE Index][PTE Index * 4]
        10          10        12
VA = [PDE Index][PTE Index][bias]
PA =        [PAGE PA]      [bias]