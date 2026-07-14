#ifndef FILE_TYPE_H
#define FILE_TYPE_H

#include "os_type.h"
#include "enum.h"

#define MAX_FD_COUNT 64

struct OpenFile {
    void *node;     // 指向对应inode
    fs_type type;   // 对应inode的解析方法
    int refcount;   // 引用该文件的fd总数    
    int attr;       // 文件attr
};

struct File {
    OpenFile* openfile; // 对应的openfile信息
    int offset;         // 对应文件已阅读偏移
};
#endif
