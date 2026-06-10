ASM_COMPILER    = nasm
AR      		= ar
CC              = gcc
CXX             = g++
LINKER          = ld
OBJCOPY         = objcopy

CXX_FLAGS       = -g -Wall -march=i386 -std=c++11 -m32 -nostdlib \
					-fno-builtin -ffreestanding -fno-pic \
					-fno-exceptions -fno-rtti -fno-threadsafe-statics
ASM_FLAGS       = -g -f elf32
ARFLAGS 		= rcs

SRCDIR          = ../src
INCLUDEDIR      = ../include
OBJDIR          = obj
RUNDIR          = ../run
LIBDIR 			= ../lib

ASM_KERNEL_INCLUDE_DIRS = -I$(INCLUDEDIR)/kernel 

CXX_KERNEL_INCLUDE_DIRS = -I$(INCLUDEDIR)/kernel -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/builtin
CXX_USER_INCLUDE_DIRS = -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/user
CXX_LIBC_INCLUDE_DIRS = -I$(INCLUDEDIR)/libc
CXX_BUILTIN_INCLUDE_DIRS  = -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/builtin

KERNEL_CPP_SRCS   := $(wildcard $(SRCDIR)/kernel/*.cpp)
LIBC_CPP_SRCS     := $(wildcard $(SRCDIR)/libc/*.cpp)
UTILS_CPP_SRCS    := $(wildcard $(SRCDIR)/utils/*.cpp)
BOOT_CPP_SRCS     := $(wildcard $(SRCDIR)/boot/*.cpp)
BUILTIN_CPP_SRCS  := $(wildcard $(SRCDIR)/builtin/*.cpp) 

KERNEL_ASM_SRCS   := $(wildcard $(SRCDIR)/kernel/*.asm)
LIBC_ASM_SRCS     := $(wildcard $(SRCDIR)/libc/*.asm)
BOOT_ASM_SRCS     := $(wildcard $(SRCDIR)/boot/*.asm)

KERNEL_CPP_OBJS   := $(patsubst $(SRCDIR)/kernel/%.cpp,$(OBJDIR)/kernel/%.o,$(KERNEL_CPP_SRCS))
LIBC_CPP_OBJS     := $(patsubst $(SRCDIR)/libc/%.cpp,$(OBJDIR)/libc/%.o,$(LIBC_CPP_SRCS))
BOOT_CPP_OBJS     := $(patsubst $(SRCDIR)/boot/%.cpp,$(OBJDIR)/boot/%.o,$(BOOT_CPP_SRCS))
BUILTIN_CPP_OBJS  := $(patsubst $(SRCDIR)/builtin/%.cpp,$(OBJDIR)/builtin/%.o,$(BUILTIN_CPP_SRCS))

KERNEL_ASM_OBJS   := $(patsubst $(SRCDIR)/kernel/%.asm,$(OBJDIR)/kernel/%.o,$(KERNEL_ASM_SRCS))
LIBC_ASM_OBJS     := $(patsubst $(SRCDIR)/libc/%.asm,$(OBJDIR)/libc/%.o,$(LIBC_ASM_SRCS))
BOOT_ASM_OBJS     := $(patsubst $(SRCDIR)/boot/%.asm,$(OBJDIR)/boot/%.o,$(BOOT_ASM_SRCS))

KERNEL_OBJS       := $(KERNEL_CPP_OBJS) $(KERNEL_ASM_OBJS) $(BUILTIN_CPP_OBJS) $(BUILTIN_ASM_OBJS)
BOOT_OBJS         := $(BOOT_ASM_OBJS) $(BOOT_CPP_OBJS)
USER_OBJS         := $(USER_CPP_OBJS) $(USER_ASM_OBJS)
LIBC_OBJS 		  := $(LIBC_CPP_OBJS) $(LIBC_ASM_OBJS)
BUILTIN_OBJS      := $(BUILTIN_CPP_OBJS) $(BUILTIN_ASM_OBJS)

LIBC_LIB = $(LIBDIR)/libc.a

BOOTLOADER_DEPS   := $(OBJDIR)/boot/page.o $(LIBC_LIB)

.PHONY: all build kernel user clean run debug gdb test libs

all: build

build: libs kernel user

libs: $(LIBC_LIB)

kernel: user builtin $(OBJDIR)/boot/mbr.bin $(OBJDIR)/boot/bootloader.bin $(OBJDIR)/boot/kernel.bin
	dd if=$(OBJDIR)/boot/mbr.bin of=$(RUNDIR)/hd.img bs=512 count=1 seek=0 conv=notrunc
	dd if=$(OBJDIR)/boot/bootloader.bin of=$(RUNDIR)/hd.img bs=512 count=5 seek=1 conv=notrunc
	dd if=$(OBJDIR)/boot/kernel.bin of=$(RUNDIR)/hd.img bs=512 count=145 seek=6 conv=notrunc

$(OBJDIR)/boot/mbr.bin: $(SRCDIR)/boot/mbr.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ -f bin $(ASM_KERNEL_INCLUDE_DIRS) $<

$(OBJDIR)/boot/bootloader.bin: $(SRCDIR)/boot/bootloader.asm $(BOOTLOADER_DEPS)
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $(OBJDIR)/boot/bootloader.o $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<
	$(LINKER) -o $(OBJDIR)/boot/bootloader.obj -melf_i386 -N $(OBJDIR)/boot/bootloader.o $(BOOTLOADER_DEPS) -Ttext 0x7e00 -e bootloader_start
	$(OBJCOPY) -O binary $(OBJDIR)/boot/bootloader.obj $@

$(OBJDIR)/boot/entry.obj: $(SRCDIR)/boot/entry.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<

$(OBJDIR)/boot/kernel.bin: $(OBJDIR)/boot/kernel.o
	@mkdir -p $(dir $@)
	$(OBJCOPY) -O binary $< $@

$(OBJDIR)/boot/kernel.o: $(OBJDIR)/boot/entry.obj $(KERNEL_OBJS) $(LIBC_LIB)
	@mkdir -p $(dir $@)
	$(LINKER) -o $@ -melf_i386 -N $(OBJDIR)/boot/entry.obj $(KERNEL_OBJS) $(LIBC_LIB) -Ttext 0xc0020000 -e enter_kernel

user: $(USER_OBJS)
	@echo "user objects ready: $^"

builtin: $(BUILTIN_OBJS)
	@echo "builtin objects ready"

# cxx
$(OBJDIR)/user/%.o: $(SRCDIR)/user/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_SHARED_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/kernel/%.o: $(SRCDIR)/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_KERNEL_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/boot/%.o: $(SRCDIR)/boot/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_KERNEL_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/libc/%.o: $(SRCDIR)/libc/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_LIBC_INCLUDE_DIRS) -c $< -o $@

$(OBJDIR)/builtin/%.o: $(SRCDIR)/builtin/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(CXX_BUILTIN_INCLUDE_DIRS) -c $< -o $@

# asm
$(OBJDIR)/kernel/%.o: $(SRCDIR)/kernel/%.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<

$(OBJDIR)/boot/%.o: $(SRCDIR)/boot/%.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(ASM_KERNEL_INCLUDE_DIRS) $<

$(OBJDIR)/libc/%.o: $(SRCDIR)/libc/%.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $<

# ar
$(LIBC_LIB): $(LIBC_OBJS)
	@mkdir -p $(LIBDIR)
	$(AR) $(ARFLAGS) $@ $^

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
