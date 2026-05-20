内存分配
0x0 - 0x100000 内核预留
其中包括:
0xA0000 Kernel PhysicalPool Bitmap (1M bit -> 1M页 -> 4GB)
0xC0000 User   PhysicalPool Bitmap (1M bit -> 1M页)
0xE0000 Kernel VirtualPool  Bitmap (1M bit -> 1M页)

0x100000 - 0x200000 PTE PDE
其中包括:
0x100000 - 0x101000 PTE表 
0x101000 - 0x200000 PDE idx768至1022项 (PTE 1023项指向0x100000 PTE表起始点)

0x200000 - 0x300000 FreeNodeList for Kernel PhysicalPool  最大可管理物理池: 1MB / sizeof(Node) * 4KB = 43690 Nodes * 4KB = 170MB
0x300000 - 0x400000 FreeNodeList for User PhysicalPool                             ↑ 24

