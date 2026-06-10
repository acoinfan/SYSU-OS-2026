#ifndef BUDDY_H
#define BUDDY_H

// 最大的Buddy节点可以包含的块数为 2 ^ (MAX_ORDER) = 1024 即 4MB
#define MAX_ORDER 10

#define GET_MAP(MAP, idx) \
    ((MAP[((idx) / 8)]) & (1 << ((idx) % 8)))

#define SET_MAP(MAP, idx, state) \
    (MAP[(idx) / 8] = (MAP[(idx) / 8] & ~(1 << ((idx) % 8))) | ((state) << ((idx) % 8)))

#define pow2(exp) (1 << (exp))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define count2order(count) (log2((count) - 1) + 1)

#include "screen.h"
#include "os_constant.h"
#include "stdlib.h"

int log2(int num);

struct FreeNode {
    int startIndex;  // 空闲块起始 bitmap index
    int order;       // 块大小 = 2^order 页
    FreeNode* next;
    FreeNode* prev;

    void release();
    FreeNode* getTail();
};

class Buddy {
public:
    void initialize(char* _bitmap, int _totalPages, char* _freeBitMap, FreeNode* _freeNodes);

    // 分配 count 页（必须是 2^n）
    int allocate(const int count); // 返回 bitmap index（交给 AddrPool 转地址）

    // 释放 count 页，从 index 开始
    void release(const int index, const int count);

public:
    // 1为被占用 0为未占用
    char* freeBitmap; // 管理freeNodes的BitMap, count = MAX_TOTAL_NODES / 8 + 1
    FreeNode* freeNodes;  // 静态数组, count = MAX_TOTAL_NODES
    FreeNode* freeArea[MAX_ORDER + 1];         // 每个 order 的链表头
    char* bitmap;                // 标记每页是否被占用
    int totalPages, bytes;

    int allocateBlock(int order);          // 按 order 分配
    void releaseBlock(int startIndex, int order); // 释放并合并
    FreeNode* findNode(int index, int order);     // 找到对应index和order的节点

    int find_avail_freeNode();  // 不允许调用
    FreeNode* malloc_freeNode();// 不允许调用
    void free_freeNode(FreeNode* node);// 不允许调用

    // -1 for fail
    int insert_freeNode(int startIndex, int order);
    void remove_freeNode(FreeNode* node);
    bool isAlloc(uint32 idx);
};

// for debug
void print_bitmap(char *bitmap, int count);
void print_freeNodes(Buddy* buddy);

#endif