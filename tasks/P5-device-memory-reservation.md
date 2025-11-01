# Priority 5 – Device Memory Reservation

## Context
While the memory system reserves allocator metadata, other firmware-provided regions (framebuffer, ACPI tables, APIC MMIO) are still treated as generic usable RAM. Future drivers or user space could accidentally reuse them.

## Goals
- Track non-allocatable device regions discovered during boot and expose them via the memory subsystem.
- Ensure allocators never hand out pages that overlap with critical MMIO or firmware buffers.
- Provide a query interface so drivers and diagnostics know which addresses are reserved.

## Suggested Approach
- Extend `init_memory_system` to collect framebuffer/APIC/ACPI ranges and mark them in the allocator or a dedicated reservation table.
- Teach `mm_phys_to_virt` helpers to reject translations inside reserved zones unless explicitly mapped.
- Surface reservation info through a small API (e.g., `mm_is_reserved`, `mm_iterate_reserved`) for future use.

## Acceptance Criteria
- Reserved regions stay out of buddy/page allocator pools and are reported in boot logs for visibility.
- Framebuffer and APIC mappings continue to work as before, with added guardrails against accidental free/alloc.
- `make build` succeeds.
- `make test` succeeds.
