include ../../build/config.mk

.PHONY: all clean
all: $(OBJDIR)/boot/mbr.bin $(OBJDIR)/boot/bootloader.bin $(OBJDIR)/boot/entry.obj

$(OBJDIR)/boot/mbr.bin: mbr.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ -f bin $(BOOT_INCLUDE) $<

$(OBJDIR)/boot/bootloader.bin: bootloader.asm $(OBJDIR)/boot/page.o
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $(OBJDIR)/boot/bootloader.o $(ASM_FLAGS) $(BOOT_INCLUDE) $<

	$(LINKER) -o $(OBJDIR)/boot/bootloader.obj -melf_i386 -N \
	$(OBJDIR)/boot/bootloader.o \
	$(OBJDIR)/boot/page.o \
	$(LIBC_LIB) \
	-Ttext 0x7e00 -e bootloader_start

	$(OBJCOPY) -O binary $(OBJDIR)/boot/bootloader.obj $@

$(OBJDIR)/boot/entry.obj: entry.asm
	@mkdir -p $(dir $@)
	$(ASM_COMPILER) -o $@ $(ASM_FLAGS) $(BOOT_INCLUDE) $<

$(OBJDIR)/boot/page.o: page.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(BOOT_INCLUDE) -c $< -o $@

clean:
	rm -rf $(OBJDIR)/boot