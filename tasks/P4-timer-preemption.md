# Priority 4 – Timer-Driven Preemption

## Context
Scheduling is strictly cooperative today. Without a periodic interrupt, long-running tasks or blocked I/O can stall the system. The APIC driver already detects the local APIC but leaves the timer masked.

## Goals
- Enable a periodic timer (PIT or LAPIC) and integrate it with the scheduler.
- Support voluntary and forced yields, tracking per-task quanta.
- Preserve correctness for existing cooperative workloads.

## Suggested Approach
- Pick a timer source (start with PIT for simplicity, graduate to LAPIC) and route its IRQ through the existing IDT entry.
- Extend the scheduler with a tick handler that decrements time slices and triggers context switches when quanta expire.
- Add safeguards so tasks marked `TASK_FLAG_NO_PREEMPT` remain cooperative.
- Update statistics counters to record tick-based runtime.

## Acceptance Criteria
- The kernel services timer interrupts without crashing and performs preemptive context switches when configured.
- Tasks that yield cooperatively keep working; long loops without yields still cede CPU after a quantum.
- `make build` succeeds.
- Verification runs succeed: `make test` exits cleanly and `make boot-log` captures a log without regressions.
