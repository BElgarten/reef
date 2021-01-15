.POSIX:

BUILDDIR = build
EFIOBJS = $(BUILDDIR)/stall.o
EFIBIN = $(BUILDDIR)/bootx64.efi
DISKIMG = disk.img
ESPIMG = esp.img
MBRIMG = $(BUILDDIR)/bootsector.img
ESPMOUNTPOINT = mnt

PARTITIONFILE = partfile.txt

VMBIOS = OVMF.fd
VMLOG = qemu.log
VM = qemu-system-x86_64

TARGET = x86_64-pe
AS = $(TARGET)-as
LD = $(TARGET)-ld
CC = clang
OBJCOPY = $(TARGET)-objcopy

CFLAGS = -target x86_64-w64-windows-gnu -ffreestanding -fshort-wchar -mno-red-zone -fno-addrsig -Werror
LDFLAGS = --oformat pei-x86-64 --subsystem 10 -pie -e kentry

.PHONY: build run log
.SUFFIXES: .c .s .o .exe .img 
build: $(DISKIMG)

run: $(DISKIMG)
	$(VM) -bios $(VMBIOS) -drive file=$<,if=ide

log: $(DISKIMG)
	$(VM) -bios $(VMBIOS) -serial file:$(VMLOG) -drive file=$<,if=ide

$(DISKIMG): $(ESPIMG) $(MBRIMG) $(PARTITIONFILE)
	tools/gpt_creator -o $@ -m $(MBRIMG) -s 5000 -b 512 -p $(PARTITIONFILE)

$(ESPIMG): $(EFIBIN)
	dd if=/dev/zero of=$@ count=4096 bs=512
	hdiutil attach -nomount $@ > .loopback
	newfs_msdos `cat .loopback`
	mount -t msdos `cat .loopback` $(ESPMOUNTPOINT)
	mkdir -p $(ESPMOUNTPOINT)/efi/boot
	cp $(EFIBIN) $(ESPMOUNTPOINT)/efi/boot
	umount $(ESPMOUNTPOINT)
	hdiutil detach `cat .loopback`
	rm .loopback

$(EFIBIN): $(EFIOBJS)
	$(LD) $(LDFLAGS) $^ -o $@

.c.s:
	$(CC) $(CFLAGS) -o $@ -S $<

$(BUILDDIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

$(basename $(MBRIMG)).exe: $(basename $(MBRIMG)).o
	$(LD) -Ttext=0x7c00 $< -o $@

.exe.img:
	$(OBJCOPY) -O binary $< $@
