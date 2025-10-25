# Priority 3 — Implement Real IRQ Dispatch & Device Routing

## Summary
Replace the `common_interrupt_handler` default in `drivers/exceptions.s` and related stubs with a proper interrupt dispatch pipeline that hands hardware IRQs to registered device handlers.

## Why This Matters
- Currently all IRQs drop into `handle_unknown_interrupt`, meaning timers, keyboard, and future devices cannot function.
- Enables integration of PIC/APIC masking logic, reduces spurious logging, and lays groundwork for scheduler ticks.
- Builds on the stabilized exception harness (Priority 2) to verify correct behaviour.

## Scope & Deliverables
- Design a dispatch table (vector → handler function) with registration APIs in `drivers/idt.c` or a new module.
- Update the assembly stubs so hardware IRQ vectors (32–47) invoke the dispatcher instead of the generic handler.
- Implement masking/unmasking helpers that interact with existing PIC/APIC code, including acknowledgement (EOI) flow.
- Provide initial handlers for the programmable interval timer and keyboard IRQs as reference implementations.
- Add diagnostics to log unhandled IRQs once (rate-limited) to avoid flooding serial output.

## Dependencies & Follow-ups
- Depends on Priority 2: need reliable exception handling to diagnose early routing issues.
- Enables Priority 4 scheduler work by making periodic timer ticks available.
- Follow-up tasks may hook the dispatcher into APIC mode, SMP support, and deferred procedure calls.

## Acceptance Criteria
- IRQ0 (timer) and IRQ1 (keyboard) are serviced by dedicated handlers and confirmed via serial log or counters.
- Unregistered IRQs do not crash the system; they trigger a single warning and are masked safely.
- Existing PIC initialisation (`pic_enable_safe_irqs`) integrates with the dispatcher without regression.
- A manual test (e.g., enabling the timer, injecting keyboard scancodes) shows handlers executing and returning control cleanly.
- Full build and boot validation passes: `meson compile -C builddir`, `scripts/build_iso.sh`, and `scripts/run_qemu_ovmf.sh builddir/slop.iso 15` complete without errors and the boot log reflects normal startup with active IRQ diagnostics.

## Notes for the Implementer
- Keep the dispatcher lightweight: save minimal context in assembly, defer heavy work to C handlers.
- Acknowledge the PIC/APIC before returning to avoid lost interrupts.
- Consider adding per-IRQ statistics (count, last timestamp) to aid debugging.

