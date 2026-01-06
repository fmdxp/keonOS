# keonOS 🚀

A monolithic 32-bit x86 operating system built from scratch in C++ and Assembly.

---

## 🏗️ Architecture & Core Features

### 🧠 Advanced Memory Management
- **Paging:** Identity mapping for the first 16MB to ensure kernel stability and seamless execution.
- **VMM & PFA:** Custom Virtual Memory Manager and Physical Frame Allocator.
- **Kernel Heap:** Fully functional dynamic memory management with support for `kmalloc`, `kfree`, and C++ `new`/`delete` operators.

### 🎭 Multitasking & Scheduling
- **Cooperative Multitasking:** Round-robin scheduler handling multiple kernel threads.
- **Process Control:** Built-in support for process listing (`ps`) and thread termination (`pkill`).
- **Post-Mortem Debugging:** Advanced **Kernel Panic** system that captures CPU registers and performs a full stack hex-dump upon failure.

### 💻 Interactive Shell
- **User Interface:** A custom shell with advanced features:
  - **Command History:** Navigate previous commands with Up/Down arrows.
  - **Tab-Completion:** Quick command execution.
  - **System Diagnostics:** Real-time stats for paging, heap, and uptime.

### 🔌 Hardware Drivers
- **VGA:** Text mode driver with custom color schemas.
- **Keyboard:** Interrupt-based driver with scancode-to-ASCII mapping.
- **PIT & CMOS:** Programmable Interval Timer for system ticks and timing.
- **PC Speaker:** Frequency-based audio feedback.

---

## 🛠️ Build & Development

### Prerequisites
You will need an `i686-elf` cross-compiler toolchain, `nasm`, and `qemu`.

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
