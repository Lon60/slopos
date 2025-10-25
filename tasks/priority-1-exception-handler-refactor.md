# Priority 1 — Refactor Exception Handler Architecture

## Summary
Restructure the exception handling system to support safe test execution by separating panic handlers from recoverable handlers, and implementing proper exception routing.

## Why This Matters
- Current `common_exception_handler` routes all exceptions to panic handlers in `drivers/exception_handlers.c`, making controlled exception testing impossible.
- The interrupt test framework needs exceptions to be caught and handled gracefully without crashing the kernel.
- This is a prerequisite for Priority 2 interrupt test harness stabilization.

## Scope & Deliverables
- Create a new exception routing system that can distinguish between "test mode" and "normal mode" exceptions.
- Implement recoverable exception handlers that can safely resume execution after handling expected exceptions.
- Add exception handler registration/override mechanism that allows the test framework to temporarily replace panic handlers.
- Ensure critical exceptions (double fault, machine check, NMI) always panic regardless of test mode.
- Add proper state restoration and stack management for exception recovery.

## Acceptance Criteria
- Test framework can install temporary exception handlers that don't panic on expected exceptions.
- Critical exceptions still trigger proper kernel panic regardless of test mode.
- Exception handlers properly restore CPU state and resume execution after handling recoverable exceptions.
- No regression in normal kernel operation when not in test mode.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors.

## Notes for the Implementer
- Consider using a handler table with priority levels (critical vs recoverable).
- Ensure proper cleanup of temporary handlers to prevent memory leaks.
- Test both normal operation and exception recovery paths thoroughly.
