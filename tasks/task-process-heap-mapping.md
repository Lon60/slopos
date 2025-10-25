# Task: Back Process Heap Allocations with Physical Pages

## Context
`process_vm_alloc()` advances heap pointers and records VMAs but never maps physical memory. As a result, any user-space heap allocation attempts to touch unmapped pages and will page fault immediately. This also affects stack growth if we ever fault on first use. The fix needs to request physical frames and map them into the process page tables during allocation.

## Goals
- Ensure `process_vm_alloc()` (and stack setup) maps physical pages with correct flags whenever the heap grows.
- Reuse existing page-mapping helpers (`map_page_4kb`) and allocator interfaces to avoid duplicate logic.
- Prepare for future userland tasks: heap regions must be readable/writable from user mode.

## Tasks
1. Update `process_vm_alloc()` to loop over the required virtual range, allocating physical frames (likely via buddy allocator) and mapping them into the process page tables with `PAGE_USER_RW` flags.
2. Add error handling: on allocation/mapping failure, roll back the heap pointer and unmap any partial mappings.
3. Make sure stack initialisation (when creating a new task) maps the stack pages eagerly.
4. Document the expected alignment and size handling in comments.

## Validation
- Rebuild: `meson compile -C builddir`.
- Boot using `scripts/run_qemu_ovmf.sh builddir/slop.iso 15`.
- (Optional) Extend `sched/test_tasks.c` to allocate memory in a task and confirm it succeeds—remove or guard behind a test flag afterwards.
- Inspect logs for new errors and ensure the kernel stays stable after multiple allocations.

## Deliverables
- Updated `mm/process_vm.c` implementing heap and stack mapping.
- Adjustments in task creation to pre-map stacks.
- Logging (where helpful) indicating successful mappings, kept concise to avoid serial spam.
