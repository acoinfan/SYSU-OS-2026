#ifndef FAT12_ENTRY_H
#define FAT12_ENTRY_H

#include "os_type.h"
#include "fileSys/file_type.h"

#define FAT12_MAX_FILENAME 256

class FAT12_FS;

enum fat12_attr : uint8 {
    READ_ONLY=0x01,
    HIDDEN=0x02,
    SYSTEM=0x04,
    VOLUME_ID=0x08,
    DIRECTORY=0x10,
    ARCHIVE=0x20
};

struct fat12_entry_location {
    bool is_root;                  // 是否根目录
    uint16 parent_cluster;         // 父目录起始簇；root 时为 0
    uint16 current_cluster;        // 当前条目所在簇；root 时可为 0 (key1)
    uint16 entry_index_in_cluster; // 簇内第几个 32B 条目          (key2)
    uint32 root_index;             // root 目录数组下标，仅 root 有意义

    bool is_same(const fat12_entry_location& other) const;
};

// 采用(fat12_fs, start_cluster)唯一标识
// 若是根目录, 则parent_dir_start_cluster = 0, start_cluster = 0;
struct fat12_inode {
    // char name[32];
    FAT12_FS* fat12_fs;
    uint32 size;
    uint16 start_cluster;
    uint16 parent_dir_start_cluster;

    uint8 attr;
    uint32 refcount;

    fat12_entry_location location;
    void dump();
};

struct __attribute__((packed)) fat12_sfn_entry {
    char   name[8];
    char   ext[3];
    uint8  attr;
    uint8  nt_reserved;
    uint8  create_time_tenths;
    uint16 create_time;
    uint16 create_date;
    uint16 last_access_date;
    uint16 first_cluster_high;   // FAT12 为 0
    uint16 write_time;
    uint16 write_date;
    uint16 first_cluster_low;
    uint32 file_size;   
};

struct __attribute__((packed)) fat12_lfn_entry {
    uint8  ordinal;        // 位7=1 表示最后一段
    uint16 name1[5];       // UCS-2
    uint8  attr;           // 固定 0x0F
    uint8  type;           // 固定 0
    uint8  checksum;       // 8.3 名校验
    uint16 name2[6];       // UCS-2
    uint16 zero;           // 固定 0
    uint16 name3[2];       // UCS-2
};

struct fat12_normalized_entry {
    char name[FAT12_MAX_FILENAME];
    uint16 start_cluster;
    uint8 attr;
    uint8 create_time_tenths;
    uint16 create_time;
    uint16 create_date;
    uint16 last_access_date;
    uint16 write_time;
    uint16 write_date;
    uint32 file_size;    

    void dump() const;
    void copy(const fat12_normalized_entry& other);
};

struct fat12_entry_buf {
    fat12_normalized_entry normalized_entry;
    uint16 tail;                // 记录name 写到了哪

    uint8 checksum;
    enum fat12_entry_buf_status {
        FREE, 
        DELETE,
        VALID,
        SKIP_LFN,
        SKIP_SFN,
        WRITE,
        END_OF_DIR
    } status;                 // 初始化后或者读完sfn entry都为true, 否则false

    void reset();

    // 解析entry, 如果是有效的sfn entry返回true(直接拒绝lfn解析)
    bool read(void* fat12_entry);

    bool write(void* fat12_entry, bool setDelete);
};


class fat12_dir_iter {
public:
    FAT12_FS* fs;
    fat12_inode* dir;

    fat12_entry_buf current;        // 当前iter到的结果

    
    uint32 global_idx;              // 总共遍历过多少个条目(包括已删除, 无效的...)
    // 用于一般目录
    uint16 current_cluster;         // 正在遍历的簇号
    uint16 entry_index_in_cluster;  // 当前簇内的条目下标
    
    // 用于根目录
    uint32 root_index;              // 根目录遍历指针

    // 控制参数
    uint32 count;                   // 已输出有效条目数
public:
    bool init(FAT12_FS* fs, fat12_inode* dir);
    bool next();
    const fat12_normalized_entry& get() const;
};
// 辅助函数
// uint8 lfn_checksum(const uint8* sfn) {
//     // sfn: 11 字节，8+3 格式
//     uint8 sum = 0;
//     for (int i = 0; i < 11; i++) {
//         // 右循环移位 + 当前字节
//         sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + sfn[i];
//     }
//     return sum;
// }


#endif