#include "syscall.h"

int main() {
    write("[user_elf] begin\n");
    write("[user_elf] running from C++ ELF file\n");
    (void)getpid();
    write("[user_elf] exit 42\n");
    exit(42);
    return 42;
}
