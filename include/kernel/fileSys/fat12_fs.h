#ifndef FAT12_FS_H
#define FAT12_FS_H

#define FAT12_MAX_CACHE 64
#define FAT12_BUFFER_SIZE 512
#define FAT12_MAX_ENTRIES 10
#define FAT12_MAX_CLUSTERS 4096
#define FAT12_SECTOR_SIZE 512
#define FAT12_ENTRY_BYTES 32
#define FAT12_MAX_INODES 256

#include "os_type.h"
#include "enum.h"
#include "fileSys/file_type.h"
#include "thread.h"
#include "fileSys/fat12_inode_pool.h"
#include "fileSys/fat12_entry.h"
class FAT12_FS;

struct fat12_cluster_buffer {
    uint16 cluster_num;
    uint32 last_used_time;
    char* buf;
    int dirty;
    int refcount;

    bool read_cluster(FAT12_FS* fs);   // 从 disk/load cluster
    bool write_cluster(FAT12_FS* fs);  // flush cluster
};


struct fat12_directory {
    IdeDrive device;
    uint16 first_cluster;
    bool is_root;
    uint32 entry_count;
    fat12_normalized_entry entries[FAT12_MAX_ENTRIES];
};


// 从bias = 11开始解析即可
struct __attribute__((packed)) fat12_BPB {
    uint16 bytes_per_sector;        // bias = 11, length = 2, MUST BE 512
    uint8  sectors_per_cluster;     // bias = 13, length = 1
    uint16 reserved_sector_count;   // bias = 14, length = 2
    uint8  num_fats;                // bias = 16, length = 1, MUST BE 2
    uint16 root_entry_count;        // bias = 17, length = 2
    uint16 total_sectors_16;        // bias = 19, length = 2
    uint8  media;
    uint16 fat_size_16;             
    uint16 sectors_per_track;
    uint16 number_of_heads;
    uint32 hidden_sectors;
    uint32 total_sectors_32;
    // 可选：扩展参数，如卷标、FAT 类型字符串等
};

class FAT12_FS {
    friend fat12_dir_iter;
public:
    fat12_cluster_buffer cache_pool[FAT12_MAX_CACHE];
    fat12_inode_pool inodepool;

    fat12_normalized_entry* root_dir;
    bool root_dir_dirty;
    uint8 tmp_sector_buffer[FAT12_SECTOR_SIZE * 3];
    fat12_BPB bpb;
    uint32 access_time;
    uint16 fat_table[FAT12_MAX_CLUSTERS];
    bool fat_table_dirty;

    IdeDrive device;

    uint32 fat_start_sector;
    uint32 root_start_sector;
    uint32 data_start_sector;

    uint32 root_sector_count;
    uint32 root_entries;
    uint32 root_pages;
    uint32 cluster_count;
    uint32 cluster_size;
public:
    bool mount(IdeDrive disk);
    bool umount();

    // name必须为小写, 若为根目录下查询, dir需要初始化start_cluster = 0
    fat12_inode* lookup(fat12_inode* dir, const char* name);
    // 在dir上创建文件名为name的文件, attr为flags, 创建成功则返回true, 否则false
    bool create_file(fat12_inode* dir, const char* name, uint8 attr);
    bool create_directory(fat12_inode* dir, const char* name);
    bool remove(fat12_inode* dir, const char* name);
    bool remove_directory(fat12_inode* dir, const char* name);
    
    // 文件读写
    int  read(fat12_inode* node, void* buf, int size, int offset);
    int  write(fat12_inode* node, const void* buf, int size, int offset);
    int  append(fat12_inode* node, const void* buf, int size);
    
    // 同步性flush, 包括更新fat, 更新对应entry, 更新文件包含的所有cluster
    void flush(fat12_inode* node);
    void flush_all();
    
    // 释放inode
    void release_inode(fat12_inode* node);
    
    // 辅助函数
    uint32 cluster2sector(uint16 cluster);
    void dump_root_dir();
    // 传入对应文件夹的inode
    // -1: invalid dir, 1: empty, 0: not empty
    int is_dir_empty(fat12_inode* dir);
    
private:
    fat12_inode* lookup_threadunsafe(fat12_inode* dir, const char* name);
    // root表管理
    bool init_root_dir();
    bool destroy_root_dir();
    bool flush_root_dir();
private:
    // cache 管理

    // 未找到则返回-1
    int find_cache(uint16 cluster);

    // 返回一个可读写的buffer, 失败则Nullptr
    fat12_cluster_buffer* get_cache(uint16 cluster);

    // 释放一个buffer指针
    void release_cache(fat12_cluster_buffer* buf);

    // flush cluster:
    bool flush_cache(uint16 cluster);

    // flush all cache
    bool flush_all_cache();
    // cache_pool 管理
    bool init_cache_pool();
    bool destroy_cache_pool();
private:
    // 非法target和找不到返回false, 找到返回true
    bool find_dir_entry(fat12_inode* target, fat12_entry_location* location);
    // 包括已删除, 空条目
    // 非法target或cluster读取失败返回-1, 找到返回0, 需要扩容处理返回1, 如果是root无法扩容返回2
    int find_free_entry(fat12_inode* dir, fat12_entry_location* location);

    bool write_dir_entry(const fat12_entry_location& location,
                        const fat12_normalized_entry& entry);
    bool delete_dir_entry(const fat12_entry_location& location);
    
private:
    // fat表管理

    // 找空闲cluster
    // 失败返回0
    uint16 allocate_cluster();
    // 释放cluster
    void free_cluster(uint16 idx);

private:
    // fat管理
    bool read_fat_table();
    bool flush_fat_table();
};


#endif