#ifndef FILE_TYPE_H
#define FILE_TYPE_H

#include "os_type.h"
#include "enum.h"

struct fat12_inode {
    char name[32];
    uint32 size;
    uint16 start_cluster;
    uint16 parent_dir_start_cluster;

    uint8 attr;
    uint32 refcount;
};

struct File {
    void *node;
    fs_type type;
    int refcount;
    int pos;
    int flags;    
};


#endif