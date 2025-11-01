# Priority 2 – Scheduler Cohesion

## Context
`sched/task.c` and `sched/scheduler.c` both maintain ready-queue state, leading to drift risks and unclear ownership. As more scheduling features arrive, duplicated structures will become a maintenance hazard.

## Goals
- Consolidate queue management so only the scheduler owns runnable-task ordering.
- Reduce the task manager to lifecycle bookkeeping (creation, termination, statistics).
- Prepare the codebase for richer policies (priority queues, preemption).

## Suggested Approach
- Audit queue-related fields in `task_manager` and remove or wrap them so all enqueue/dequeue operations flow through the scheduler API.
- Provide clear interfaces for the scheduler to query task metadata (current state, flags) without exposing internal arrays.
- Add assertions or logging to detect bad state transitions during the refactor.

## Acceptance Criteria
- There is a single canonical ready queue implementation (scheduler-side) and no redundant pointers in `task_manager`.
- All existing scheduling operations (`schedule_task`, `unschedule_task`, idle task handling) continue to work and the shell still launches.
- `make build` succeeds.
- `make test` succeeds.
