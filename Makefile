# *****************************************************************************
# * keonOS - Makefile
# * Copyright (C) 2025-2026 fmdxp
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * ADDITIONAL TERMS (Per Section 7 of the GNU GPLv3):
# * - Original author attributions must be preserved in all copies.
# * - Modified versions must be marked as different from the original.
# * - The name "keonOS" or "fmdxp" cannot be used for publicity without permission.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# * See the GNU General Public License for more details.
# *****************************************************************************

CC = cross/bin/x86_64-elf-g++
LD = cross/bin/x86_64-elf-ld
ASM = nasm
PYTHON = python3
CC_USER = cross/bin/x86_64-elf-gcc


ARCH_TO_COMPILE = x86_64

CFLAGS = -m64 -march=x86-64 -ffreestanding -O2 -Wall -Wextra  \
		 -fno-exceptions -fno-rtti -fno-pic -fno-pie \
		 -fno-stack-protector -mno-stack-arg-probe \
		 -fno-use-cxa-atexit \
		 -mno-red-zone -mno-mmx -mno-sse \
		 -mcmodel=kernel \
		 -I$(INCLUDE_DIR) -I$(LIBC_INCLUDE) -D__is_libk \
		 -fno-omit-frame-pointer -g

ASMFLAGS = -felf64
LDFLAGS = -T linker.ld -m elf_x86_64 -no-pie

BOOT_DIR = boot
INCLUDE_DIR = include
LIBC_INCLUDE = include/libc
LIBC_DIR = libc
SYSCALLS_DIR = kernel/syscalls
DRIVERS_DIR = drivers
BUILD_DIR = build
KERNEL_DIR = kernel
ARCH_DIR = kernel/arch
MM_DIR = mm
FS_DIR = fs
PROC_DIR = proc
ISO_DIR	= iso
SCRIPTS_DIR = scripts
INITRD_SRC = initrd
INITRD_IMG = initrd.img
ISO_IMG = keonOS.iso
GRUB_CFG = $(ISO_DIR)/boot/grub/grub.cfg
HDA_IMG = harddisk.img
HDA_SIZE = 64


SRC_C = $(wildcard $(KERNEL_DIR)/*.cpp) $(wildcard $(KERNEL_DIR)/exec/*.cpp) $(wildcard $(FS_DIR)/*.cpp) $(wildcard $(LIBC_DIR)/*/*.cpp) $(wildcard $(DRIVERS_DIR)/*.cpp) $(wildcard $(ARCH_DIR)/$(ARCH_TO_COMPILE)/*.cpp) $(wildcard $(MM_DIR)/*.cpp) $(wildcard $(PROC_DIR)/*.cpp) $(wildcard $(SYSCALLS_DIR)/*.cpp)
OBJ_C = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRC_C))

SRC_ASM = $(wildcard $(BOOT_DIR)/*.asm)	$(wildcard $(ARCH_DIR)/$(ARCH_TO_COMPILE)/asm/*.asm)
OBJ_ASM = $(patsubst %.asm,$(BUILD_DIR)/%.o,$(SRC_ASM))
OBJ_LINK_LIST := $(OBJ_ASM) $(OBJ_C)


KERNEL = $(BUILD_DIR)/kernel.bin

all: $(KERNEL)

$(KERNEL): $(OBJ_LINK_LIST)
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJ_LINK_LIST)
	@echo "Kernel compiled in $@"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "Compiler C++ files: $<"

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@
	@echo "Compiled ASM files: $<"

$(ISO_DIR)/boot/kernel.bin: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot
	cp $< $@

$(GRUB_CFG):
	@mkdir -p $(ISO_DIR)/boot/grub
	echo 'set timeout=0' > $(GRUB_CFG)
	echo 'set default=0' >> $(GRUB_CFG)
	echo 'menuentry "keonOS" {' >> $(GRUB_CFG)
	echo '	multiboot2 /boot/kernel.bin' >> $(GRUB_CFG)
	echo '  module2 /boot/initrd.img' >> $(GRUB_CFG)
	echo '	boot' >> $(GRUB_CFG)
	echo '}' >> $(GRUB_CFG)

$(INITRD_IMG): $(INITRD_SRC) $(INITRD_SRC)/hello.kex $(INITRD_SRC)/test_file.kex $(INITRD_SRC)/test_sys.kex $(INITRD_SRC)/test_kdl.kex $(INITRD_SRC)/math.kdl
	@mkdir -p $(ISO_DIR)/boot
	@echo "Packing RamFS (keonFS)..."
	@$(PYTHON) $(SCRIPTS_DIR)/pack_keonfs.py

$(HDA_IMG):
	@echo "Creating ext4 Hard Disk image..."
	@dd if=/dev/zero of=$(HDA_IMG) bs=1M count=$(HDA_SIZE) 2>/dev/null
	@parted -s $(HDA_IMG) mklabel msdos
	@parted -s $(HDA_IMG) mkpart primary ext4 1MiB 100%
	@# Format partition 1 as ext4
	@dd if=/dev/zero of=partition.img bs=1M count=63 2>/dev/null
	@mkfs.ext4 -q -F partition.img
	@dd if=partition.img of=$(HDA_IMG) bs=1M seek=1 conv=notrunc 2>/dev/null
	@rm partition.img
	@echo "Hard disk created (ext4)."


iso: $(ISO_DIR)/boot/kernel.bin $(GRUB_CFG) $(INITRD_IMG)
	grub-mkrescue -o $(ISO_IMG) $(ISO_DIR)
	@echo "Created iso file: $(ISO_IMG)"

run: iso $(HDA_IMG)
 
	qemu-system-x86_64 -cdrom $(ISO_IMG) -hda $(HDA_IMG) -serial stdio -m 512M -boot d -machine acpi=off -d int,cpu_reset -D qemu.log

debug: iso $(HDA_IMG)
	qemu-system-x86_64 -cdrom $(ISO_IMG) -hda $(HDA_IMG) -serial stdio -m 512M -boot d -machine acpi=off -d int,cpu_reset -D qemu.log -s -S

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISO_DIR)
	rm -rf $(ISO_IMG)
	rm -rf $(HDA_IMG)
	rm -rf qemu.log
	rm -rf $(INITRD_SRC)/*.kex
	$(MAKE) -C user clean

$(INITRD_SRC)/hello.kex: user/hello.c user/kex.ld
	@mkdir -p $(INITRD_SRC)
	$(MAKE) -C user
	cp user/hello.kex $@

$(INITRD_SRC)/test_file.kex: user/tests/test_file.c
	$(MAKE) -C user
	cp user/test_file.kex $@

$(INITRD_SRC)/test_sys.kex: user/tests/test_sys.c
	$(MAKE) -C user
	cp user/test_sys.kex $@

$(INITRD_SRC)/test_kdl.kex: user/tests/test_kdl.c
	$(MAKE) -C user
	cp user/test_kdl.kex $@

$(INITRD_SRC)/math.kdl: user/libkex/libmath.c
	$(MAKE) -C user
	cp user/math.kdl $@

.PHONY: all clean iso run