# Priority 1 – Boot Init Refactor

## Context
`boot/early_init.c` currently performs serial bring-up, Limine validation, memory initialization, graphics demo, and task creation inside a single `kernel_main`. The lack of explicit staging makes it difficult to inject new subsystems, gate optional features, or recover cleanly from early failures.

## Goals
- Extract the bring-up sequence into clearly ordered init stages with explicit dependencies.
- Allow future subsystems to register themselves without editing `kernel_main` directly.
- Keep failure handling and reporting consistent across stages so an AI agent can isolate regressions quickly.

## Suggested Approach
- Introduce lightweight init tables (e.g., `struct boot_init_step { const char *name; int (*fn)(void); }`) grouped by phase (early hardware, memory, drivers, services).
- Convert existing setup code to registered steps and move non-critical demos (framebuffer drawing, etc.) into optional stages or shell commands.
- Ensure the init runner stops on first failure, emits a descriptive message, and invokes `kernel_panic`.
- Document the new sequencing in a README or header comment for future contributors.

## Acceptance Criteria
- Init stages cover the current responsibilities of `kernel_main` without altering boot-visible behaviour (aside from relocating the demo).
- Optional/demo code lives outside the critical boot path and can be toggled off for headless boots.
- `make build` succeeds.
- `make test` succeeds.
