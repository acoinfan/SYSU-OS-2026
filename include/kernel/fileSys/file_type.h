#ifndef FILE_TYPE_H
#define FILE_TYPE_H

#include "os_type.h"
#include "enum.h"

struct File {
    void *node;     // 指向对应inode
    fs_type type;   // 对应inode的解析方法
    int refcount;   // 引用该文件的fd总数
    int offset;     // 对应文件已阅读偏移
    int attr;       // 文件attr
};


#endif