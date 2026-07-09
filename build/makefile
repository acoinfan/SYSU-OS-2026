include config.mk
.PHONY: all build kernel user clean run debug gdb test libs export export-log boot-pre bootloader

all: build

build: image

libs: 
	$(MAKE) -C ../src/libc

builtin:
	$(MAKE) -C ../src/builtin
	@echo "builtin objects ready"

boot-pre:
	$(MAKE) -C ../src/boot pre
	@echo "boot pre objects ready"

bootloader: kernel
	@KERNEL_SECTORS=$$((($$(wc -c < $(OBJDIR)/boot/kernel.bin) + 511) / 512)); \
	$(MAKE) -C ../src/boot bootloader KERNEL_SECTOR_COUNT=$$KERNEL_SECTORS
	@echo "bootloader ready"

kernel: libs builtin boot-pre
	$(MAKE) -C ../src/kernel
	@echo "kernel objects ready"

user: $(USER_OBJS)
	@echo "user objects ready: $^"

image: bootloader
	dd if=$(OBJDIR)/boot/mbr.bin of=$(RUNDIR)/hd.img bs=512 count=1 seek=0 conv=notrunc
	dd if=$(OBJDIR)/boot/bootloader.bin of=$(RUNDIR)/hd.img bs=512 count=5 seek=1 conv=notrunc
	@KERNEL_SECTORS=$$((($$(wc -c < $(OBJDIR)/boot/kernel.bin) + 511) / 512)); \
	dd if=$(OBJDIR)/boot/kernel.bin of=$(RUNDIR)/hd.img bs=512 count=$$KERNEL_SECTORS seek=6 conv=notrunc,sync
	@mkdir -p $(RUNDIR)/test
	dd if=/dev/zero of=$(RUNDIR)/test.img bs=1M count=8

	mkfs.fat -F 12 -n TESTVOL $(RUNDIR)/test.img
	          # 每扇区 512 bytes
	           # 每簇 8 扇区
	           # 根目录 512 entries → 32 sectors
	           # FAT 表大小 12 sectors（可选，如果不指定 mkfs 会自动调整）
	      # 卷标
	

	@if find $(RUNDIR)/test -mindepth 1 -print -quit >/dev/null 2>&1; then \
		mcopy -i $(RUNDIR)/test.img -s $(RUNDIR)/test/* :: ; \
	fi

clean:
	$(MAKE) -C ../src/libc clean
	$(MAKE) -C ../src/builtin clean
	$(MAKE) -C ../src/boot clean
	$(MAKE) -C ../src/kernel clean
	rm -f $(RUNDIR)/test.img

run:
	qemu-system-i386 -hda $(RUNDIR)/hd.img -hdb $(RUNDIR)/test.img -display curses

export:
	@mkdir -p $(RUNDIR)/export
	@rm -rf $(RUNDIR)/export/*
	mcopy -i $(RUNDIR)/test.img -s "::/*" $(RUNDIR)/export/

export-log:
	@mkdir -p $(RUNDIR)/export
	mcopy -i $(RUNDIR)/test.img ::/fat12.log $(RUNDIR)/export/fat12.log

debug:
	qemu-system-i386 -S -s -hda $(RUNDIR)/hd.img -hdb $(RUNDIR)/test.img

gdb:
	gdb -x gdbinit
