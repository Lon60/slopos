# Task 04 – Map framebuffer through virtual address helpers

## Background
`video/framebuffer.c:153` assumes a direct mapping by casting the physical framebuffer address to a pointer. On hardware where the GOP surface is not identity-mapped, touching the buffer will fault. The kernel already exposes `mm_phys_to_virt`, but the driver never uses it.

## Work Items
- Translate the physical framebuffer base via `mm_phys_to_virt` (or equivalent) and fail gracefully if no mapping is available.
- Document the dependency on HHDM/identity mappings and consider reserving the framebuffer range during memory init.
- Propagate the virtual pointer to the console/graphics helpers.

## Definition of Done
- Add a framebuffer initialisation test that stubs `mm_phys_to_virt` (or uses a controlled mapping) and expects initialisation to fail when no mapping exists—this must fail today because the code “succeeds” erroneously.
- Extend the graphics smoke test (e.g. run under `VIDEO=1`) to verify that drawing operations touch the translated address and produce visible output once the fix is applied.
- Confirm the new tests plus `make test` succeed after the implementation.
