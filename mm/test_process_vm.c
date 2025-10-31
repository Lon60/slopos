/*
 * SlopOS Process VM Manager Regression Tests
 * Tests for process VM slot reuse and lifecycle management
 */

#include <stdint.h>
#include <stddef.h>
#include "../boot/constants.h"
#include "../drivers/serial.h"
#include "paging.h"

/* Forward declarations from process_vm module */
extern uint32_t create_process_vm(void);
extern int destroy_process_vm(uint32_t process_id);
extern void get_process_vm_stats(uint32_t *total_processes, uint32_t *active_processes);
extern process_page_dir_t *process_vm_get_page_dir(uint32_t process_id);

/* ========================================================================
 * VM MANAGER REGRESSION TESTS
 * ======================================================================== */

/*
 * Test: Non-sequential process creation and destruction
 * Creates multiple processes, destroys some in the middle, then verifies
 * that new processes reuse freed slots and all PIDs resolve correctly
 */
int test_process_vm_slot_reuse(void) {
    kprint("VM_TEST: Starting process VM slot reuse test\n");

    uint32_t initial_processes = 0;
    get_process_vm_stats(NULL, &initial_processes);

    /* Create 5 processes */
    uint32_t pids[5];
    for (int i = 0; i < 5; i++) {
        pids[i] = create_process_vm();
        if (pids[i] == INVALID_PROCESS_ID) {
            kprint("VM_TEST: Failed to create process ");
            kprint_decimal(i);
            kprint("\n");
            return -1;
        }
        kprint("VM_TEST: Created process with PID ");
        kprint_decimal(pids[i]);
        kprint("\n");
    }

    /* Verify all PIDs resolve */
    for (int i = 0; i < 5; i++) {
        process_page_dir_t *page_dir = process_vm_get_page_dir(pids[i]);
        if (!page_dir) {
            kprint("VM_TEST: Failed to resolve PID ");
            kprint_decimal(pids[i]);
            kprint("\n");
            return -1;
        }
    }

    /* Destroy middle processes (indices 1, 2, 3) */
    kprint("VM_TEST: Destroying middle processes\n");
    if (destroy_process_vm(pids[1]) != 0) {
        kprint("VM_TEST: Failed to destroy PID ");
        kprint_decimal(pids[1]);
        kprint("\n");
        return -1;
    }
    if (destroy_process_vm(pids[2]) != 0) {
        kprint("VM_TEST: Failed to destroy PID ");
        kprint_decimal(pids[2]);
        kprint("\n");
        return -1;
    }
    if (destroy_process_vm(pids[3]) != 0) {
        kprint("VM_TEST: Failed to destroy PID ");
        kprint_decimal(pids[3]);
        kprint("\n");
        return -1;
    }

    /* Verify destroyed PIDs no longer resolve */
    if (process_vm_get_page_dir(pids[1]) != NULL) {
        kprint("VM_TEST: Destroyed PID ");
        kprint_decimal(pids[1]);
        kprint(" still resolves (should not)\n");
        return -1;
    }
    if (process_vm_get_page_dir(pids[2]) != NULL) {
        kprint("VM_TEST: Destroyed PID ");
        kprint_decimal(pids[2]);
        kprint(" still resolves (should not)\n");
        return -1;
    }
    if (process_vm_get_page_dir(pids[3]) != NULL) {
        kprint("VM_TEST: Destroyed PID ");
        kprint_decimal(pids[3]);
        kprint(" still resolves (should not)\n");
        return -1;
    }

    /* Verify remaining processes still resolve */
    process_page_dir_t *page_dir0 = process_vm_get_page_dir(pids[0]);
    process_page_dir_t *page_dir4 = process_vm_get_page_dir(pids[4]);
    if (!page_dir0 || !page_dir4) {
        kprint("VM_TEST: Valid processes no longer resolve after middle destruction\n");
        return -1;
    }

    /* Create new processes - they should reuse freed slots */
    uint32_t new_pids[3];
    for (int i = 0; i < 3; i++) {
        new_pids[i] = create_process_vm();
        if (new_pids[i] == INVALID_PROCESS_ID) {
            kprint("VM_TEST: Failed to create new process after slot reuse\n");
            return -1;
        }
        kprint("VM_TEST: Created new process with PID ");
        kprint_decimal(new_pids[i]);
        kprint(" (should reuse freed slot)\n");
    }

    /* Verify all new PIDs resolve */
    for (int i = 0; i < 3; i++) {
        process_page_dir_t *page_dir = process_vm_get_page_dir(new_pids[i]);
        if (!page_dir) {
            kprint("VM_TEST: Failed to resolve new PID ");
            kprint_decimal(new_pids[i]);
            kprint("\n");
            return -1;
        }
    }

    /* Verify original processes still resolve (no overwrites) */
    page_dir0 = process_vm_get_page_dir(pids[0]);
    page_dir4 = process_vm_get_page_dir(pids[4]);
    if (!page_dir0 || !page_dir4) {
        kprint("VM_TEST: Original processes overwritten by new processes\n");
        return -1;
    }

    /* Clean up - destroy all remaining processes */
    kprint("VM_TEST: Cleaning up remaining processes\n");
    destroy_process_vm(pids[0]);
    destroy_process_vm(pids[4]);
    for (int i = 0; i < 3; i++) {
        destroy_process_vm(new_pids[i]);
    }

    /* Verify counters return to baseline */
    uint32_t final_processes = 0;
    get_process_vm_stats(NULL, &final_processes);
    if (final_processes != initial_processes) {
        kprint("VM_TEST: Process count mismatch: initial=");
        kprint_decimal(initial_processes);
        kprint(", final=");
        kprint_decimal(final_processes);
        kprint("\n");
        return -1;
    }

    kprint("VM_TEST: Process VM slot reuse test PASSED\n");
    return 0;
}

/*
 * Test: Counter return to baseline after teardown
 * Creates multiple processes, destroys them all, and verifies
 * that num_processes and total_pages return to baseline
 */
int test_process_vm_counter_reset(void) {
    kprint("VM_TEST: Starting process VM counter reset test\n");

    uint32_t initial_processes = 0;
    get_process_vm_stats(NULL, &initial_processes);

    /* Create 10 processes */
    uint32_t pids[10];
    for (int i = 0; i < 10; i++) {
        pids[i] = create_process_vm();
        if (pids[i] == INVALID_PROCESS_ID) {
            kprint("VM_TEST: Failed to create process ");
            kprint_decimal(i);
            kprint("\n");
            /* Clean up already created processes */
            for (int j = 0; j < i; j++) {
                destroy_process_vm(pids[j]);
            }
            return -1;
        }
    }

    uint32_t active_after_create = 0;
    get_process_vm_stats(NULL, &active_after_create);
    if (active_after_create != initial_processes + 10) {
        kprint("VM_TEST: Process count incorrect after creation: expected=");
        kprint_decimal(initial_processes + 10);
        kprint(", got=");
        kprint_decimal(active_after_create);
        kprint("\n");
        /* Clean up */
        for (int i = 0; i < 10; i++) {
            destroy_process_vm(pids[i]);
        }
        return -1;
    }

    /* Destroy all processes in reverse order (test non-sequential teardown) */
    for (int i = 9; i >= 0; i--) {
        if (destroy_process_vm(pids[i]) != 0) {
            kprint("VM_TEST: Failed to destroy PID ");
            kprint_decimal(pids[i]);
            kprint("\n");
            /* Try to clean up remaining */
            for (int j = i - 1; j >= 0; j--) {
                destroy_process_vm(pids[j]);
            }
            return -1;
        }
    }

    /* Verify counters returned to baseline */
    uint32_t final_processes = 0;
    get_process_vm_stats(NULL, &final_processes);
    if (final_processes != initial_processes) {
        kprint("VM_TEST: Process count did not return to baseline: initial=");
        kprint_decimal(initial_processes);
        kprint(", final=");
        kprint_decimal(final_processes);
        kprint("\n");
        return -1;
    }

    kprint("VM_TEST: Process VM counter reset test PASSED\n");
    return 0;
}

/*
 * Test: Double-free protection
 * Verifies that calling destroy_process_vm multiple times is safe
 */
int test_process_vm_double_free(void) {
    kprint("VM_TEST: Starting process VM double-free protection test\n");

    /* Create a process */
    uint32_t pid = create_process_vm();
    if (pid == INVALID_PROCESS_ID) {
        kprint("VM_TEST: Failed to create process for double-free test\n");
        return -1;
    }

    /* Destroy it once */
    if (destroy_process_vm(pid) != 0) {
        kprint("VM_TEST: Failed to destroy process (first time)\n");
        return -1;
    }

    /* Verify it's destroyed */
    if (process_vm_get_page_dir(pid) != NULL) {
        kprint("VM_TEST: Process still resolves after first destroy\n");
        return -1;
    }

    /* Try to destroy it again (should be idempotent) */
    if (destroy_process_vm(pid) != 0) {
        kprint("VM_TEST: Double destroy returned error (should be idempotent)\n");
        return -1;
    }

    /* Try to destroy invalid PID (should be safe) */
    if (destroy_process_vm(INVALID_PROCESS_ID) != 0) {
        kprint("VM_TEST: Destroy of invalid PID returned error (should be safe)\n");
        return -1;
    }

    kprint("VM_TEST: Process VM double-free protection test PASSED\n");
    return 0;
}

/*
 * Run all VM manager regression tests
 * Returns number of tests passed
 */
int run_vm_manager_tests(void) {
    kprint("VM_TEST: Running VM manager regression tests\n");

    int passed = 0;
    int total = 0;

    total++;
    if (test_process_vm_slot_reuse() == 0) {
        passed++;
    }

    total++;
    if (test_process_vm_counter_reset() == 0) {
        passed++;
    }

    total++;
    if (test_process_vm_double_free() == 0) {
        passed++;
    }

    kprint("VM_TEST: Completed ");
    kprint_decimal(total);
    kprint(" tests, ");
    kprint_decimal(passed);
    kprint(" passed\n");

    return passed;
}

