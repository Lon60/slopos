# Task: Implement Zero-on-Allocation Support

## Context
`alloc_page_frame()` exposes `ALLOC_FLAG_ZERO`, but the implementation merely logs that zeroing is unimplemented. Callers cannot rely on the flag, which undermines security and consistency expectations. We already have the information required to map the page via HHDM or other means; we just need to plumb a safe path that clears memory before handing it out.

## Goals
- Honour `ALLOC_FLAG_ZERO` (and matching buddy allocator flags) so callers receive zeroed pages when requested.
- Avoid double-mapping or expensive per-byte loops when possible; use `memset` on the translated virtual address once.
- Ensure zeroing is safe both before and after the memory system is fully initialised.

## Tasks
1. Decide on a translation strategy (likely reusing the phys→virt helper from the mapping task) to obtain a writable virtual pointer for the allocated frame.
2. Add zeroing logic to `alloc_page_frame()` and, if needed, `alloc_page_frames()` / buddy allocator entry points when `ALLOC_FLAG_ZERO` is specified.
3. Review current call sites to confirm they stop printing warnings and rely on the new behaviour.
4. Add defensive logging or assertions if zeroing fails (e.g., translation returns NULL).

## Validation
- Build the kernel: `meson compile -C builddir`.
- Boot with `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` and inspect `test_output.log` to confirm no warning about zeroing remains.
- Optionally instrument a temporary test (e.g., allocate a page, fill sentinel bytes, free, reallocate with zero flag) to verify behaviour before removing the test.

## Deliverables
- Updated allocator code that reliably zeroes pages on request.
- Documentation/comments near the flag definition describing the new guarantees.
- Boot log showing clean allocation without warnings.
