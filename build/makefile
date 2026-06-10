include config.mk
.PHONY: all build kernel user clean run debug gdb test libs

all: build

build: image

libs: 
	$(MAKE) -C ../src/libc

builtin:
	$(MAKE) -C ../src/builtin
	@echo "builtin objects ready"

boot:
	$(MAKE) -C ../src/boot
	@echo "boot objects ready"

kernel:
	$(MAKE) -C ../src/kernel
	@echo "kernel objects ready"

user: $(USER_OBJS)
	@echo "user objects ready: $^"

image: libs builtin boot kernel  
	dd if=$(OBJDIR)/boot/mbr.bin of=$(RUNDIR)/hd.img bs=512 count=1 seek=0 conv=notrunc
	dd if=$(OBJDIR)/boot/bootloader.bin of=$(RUNDIR)/hd.img bs=512 count=5 seek=1 conv=notrunc
	dd if=$(OBJDIR)/boot/kernel.bin of=$(RUNDIR)/hd.img bs=512 count=145 seek=6 conv=notrunc

clean:
	$(MAKE) -C ../src/libc clean
	$(MAKE) -C ../src/builtin clean
	$(MAKE) -C ../src/boot clean
	$(MAKE) -C ../src/kernel clean

run:
	qemu-system-i386 -hda $(RUNDIR)/hd.img -display curses

debug:
	qemu-system-i386 -S -s -hda $(RUNDIR)/hd.img

gdb:
	gdb -x gdbinit