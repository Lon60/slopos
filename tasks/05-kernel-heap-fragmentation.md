# Task 05 – Fix kernel heap free-list search

## Background
`mm/kernel_heap.c:303` returns the head of the size class list without walking it. If that block is smaller than the request, the allocator misses larger blocks in the same class, immediately expands the heap, and leaks memory.

## Work Items
- Iterate each candidate list until a block large enough is found, or keep the lists ordered by size.
- Add diagnostics to reveal when heap expansion occurs despite sufficient free space.
- Review coalescing code to ensure it re-inserts blocks in a way that preserves the new search strategy.

## Definition of Done
- Add a heap regression test that allocates/free patterns designed to create a suitable block behind a smaller head node; with the current code the test should trigger an unnecessary expansion or allocation failure.
- Ensure the test records heap statistics before/after the pattern to confirm no extra pages were mapped after the fix.
- Run the new test suite together with `make test` and confirm all pass.
