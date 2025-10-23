# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**SlopOS** is an x86_64 kernel that boots via **Limine bootloader** using UEFI/Multiboot2. The kernel transitions from 32-bit to 64-bit long mode with higher-half mapping and framebuffer-only output.

**✅ Status**: Kernel boots successfully! Reaches 64-bit mode and outputs via serial port.

## Build System

- **Build System**: Meson + LLVM/Clang cross-compilation
- **Target**: `x86_64-unknown-none` (freestanding)
- **Config**: `metal.ini` (Meson cross-file)
- **Bootloader**: Limine (multiboot2)

### Quick Build
```bash
# Setup (first time only)
meson setup builddir --cross-file=metal.ini

# Build kernel
meson compile -C builddir

# Create bootable ISO
scripts/build_iso.sh
```

The build script auto-downloads Limine to `third_party/limine/` and creates a hybrid BIOS/UEFI ISO at `builddir/slop.iso`.

### Testing
```bash
# Interactive (Ctrl+C to exit)
scripts/run_qemu_ovmf.sh

# With timeout (for AI agents)
scripts/run_qemu_ovmf.sh builddir/slop.iso 15
cat test_output.log | grep "KERN"

# Help
scripts/run_qemu_ovmf.sh --help
```

**Success**: Should output `KERN` via serial port.

Scripts auto-download OVMF firmware to `third_party/ovmf/` if needed.

## CRITICAL SAFETY GUIDELINES

**NEVER leave this directory or copy the kernel anywhere else on the host system.**

- **NEVER copy kernel files to /boot or any system directory**
- **NEVER install the kernel on the host system**
- **ALWAYS run and test ONLY with QEMU virtualization**
- **NEVER attempt to boot this kernel on real hardware**
- **Keep all development confined to this project directory**

This is a development kernel that must only be tested in virtualized environments.

## Architecture

### Boot Flow
1. **Limine** loads kernel via Multiboot2 (`limine.cfg`)
2. **32-bit entry** (`boot/entry32.s`) - verify CPU, setup page tables (PML4/PDPT/PD)
3. **Enable long mode** - PAE + paging + 64-bit GDT
4. **64-bit entry** (`boot/entry64.s`) - currently outputs "KERN" and halts
5. **Higher-half kernel** at `0xFFFFFFFF80000000` (`link.ld`)

### Memory
- **Paging**: Identity map (0-2GB) + higher-half kernel
- **Allocators**: Buddy allocator (physical), kmalloc (virtual)
- **Stack**: 16KB at 0x20000 for early boot

### Components
- `boot/` - Assembly entry points and transitions
- `mm/` - Memory management (paging, allocators)
- `video/` - Framebuffer driver (software rendering)
- `drivers/` - Interrupt handling, serial, PIC/APIC
- `sched/` - Task switcher (cooperative, single-threaded)

## Technical Details

### Language
- **C/C++ freestanding** (no stdlib)
- **AT&T assembly** for boot code
- Flags: `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, `-mcmodel=kernel`

### Constraints
- UEFI-only (no BIOS/VGA text mode)
- Framebuffer-only output (software rendering)
- No legacy hardware drivers

## Development Status

### ✅ Completed
1. Boot & 64-bit transition
2. Paging (identity + higher-half)
3. GDT/IDT setup
4. Serial output

### ⏳ In Progress / TODO
5. Call `kernel_main()` from assembly
6. Memory management (buddy allocator)
7. Parse multiboot2 info
8. Framebuffer initialization
9. Task switching & scheduler

See `GUIDANCE.md` for detailed roadmap and `QUICKSTART.md` for quick commands.