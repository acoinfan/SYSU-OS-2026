#include "syscall.h"

int main() {
    write("[user_elf] begin\n");
    write("[user_elf] running from C++ ELF file\n");
    (void)getpid();
    write("[user_elf] return 42\n");
    return 42;
}
