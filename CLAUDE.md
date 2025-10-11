# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **SlopOS**, an x86_64 kernel project that boots via GRUB2 with UEFI/Multiboot2. The kernel transitions from 32-bit mode to 64-bit long mode and implements a higher-half mapped kernel with framebuffer-only output.

## Build System & Commands

### Build Configuration
- Uses **Meson** build system with **LLVM/Clang** cross-compilation setup
- Cross-compilation target: `x86_64-unknown-none` (freestanding)
- Configuration file: `metal.ini` (Meson cross-file)

### Key Build Commands
```bash
# Configure and build the freestanding kernel
meson setup builddir --cross-file=metal.ini
meson compile -C builddir

# Package a UEFI-bootable ISO (produces builddir/slop.iso by default)
scripts/build_iso.sh
```

The ISO helper automatically:

* copies `builddir/kernel.elf` into a temporary staging tree,
* embeds `iso/boot/grub/grub.cfg` into a freshly generated `EFI/BOOT/BOOTX64.EFI` via `grub-mkstandalone`, and
* emits a GPT/UEFI compatible image using `xorriso`.

Missing prerequisites (e.g. `grub-efi-amd64-bin`, `xorriso`) are reported with install hints before any artifacts are touched.

### Testing
Run the kernel under QEMU + OVMF once the ISO has been produced:
```bash
scripts/run_qemu_ovmf.sh builddir/slop.iso
```
* Pass `scripts/run_qemu_ovmf_video.sh builddir/slop.iso` for a graphical window (GTK by default).
* Both launchers reuse `scripts/setup_ovmf.sh` to fetch firmware when it is not already cached under `third_party/ovmf/`.

## CRITICAL SAFETY GUIDELINES

**NEVER leave this directory or copy the kernel anywhere else on the host system.**

- **NEVER copy kernel files to /boot or any system directory**
- **NEVER install the kernel on the host system**
- **ALWAYS run and test ONLY with QEMU virtualization**
- **NEVER attempt to boot this kernel on real hardware**
- **Keep all development confined to this project directory**

This is a development kernel that must only be tested in virtualized environments.

## Architecture & Design

### Boot Process
1. **GRUB2 UEFI** loads kernel ELF via Multiboot2 protocol
2. **32-bit assembly entry** (`_start`) with Multiboot2 header in `.multiboot2_header` section
3. **Transition to 64-bit long mode** with GDT/IDT setup
4. **Higher-half kernel mapping** at `0xFFFFFFFF80000000` (see `link.ld`)

### Memory Layout
- **Linker script**: `link.ld` - ensures Multiboot2 header first, higher-half mapping
- **Memory management**: Buddy allocator backed by UEFI memory descriptors
- **Paging**: PML4/PDPT/PD/PT structure with identity mapping for early boot

### Key Components
- **boot/**: 32-bit to 64-bit transition assembly
- **mm/**: Memory management (buddy allocator, paging)
- **video/**: Framebuffer driver with software rendering
- **drivers/**: Device drivers and interrupt handling

### Display System
- **Framebuffer-only output** (no VGA text mode, no legacy BIOS)
- Uses **UEFI GOP** (Graphics Output Protocol) for framebuffer access
- **Shared framebuffer** accessible by all processes
- Software rendering with basic primitives (text, rectangles, pixels)

### Task Management
- **Single-threaded cooperative scheduler**
- Tasks = function pointers + allocated stacks + state
- Round-robin scheduling with yield-based task switching

## Technical Constraints

### Language & Compilation
- **C/C++ freestanding** (no stdlib)
- **AT&T assembly** for boot code
- Compiler flags: `-ffreestanding`, `-fno-builtin`, `-fno-stack-protector`, `-fno-pic`, `-fno-pie`
- Linker: `-nostdlib`, uses custom `link.ld`

### No Legacy Support
- No BIOS compatibility
- No VGA text mode
- No HDMI-specific drivers
- UEFI-only boot path

## Development Guidelines

### File Structure
- Empty directories contain `.gitkeep` files
- Source code not yet implemented (directories prepared for development)
- `iso/` now only hosts template configuration; `EFI/BOOT/BOOTX64.EFI` is generated on demand by `scripts/build_iso.sh`
- GRUB configuration template: `iso/boot/grub/grub.cfg`

### Key Implementation Milestones
1. Boot & 64-bit transition
2. Paging & higher-half kernel
3. Memory management (buddy allocator)
4. Interrupt handling & exceptions
5. Framebuffer initialization & software rendering
6. Task switching & cooperative scheduler

### Error Handling
- Kernel panic routine displays errors to framebuffer
- No serial/console output - framebuffer is the only output method

This project follows the detailed guidance in `GUIDANCE.md` for AI agents building the kernel step-by-step.