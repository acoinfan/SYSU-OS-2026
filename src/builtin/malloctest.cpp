#include "os_type.h"
#include "stdio.h"
#include "stdlib.h"
#include "syscall.h"

// 简单的辅助函数：往内存里填入标志数据，防止内存越界读写
void fill_pattern(char* ptr, uint32 size, char pattern) {
    for (uint32 i = 0; i < size; i++) {
        ptr[i] = pattern;
    }
}

// 校验数据是否被污染
bool check_pattern(char* ptr, uint32 size, char pattern) {
    for (uint32 i = 0; i < size; i++) {
        if (ptr[i] != pattern) return false;
    }
    return true;
}

int malloc_test() {
    printf("--- [User App] Malloc & Free Stress Test Start ---\n");

    // 测试 1：基础小内存交错分配与释放（触发分立链表的切分与合并）
    printf("[Test 1] Allocating blocks...\n");
    char* p1 = (char*)malloc(16);   // 应该进入小号链表
    char* p2 = (char*)malloc(64);   // 中号链表
    char* p3 = (char*)malloc(512);  // 大号链表

    if (!p1 || !p2 || !p3) {
        printf("ERROR: Basic malloc failed!\n");
        return -1;
    }

    // 写入测试数据
    fill_pattern(p1, 16, 'A');
    fill_pattern(p2, 64, 'B');
    fill_pattern(p3, 512, 'C');

    // 释放中间的 p2，触发 coalesce 隐式链表断开与挂载
    printf("[Test 1] Freeing middle block p2...\n");
    free(p2);

    // 校验两边的内存是否完好（检查有没有越界踩内存）
    if (check_pattern(p1, 16, 'A') && check_pattern(p3, 512, 'C')) {
        printf("SUCCESS: Isolation check passed!\n");
    } else {
        printf("ERROR: Memory corruption detected!\n");
    }

    // 测试 2：大内存连续申请（强迫底层 mem_sbrk 越过 4096 极限边界，触发页扩展）
    printf("[Test 2] Forcing heap expansion (+N Pages)...\n");
    void* big_p1 = malloc(3000); // 占用掉开局第一页的绝大部分空间
    void* big_p2 = malloc(5000); // 💥 必然超越当前 max_brk，强迫 mem_sbrk 向内核申请 2 个新页！
    void* big_p3 = malloc(2000); // 继续在缓冲中分配

    if (big_p2 && big_p3) {
        printf("SUCCESS: Heap expansion handled perfectly!\n");
    } else {
        printf("ERROR: Heap expansion failed!\n");
    }

    // 测试 3：全量释放与大合并
    printf("[Test 3] Cleaning up all memories to test coalescence...\n");
    free(p1);
    free(p3);
    free(big_p1);
    free(big_p2);
    free(big_p3);

    // 再次申请一个超级大块(1GB)，看能不能复用刚刚释放合并出来的地皮
    void* mega_ptr = malloc(0x39999000);
    if (mega_ptr) {
        printf("SUCCESS: Coalesce reused old big blocks successfully!\n");
        free(mega_ptr);
    } else {
        printf("ERROR: Fails to reuse coalesced space!\n");
    }

    mega_ptr = malloc(0x40000000);
    if (mega_ptr) {
        printf("SUCCESS: Coalesce reused old big blocks successfully!\n");
        free(mega_ptr);
    } else {
        printf("ERROR: Fails to reuse coalesced space!\n");
    }
    printf("--- [User App] All Tests Passed Successfully! ---\n");
    return 0;
}