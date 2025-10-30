# Task 03 – Correct user-mode paging paths

## Background
`map_user_range` (mm/process_vm.c:118) operates on the currently active CR3 and installs intermediate tables with `PAGE_KERNEL_RW`. As a result, newly created process address spaces never receive their user stack mappings and, even if they did, the supervisor-only flags would prevent ring 3 access.

## Work Items
- Pass the target `process_page_dir_t` (or temporarily switch CR3) so that all mappings land in the intended address space.
- Ensure every level of the page tables is created with `PAGE_USER` where appropriate, preserving kernel-only mappings elsewhere.
- Audit the teardown path to confirm the same address space is active when unmapping.

## Definition of Done
- Add an integration test that creates a process VM, maps a scratch page, switches to its CR3, performs a user-mode access (e.g. via a carefully crafted task), and expects a successful read/write. The test should initially fault with the current code.
- Enhance the interrupt test harness (or a new harness scenario) to assert that user stacks are present and accessible before executing a user task.
- After applying fixes, all new tests and `make test` must pass.
