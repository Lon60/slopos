# Priority 3 — Complete Interrupt Test Framework Implementation

## Summary
Implement the missing functions and test cases in the interrupt test framework to provide comprehensive exception testing capabilities.

## Why This Matters
- Many functions declared in `drivers/interrupt_test.h` are not implemented in `drivers/interrupt_test.c`.
- The test framework currently only runs `test_invalid_opcode` with other tests commented out.
- Complete implementation is needed before the framework can be stabilized and enabled.

## Scope & Deliverables
- Implement missing functions: `test_expect_exception`, `run_memory_access_tests`, `run_control_flow_tests`.
- Implement missing memory manipulation utilities: `allocate_test_memory`, `free_test_memory`, `map_test_memory`, `unmap_test_memory`.
- Uncomment and fix the commented test cases: breakpoint, page fault variants, null pointer dereference.
- Add proper test case registration and execution framework.
- Implement comprehensive test result reporting and statistics.

## Acceptance Criteria
- All declared functions in `interrupt_test.h` are implemented and functional.
- All test cases (breakpoint, page fault, null pointer, etc.) run successfully and report correct results.
- Test framework provides comprehensive coverage of exception types and recovery scenarios.
- Test results are properly logged and statistics are accurate.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors.

## Notes for the Implementer
- Focus on implementing the core test execution framework first.
- Ensure all test cases have proper setup, execution, and cleanup phases.
- Test both success and failure scenarios for each exception type.
