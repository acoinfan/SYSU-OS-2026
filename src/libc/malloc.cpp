#include "os_type.h"
#include "syscall.h"
#include "stdlib.h"
#include "malloc.h"
#include "stdio.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(uint32)))

#define INITIALIZE_SIZE 1024
#define MAX_INCR 2048
static char* brk_base = nullptr;
static char* table_base = nullptr;

#define MAX_GROUP 20 // payload: 16-24, 24-40, ... // blksize: 24-32, 32-48, ...
#define WSIZE 4
#define DSIZE 8
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MININUM_BLK_SIZE 24
// Internal Fragmentation Size 16 (for alloc blk, with 4 for header, 4 for footer, 8 for ptr)
#define INTER_FRAG_SIZE 16 

// 0 for free, 1 for alloc
#define PACK(size, alloc, prev_alloc) ((size) | (alloc) | ((prev_alloc) << 1))
#define GET(p) (*(unsigned *) (p))
#define PUT(p, val) (*(unsigned *) (p) = (val))
#define PUT_PTR(p, ptr) (*(char **) (p) = (ptr))

#define GET_SIZE(p) (GET(p) & ~0x7u)
#define GET_ALLOC(p) (GET(p) & 0x1u)
#define GET_PREVALLOC(p) ((GET(p) & 0x2u) >> 1)
#define GET_PTR(p) (*(char**) (p))

#define SET_ALLOC(p, alloc) PUT(p, ((GET(p) & ~0x1u) | ((alloc) & 0x1u)))
#define SET_PREVALLOC(p, prev_alloc) PUT(p, ((GET(p) & ~0x2u) | (((prev_alloc) << 1) & 0x2u)))

#define HDRP(bp) ((char *)(bp) - WSIZE)                         // block ptr 对应的 header ptr
#define FTRP(bp) ((char *)(bp) - DSIZE + GET_SIZE(HDRP(bp)))    // block ptr 对应的 footer ptr
#define PREV_FTRP(bp) ((char *) (bp) - DSIZE)                   // block ptr 的上一个块的 footer ptr

#define NEXT_BLKP(p) ((char *)(p) + GET_SIZE(HDRP(p)))          // block ptr 的下一个块的 block ptr
#define PREV_BLKP(p) ((char *)(p) - GET_SIZE((char *) (p) - DSIZE)) // block ptr 的上一个块的 block ptr

#define NEXT_FREEBLKP_PTR(p) ((char *) (p))                     // free block ptr 链表指向的下一个free block的指针的指针: p[0:8]
#define PREV_FREEBLKP_PTR(p) ((char *) (p) + DSIZE)             // free block ptr 链表指向的上一个free block的指针的指针: p[8:16]

#define GET_NEXT_FREEBLKP(p) (GET_PTR((char **) (NEXT_FREEBLKP_PTR(p)))) // free block ptr 链表的下一个free block指针
#define GET_PREV_FREEBLKP(p) (GET_PTR((char **) (PREV_FREEBLKP_PTR(p)))) // free block ptr 链表的上一个free block指针

#define PUT_NEXT_FREEBLKP(p, ptr) (PUT_PTR(NEXT_FREEBLKP_PTR(p), ptr))
#define PUT_PREV_FREEBLKP(p, ptr) (PUT_PTR(PREV_FREEBLKP_PTR(p), ptr))

#define GROUP_SIZE(idx) ((1 << (idx + 3)) + 2 * (DSIZE)) // 组大小(左值)

#define ALIGN_POW2(num, align) ((((uint32)(num)) + ((align) - 1)) & ~((align) - 1))
#define PAGE_SIZE 4096

void *mem_sbrk(int incr) 
{
    // mem_brk = brk_base, mem_max_brk: lazyAlloc申请到的边界
    static uint32 mem_brk = 0;
    static uint32 mem_max_brk = 0;
    if (incr < 0) {
        printf("ERROR: mem_sbrk failed. Negative increment not supported.\n");
        return (void *)-1;
    }
    if (incr == 0) {
        return (void *)mem_brk;
    }

    // Init
    if (mem_brk == 0) {
        uint32 bytes = ALIGN_POW2(incr, PAGE_SIZE); 
        uint32 startAddr = expandHeap(bytes / PAGE_SIZE);
        if (startAddr == 0) {
            printf("ERROR: mem_sbrk initialize Failed\n");
            return (void*)-1;
        }
        mem_brk = startAddr;               
        mem_max_brk = startAddr + bytes; 
    }

    // 保存原来的mem_brk
    void *old_brk = (void *)mem_brk;

    // 检查现有空间是否足够分配
    if (mem_brk + incr > mem_max_brk) {
        // 不足分配, 计算需要扩大的空间
        uint32 deficit = (mem_brk + incr) - mem_max_brk;
        uint32 bytes_to_request = ALIGN_POW2(deficit, PAGE_SIZE);

        // 扩充heap
        uint32 startAddr = expandHeap(bytes_to_request / PAGE_SIZE);
        if (startAddr == 0) {
            printf("ERROR: mem_sbrk failed. Ran out of memory...\n");
            return (void *)-1;
        }

        // 更新mem_max_brk边界
        mem_max_brk += bytes_to_request;
    }

    // 移动brk上界
    mem_brk += incr;

    // 返回原brk上界
    return old_brk;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // Initialize Table
    int i = 0;
    if ((brk_base = (char*)mem_sbrk(4 * WSIZE + MAX_GROUP * DSIZE)) == (void *)-1) {
        return -1;
    }
    for (; i < MAX_GROUP; i++) {
        PUT_PTR(brk_base + DSIZE * i, nullptr);
    }
    table_base = brk_base;
    brk_base += i * DSIZE;
    
    // Padding
    PUT(brk_base, 0);
    // Initialize Prologue Header and Footer
    PUT(brk_base + WSIZE * 1, PACK(DSIZE, 1, 1));
    PUT(brk_base + WSIZE * 2, PACK(DSIZE, 1, 1));
    // Initialize Epilogue Header
    PUT(brk_base + WSIZE * 3, PACK(0, 1, 1));
    brk_base += 2 * WSIZE;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(uint32 size)
{
    if (brk_base == nullptr) {
        // 第一次malloc, 初始化
        mm_init();
    }
    if (size == 0) return nullptr;
    void* ptr;
    uint32 new_size = ALIGN(size) + 2 * DSIZE;
    uint32 idx = ilog2(new_size - INTER_FRAG_SIZE) - 3;  // [24-31, 32-48, ...]
    if (idx >= MAX_GROUP) {
        idx = MAX_GROUP - 1;
    }
    while (idx < MAX_GROUP) {
        char* cnt_group = table_base + idx * DSIZE;
        ptr = GET_PTR(cnt_group);
        while (ptr != nullptr) {
            uint32 blk_size = GET_SIZE(HDRP(ptr));
            if (blk_size >= new_size + MININUM_BLK_SIZE) {
                // 余下空间可以多分出来一个小块 split and malloc
                // TO DO
                uint32 res_size = blk_size - new_size;
                remove_freelist(ptr);
                uint32 prev_alloc = GET_PREVALLOC(HDRP(ptr));

                    // 更新头块
                PUT(HDRP(ptr), PACK(new_size, 1, prev_alloc));
                    // 更新被分割块
                void* res_bp = NEXT_BLKP(ptr);
                PUT(HDRP(res_bp), PACK(res_size, 0, 1));
                PUT(FTRP(res_bp), PACK(res_size, 0, 1));
                append_freelist(res_bp, res_size);   
                // END
                return ptr;

            } else if (blk_size >= new_size) {
                // 直接分配
                // TO DO
                remove_freelist(ptr);
                uint32 prev_alloc = GET_PREVALLOC(HDRP(ptr));
                PUT(HDRP(ptr), PACK(blk_size, 1, prev_alloc));

                void* next_bp = NEXT_BLKP(ptr);
                if (!GET_ALLOC(HDRP(next_bp))) {
                    SET_PREVALLOC(FTRP(next_bp), 1);
                }
                SET_PREVALLOC(HDRP(next_bp), 1);
                // END
                return ptr;
            }
            ptr = GET_NEXT_FREEBLKP(ptr);
        }
        idx++;
    }
    // failed to malloc, call expand: TODO
    if (expand_heap(MAX(new_size, MAX_INCR)) == nullptr) {
        return nullptr;
    }

    return mm_malloc(size);
    // END
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (!GET_ALLOC(HDRP(ptr))) {
        printf("ERROR: [mm_free] double free detected at 0x%p\n", ptr);
        return;
    }
    uint32 size = GET_SIZE(HDRP(ptr));
    uint32 prev_alloc = GET_PREVALLOC(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0, prev_alloc));
    PUT(FTRP(ptr), PACK(size, 0, prev_alloc));

    PUT_NEXT_FREEBLKP(ptr, nullptr);
    PUT_PREV_FREEBLKP(ptr, nullptr);
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, uint32 size)
{
    void *new_ptr, *next_ptr = NEXT_BLKP(ptr);

    uint32 requestSize = ALIGN(size) + 2 * DSIZE;
    uint32 oldSize = GET_SIZE(HDRP(ptr)), nextSize = GET_SIZE(HDRP(next_ptr));
    uint32 aggSize = nextSize + oldSize;
    if (!GET_ALLOC(HDRP(next_ptr)) && aggSize >= requestSize) {
        if (aggSize >= requestSize + MININUM_BLK_SIZE) {
            // merge and split
            uint32 resSize = aggSize - requestSize;
            // 清理free块链表
            remove_freelist(next_ptr);

            // 更新本块信息
            PUT(HDRP(ptr), PACK(requestSize, 1, GET_PREVALLOC(HDRP(ptr))));

            // 更新被分离块信息
            void* res_ptr = NEXT_BLKP(ptr);
            PUT(HDRP(res_ptr), PACK(resSize, 0, 1));
            PUT(FTRP(res_ptr), PACK(resSize, 0, 1));
            append_freelist(res_ptr, resSize);

            // 更新被分离块的下一块的prev
            SET_PREVALLOC(HDRP(NEXT_BLKP(res_ptr)), 0);
            return ptr;
        } else {
            // merge only
            // 清理free块链表
            remove_freelist(next_ptr);

            // 更新本块信息
            PUT(HDRP(ptr), PACK(aggSize, 1, GET_PREVALLOC(HDRP(ptr))));

            // 更新free块的下一块的prev
            SET_PREVALLOC(HDRP(NEXT_BLKP(ptr)), 1);
            return ptr;
        }
    }
    new_ptr = mm_malloc(size);
    if (!new_ptr) return nullptr;
    uint32 old_payload = oldSize - 2 * DSIZE;
    uint32 copy = size < old_payload ? size : old_payload;
    memcpy(new_ptr, ptr, copy);
    mm_free(ptr);
    return new_ptr;
}

static void *expand_heap(uint32 incr) {
    char *bp;

    // 扩张heap
    if ((bp = (char*)mem_sbrk(ALIGN(incr))) == (void *)-1) {
        return nullptr;
    }
    
    // 设置新free block, 同时清理了原有epilogue
    uint32 prev_alloc = GET_PREVALLOC(HDRP(bp));
    PUT(HDRP(bp), PACK(ALIGN(incr), 0, prev_alloc)); // brk_base - 4插入
    PUT(FTRP(bp), PACK(ALIGN(incr), 0, prev_alloc)); // footer插入
        // 注意 将前后链表都设置为nullptr, 意味其未加入表, 可以忽略出表 (TODO in coalesce)
    PUT_NEXT_FREEBLKP(bp, nullptr);
    PUT_PREV_FREEBLKP(bp, nullptr);
    // 设置新epilogue
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1, 0));

    // 合并free block (传入原有prologue)
    return coalesce(bp);
}

// 基于给出的bp 合并前后free block (默认bp已经指向free block, 不负责检查)
static void* coalesce(void* bp) {
    uint32 prev_alloc = GET_PREVALLOC(HDRP(bp));
    uint32 next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    uint32 size = GET_SIZE(HDRP(bp));
    uint32 new_size;
    
    if (prev_alloc && next_alloc) {  // 无需合并
        new_size = size;
    } else if (prev_alloc && !next_alloc) { // 后面需要合并
        uint32 next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        new_size = next_size + size;
        
        // 将后面free block的链表清理
        remove_freelist(NEXT_BLKP(bp));

        PUT(HDRP(bp), PACK(new_size, 0, prev_alloc));
        PUT(FTRP(bp), PACK(new_size, 0, prev_alloc));

    } else if (next_alloc && !prev_alloc) { // 前面需要合并
        uint32 prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        new_size = prev_size + size;
        
        // 将前面free block的链表清理
        remove_freelist(PREV_BLKP(bp));
        
        PUT(FTRP(bp), PACK(new_size, 0, GET_PREVALLOC(HDRP(PREV_BLKP(bp)))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(new_size, 0, GET_PREVALLOC(HDRP(PREV_BLKP(bp)))));
        
        // 返回新bp
        bp = PREV_BLKP(bp);
        
    } else { // 前后都需要合并
        new_size = GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))) + size;
        
        // 清理前后free block链表
        remove_freelist(NEXT_BLKP(bp));
        remove_freelist(PREV_BLKP(bp));
        
        
        PUT(HDRP(PREV_BLKP(bp)), PACK(new_size, 0, GET_PREVALLOC(HDRP(PREV_BLKP(bp)))));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(new_size, 0, GET_PREVALLOC(HDRP(PREV_BLKP(bp)))));
        
        // 返回新bp
        bp = PREV_BLKP(bp);
    }
    append_freelist(bp, new_size);

    // 更新后一位的prev-alloc
    SET_PREVALLOC(HDRP(NEXT_BLKP(bp)), 0);
    return bp;
}

static int remove_freelist(void* bp) {
    char* next_blkp = GET_NEXT_FREEBLKP(bp);
    char* prev_blkp = GET_PREV_FREEBLKP(bp);

    if (next_blkp)
        PUT_PREV_FREEBLKP(next_blkp, prev_blkp);

    if (prev_blkp)
        PUT_NEXT_FREEBLKP(prev_blkp, next_blkp);

    PUT_NEXT_FREEBLKP(bp, nullptr);
    PUT_PREV_FREEBLKP(bp, nullptr);

    return 0;
}

static int append_freelist(void* bp, uint32 size) {
    if (GET_NEXT_FREEBLKP(bp) != nullptr || GET_PREV_FREEBLKP(bp) != nullptr) {
        return 0;
    }
    
    if (GET_NEXT_FREEBLKP(bp) || GET_PREV_FREEBLKP(bp))
    {
        printf(
            "Segmentation Fault: DUPLICATE APPEND! bp=%p next=%p prev=%p\n",
            bp,
            GET_NEXT_FREEBLKP(bp),
            GET_PREV_FREEBLKP(bp)
        );
        exit(-1);
    }

    if (size < 24) {
        printf("Segmentation Fault: [malloc.cpp: append_freelist] Invalid append\n");
        exit(-1);
    }
    uint32 idx = ilog2(size - INTER_FRAG_SIZE) - 3;  // [24-31, 32-48, ...]
    if (idx >= MAX_GROUP) {
        idx = MAX_GROUP - 1;
    }

    char* table_elem = table_base + idx * DSIZE;
    PUT_PREV_FREEBLKP(bp, table_elem);
    PUT_NEXT_FREEBLKP(bp, GET_PTR(table_elem));
    if (GET_PTR(table_elem))
        PUT_PREV_FREEBLKP(GET_PTR(table_elem), (char*)bp);
    PUT_NEXT_FREEBLKP(table_elem, (char*)bp);
    return 0;
}

static uint32 ilog2(uint32 x) {
    if (x == 0) return 0;
    uint32 k = 0;
    while (x >>= 1) k++;
    return k;
}