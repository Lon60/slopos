# Priority 8 — Finalize Scheduler Termination & Diagnostic Hooks

## Summary
Tighten the cooperative scheduler so tasks that return are properly terminated, statistics are accurate, and debugging hooks (stack traces, timestamps) are usable without halting the system.

## Why This Matters
- `task_entry_wrapper` currently halts the CPU when a task returns instead of calling `task_terminate`.
- Exception diagnostics cannot produce useful stack traces due to the "break immediately" stub, complicating scheduler debugging.
- Builds on the IRQ dispatcher (Priority 3) to eventually support timer-driven preemption or yields.

## Scope & Deliverables
- Implement the missing termination flow in `sched/context_switch.s` to call into the C `task_terminate` path when tasks exit naturally.
- Ensure `task_terminate` handles `task_id == -1` (current task) safely and unblocks dependents.
- Extend stack trace support in `drivers/exception_handlers.c` to walk multiple frames while preventing double faults (e.g., via guard checks or temporary IST stack).
- Replace the placeholder timestamp counter in `boot/debug.c` with a real-time source (PIT tick accumulator or APIC timer) so scheduler metrics are meaningful.
- Update scheduler statistics reporting and add docs on interpreting them.

## Acceptance Criteria
- Tasks that return no longer hang the CPU; they cleanly terminate and release their resources.
- Scheduler statistics (context switches, yields) increment accurately during demo tasks.
- `dump_stack_trace` prints at least a few frames when debugging, without triggering recursive faults.
- Debug timestamps correlate with timer ticks or elapsed time rather than a synthetic counter.
- Full build and boot validation passes: `meson compile -C builddir`, `make iso`, and `make boot-log BOOT_LOG_TIMEOUT=15` complete without errors and the boot log confirms scheduler diagnostics operating normally.

## Notes for the Implementer
- Keep assembly changes minimal and well-commented; document register usage clearly.
- Validate termination paths using the existing scheduler test harness (`run_scheduler_test`).
- Consider adding optional debug builds to stress returning tasks and stack tracing.
