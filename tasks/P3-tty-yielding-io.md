# Priority 3 – TTY Yielding I/O

## Context
`drivers/tty.c:61` busy-waits for keyboard or serial input, monopolising the CPU under the current cooperative scheduler. Once more tasks exist, the shell will starve other work.

## Goals
- Allow the TTY layer to deschedule the calling task while waiting for input.
- Provide a pathway for input devices to wake blocked tasks.
- Establish groundwork for generalized blocking I/O primitives.

## Suggested Approach
- Add a wait primitive to the scheduler/task manager (e.g., block on an event or queue) and surface helpers such as `task_wait_for_input`.
- Update `tty_read_line` to register the caller as blocked when no characters are ready, and resume it from the keyboard/serial interrupt paths.
- Introduce a simple event or ring buffer so incoming bytes can wake sleepers without losing data.

## Acceptance Criteria
- The shell remains responsive, but the CPU no longer spins when idle at the prompt (verify via serial/log timestamps or instrumentation).
- Keyboard/serial interrupts correctly wake the waiting task; no deadlocks when multiple tasks request input.
- `make build` succeeds.
- `make test` succeeds.
