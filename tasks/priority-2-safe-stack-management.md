# Priority 2 — Implement Safe Stack Management for Exception Handling

## Summary
Add Interrupt Stack Table (IST) support and safe stack management to prevent stack overflow during exception handling and enable reliable exception recovery.

## Why This Matters
- Current exception handlers run on the same stack as the interrupted code, risking stack overflow during nested exceptions.
- The interrupt test framework needs guaranteed stack space for exception recovery without triggering double faults.
- Safe stack management is essential for reliable exception handling in kernel space.

## Scope & Deliverables
- Implement IST (Interrupt Stack Table) support in the IDT setup, providing dedicated stacks for critical exceptions.
- Add stack overflow detection and recovery mechanisms for exception handlers.
- Create safe stack allocation/deallocation routines for exception handling contexts.
- Implement stack guard pages and bounds checking for exception handler stacks.
- Add diagnostics for stack usage during exception handling.

## Acceptance Criteria
- Critical exceptions (double fault, page fault, general protection) use dedicated IST stacks.
- Stack overflow during exception handling is detected and handled gracefully.
- Exception handlers have guaranteed stack space and cannot cause recursive stack faults.
- Stack usage diagnostics provide visibility into exception handling stack consumption.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors.

## Notes for the Implementer
- Start with IST support for the most critical exceptions (8, 12, 13, 14).
- Ensure proper stack alignment and guard page setup.
- Test stack overflow scenarios to verify recovery mechanisms.
