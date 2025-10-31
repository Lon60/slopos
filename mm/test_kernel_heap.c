/*
 * SlopOS Kernel Heap Regression Tests
 * Tests for heap free-list search correctness and fragmentation handling
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"
#include "kernel_heap.h"

/* ========================================================================
 * HEAP REGRESSION TESTS
 * ======================================================================== */

/*
 * Test: Free-list search with suitable block behind smaller head node
 * 
 * This test creates a scenario where:
 * 1. A small block is at the head of a size class list
 * 2. A larger block suitable for allocation is later in the same list
 * 3. An allocation request that should find the larger block
 * 
 * With the buggy code, this would trigger unnecessary heap expansion.
 * With the fix, it should find the suitable block without expansion.
 */
int test_heap_free_list_search(void) {
    kprint("HEAP_TEST: Starting free-list search regression test\n");

    heap_stats_t stats_before, stats_after, stats_mid;
    get_heap_stats(&stats_before);

    /* Record initial heap size (total pages mapped) */
    uint64_t initial_heap_size = stats_before.total_size;
    
    kprint("HEAP_TEST: Initial heap size: ");
    kprint_decimal(initial_heap_size);
    kprint(" bytes\n");

    /* Step 1: Allocate a small block that will be at the head */
    void *small_ptr = kmalloc(32);
    if (!small_ptr) {
        kprint("HEAP_TEST: Failed to allocate small block\n");
        return -1;
    }
    kprint("HEAP_TEST: Allocated small block at head (32 bytes)\n");

    /* Step 2: Allocate a larger block (this will be in a larger size class or later) */
    void *large_ptr = kmalloc(1024);
    if (!large_ptr) {
        kprint("HEAP_TEST: Failed to allocate large block\n");
        kfree(small_ptr);
        return -1;
    }
    kprint("HEAP_TEST: Allocated large block (1024 bytes)\n");

    /* Step 3: Allocate another medium block to create fragmentation */
    void *medium_ptr = kmalloc(256);
    if (!medium_ptr) {
        kprint("HEAP_TEST: Failed to allocate medium block\n");
        kfree(small_ptr);
        kfree(large_ptr);
        return -1;
    }
    kprint("HEAP_TEST: Allocated medium block (256 bytes)\n");

    get_heap_stats(&stats_mid);
    uint64_t mid_heap_size = stats_mid.total_size;

    /* Step 4: Free the large block first, then the small block
     * This should create a situation where a small block is at head
     * and a larger block is available later in the list (after coalescing or in same class)
     */
    kfree(large_ptr);
    kprint("HEAP_TEST: Freed large block\n");
    
    kfree(small_ptr);
    kprint("HEAP_TEST: Freed small block\n");

    /* Step 5: Now allocate a size that should fit in the large freed block
     * but might be in a size class where small block is at head
     * We need to request something larger than the small block (32) but
     * that could be satisfied by the large block (1024+256 coalesced potentially)
     */
    void *requested_size = kmalloc(512);
    if (!requested_size) {
        kprint("HEAP_TEST: Failed to allocate 512-byte block (should have found free space)\n");
        kfree(medium_ptr);
        get_heap_stats(&stats_after);
        
        /* Check if heap expanded unnecessarily */
        if (stats_after.total_size > mid_heap_size) {
            kprint("HEAP_TEST: FAILED - Heap expanded despite having suitable free block\n");
            return -1;
        }
        return -1;
    }
    kprint("HEAP_TEST: Successfully allocated 512-byte block\n");

    get_heap_stats(&stats_after);
    uint64_t final_heap_size = stats_after.total_size;

    /* Verify that heap did not expand */
    if (final_heap_size > mid_heap_size) {
        kprint("HEAP_TEST: FAILED - Heap expanded from ");
        kprint_decimal(mid_heap_size);
        kprint(" to ");
        kprint_decimal(final_heap_size);
        kprint(" bytes despite having sufficient free space\n");
        
        kprint("HEAP_TEST: Free size before allocation: ");
        kprint_decimal(stats_mid.free_size);
        kprint(" bytes\n");
        
        kfree(requested_size);
        kfree(medium_ptr);
        return -1;
    }

    kprint("HEAP_TEST: Heap did not expand (correct behavior)\n");
    kprint("HEAP_TEST: Heap size remained at ");
    kprint_decimal(final_heap_size);
    kprint(" bytes\n");

    /* Clean up */
    kfree(requested_size);
    kfree(medium_ptr);

    /* Verify final state */
    get_heap_stats(&stats_after);
    uint64_t cleanup_heap_size = stats_after.total_size;
    
    if (cleanup_heap_size != final_heap_size) {
        kprint("HEAP_TEST: WARNING - Heap size changed during cleanup\n");
    }

    kprint("HEAP_TEST: Free-list search regression test PASSED\n");
    return 0;
}

/*
 * Test: Create scenario where suitable block is definitely behind smaller head
 * 
 * This test explicitly creates multiple blocks in the same size class
 * with a small one at head and a larger one later.
 */
int test_heap_fragmentation_behind_head(void) {
    kprint("HEAP_TEST: Starting fragmentation behind head test\n");

    heap_stats_t stats_before, stats_after;
    get_heap_stats(&stats_before);
    (void)stats_before; /* Used for comparison below */

    /* Allocate several blocks of similar size (same size class) */
    void *ptrs[5];
    size_t sizes[] = {128, 256, 128, 512, 256}; /* Mix to create same-size-class blocks */
    
    for (int i = 0; i < 5; i++) {
        ptrs[i] = kmalloc(sizes[i]);
        if (!ptrs[i]) {
            kprint("HEAP_TEST: Failed to allocate block ");
            kprint_decimal(i);
            kprint("\n");
            /* Clean up allocated blocks */
            for (int j = 0; j < i; j++) {
                kfree(ptrs[j]);
            }
            return -1;
        }
    }
    kprint("HEAP_TEST: Allocated 5 blocks\n");

    heap_stats_t stats_allocated;
    get_heap_stats(&stats_allocated);
    uint64_t allocated_heap_size = stats_allocated.total_size;

    /* Free blocks in a pattern that leaves a small block at head and larger later */
    /* Free index 0 (small, will be at head) */
    kfree(ptrs[0]);
    kprint("HEAP_TEST: Freed block 0 (small, now at head)\n");

    /* Free index 2 (another small, might coalesce or stay separate) */
    kfree(ptrs[2]);
    kprint("HEAP_TEST: Freed block 2 (small)\n");

    /* Now free a larger one (index 1 or 3) */
    kfree(ptrs[3]); /* 512 bytes - larger */
    kprint("HEAP_TEST: Freed block 3 (large, should be behind head in list)\n");

    /* Now try to allocate something that needs the large block but is in same size class */
    /* Request something larger than the small blocks but that fits in the 512-byte block */
    void *needed = kmalloc(400); /* Needs more than small blocks, fits in 512 */
    if (!needed) {
        kprint("HEAP_TEST: Failed to allocate 400-byte block\n");
        kfree(ptrs[1]);
        kfree(ptrs[4]);
        get_heap_stats(&stats_after);
        if (stats_after.total_size > allocated_heap_size) {
            kprint("HEAP_TEST: FAILED - Heap expanded when suitable block exists\n");
            return -1;
        }
        return -1;
    }

    get_heap_stats(&stats_after);
    uint64_t final_heap_size = stats_after.total_size;

    if (final_heap_size > allocated_heap_size) {
        kprint("HEAP_TEST: FAILED - Heap expanded from ");
        kprint_decimal(allocated_heap_size);
        kprint(" to ");
        kprint_decimal(final_heap_size);
        kprint(" bytes\n");
        kprint("HEAP_TEST: This indicates the free-list search missed a suitable block\n");
        
        kfree(needed);
        kfree(ptrs[1]);
        kfree(ptrs[4]);
        return -1;
    }

    kprint("HEAP_TEST: Successfully allocated without heap expansion\n");
    kprint("HEAP_TEST: Heap size: ");
    kprint_decimal(allocated_heap_size);
    kprint(" bytes (no change)\n");

    /* Clean up */
    kfree(needed);
    kfree(ptrs[1]);
    kfree(ptrs[4]);

    kprint("HEAP_TEST: Fragmentation behind head test PASSED\n");
    return 0;
}

/*
 * Run all kernel heap regression tests
 * Returns number of tests passed
 */
int run_kernel_heap_tests(void) {
    kprint("HEAP_TEST: Running kernel heap regression tests\n");

    int passed = 0;
    int total = 0;

    total++;
    if (test_heap_free_list_search() == 0) {
        passed++;
    } else {
        kprint("HEAP_TEST: test_heap_free_list_search FAILED\n");
    }

    total++;
    if (test_heap_fragmentation_behind_head() == 0) {
        passed++;
    } else {
        kprint("HEAP_TEST: test_heap_fragmentation_behind_head FAILED\n");
    }

    kprint("HEAP_TEST: Completed ");
    kprint_decimal(total);
    kprint(" tests, ");
    kprint_decimal(passed);
    kprint(" passed\n");

    return passed;
}

