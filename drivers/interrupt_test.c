/*
 * SlopOS Interrupt Test Framework
 * Controlled exception generation and interrupt testing
 */

#include "interrupt_test.h"
#include "serial.h"
#include "../boot/idt.h"
#include <stddef.h>

// Global test state
static struct test_context test_ctx = {0};
static struct test_stats test_statistics = {0};
static uint32_t test_flags = 0;

/*
 * Initialize interrupt test framework
 */
void interrupt_test_init(void) {
    kprintln("INTERRUPT_TEST: Initializing test framework");

    // Clear test context
    test_ctx.test_active = 0;
    test_ctx.expected_exception = -1;
    test_ctx.exception_occurred = 0;
    test_ctx.exception_vector = -1;
    test_ctx.test_rip = 0;
    test_ctx.resume_rip = 0;
    test_ctx.last_frame = NULL;

    // Clear statistics
    test_statistics.total_tests = 0;
    test_statistics.passed_tests = 0;
    test_statistics.failed_tests = 0;
    test_statistics.exceptions_caught = 0;
    test_statistics.unexpected_exceptions = 0;

    // Install our exception handler for testing
    // Skip critical exceptions that should not be overridden
    for (int i = 0; i < 32; i++) {
        // Don't override double fault (8), machine check (18), or NMI (2)
        if (i == EXCEPTION_DOUBLE_FAULT || i == EXCEPTION_MACHINE_CHECK || i == EXCEPTION_NMI) {
            continue;
        }
        idt_install_exception_handler(i, test_exception_handler);
    }

    kprintln("INTERRUPT_TEST: Framework initialized");
}

/*
 * Cleanup interrupt test framework
 */
void interrupt_test_cleanup(void) {
    kprintln("INTERRUPT_TEST: Cleaning up test framework");

    // Remove our exception handlers
    for (int i = 0; i < 32; i++) {
        idt_install_exception_handler(i, NULL);
    }

    test_ctx.test_active = 0;
}

/*
 * Start a test
 */
void test_start(const char *name, int expected_exception) {
    test_ctx.test_active = 1;
    test_ctx.expected_exception = expected_exception;
    test_ctx.exception_occurred = 0;
    test_ctx.exception_vector = -1;
    test_ctx.resume_rip = 0;
    test_ctx.last_frame = NULL;

    // Copy test name
    int i = 0;
    while (name && name[i] && i < 63) {
        test_ctx.test_name[i] = name[i];
        i++;
    }
    test_ctx.test_name[i] = '\0';

    test_statistics.total_tests++;

    if (test_flags & TEST_FLAG_VERBOSE) {
        kprint("INTERRUPT_TEST: Starting test '");
        kprint(test_ctx.test_name);
        if (expected_exception >= 0) {
            kprint("' (expecting exception ");
            kprint_dec(expected_exception);
            kprintln(")");
        } else {
            kprintln("' (no exception expected)");
        }
    }
}

/*
 * End a test and return result
 */
int test_end(void) {
    int result = TEST_SUCCESS;

    if (test_ctx.expected_exception >= 0) {
        // We expected an exception
        if (test_ctx.exception_occurred &&
            test_ctx.exception_vector == test_ctx.expected_exception) {
            result = TEST_EXCEPTION_CAUGHT;
            test_statistics.passed_tests++;
        } else if (!test_ctx.exception_occurred) {
            result = TEST_NO_EXCEPTION;
            test_statistics.failed_tests++;
        } else {
            result = TEST_WRONG_EXCEPTION;
            test_statistics.failed_tests++;
        }
    } else {
        // We expected no exception
        if (test_ctx.exception_occurred) {
            result = TEST_FAILED;
            test_statistics.failed_tests++;
            test_statistics.unexpected_exceptions++;
        } else {
            result = TEST_SUCCESS;
            test_statistics.passed_tests++;
        }
    }

    // Log test results (safe to do here after handler has completed)
    if (test_flags & TEST_FLAG_VERBOSE) {
        kprint("INTERRUPT_TEST: Test '");
        kprint(test_ctx.test_name);
        kprint("' ");
        kprint(get_test_result_string(result));
        
        if (test_ctx.exception_occurred) {
            kprint(" - exception ");
            kprint_dec(test_ctx.exception_vector);
            kprint(" at RIP ");
            kprint_hex(test_ctx.test_rip);
        }
        kprintln("");
    }

    test_ctx.test_active = 0;
    test_ctx.resume_rip = 0;
    return result;
}

/*
 * Test exception handler
 * This handler is called during test execution and must be very careful
 * to avoid causing secondary faults
 */
void test_exception_handler(struct interrupt_frame *frame) {
    // Update test context first (minimal operations)
    if (test_ctx.test_active) {
        test_ctx.exception_occurred = 1;
        test_ctx.exception_vector = frame->vector;
        test_ctx.last_frame = frame;
        test_ctx.test_rip = frame->rip;
        test_statistics.exceptions_caught++;

        // Adjust RIP to resume execution
        if (test_ctx.resume_rip != 0) {
            frame->rip = test_ctx.resume_rip;
            test_ctx.resume_rip = 0;
        } else {
            // Fallback to basic instruction length emulation
            switch (frame->vector) {
                case EXCEPTION_INVALID_OPCODE:
                    frame->rip += 2;  // UD2 is 2 bytes
                    break;
                case EXCEPTION_BREAKPOINT:
                    frame->rip += 1;  // INT3 is 1 byte
                    break;
                case EXCEPTION_PAGE_FAULT:
                case EXCEPTION_GENERAL_PROTECTION:
                    frame->rip += 1;  // Best-effort - skip one byte
                    break;
                default:
                    frame->rip += 1;  // Best-effort fallback
                    break;
            }
        }
    }
    // Note: We intentionally don't log here to avoid causing secondary faults
    // Logging will be done after the exception handler returns
}

/*
 * Safe test execution wrapper
 */
int safe_execute_test(test_function_t test_func, const char *test_name, int expected_exception) {
    test_start(test_name, expected_exception);

    // Execute the test function
    test_func();

    return test_end();
}

/*
 * Test: Divide by zero
 */
__attribute__((noinline)) int test_divide_by_zero(void) {
    __label__ resume_point;
    void *resume = &&resume_point;
    if (0) goto *resume;

    test_set_resume_point(resume);
    __asm__ volatile (
        "movl $1, %%eax\n\t"
        "xorl %%edx, %%edx\n\t"
        "movl $0, %%ecx\n\t"
        "idivl %%ecx\n\t"
        :
        :
        : "rax", "rdx", "rcx", "memory", "cc");
resume_point:
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Invalid opcode  
 */
__attribute__((noinline)) int test_invalid_opcode(void) {
    // Use fallback - handler will skip the UD2 instruction (2 bytes)
    test_ctx.resume_rip = 0;  // Let handler use instruction length
    
    __asm__ volatile (
        "ud2\n\t"
        "nop\n\t"
        :
        :
        : "memory");
    
    return TEST_SUCCESS;
}

/*
 * Test: Page fault (read from unmapped memory)
 */
__attribute__((noinline)) int test_page_fault_read(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");
    
    test_ctx.resume_rip = resume_addr;
    
    __asm__ volatile (
        "movabs $0xDEADBEEF, %%rsi\n\t"
        "movb (%%rsi), %%al\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rax", "rsi", "memory");
    
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Page fault (write to unmapped memory)
 */
__attribute__((noinline)) int test_page_fault_write(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");
    
    test_ctx.resume_rip = resume_addr;
    
    __asm__ volatile (
        "movabs $0xDEADBEEF, %%rsi\n\t"
        "movb $0x42, (%%rsi)\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rsi", "memory");
    
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: General protection fault
 */
__attribute__((noinline)) int test_general_protection_fault(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");
    
    test_ctx.resume_rip = resume_addr;
    
    __asm__ volatile (
        "movw $0x1234, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rax", "memory");
    
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Breakpoint
 */
__attribute__((noinline)) int test_breakpoint(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");
    
    test_ctx.resume_rip = resume_addr;
    
    __asm__ volatile (
        "int3\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "memory");
    
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Stack overflow (stack fault)
 */
__attribute__((noinline)) int test_stack_overflow(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");
    
    test_ctx.resume_rip = resume_addr;
    
    __asm__ volatile (
        "movq %%rsp, %%rsi\n\t"
        "subq $0x100000, %%rsi\n\t"
        "movb (%%rsi), %%al\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rax", "rsi", "memory");
    
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Null pointer dereference
 */
__attribute__((noinline)) int test_null_pointer_dereference(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");
    
    test_ctx.resume_rip = resume_addr;
    
    __asm__ volatile (
        "xor %%rdi, %%rdi\n\t"
        "movb (%%rdi), %%al\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rax", "rdi", "memory");
    
    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Run basic exception tests
 */
int run_basic_exception_tests(void) {
    int total_passed = 0;
    int result;

    kprintln("INTERRUPT_TEST: Running basic exception tests");

    result = RUN_TEST(test_invalid_opcode, EXCEPTION_INVALID_OPCODE);
    if (result == TEST_EXCEPTION_CAUGHT) total_passed++;

    /*result = RUN_TEST(test_breakpoint, EXCEPTION_BREAKPOINT);
    if (result == TEST_EXCEPTION_CAUGHT) total_passed++;

    result = RUN_TEST(test_page_fault_read, EXCEPTION_PAGE_FAULT);
    if (result == TEST_EXCEPTION_CAUGHT) total_passed++;

    result = RUN_TEST(test_page_fault_write, EXCEPTION_PAGE_FAULT);
    if (result == TEST_EXCEPTION_CAUGHT) total_passed++;

    result = RUN_TEST(test_null_pointer_dereference, EXCEPTION_PAGE_FAULT);
    if (result == TEST_EXCEPTION_CAUGHT) total_passed++;*/

    kprint("INTERRUPT_TEST: Basic tests completed - ");
    kprint_dec(total_passed);
    kprint(" passed\n");

    return total_passed;
}

/*
 * Run all interrupt tests
 */
int run_all_interrupt_tests(void) {
    kprintln("INTERRUPT_TEST: Starting comprehensive interrupt tests");

    // Set verbose mode
    test_flags |= TEST_FLAG_VERBOSE;

    int basic_passed = run_basic_exception_tests();

    test_report_results();

    return basic_passed;
}

/*
 * Set test flags
 */
void test_set_flags(uint32_t flags) {
    test_flags = flags;
}

/*
 * Check if exception is expected
 */
int test_is_exception_expected(void) {
    return test_ctx.test_active && test_ctx.expected_exception >= 0;
}

void test_set_resume_point(void *rip) {
    test_ctx.resume_rip = (uint64_t)rip;
}

void test_clear_resume_point(void) {
    test_ctx.resume_rip = 0;
}

/*
 * Report test results
 */
void test_report_results(void) {
    kprintln("=== INTERRUPT TEST RESULTS ===");
    kprint("Total tests: ");
    kprint_dec(test_statistics.total_tests);
    kprintln("");

    kprint("Passed: ");
    kprint_dec(test_statistics.passed_tests);
    kprintln("");

    kprint("Failed: ");
    kprint_dec(test_statistics.failed_tests);
    kprintln("");

    kprint("Exceptions caught: ");
    kprint_dec(test_statistics.exceptions_caught);
    kprintln("");

    kprint("Unexpected exceptions: ");
    kprint_dec(test_statistics.unexpected_exceptions);
    kprintln("");

    if (test_statistics.total_tests > 0) {
        int success_rate = (test_statistics.passed_tests * 100) / test_statistics.total_tests;
        kprint("Success rate: ");
        kprint_dec(success_rate);
        kprintln("%");
    }

    kprintln("=== END TEST RESULTS ===");
}

/*
 * Get test statistics
 */
struct test_stats *test_get_stats(void) {
    return &test_statistics;
}

/*
 * Get test result string
 */
const char *get_test_result_string(int result) {
    switch (result) {
        case TEST_SUCCESS:
            return "PASSED";
        case TEST_EXCEPTION_CAUGHT:
            return "PASSED (exception caught as expected)";
        case TEST_FAILED:
            return "FAILED";
        case TEST_NO_EXCEPTION:
            return "FAILED (expected exception not triggered)";
        case TEST_WRONG_EXCEPTION:
            return "FAILED (wrong exception triggered)";
        default:
            return "UNKNOWN";
    }
}

/*
 * Dump test context for debugging
 */
void dump_test_context(void) {
    kprintln("=== TEST CONTEXT DUMP ===");
    kprint("Test active: ");
    kprintln(test_ctx.test_active ? "Yes" : "No");

    if (test_ctx.test_active) {
        kprint("Test name: ");
        kprint(test_ctx.test_name);
        kprintln("");

        kprint("Expected exception: ");
        if (test_ctx.expected_exception >= 0) {
            kprint_dec(test_ctx.expected_exception);
        } else {
            kprint("None");
        }
        kprintln("");

        kprint("Exception occurred: ");
        kprintln(test_ctx.exception_occurred ? "Yes" : "No");

        if (test_ctx.exception_occurred) {
            kprint("Exception vector: ");
            kprint_dec(test_ctx.exception_vector);
            kprintln("");
        }
    }

    kprintln("=== END TEST CONTEXT DUMP ===");
}

/*
 * Log test exception
 */
void log_test_exception(struct interrupt_frame *frame) {
    kprint("TEST_EXCEPTION: Vector ");
    kprint_dec(frame->vector);
    kprint(" at RIP ");
    kprint_hex(frame->rip);
    kprintln("");
}
