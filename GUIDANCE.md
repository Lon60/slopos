Here’s a **guidance README draft** you could hand to an “AI agent leader” to orchestrate sub-agents for building this kernel.
It’s structured with **milestones, technical anchors, and guiding keywords** so the agents know what concepts and deliverables to converge on.

---

# Kernel Project Guidance – AI Agent Leader

## Project Goal

Develop an **x86_64 kernel** that:

* Boots via **GRUB2 (UEFI, Multiboot2 header)**
* Transitions from **i386 assembly → 64-bit long mode**
* Initializes **paging, higher half kernel**, and minimal device setup
* Provides:

  * A **buddy allocator** (dynamic allocations)
  * **Stack allocations** for tasks
  * **Single-threaded task switching** (basic scheduler)
  * A **shared framebuffer** as the only output target
  * Software rendering → usable display output (QEMU & general UEFI GOP)

The framebuffer is shared by all processes; no separate console driver. No legacy BIOS (no VGA text mode). No HDMI-specific drivers needed.

Language: **C or C++** (freestanding, no stdlib). Use **AT&T assembly** for boot code.
Build system: **Meson + LLVM/Clang** cross setup.

---

## Roadmap / Milestones

### 1. Boot & Entry

* Provide a **Multiboot2 header** (`.multiboot2_header` section).
* GRUB loads the kernel ELF.
* Step 1: **i386 assembly entry**

  * Set up minimal stack.
  * Prepare for long mode.
  * Build a provisional **GDT** and **IDT** in 32-bit.
* Step 2: **Transition to 64-bit long mode**

  * Load 64-bit GDT.
  * Identity map low memory (1:1 physical → virtual).
  * Jump to higher half entry point.

**Guiding keywords**: multiboot2 header, i386 asm, long mode, GDT, IDT, identity mappings, higher half.

---

### 2. Memory Management

* Enable **paging** early:

  * Create **PML4, PDPT, PD, PT**.
  * Map kernel higher half.
  * Identity-map physical memory needed for early boot.
* Implement a **buddy allocator**:

  * Backed by physical memory map from **ACPI/UEFI memory descriptors**.
  * Support allocation/free in powers-of-two blocks.
* Provide **dynamic heap allocator** (kmalloc-style) built on buddy.
* Allocate per-task **stacks** from buddy.

**Guiding keywords**: ACPI, UEFI memory map, paging, higher half, buddy allocator, kmalloc, stack allocation.

---

### 3. Interrupts & Exceptions

* Full **IDT setup in 64-bit**.
* Handle CPU exceptions (page fault, GP fault, etc.).
* Provide a **kernel panic routine** with error code display.
* Basic **PIC/APIC initialization** (mask interrupts for now).

**Guiding keywords**: IDT, PIC, exceptions, kernel panic.

---

### 4. Display Output

* Initialize a **framebuffer**:

  * Use **UEFI GOP** info passed from GRUB.
  * Map framebuffer memory into kernel space.
* Provide simple **software rendering**:

  * Clear screen.
  * Print text (bitmap font).
  * Basic primitives (rectangles, pixels).
* Framebuffer is **shared memory**, accessible by all processes.

**Guiding keywords**: framebuffer, GOP, software rendering, display output, shared buffer.

---

### 5. Task Switching

* Implement **single-threaded cooperative scheduler**:

  * Tasks = function pointers + stack + state.
  * Scheduler round-robins across tasks.
* Provide system calls for:

  * Yielding execution.
  * Access to framebuffer memory.

**Guiding keywords**: single-threaded task switcher, stack allocations, cooperative scheduling.

---

## Deliverables

* **Meson build files** with cross setup for `clang` targeting bare-metal x86_64.
* **Boot assembly**  handling 32-bit → 64-bit transition.
* **Linker script** ensuring MB2 header first, higher-half mapping.
* **Kernel entry initializing paging, IDT, allocator.
* **Buddy allocator with tests.
* **Framebuffer driver with minimal software rendering fixed function pipeline.
* **Scheduler implementing cooperative task switching.
* **Panic handler** printing to framebuffer.

---

## Testing

  * Run under **QEMU** with **OVMF** firmware:
    Remember you cannot close qemu nor qemu windows you have to use log files and timeouts

    Install the emulator and firmware once on a fresh environment:

    ```
    sudo apt-get install -y qemu-system-x86 ovmf
    ```

    Build the UEFI ISO (ensures `/EFI/BOOT/BOOTX64.EFI` is published via the El Torito catalog):

    ```
    # After compiling the kernel (meson compile -C builddir)
    scripts/build_iso.sh builddir/slop.iso
    ```

    Then boot the ISO headlessly:

    ```
    scripts/run_qemu_ovmf.sh
    ```

    The helper script now prefers the distro-provided firmware (via `scripts/setup_ovmf.sh`) and falls back to downloading when
    needed. It launches QEMU without requiring a GUI, copying a fresh OVMF variables image each run so UEFI boots reliably from
    the attached ISO. Pass a custom ISO path as an argument when needed. Before QEMU starts it inspects the ISO with `xorriso`
    and aborts with guidance if the El Torito catalog does not advertise a UEFI boot image—this prevents the typical
    `BdsDxe: failed to load Boot0001 ...` loop you see when `/EFI/BOOT/BOOTX64.EFI` is missing or baked in with BIOS-only
    metadata.
* Confirm:

  * Entry into long mode.
  * Higher half mapping works.
  * Framebuffer is cleared to a color.
  * Kernel can allocate memory, start a few tasks, and render pixels.

---

## Summary

This kernel project builds step by step: **boot → 64-bit → paging → buddy allocator → framebuffer → task switching.**
Agents should focus on **small, verifiable milestones**, keeping code freestanding and portable to QEMU.
Use the **guiding keywords** (ACPI, PIC, paging, higher half, framebuffer, software rendering, etc.) as checkpoints for agent alignment.


