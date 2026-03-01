# keonOS 🚀

A monolithic **64-bit x86_64 operating system** built from scratch in **C++ and Assembly**.

---

## 🏗️ Architecture & Core Features

### 🧠 Core Architecture & Isolation
- **Higher Half Kernel:** Kernel mapped in the higher half (`0xC0000000+`) with hardware-enforced Ring 0 / Ring 3 privilege separation.
- **x86_64 Long Mode Paging:** Fully adapted paging and memory layout tailored for 64-bit user-space process isolation.
- **Custom VMM & PFA:** Physical Frame Allocator and Virtual Memory Manager with structured virtual address handling.
- **User-Mode Execution:** Secure transition mechanics and isolated contexts to run user applications entirely in Ring 3.

### 🎭 Multitasking & Process Model
- **Hybrid Task Management:** Round-robin scheduler supporting both isolated user-mode processes and kernel threads.
- **Thread Lifecycle & Zombie Handling:** Proper thread termination and return status preservation.
- **Syscall ABI:** Expanding system call interface (`sys_mem`, `sys_proc`, `sys_stat`, file operations) for robust kernel/user interaction.
- **Safe Returns:** Extended context switching logic (`switch.asm`) with dedicated kernel stacks for secure Ring 3 -> Ring 0 transitions.

### 📦 Application Execution & Libc
- **Keon Executable (KEX) Format:** Dedicated, dynamically-loadable binary format with an ELF-inspired kernel loader (`kex_loader`).
- **Custom Libc:** A foundational user-space C standard library (`libc.klb`) featuring `stdio`, `stdlib`, `string`, `unistd`, and `sys/stat`.
- **User-Space Tools:** Included application toolchain with `crt0` initialization and test suites like `klbtool`.

---

## 💻 Interactive Shell
- **Custom CLI Shell** with:
  - **Command History** (Up / Down navigation)
  - **Tab Completion**
  - **Filesystem Commands:** `ls`, `cd`, `cat`, `touch`, `rm`, `mkdir`
  - **Process Commands:** `ps`, `pkill`
- Designed to directly interface with the VFS and future userland APIs.

---

### 🔌 Hardware Drivers
- **VGA:** Text mode driver with custom color schemas.
- **Keyboard:** Interrupt-based driver with scancode-to-ASCII mapping.
- **PIT & CMOS:** Programmable Interval Timer for system ticks and timing.
- **PC Speaker:** Frequency-based audio feedback.
- **ATA (PIO) Driver:** Low-level ATA driver providing block device access for persistent storage.

### 📂 Virtual File System (VFS) & Storage
- **Unified VFS Layer:** An abstract interface that allows the system to interact with different file systems via standard operations like `vfs_open`, `vfs_read`, and `vfs_readdir`.
- **Ext4 Filesystem Access:** Modern, robust disk-based filesystem integration (`ext4_vfs.cpp`) with foundational journaling components (`ext4_journal.cpp`).
- **KeonFS (Ramdisk):** A custom RAM-based file system loaded as a Multiboot module. It supports packed file headers and linear scanning for fast resource access during boot.
- **Mount System:** Support for mounting multiple file systems under a virtual root (`/`), enabling structured access to system resources (e.g., `/initrd`, `/mnt`).

### 📚 Standard Library (libc)
- **File I/O:** Implementation of standard C functions such as `fopen`, `fread`, `fclose`, and `fseek` to bridge the gap between user-level logic and the VFS.
- **String Manipulation:** A growing suite of memory and string functions (`memcpy`, `memset`, `strcmp`, `strcpy`) optimized for kernel performance.

---

## 🛠️ Build & Development

### Prerequisites
You will need an `x86_64-elf` cross-compiler toolchain, `nasm`, and `qemu` and `mtools`.

### Tooling
- **Ramdisk Packer:** Includes a custom Python script (`scripts/pack_keonfs.py`) that automatically bundles assets into a KeonFS image during the build process.

### Compilation
```bash
make        # Compiles the kernel
make iso    # Creates the ISO
make run    # Boots the OS in QEMU
```

All-in-one command:
```bash
make && make iso && make run
```

There's also 
```bash
make clean  # Cleans all the object files, the ISO and the logs (qemu.log)
```

---

## 📝 License

keonOS is licensed under the GNU General Public License v3.0 (GPLv3) with Additional Terms as permitted by Section 7 of the license.

Copyright (c) 2025-2026 fmdxp
Additional Terms (Per Section 7 of the GNU GPLv3):

* Attribution: Original author attributions (fmdxp) must be preserved in all copies and substantial portions of the software.

* Modifications: Modified versions must be clearly marked as different from the original version.

* Branding: The name "keonOS" or the author's name "fmdxp" cannot be used for publicity or promotional purposes without explicit prior written permission.

The software is provided "AS IS", without warranty of any kind. See the LICENSE file for the full text of the GPLv3.
