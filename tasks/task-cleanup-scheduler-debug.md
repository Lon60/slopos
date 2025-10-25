# Task: Remove Bring-Up Debug Artifacts from Scheduler/Task Init

## Context
`sched/scheduler.c` and `sched/task.c` still contain extensive bring-up instrumentation (verbose `kprint` spam, volatile stores, skipped zeroing). These were added to triage early boot issues but now obscure genuine logs and complicate reasoning about state. The goal is to cleanly initialise scheduler/task structures without excessive logging, while keeping any necessary safeguards.

## Goals
- Reduce serial output to meaningful events; remove or gate verbose tracing behind a debug flag.
- Replace volatile store hacks with deterministic initialisation (e.g., `memset`, static initialisers) now that the “Invalid Opcode” bug is understood.
- Ensure scheduler/task initialisation still works under the existing cooperative model.

## Tasks
1. Review `init_task_manager()` and `init_scheduler()` for debug-only code paths.
2. Simplify field initialisation using standard idioms (`memset`, struct literals) and remove redundant logs.
3. Confirm that any safety checks (e.g., skipping array clearing) are either no longer needed or replaced with documented, minimal logic.
4. Run through a normal boot and make sure the scheduler can create/test tasks without excessive serial noise.

## Validation
- Rebuild: `meson compile -C builddir`.
- Boot with `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` and inspect `test_output.log`; ensure only high-level scheduler messages remain.
- Optionally run the cooperative task tests (`sched/test_tasks.c` if available) to confirm behaviour.

## Deliverables
- Cleaner scheduler/task initialisation code.
- Updated comments explaining any remaining necessary quirks.
- Boot log excerpt demonstrating reduced noise (include in PR/commit notes).
