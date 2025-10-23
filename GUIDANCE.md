Here’s a **guidance README draft** you could hand to an “AI agent leader” to orchestrate sub-agents for building this kernel.
It’s structured with **milestones, technical anchors, and guiding keywords** so the agents know what concepts and deliverables to converge on.

---

# Kernel Project Guidance – AI Agent Leader

## Project Goal

Develop an **x86_64 kernel** that:

* Boots via **Limine bootloader (UEFI, Multiboot2 protocol)**
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

**Current Status**: ✅ Kernel boots successfully and reaches 64-bit mode!

---

## Roadmap / Milestones

### 1. Boot & Entry ✅ COMPLETE

* Provide a **Multiboot2 header** (`.multiboot2_header` section).
* Limine bootloader loads the kernel ELF.
* Step 1: **i386 assembly entry** (`boot/entry32.s`)

  * Set up minimal stack at 0x20000.
  * Verify CPU supports CPUID and long mode.
  * Create page tables (PML4/PDPT/PD) for identity mapping and higher-half.
  * Build 64-bit **GDT** and enable PAE + long mode.
* Step 2: **Transition to 64-bit long mode** (`boot/entry64.s`)

  * Load 64-bit segments.
  * Set up 64-bit stack.
  * Preserve multiboot2 info pointer in RDI (SysV ABI).
  * Currently outputs "KERN" via serial and halts.

**Guiding keywords**: multiboot2 header, Limine, i386 asm, long mode, GDT, paging, identity mappings, higher half.

**Next step**: Make `boot/entry64.s` call `kernel_main()` instead of halting.

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

  * Use **UEFI GOP** info passed via multiboot2 from Limine.
  * Map framebuffer memory into kernel space.
* Provide simple **software rendering**:

  * Clear screen.
  * Print text (bitmap font).
  * Basic primitives (rectangles, pixels).
* Framebuffer is **shared memory**, accessible by all processes.

**Guiding keywords**: framebuffer, GOP, multiboot2 tags, software rendering, display output, shared buffer.

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

Run under **QEMU** with **OVMF** firmware:

### Quick Start
```bash
# Compile kernel
meson compile -C builddir

# Build bootable ISO with Limine
scripts/build_iso.sh

# Test interactively (Ctrl+C to exit)
scripts/run_qemu_ovmf.sh

# OR test with timeout for AI agents (auto-exits after 15s)
scripts/run_qemu_ovmf.sh builddir/slop.iso 15

# Check logs (timeout mode only)
cat test_output.log | grep "KERN"
```

### Prerequisites
```bash
sudo apt-get install -y qemu-system-x86 ovmf xorriso git
```

### Current Boot Status ✅

* ✅ Entry into long mode - **WORKING**
* ✅ Higher half mapping - **WORKING**
* ✅ Paging setup complete - **WORKING**
* ✅ Serial output functional - **WORKING** (outputs "KERN")
* ⏳ Framebuffer initialization - **TODO**
* ⏳ Memory allocator - **TODO**
* ⏳ Task switching - **TODO**

---

## Summary

This kernel project builds step by step: **boot → 64-bit → paging → buddy allocator → framebuffer → task switching.**
Agents should focus on **small, verifiable milestones**, keeping code freestanding and portable to QEMU.
Use the **guiding keywords** (ACPI, PIC, paging, higher half, framebuffer, software rendering, etc.) as checkpoints for agent alignment.


