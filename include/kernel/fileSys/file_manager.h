#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "os_type.h"
#include "fileSys/fat12_fs.h"
#include "fileSys/file_type.h"
#include "enum.h"

#define MAX_FD_COUNT     1024
#define MAX_MOUNT_COUNT  2
#define MAX_DISK_COUNT   4
#define MAX_DISK_NAME    32
#define MAX_FILESYS_SIZE 4096

struct FS_info {
    char disk_name[MAX_DISK_NAME];
    fs_type type;
    IdeDrive disk;
    bool inuse;
};

struct FileEntry {

};

class FileManager {
public:
    File fd_table[MAX_FD_COUNT];

    // idx = 0是root
    FS_info fs_table[MAX_DISK_COUNT];
    // idx = 0是root
    char fileSystems[PAGE_SIZE * MAX_DISK_COUNT];

    int total_mount_disk;
    
    void initialize(IdeDrive disk, fs_type fs_t);
    int open(const char* path, int flags);
    int read(int fd, void* buf, int size);
    int write(int fd, void* buf, int size);
    int close(int fd);

    // 将IdeDrive disk挂载到/mount/disk_name
    // return 0 if success
    int mount(const char* disk_name, IdeDrive disk, fs_type fs_t);
    int umount(const char* disk_name);

    File* lookup(const char* path);
    int create(const char* path, int flags);
    int remove(const char* path);

    // File* get_cwd(PCB* p);
    // int cd(PCB* p, const char* path);
    int ls(File* dir, FileEntry* entries, int max_entries);
    int mkdir(const char* path);
    int rmdir(const char* path);
private:
    int allocate_fd();
    void release_fd(int idx);
    File* get_file(int fd);
    // File* lookup(const char* path);

};
/*
考虑root盘和unix一致
如果进入mount, 则解析到fs_infos
*/
#endif