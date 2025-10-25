# Priority 1 — Implement Contiguous Physical Page Allocation

## Summary
Replace the temporary multi-page allocation stub in `mm/page_alloc.c` with a real contiguous allocator capable of reserving `count > 1` pages under different flags (zeroed, DMA, kernel-only).

## Why This Matters
- Drivers that require physically contiguous buffers (framebuffer, DMA, paging structures) cannot rely on the current implementation.
- Higher-level allocators and the scheduler will soon need deterministic contiguous backing for stacks and IO buffers.
- Removes the explicit TODO at `alloc_page_frames` and eliminates the noisy "not fully implemented" log during boot.

## Scope & Deliverables
- Design and implement contiguous allocation using the existing page frame metadata (`page_frame_t`) and free list.
- Ensure allocations respect flags (DMA below 16MB, zeroing) and update accounting details (`free_frames`, `allocated_frames`).
- Introduce failure handling that rolls back partial reservations.
- Provide fast-path single page fallback without regressing current behaviour.
- Update documentation/comments illustrating allocation strategy and complexity.

## Dependencies & Follow-ups
- Relies on current singleton free-list; may optionally reuse buddy allocator for larger runs if practical.
- Enables downstream tasks:
  - Priority 2 (interrupt test harness) needs reliable zeroed contiguous buffers for controlled fault scenarios.
  - Priority 3 (interrupt dispatch) can assume DMA buffers are real.

## Acceptance Criteria
- `alloc_page_frames` returns contiguous physical ranges for `count > 1` and never logs the existing placeholder warning.
- Kernel boots successfully and reports accurate allocator statistics (`Page allocator ready: X pages`).
- Manual smoke test: allocate spans of varying sizes (2, 4, 16, 64 pages) and verify physical address deltas are page-sized and sequential.
- Add diagnostic logging (guarded behind debug flag) or a small test routine to exercise the contiguous path during boot.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors and the boot log shows the expected SlopOS banner.

## Notes for the Implementer
- Preserve existing single-page path fast case.
- Consider using a simple first-fit scan over regions before porting or integrating with the buddy allocator.
- Document limitations (e.g., maximum span length) if any remain after implementation.

