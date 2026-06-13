#ifndef FAT12_INODE_POOL_H
#define FAT12_INODE_POOL_H

#include "bitmap.h"
#include "fileSys/fat12_entry.h"

class fat12_inode_pool {
    BitMap bitmap;
    fat12_inode* inodes;
    uint32 node_count, total_pages;

public:
    bool initialize(uint32 total_inode);

    void destroy();

    // ref通过lookup和外部的close函数维护
    // 其中lookup结果的ref++
    // 调用close时ref--
    // lookup的中途结果交给manager维护
    fat12_inode* allocate();
    
    // ref通过lookup和外部的close函数维护
    // 其中lookup结果的ref++
    // 调用close时ref--
    // lookup的中途结果交给manager维护
    void release(fat12_inode* ptr);
    // 传入nullptr则视作根目录(0)
    // TODO 更好的文件名处理
    fat12_inode* get_inode(fat12_inode* dir, const char* name);
        // uint16 parent_cluster = dir ? dir->start_cluster : 0;
        // for (uint32 idx = 0; idx < node_count; idx++) {
        //     if (bitmap.get(idx) &&
        //         inodes[idx].parent_dir_start_cluster == parent_cluster &&
        //         strcmp(inodes[idx].name, name) == 0) {
        //         return &inodes[idx];
        //     }
        // }


    // TODO
    fat12_inode* get_inode(uint32 start_cluster);
    fat12_inode* get_inode(const fat12_entry_location& location);
};
#endif