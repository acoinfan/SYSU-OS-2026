#include "syscall.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    write("[user_elf] begin\n");
    write("[user_elf] running from C++ ELF file\n");
    (void)getpid();
    write("[user_elf] return 42\n");
    return 42;
}
