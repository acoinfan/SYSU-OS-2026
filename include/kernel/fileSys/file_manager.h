#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "os_type.h"
#include "fat12_fs.h"
#include "file_type.h"

#define MAX_FD_COUNT 1024
#define MAX_MOUNT_COUNT 16
#define MAX_DISK_NAME   32

struct FS_info {
    void* fileSystem;
    char disk_name[MAX_DISK_NAME];
    enum FileSystemType fs_type;
};

struct FileEntry {

};

// class FileManager {
// public:
//     File fd_table[MAX_FD_COUNT];

//     FS_info fs_table[MAX_MOUNT_COUNT];

//     FS_info root_fs;

//     int open(const char* path, int flags);
//     int read(int fd, void* buf, int size);
//     int write(int fd, void* buf, int size);
//     int close(int fd);

//     // 将disk_file挂载到/mount/disk_name
//     int mount(const char* disk_name, const char* disk_file);
//     int umount(const char* disk_name);

//     inode* lookup(const char* path);
//     int create(const char* path, int flags);
//     int remove(const char* path);

//     inode* get_cwd(PCB* p);
//     int cd(PCB* p, const char* path);
//     int ls(inode* dir, FileEntry* entries, int max_entries);
//     int mkdir(const char* path);
//     int rmdir(const char* path);
// private:
//     int alloc_fd(File* f);
//     File* get_file(int fd);
//     inode* lookup(const char* path);

// };
/*
考虑root盘和unix一致
如果进入mount, 则解析到fs_infos
*/
#endif