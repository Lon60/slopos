# Task 01 – Harden `context_switch` stack discipline

## Background
The current implementation in `sched/context_switch.s:153` pushes a five-entry IRET frame even though we are returning to ring 0. Every switch leaves `SS` and `RSP` words behind, so repeated task swaps eventually corrupt the stack. The routine also stores the ABI argument pointers instead of the task’s saved `RSI`/`RDI` values, so re-entry never restores those registers correctly.

## Work Items
- Rework the IRET frame construction to push only the words that will actually be consumed (RIP/CS/RFLAGS for ring 0) and ensure the stack pointer is restored exactly once.
- Capture and restore the real `RSI`/`RDI` register contents before repurposing them as context pointers.
- Audit the `simple_context_switch` helper to keep behaviour aligned after the main fix.

## Definition of Done
- Add a cooperative scheduler smoke test (e.g. extend `sched/test_tasks.c`) that runs two kernel tasks for several hundred yields and asserts no unexpected stack growth; the test must fail on the current code due to stack corruption or bad register state.
- Extend the interrupt harness (or a new harness scenario) to assert that `task_record_context_switch` always receives balanced transitions after the smoke test finishes.
- With the new tests in place, verify they fail on the old code, apply the fixes, and ensure `make test` passes.
