# Task: Fix Physical-to-Virtual Mapping Usage in Memory Managers

## Context
During process VM creation (`mm/process_vm.c`), freshly allocated page tables are treated as if the physical address is directly dereferenceable. This assumption only holds while Limine identity-maps the region. On systems where usable RAM resides above the identity range, dereferencing a raw phys pointer will cause page faults. Several other call sites (`page_alloc.c`, buddy allocator initialisation) make similar assumptions.

## Goals
- Ensure every subsystem that allocates physical pages immediately converts to a safe virtual address before writing to that memory.
- Centralise phys↔virt helpers so the codebase consistently uses HHDM offsets (via `get_hhdm_offset()`) with a fallback path when the higher-half direct map is unavailable.
- Audit the callers of `alloc_page_frame`, `buddy_alloc_pages`, and related helpers to prevent regressions.

## Tasks
1. Add or reuse a translation helper (e.g., `mm_phys_to_virt(uint64_t phys)` and inverse) that prefers HHDM and falls back to known identity ranges. Place it in a shared module (`memory_layout.c` or a new header) so all subsystems include the same logic.
2. Update `process_vm.c` to obtain a virtual pointer when zeroing or writing to new page tables.
3. Inspect other direct casts of physical addresses within `mm/` and `drivers/` (APIC code, early paging) and update them to use the helper where appropriate.
4. Make sure any assumptions about identity mapping are documented and verified at runtime (e.g., assert when HHDM is missing for critical paths).

## Validation
- Rebuild: `meson compile -C builddir`.
- Boot via `scripts/run_qemu_ovmf.sh builddir/slop.iso 15`.
- Confirm the kernel reaches “SlopOS Kernel Started!” with no new exceptions.
- (Optional) Use QEMU `-m 1G` to ensure high physical frames are exercised.

## Deliverables
- Updated code with centralised translation helpers.
- Inline documentation describing behaviour when HHDM is unavailable.
- Boot log snippet in PR/commit notes demonstrating success after changes.
