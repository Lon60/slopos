# Task 02 – Stabilise process VM slot management

## Background
`mm/process_vm.c:293` always writes new descriptors into `processes[num_processes]`. When a middle slot is freed, it remains invisible to `find_process_vm`, so subsequent creations overwrite live processes while lookups for the freed PID fail. This breaks task teardown once user processes exist.

## Work Items
- Replace the append-only indexing with a search for the first `process_id == INVALID_PROCESS_ID`, or track a freelist of vacant slots.
- Ensure the process list bookkeeping (`process_list`, `num_processes`, `active_process`) stays consistent when slots are reclaimed.
- Harden `destroy_process_vm` so it cannot leak or double-free metadata when invoked repeatedly or out of order.

## Definition of Done
- Introduce a VM manager regression test (can live alongside the future scheduler tests) that creates and destroys multiple processes in non-sequential order, then asserts that each PID resolves correctly and no live descriptor is overwritten. This test must fail with the current implementation.
- Extend the test to confirm that tearing down all processes returns allocation counters (`num_processes`, `total_pages`) to their baseline values.
- After implementing the fix, ensure the new tests plus `make test` pass.
