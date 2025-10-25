# Priority 4 — Add Configuration and Boot Integration for Interrupt Tests

## Summary
Implement configuration mechanisms and boot-time integration for the interrupt test framework, including command-line options, timeout handling, and result logging.

## Why This Matters
- The interrupt test framework currently has no configuration mechanism and is hard-coded to disabled state.
- Boot-time integration is needed to run tests during kernel initialization with proper timeout and logging.
- Configuration support enables developers to control test execution and verbosity levels.

## Scope & Deliverables
- Add Meson build options for enabling/disabling interrupt tests and configuring test parameters.
- Implement command-line parsing for test configuration options (timeout, verbosity, test selection).
- Add boot-time test execution with timeout handling and result logging to `test_output.log`.
- Create configuration header with compile-time and runtime test options.
- Add documentation for configuring and running interrupt tests.

## Acceptance Criteria
- Interrupt tests can be enabled/disabled via Meson configuration options.
- Test execution respects timeout limits and doesn't hang the kernel during boot.
- Test results are properly logged to `test_output.log` with clear pass/fail reporting.
- Configuration options are documented and easy to use.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors.

## Notes for the Implementer
- Start with Meson build options for basic enable/disable functionality.
- Ensure timeout handling doesn't interfere with normal kernel operation.
- Test both enabled and disabled configurations to verify no regressions.
