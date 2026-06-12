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
public:
    fat12_cluster_buffer cache_pool[FAT12_MAX_CACHE];
    fat12_inode_pool inodepool;
    fat12_entry_buf entry_buf;
    fat12_normalized_entry* root_dir;
    uint8 tmp_sector_buffer[FAT12_SECTOR_SIZE * 3];
    fat12_BPB bpb;
    uint32 access_time;
    uint16 fat_table[FAT12_MAX_CLUSTERS];


    IdeDrive device;

    uint32 fat_start_sector;
    uint32 root_start_sector;
    uint32 data_start_sector;

    uint32 root_sector_count;
    uint32 root_entries;
    uint32 cluster_count;
    uint32 cluster_size;
public:
    bool mount(IdeDrive disk);
    bool umount();

    bool lookup(fat12_inode* dir, const char* name, fat12_inode* out);
    bool create_file(fat12_inode* dir, const char* name, int flags, fat12_inode* out);
    bool create_directory(fat12_inode* dir, const char* name);
    bool remove(fat12_inode* dir, const char* name);

    int  read(fat12_inode* node, void* buf, int size, int offset);
    int  write(fat12_inode* node, const void* buf, int size, int offset);

    void flush();
    uint32 cluster2sector(uint16 cluster);
private:
    // root表管理
    bool init_root_dir();
    void destroy_root_dir();
    void dump_root_dir();
    // cache 管理

    // 未找到则返回-1
    int find_cache(uint16 cluster);

    // 一定会返回一个可读写的buffer
    fat12_cluster_buffer* get_cache(uint16 cluster);

    // cache_pool 管理
    bool init_cache_pool();
    void destroy_cache_pool();

    // fat表管理

    // 找空闲cluster
    // 失败返回0
    uint16 allocate_cluster();
    // 释放cluster
    void free_cluster(uint16 idx);

    // 一些flush函数
    void flush_buffer(uint16 idx);
    void flush_all();
private:
    // fat管理
    bool read_fat_table();
    bool flush_fat_table();

    // cluster管理
    void read_dir(fat12_inode* dir, void* entries, int max_entries);
    fat12_inode find_file(const char* const filename);

    // 名称转换
};


#endif