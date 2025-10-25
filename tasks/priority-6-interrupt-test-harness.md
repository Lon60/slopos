# Priority 6 — Stabilize Interrupt Test Harness & Exception Recovery

## Summary
Re-enable and harden the SlopOS interrupt test framework so it can execute controlled exceptions without triggering triple faults or halting the kernel.

## Why This Matters
- The framework is currently disabled (`ENABLE_INTERRUPT_TESTS` = 0) because it can trap in unrecoverable states.
- Future work on interrupt routing and exception diagnostics depends on repeatable, safe test runs.
- Completing this task addresses the TODO in `boot/early_init.c` and prepares the ground for automatic regression checks.

## Scope & Deliverables
- Refine `drivers/interrupt_test.c` so each test sets and clears resume points safely, restores state, and handles exceptions beyond simple cases.
- Bring the canned tests (breakpoint, page fault variants, null pointer) back online with proper recovery logic.
- Add guardrails to avoid double-faults (e.g., ensure stack usage is bounded, use dedicated IST stack if necessary).
- Introduce a configuration mechanism to run the suite during boot with a timeout and result summary in `test_output.log`.
- Update documentation (README or inline comments) describing how to enable tests, expected output, and manual invocation steps.

## Acceptance Criteria
- Boot with `ENABLE_INTERRUPT_TESTS=1` completes without triple faulting or hanging, printing a concise pass/fail report.
- All sample tests in `drivers/interrupt_test.c` run and increment their pass counters appropriately.
- Interrupt handlers restore normal kernel operation after each injected fault.
- Serial log (`test_output.log`) includes a clear summary: total tests, passed, failed, unexpected exceptions.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors and the boot log shows the expected SlopOS banner with test results present.

## Notes for the Implementer
- Consider isolating tests onto a dedicated kernel thread/task to simplify recovery.
- Use `cli`/`sti` carefully; ensure global interrupt state is restored.
- Avoid heavy kprint usage inside the low-level handlers to minimise reentrancy risk.

