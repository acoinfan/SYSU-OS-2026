ASM_COMPILER    = nasm
CC              = gcc
CXX             = g++
CXX_FLAGS       = -g -Wall -march=i386 -std=c++11 -m32 -nostdlib -fno-builtin -ffreestanding -fno-pic
ASM_FLAGS       = -g -f elf32
LINKER          = ld
OBJCOPY         = objcopy

SRCDIR          = ../src
INCLUDEDIR      = ../include
OBJDIR          = obj
RUNDIR          = ../run

ASM_KERNEL_INCLUDE_DIRS = -I$(INCLUDEDIR)/kernel -I$(INCLUDEDIR)/shared
CXX_KERNEL_INCLUDE_DIRS = -I$(INCLUDEDIR)/kernel -I$(INCLUDEDIR)/shared -I$(INCLUDEDIR)/test
CXX_USER_INCLUDE_DIRS = -I$(INCLUDEDIR)/libuser -I$(INCLUDEDIR)/shared -I$(INCLUDEDIR)/test
CXX_TEST_INCLUDE_DIRS = -I$(INCLUDEDIR)/libuser -I$(INCLUDEDIR)/shared -I$(INCLUDEDIR)/kernel -I$(INCLUDEDIR)/test
CXX_SHARED_INCLUDE_DIRS = -I$(INCLUDEDIR)/shared -I$(INCLUDEDIR)/test

SHARED_CPP_SRCS   := $(wildcard $(SRCDIR)/shared/*.cpp)
KERNEL_CPP_SRCS   := $(wildcard $(SRCDIR)/kernel/*.cpp)
LIBUSER_CPP_SRCS  := $(wildcard $(SRCDIR)/libuser/*.cpp)
UTILS_CPP_SRCS    := $(wildcard $(SRCDIR)/utils/*.cpp)
BOOT_CPP_SRCS     := $(wildcard $(SRCDIR)/boot/*.cpp)
TEST_CPP_SRCS     := $(wildcard $(SRCDIR)/test/*.cpp)

SHARED_ASM_SRCS   := $(wildcard $(SRCDIR)/shared/*.asm)
KERNEL_ASM_SRCS   := $(wildcard $(SRCDIR)/kernel/*.asm)
LIBUSER_ASM_SRCS  := $(wildcard $(SRCDIR)/libuser/*.asm)
UTILS_ASM_SRCS    := $(wildcard $(SRCDIR)/utils/*.asm)
BOOT_ASM_SRCS     := $(wildcard $(SRCDIR)/boot/*.asm)

SHARED_CPP_OBJS   := $(patsubst $(SRCDIR)/shared/%.cpp,$(OBJDIR)/shared/%.o,$(SHARED_CPP_SRCS))
KERNEL_CPP_OBJS   := $(patsubst $(SRCDIR)/kernel/%.cpp,$(OBJDIR)/kernel/%.o,$(KERNEL_CPP_SRCS))
LIBUSER_CPP_OBJS  := $(patsubst $(SRCDIR)/libuser/%.cpp,$(OBJDIR)/libuser/%.o,$(LIBUSER_CPP_SRCS))
UTILS_CPP_OBJS    := $(patsubst $(SRCDIR)/utils/%.cpp,$(OBJDIR)/utils/%.o,$(UTILS_CPP_SRCS))
BOOT_CPP_OBJS     := $(patsubst $(SRCDIR)/boot/%.cpp,$(OBJDIR)/boot/%.o,$(BOOT_CPP_SRCS))
TEST_CPP_OBJS     := $(patsubst $(SRCDIR)/test/%.cpp,$(OBJDIR)/test/%.o,$(TEST_CPP_SRCS))

SHARED_ASM_OBJS   := $(patsubst $(SRCDIR)/shared/%.asm,$(OBJDIR)/shared/%.o,$(SHARED_ASM_SRCS))
KERNEL_ASM_OBJS   := $(patsubst $(SRCDIR)/kernel/%.asm,$(OBJDIR)/kernel/%.o,$(KERNEL_ASM_SRCS))
LIBUSER_ASM_OBJS  := $(patsubst $(SRCDIR)/libuser/%.asm,$(OBJDIR)/libuser/%.o,$(LIBUSER_ASM_SRCS))
UTILS_ASM_OBJS    := $(patsubst $(SRCDIR)/utils/%.asm,$(OBJDIR)/utils/%.o,$(UTILS_ASM_SRCS))
BOOT_ASM_OBJS     := $(patsubst $(SRCDIR)/boot/%.asm,$(OBJDIR)/boot/%.o,$(BOOT_ASM_SRCS))

SHARED_OBJS       := $(SHARED_CPP_OBJS) $(SHARED_ASM_OBJS)
KERNEL_OBJS       := $(SHARED_OBJS) $(KERNEL_CPP_OBJS) $(KERNEL_ASM_OBJS) $(TEST_CPP_OBJS)
USER_OBJS         := $(SHARED_OBJS) $(LIBUSER_CPP_OBJS) $(LIBUSER_ASM_OBJS) $(TEST_CPP_OBJS)

BOOTLOADER_DEPS   := $(OBJDIR)/shared/stdlib.o $(OBJDIR)/boot/page.o

.PHONY: all build kernel user clean run debug gdb test

all: build

build: mbr.bin bootloader.bin kernel.bin
	dd if=mbr.bin of=$(RUNDIR)/hd.img bs=512 count=1 seek=0 conv=notrunc
	dd if=bootloader.bin of=$(RUNDIR)/hd.img bs=512 count=5 seek=1 conv=notrunc
	dd if=kernel.bin of=$(RUNDIR)/hd.img bs=512 count=145 seek=6 conv=notrunc

kernel: kernel.bin

mbr.bin: $(SRCDIR)/boot/mbr.asm
	$(ASM_COMPILER) -o $@ -f bin $(ASM_KERNEL_INCLUDE_DIRS) $<

bootloader.bin: $(SRCDIR)/boot/bootloader.asm $(BOOTLOADER_DEPS)
	$(ASM_COMPILER) -o bootloader.o $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<
	$(LINKER) -o bootloader.obj -melf_i386 -N bootloader.o $(OBJDIR)/boot/page.o $(OBJDIR)/shared/stdlib.o -Ttext 0x7e00 -e bootloader_start
	$(OBJCOPY) -O binary bootloader.obj $@

entry.obj: $(SRCDIR)/boot/entry.asm
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<

kernel.bin: kernel.o
	$(OBJCOPY) -O binary $< $@

kernel.o: entry.obj $(KERNEL_OBJS)
	$(LINKER) -o $@ -melf_i386 -N entry.obj $(KERNEL_OBJS) -Ttext 0xc0020000 -e enter_kernel

user: $(USER_OBJS)
	@echo "user objects ready: $^"

# cxx
$(OBJDIR)/shared/%.o: $(SRCDIR)/shared/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_SHARED_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/kernel/%.o: $(SRCDIR)/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_KERNEL_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/boot/%.o: $(SRCDIR)/boot/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_KERNEL_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/test/%.o: $(SRCDIR)/test/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_TEST_INCLUDE_DIRS) -c $< -o $@

# asm
$(OBJDIR)/kernel/%.o: $(SRCDIR)/kernel/%.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<

$(OBJDIR)/boot/%.o: $(SRCDIR)/boot/%.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<

clean:
	rm -f *.o *.obj *.bin bootloader.o bootloader.obj entry.obj
	rm -rf $(OBJDIR)

run:
	qemu-system-i386 -hda $(RUNDIR)/hd.img -display curses

debug:
	qemu-system-i386 -S -s -hda $(RUNDIR)/hd.img

gdb:
	gdb -x gdbinit

test:
	@echo "kernel objects: $(KERNEL_OBJS)"
