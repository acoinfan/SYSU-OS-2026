#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "os_type.h"
#include "fileSys/fat12_fs.h"
#include "fileSys/file_type.h"
#include "enum.h"

#define MAX_OPENFILE_COUNT     1024
#define MAX_MOUNT_COUNT        2
#define MAX_DISK_COUNT         4
#define MAX_DISK_NAME          32
#define MAX_FILESYS_SIZE       4096
#define MAX_PATH_LENGTH        256

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
    OpenFile openfile_table[MAX_OPENFILE_COUNT];
    char openfile_bitmap[(MAX_OPENFILE_COUNT + 7) / 8];
    BitMap openfile_map;

    // idx = 0是root
    FS_info fs_table[MAX_DISK_COUNT];
    // idx = 0是root
    char fileSystems[PAGE_SIZE * MAX_DISK_COUNT];

    int total_mount_disk;
    bool dirty;
    
    void initialize(IdeDrive disk, fs_type fs_t);
    int open(const char* path, int flags);
    int read(int fd, void* buf, int size);
    int write(int fd, void* buf, int size);
    int append(int fd, void* buf, int size);
    int fseek(int fd, int bias, int whence);
    int close(int fd);
    int dump_fd(int fd);

    // 将IdeDrive disk挂载到/mount/disk_name
    // return 0 if success
    int mount(const char* disk_name, IdeDrive disk, fs_type fs_t);
    int umount(const char* disk_name);
    void sync_all();

    OpenFile* lookup(const char* path);
    int create_file(const char* path, int flags);
    int remove_file(const char* path);
    int create(const char* path, int flags);
    int remove(const char* path);

    // Path helpers. Return 0 on success, negative on invalid/too long input.
    int normalizePath(const char* cwd, const char* path, char* out);
    int normalizePath(const char* path, char* out);
    int splitPath(const char* path, char* parent, char* name);

    // File* get_cwd(PCB* p);
    // int cd(PCB* p, const char* path);
    int ls(OpenFile* dir, FileEntry* entries, int max_entries);
    int mkdir(const char* path);
    int rmdir(const char* path);
private:
    int allocate_openfile();
    void release_openfile(int idx);
    OpenFile* get_openfile(int idx);
    void release_lookup_openfile(OpenFile* openfile);
    int resolve_parent(const char* path, char* name, int* fs_idx, void** parent_node, OpenFile** parent_file);
    // File* lookup(const char* path);

};
/*
考虑root盘和unix一致
如果进入mount, 则解析到fs_infos
*/
#endif
