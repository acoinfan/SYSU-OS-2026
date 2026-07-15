include config.mk
.PHONY: all build kernel test bin clean run debug gdb libs export export-root export-test export-log boot-pre bootloader

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

test: libs
	$(MAKE) -C ../src/test
	@echo "test elfs ready"

bin: libs
	$(MAKE) -C ../src/bin
	@echo "bin elfs ready"

image: bootloader test bin
	dd if=$(OBJDIR)/boot/mbr.bin of=$(RUNDIR)/hd.img bs=512 count=1 seek=0 conv=notrunc
	dd if=$(OBJDIR)/boot/bootloader.bin of=$(RUNDIR)/hd.img bs=512 count=5 seek=1 conv=notrunc
	@KERNEL_SECTORS=$$((($$(wc -c < $(OBJDIR)/boot/kernel.bin) + 511) / 512)); \
	dd if=$(OBJDIR)/boot/kernel.bin of=$(RUNDIR)/hd.img bs=512 count=$$KERNEL_SECTORS seek=6 conv=notrunc,sync
	@mkdir -p $(RUNDIR)/root/bin $(RUNDIR)/test
	dd if=/dev/zero of=$(RUNDIR)/root.img bs=1M count=8
	mkfs.fat -F 12 -n ROOTVOL $(RUNDIR)/root.img
	mmd -i $(RUNDIR)/root.img ::/bin ::/usr ::/usr/bin ::/home ::/tmp ::/mnt ::/mnt/test ::/dev ::/etc ::/var
	@if find $(RUNDIR)/root/bin -mindepth 1 -maxdepth 1 -print -quit >/dev/null 2>&1; then \
		mcopy -i $(RUNDIR)/root.img $(RUNDIR)/root/bin/* ::/bin ; \
	fi
	dd if=/dev/zero of=$(RUNDIR)/test.img bs=1M count=8

	mkfs.fat -F 12 -n TESTVOL $(RUNDIR)/test.img
	          # 每扇区 512 bytes
	           # 每簇 8 扇区
	           # 根目录 512 entries → 32 sectors
	           # FAT 表大小 12 sectors（可选，如果不指定 mkfs 会自动调整）
	      # 卷标
	

	@if find $(RUNDIR)/test -mindepth 1 -maxdepth 1 ! -name bin -print -quit >/dev/null 2>&1; then \
		for item in $(RUNDIR)/test/*; do \
			if [ "$$(basename "$$item")" != "bin" ]; then \
				mcopy -i $(RUNDIR)/test.img -s "$$item" :: ; \
			fi; \
		done; \
	fi

clean:
	$(MAKE) -C ../src/libc clean
	$(MAKE) -C ../src/builtin clean
	$(MAKE) -C ../src/test clean
	$(MAKE) -C ../src/bin clean
	$(MAKE) -C ../src/boot clean
	$(MAKE) -C ../src/kernel clean
	rm -f $(RUNDIR)/root.img $(RUNDIR)/test.img

run:
	qemu-system-i386 -hda $(RUNDIR)/hd.img -hdb $(RUNDIR)/root.img -hdc $(RUNDIR)/test.img -display curses

export:
	@mkdir -p $(RUNDIR)/export
	@rm -rf $(RUNDIR)/export/*
	mcopy -i $(RUNDIR)/root.img -s "::/*" $(RUNDIR)/export/
	@find $(RUNDIR)/export -depth -exec sh -c 'base=$$(basename "$$1"); dir=$$(dirname "$$1"); lower=$$(printf "%s" "$$base" | tr A-Z a-z); if [ "$$base" != "$$lower" ] && [ ! -e "$$dir/$$lower" ]; then mv "$$1" "$$dir/$$lower"; fi' sh {} \;
	@mkdir -p $(RUNDIR)/export/mnt/test
	@rm -rf $(RUNDIR)/export/mnt/test/*
	mcopy -i $(RUNDIR)/test.img -s "::/*" $(RUNDIR)/export/mnt/test/
	@find $(RUNDIR)/export/mnt/test -depth -exec sh -c 'base=$$(basename "$$1"); dir=$$(dirname "$$1"); lower=$$(printf "%s" "$$base" | tr A-Z a-z); if [ "$$base" != "$$lower" ] && [ ! -e "$$dir/$$lower" ]; then mv "$$1" "$$dir/$$lower"; fi' sh {} \;

export-root:
	@mkdir -p $(RUNDIR)/export
	@find $(RUNDIR)/export -mindepth 1 -maxdepth 1 ! -name mnt -exec rm -rf {} +
	mcopy -i $(RUNDIR)/root.img -s "::/*" $(RUNDIR)/export/
	@find $(RUNDIR)/export -depth -exec sh -c 'base=$$(basename "$$1"); dir=$$(dirname "$$1"); lower=$$(printf "%s" "$$base" | tr A-Z a-z); if [ "$$base" != "$$lower" ] && [ ! -e "$$dir/$$lower" ]; then mv "$$1" "$$dir/$$lower"; fi' sh {} \;

export-test:
	@mkdir -p $(RUNDIR)/export/mnt/test
	@rm -rf $(RUNDIR)/export/mnt/test/*
	mcopy -i $(RUNDIR)/test.img -s "::/*" $(RUNDIR)/export/mnt/test/
	@find $(RUNDIR)/export/mnt/test -depth -exec sh -c 'base=$$(basename "$$1"); dir=$$(dirname "$$1"); lower=$$(printf "%s" "$$base" | tr A-Z a-z); if [ "$$base" != "$$lower" ] && [ ! -e "$$dir/$$lower" ]; then mv "$$1" "$$dir/$$lower"; fi' sh {} \;

export-log:
	@mkdir -p $(RUNDIR)/export
	mcopy -i $(RUNDIR)/root.img ::/fat12.log $(RUNDIR)/export/fat12.log

debug:
	qemu-system-i386 -S -s -hda $(RUNDIR)/hd.img -hdb $(RUNDIR)/root.img -hdc $(RUNDIR)/test.img

gdb:
	gdb -x gdbinit
