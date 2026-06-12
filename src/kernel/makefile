include ../../build/config.mk

# 搜索当前目录及子目录
CPP_SRCS := $(shell find . -name "*.cpp")
ASM_SRCS := $(shell find . -name "*.asm")

# ./mm/buddy.cpp
# -> obj/kernel/mm/buddy.o

CPP_OBJS := $(patsubst ./%.cpp,$(OBJDIR)/kernel/%.o,$(CPP_SRCS))
ASM_OBJS := $(patsubst ./%.asm,$(OBJDIR)/kernel/%.o,$(ASM_SRCS))

KERNEL_OBJS := $(CPP_OBJS) $(ASM_OBJS)

.PHONY: all clean

all: $(OBJDIR)/boot/kernel.bin

# CXX
$(OBJDIR)/kernel/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(KERNEL_INCLUDE) -c $< -o $@

# asm
$(OBJDIR)/kernel/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(KERNEL_INCLUDE) $<

# Link kernel.elf
$(OBJDIR)/boot/kernel.elf: $(KERNEL_OBJS)
	@mkdir -p $(dir $@)

	$(LINKER) \
		-o $@ \
		-melf_i386 \
		-N \
		$(OBJDIR)/boot/entry.obj \
		$(KERNEL_OBJS) \
		$(BUILTIN_LIB) \
		$(LIBC_LIB) \
		-Ttext 0xc0020000 \
		-e enter_kernel

# objcopy
$(OBJDIR)/boot/kernel.bin: $(OBJDIR)/boot/kernel.elf
	@mkdir -p $(dir $@)
	$(OBJCOPY) --only-keep-debug $< $(OBJDIR)/boot/kernel.sym
	$(OBJCOPY) -O binary --strip-debug $< $@

clean:
	rm -rf $(OBJDIR)/kernel
	rm -f $(OBJDIR)/boot/kernel.o
	rm -f $(OBJDIR)/boot/kernel.bin