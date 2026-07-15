#include "stdio.h"
#include "syscall.h"
#include "vfs.h"

#define DIR_MAX_ENTRIES 64

static void print_entry(const LsEntry& entry)
{
    printf("%c %8u %s\n", entry.type == VFS_ENTRY_DIR ? 'd' : '-', entry.size, entry.name);
}

int main(int argc, char** argv)
{
    LsEntry entries[DIR_MAX_ENTRIES];
    const char* path = argc >= 2 ? argv[1] : ".";
    int count = vfs_ls(path, entries, DIR_MAX_ENTRIES);
    if (count < 0) {
        printf("dir: cannot list %s\n", path);
        return 1;
    }

    for (int i = 0; i < count; i++) {
        print_entry(entries[i]);
    }
    return 0;
}
