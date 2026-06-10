ASM_COMPILER = nasm
AR           = ar
CC           = gcc
CXX          = g++
LINKER       = ld
OBJCOPY      = objcopy

CXX_FLAGS = \
    -g \
    -Wall \
    -march=i386 \
    -std=c++11 \
    -m32 \
    -nostdlib \
    -fno-builtin \
    -ffreestanding \
    -fno-pic \
    -fno-exceptions \
    -fno-rtti \
    -fno-threadsafe-statics

ASM_FLAGS = -g -f elf32
ARFLAGS   = rcs

ROOT       := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)
INCLUDEDIR := $(ROOT)/include
BUILDDIR   := $(ROOT)/build
LIBDIR     := $(ROOT)/lib
RUNDIR     := $(ROOT)/run
OBJDIR     := $(BUILDDIR)/obj

LIBC_INCLUDE    := -I$(INCLUDEDIR)/libc
KERNEL_INCLUDE  := -I$(INCLUDEDIR)/kernel -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/builtin
BUILTIN_INCLUDE := -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/builtin
BOOT_INCLUDE    := -I$(INCLUDEDIR)/kernel -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/builtin
USER_INCLUDE    := -I$(INCLUDEDIR)/libc -I$(INCLUDEDIR)/user

LIBC_LIB		:= $(LIBDIR)/libc.a
BUILTIN_LIB     := $(LIBDIR)/builtin.a
