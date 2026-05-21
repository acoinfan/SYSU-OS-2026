#include "buddy.h"
#include <stdlib.h>
#include <string.h>


void print_bitmap(char *bitmap, int count)
{
    for (int i = 0; i < count; i++)
    {
        int bit = (bitmap[i / 8] >> (i % 8)) & 1;
        printf("%d", bit);
        if ((i + 1) % 8 == 0)
            printf(" ");
    }
    printf("\n");
}

void print_freeNodes(Buddy* buddy)
{
    for (int i = 0; i < MAX_ORDER + 1; i++)
    {
        FreeNode *cnt = buddy->freeArea[i];
        printf("order %d: ", i);
        while (cnt)
        {
            printf("idx = %d -> ", cnt->startIndex);
            cnt = cnt->next;
        }
        printf("NULL\n");
    }
}

void test()
{

    // 4MB = 2 ^ 22B 内存, 采用小页 4KB = 2 ^ 12B, 共2 ^ 10个页
    char *bitmap = (char *)malloc(1 << 19);
    memset(bitmap, 0, 1 << 19);
    bitmap[0] = 0b00010011; bitmap[1] = 0x34; bitmap[2] = 0x6D;
    buddy.initialize(bitmap, 1 << 10);

    print_bitmap(bitmap, 48);
    print_freeNodes();
    printf("=== Buddy Manager Test ===\n");

    // 4
    int a1 = buddy.allocate(4);
    printf("Allocated 4 pages at index: %d\n", a1);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 8
    int a2 = buddy.allocate(8);
    printf("Allocated 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 8
    int a3 = buddy.allocate(8);
    printf("Allocated 8 pages at index: %d\n", a3);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 2
    int a4 = buddy.allocate(2);
    printf("Allocated 2 pages at index: %d\n", a4);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // r前8
    buddy.release(a2, 8);
    printf("Released 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // r后8  理论此时合并
    // error
    buddy.release(a3, 8);
    printf("Released 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 8
    a2 = buddy.allocate(8);
    printf("Allocated 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();
    // exit(0);

    // 8
    a3 = buddy.allocate(8);
    printf("Allocated 8 pages at index: %d\n", a3);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // r后8  理论此时合并
    buddy.release(a3, 8);
    printf("Released 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // r前8
    buddy.release(a2, 8);
    printf("Released 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 8
    a2 = buddy.allocate(8);
    printf("Allocated 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 8
    a3 = buddy.allocate(8);
    printf("Allocated 8 pages at index: %d\n", a3);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 1
    int a5 = buddy.allocate(1);
    printf("Allocated 1 pages at index: %d\n", a5);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 2 理论需要再拆
    int a6 = buddy.allocate(2);
    printf("Allocated 2 pages at index: %d\n", a6);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 1 利用上一个2的剩余
    int a7 = buddy.allocate(1);
    printf("Allocated 1 pages at index: %d\n", a7);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 尝试连锁释放 前8->4->前2->前1 (此时会得到一个16)
    buddy.release(a2, 8);
    printf("Released 8 pages at index: %d\n", a2);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    buddy.release(a1, 4);
    printf("Released 4 pages at index: %d\n", a1);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    buddy.release(a4, 2);
    printf("Released 2 pages at index: %d\n", a4);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    buddy.release(a5, 1);
    printf("Released 1 pages at index: %d\n", a5);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 获取超出大小的
    int e = buddy.allocate(1025);
    printf("should be -1, e = %d\n", e);
    print_bitmap(bitmap, 48);

    // 获取空间不足的
    int f = buddy.allocate(1024);
    printf("should be -1, f = %d\n", f);
    print_bitmap(bitmap, 48);

    // 释放余下的
    buddy.release(a3, 8);
    printf("Released 8 pages at index: %d\n", a3);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    buddy.release(a6, 2);
    printf("Released 2 pages at index: %d\n", a6);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    buddy.release(a7, 1);
    printf("Released 1 pages at index: %d\n", a7);
    print_bitmap(bitmap, 48);
    print_freeNodes();

    // 刚好要个最大
    // 获取空间不足的
    int g = buddy.allocate(1024);
    printf("Allocated 1024 pages at index: %d\n", g);
    print_bitmap(bitmap, 48);
    print_freeNodes();
}
int main()
{
    test();
}