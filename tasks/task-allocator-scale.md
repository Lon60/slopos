# Task: Scale Physical Page Allocators Beyond 128 MB

## Context
The current physical page allocator stack (`mm/memory_init.c`, `mm/page_alloc.c`, `mm/buddy_alloc.c`) was sized during early boot bring-up. `MAX_PAGE_FRAMES` is compiled to 32,768 entries (~128 MB), so `finalize_page_allocator()` silently drops frames above this bound. Limine routinely reports hundreds of megabytes, and the allocator reports success while discarding the majority of RAM. The buddy allocator tables are likewise sized statically, so scaling one without the other leaves the subsystems inconsistent.

## Goals
- Resize allocator metadata so the system can ingest the full Limine memory map provided at boot time (at least up to 4 GB, ideally dynamic).
- Ensure both the simple free-list allocator (`page_alloc.c`) and the buddy allocator (`buddy_alloc.c`) remain consistent with the new sizing.
- Avoid regressions on small-memory boots; the allocators must still initialise when total memory is <128 MB.

## Tasks
1. Audit the data structures that depend on `MAX_PAGE_FRAMES`, `MAX_BUDDY_BLOCKS`, or similar constants. Confirm how memory is laid out in the `.bss` to avoid exceeding boot-time limits.
2. Implement a dynamic sizing strategy:
   - Use Limine’s memmap entry count and largest usable base+length to calculate required descriptor counts.
   - Allocate descriptor buffers either from a larger static pool or via a staged allocator (e.g., temporary bump allocator backed by HHDM).
3. Update `init_page_allocator()` and `init_buddy_allocator()` to accept the new buffer sizes and record actual limits so `is_valid_frame()` covers all frames.
4. Verify that `finalize_page_allocator()` walks every usable region and that the buddy allocator reports matching totals.

## Validation
- Rebuild the kernel: `meson compile -C builddir`.
- Build the ISO and run QEMU with a generous memory size (e.g., 512 MB):
  - `scripts/build_iso.sh`
  - `scripts/run_qemu_ovmf.sh builddir/slop.iso 15 --memory 512`
- Inspect `test_output.log`:
  - Confirm log lines from `page_alloc.c` and `buddy_alloc.c` show the full page counts and no warnings about “invalid frame”.
  - Ensure the boot log still prints `SlopOS Kernel Started!` and no new panics occur.

## Deliverables
- Updated allocator code with dynamic sizing.
- Any supporting helpers added to `mm/` (e.g., temporary allocators) documented inline.
- Notes in the commit message summarising memory-test coverage.
