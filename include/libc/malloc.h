#ifndef MALLOC_H
#define MALLOC_H

#include "os_type.h"

// 改自csapp malloc lab的作业结果, CMU真是太强了
void *mem_sbrk(int incr);
int mm_init(void);
void *mm_malloc(uint32 size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, uint32 size);

static void* coalesce(void* bp);
static void *expand_heap(uint32 incr);
static int remove_freelist(void* bp);
static int append_freelist(void* bp, uint32 size);
static uint32 ilog2(uint32 x);
#endif