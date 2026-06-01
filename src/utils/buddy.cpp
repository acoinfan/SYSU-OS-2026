#include "buddy.h"

// num <= 0 返回-1
int log2(int num) {
    if (num <= 0) return -1;
    int res = 0;
    while (num > 1) {
        num >>= 1;
        res++;
    }
    return res;
}

void FreeNode::release() {
    if (prev) {
        prev->next = next;
    }
    if (next) {
        next->prev = prev;
    }
}

FreeNode* FreeNode::getTail() {
    FreeNode* cnt = this;
    while (cnt->next) {
        cnt = cnt->next;
    }
    return cnt;
}

int Buddy::insert_freeNode(int startIndex, int order) {
    FreeNode* node = malloc_freeNode();
    if (!node) {
        printf("Error: fail to insert freeNode to order %d\n", order);
        return -1;
    }
    // 插入新freeNode节点
    // DEBUG:
    // printf("insert idx = %d, order = %d\n", startIndex, order);
    node->startIndex = startIndex;
    node->order = order;
    node->next = nullptr;
    node->prev = nullptr;
    if (freeArea[order]) {
        // 该链表头不为空, 头插
        node->next = freeArea[order];
        // 害死我了!!!
        node->next->prev = node;
        freeArea[order] = node;
    } else {
        freeArea[order] = node;
    }   
    return 0;
}

void Buddy::remove_freeNode(FreeNode* node) {
    // 头要额外处理
    if (node == freeArea[node->order]) {
        freeArea[node->order] = node->next;
    }
    free_freeNode(node);
}

// void Buddy::initialize(char* _bitmap, int _totalPages) {
//     bitmap = _bitmap;
//     totalPages = _totalPages;

//     // ceil(totalPages, 8);
//     // 所有页对应的BitMap大小
//     bytes = (totalPages + 7) >> 3;

//     for (int i = 0; i < bytes; ++i) {
//         bitmap[i] = 0;
//         freeBitmap[i] = 0;
//     }   

//     for (int i = 0; i < MAX_ORDER + 1; i++) {
//         freeArea[i] = nullptr;
//     }
//     int start_idx = 0;

//     while (_totalPages > 0) {
//         int exp = log2(_totalPages);
//         // 最大块order与最大块对应块数
//         int biggest_area_order = min(exp, MAX_ORDER);
//         int biggest_area_size = pow2(biggest_area_order);
        
//         insert_freeNode(start_idx, biggest_area_order);

//         _totalPages = _totalPages - biggest_area_size;
//         start_idx += biggest_area_size;
//     }

// }

void Buddy::initialize(char* _bitmap, int _totalPages, char* _freeBitMap, FreeNode* _freeNodes) {
    bitmap = _bitmap;
    totalPages = _totalPages;
    freeBitmap = _freeBitMap;
    freeNodes = _freeNodes;

    // Warning: 这里不保证对BitMap FreeBitMap FreeNodes的清空

    // ceil(totalPages, 8);
    bytes = (totalPages + 7) >> 3;
    int idx = 0, order = 0;
    while (idx < totalPages) {
        if (GET_MAP(bitmap, idx)) {
            // 被占用
            idx++;
            continue;
        }

        // 以此为起点，开始探测最大order
        int startIdx = idx, length = 0;
        int max_order = 0;
        while (((startIdx & (1 << max_order)) == 0) && max_order < MAX_ORDER) {
            max_order++;
        }


        while (!GET_MAP(bitmap, idx) && idx - startIdx + 1 <= pow2(max_order) && idx < totalPages) {
            idx++;
        }
        idx--;
        length = idx - startIdx + 1;
        order = log2(length);

        releaseBlock(startIdx, order);
        idx = startIdx + pow2(order);
    }

}

int Buddy::allocate(const int count) {
    // eg. allocate(3) = allocBlock(log2(3-1) + 1) = allocBlock(2);
    //     allocate(4) = allocBlock(log2(4-1) + 1) = allocBlock(3);
    // #define count2order(count) (log2((count) - 1) + 1)
    int order = count2order(count);
    int startIndex = allocateBlock(order);
    // 设置bitMap = 1
    if (startIndex != -1) {
        for (int i = 0; i < pow2(order); ++i) {
            SET_MAP(bitmap, startIndex + i, 1);
        }        
    }
    return startIndex;
}

void Buddy::release(const int index, const int count) {
    // 设置bitMap = 0
    releaseBlock(index, count2order(count));
    for (int i = 0; i < pow2(count2order(count)); i++) {
        SET_MAP(bitmap, index + i, 0);
    }
}

int Buddy::find_avail_freeNode(){
    int boundary = totalPages;
    for (int i = 0; i < boundary; i++) {
        if (!GET_MAP(freeBitmap, i)) {
            return i;
        }
    }
    return -1;
}

FreeNode* Buddy::malloc_freeNode() {
    int idx = find_avail_freeNode();
    if (idx == -1) return nullptr;
    SET_MAP(freeBitmap, idx, 1);
    return &freeNodes[idx];
}

void Buddy::free_freeNode(FreeNode* node) {
    int idx = node - freeNodes;
    SET_MAP(freeBitmap, idx, 0);
    node->release();
}

int Buddy::allocateBlock(int order) {
    if (order > MAX_ORDER) return -1;
    if (freeArea[order]) {
        // 找到了对应大小的
        int idx = freeArea[order]->startIndex;
        remove_freeNode(freeArea[order]);
        return idx;
    } else {
        // 考虑递归
        int idx = allocateBlock(order + 1);
        if (idx == -1) return -1;
        
        // 后面的加入链表，前面的分配给需要者
        int idx_back = idx + pow2(order);
        insert_freeNode(idx_back, order);
        return idx;
    }
}

FreeNode* Buddy::findNode(int index, int order) {
    FreeNode* cnt = freeArea[order];
    while (cnt) {
        if (cnt->startIndex == index) return cnt;
        cnt = cnt->next;
    }
    return nullptr;
}

void Buddy::releaseBlock(int startIndex, int order) {
    while (order < MAX_ORDER + 1) {
        int buddyIndex = startIndex ^ (1 << order);
        FreeNode* buddyNode = nullptr;
        // 若伙伴未被分配
        // 快速判断首块是否被用 && 是否存在该空块
        if (GET_MAP(bitmap, buddyIndex) == 0 && (buddyNode = findNode(buddyIndex, order))) {
            remove_freeNode(buddyNode);
            startIndex = min(startIndex, buddyIndex);
            ++order;
        } else {
            break;
        }
    }
    // 确认完成
    insert_freeNode(startIndex, min(order, MAX_ORDER));
}


// for debug
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