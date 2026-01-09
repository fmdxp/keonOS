CC = cross/bin/i686-elf-g++
LD = cross/bin/i686-elf-ld
ASM = nasm
PYTHON = python3

ARCH_TO_COMPILE = i386

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -march=i686 \
		 -fno-exceptions -fno-rtti -fno-pic -fno-pie \
		 -I$(INCLUDE_DIR) -I$(LIBC_INCLUDE) -D__is_libk \
		 -fno-stack-protector -mno-stack-arg-probe \
		 -fno-omit-frame-pointer -g \

ASMFLAGS = -felf32
LDFLAGS = -T linker.ld -m elf_i386 -no-pie

BOOT_DIR = boot
PACKER = scripts/pack_keonfs.py
INITRD_SRC = initrd
INITRD_IMG = initrd.img
KERNEL_DIR = kernel
ARCH_DIR = kernel/arch
INCLUDE_DIR = include
LIBC_INCLUDE = include/libc
LIBC_DIR = libc
DRIVERS_DIR = drivers
BUILD_DIR = build
MM_DIR = mm
FS_DIR = fs
PROC_DIR = proc
ISO_DIR	= iso
ISO_IMG = keonOS.iso
GRUB_CFG = $(ISO_DIR)/boot/grub/grub.cfg


SRC_C = $(wildcard $(KERNEL_DIR)/*.cpp) $(wildcard $(FS_DIR)/*.cpp) $(wildcard $(LIBC_DIR)/*/*.cpp) $(wildcard $(DRIVERS_DIR)/*.cpp) $(wildcard $(ARCH_DIR)/$(ARCH_TO_COMPILE)/*.cpp) $(wildcard $(MM_DIR)/*.cpp) $(wildcard $(PROC_DIR)/*.cpp) 
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
	echo '	multiboot /boot/kernel.bin' >> $(GRUB_CFG)
	echo '  module /boot/initrd.img' >> $(GRUB_CFG)
	echo '	boot' >> $(GRUB_CFG)
	echo '}' >> $(GRUB_CFG)

$(INITRD_IMG): $(INITRD_SRC)
	@mkdir -p $(ISO_DIR)/boot
	@echo "Packing RamFS (keonFS)..."
	@$(PYTHON) $(PACKER)


iso: $(ISO_DIR)/boot/kernel.bin $(GRUB_CFG) $(INITRD_IMG)
	grub-mkrescue -o $(ISO_IMG) $(ISO_DIR)
	@echo "Created iso file: $(ISO_IMG)"

run: $(ISO_IMG)
 
	qemu-system-i386 -cdrom $(ISO_IMG) -serial stdio -m 512M -boot d -machine acpi=off -d int,cpu_reset -D qemu.log

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(ISO_DIR)
	rm -rf $(ISO_IMG)
	rm -rf qemu.log

.PHONY: all clean iso run