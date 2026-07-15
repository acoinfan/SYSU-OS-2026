#include "stdio.h"
#include "syscall.h"
#include "vfs.h"

#define LS_MAX_ENTRIES 64

static void print_entry(const LsEntry& entry)
{
    if (entry.type == VFS_ENTRY_DIR) {
        printf("d ");
    } else {
        printf("- ");
    }

    printf("%8u ", entry.size);
    printf("%s\n", entry.name);
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    LsEntry entries[LS_MAX_ENTRIES];
    int count = vfs_ls(".", entries, LS_MAX_ENTRIES);
    if (count < 0) {
        printf("ls: cannot list current directory\n");
        return 1;
    }

    for (int i = 0; i < count; i++) {
        print_entry(entries[i]);
    }

    return 0;
}
