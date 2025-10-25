/*
 * SlopOS Interrupt Test Framework
 * Controlled exception generation and interrupt testing
 */

#include "interrupt_test.h"
#include "serial.h"
#include "apic.h"
#include "../boot/idt.h"
#include "../boot/constants.h"
#include "../mm/kernel_heap.h"
#include "../mm/phys_virt.h"
#include <stddef.h>
#include <stdint.h>

/* External paging helpers */
extern int map_page_4kb(uint64_t vaddr, uint64_t paddr, uint64_t flags);
extern int unmap_page(uint64_t vaddr);

// Global test state
static struct test_context test_ctx = {0};
static struct test_stats test_statistics = {0};
static uint32_t test_flags = 0;
static struct interrupt_test_config active_config = {
    .enabled = 0,
    .verbosity = INTERRUPT_TEST_VERBOSITY_SUMMARY,
    .suite_mask = INTERRUPT_TEST_SUITE_ALL,
    .timeout_ms = 0,
};
static uint64_t estimated_cycles_per_ms = 0;
static uint64_t test_timeout_cycles = 0;

static uint64_t read_tsc(void) {
    uint32_t low = 0;
    uint32_t high = 0;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | (uint64_t)low;
}

static uint64_t estimate_cycles_per_ms(void) {
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;

    cpuid(0, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x16) {
        cpuid(0x16, &eax, &ebx, &ecx, &edx);
        if (eax != 0) {
            return (uint64_t)eax * 1000ULL;
        }
    }

    /* Fallback assumption: 3 GHz base frequency */
    return 3000000ULL;
}

static void refresh_timeout_cache(void) {
    if (active_config.timeout_ms == 0) {
        test_timeout_cycles = 0;
        return;
    }

    if (estimated_cycles_per_ms == 0) {
        estimated_cycles_per_ms = estimate_cycles_per_ms();
        if (estimated_cycles_per_ms == 0) {
            estimated_cycles_per_ms = 3000000ULL;
        }
    }

    test_timeout_cycles = estimated_cycles_per_ms * (uint64_t)active_config.timeout_ms;
    if (test_timeout_cycles == 0) {
        /* Prevent zero timeout due to overflow */
        test_timeout_cycles = (uint64_t)active_config.timeout_ms * 3000000ULL;
    }
}

static uint64_t cycles_to_ms(uint64_t cycles) {
    if (estimated_cycles_per_ms == 0) {
        return 0;
    }
    return cycles / estimated_cycles_per_ms;
}

static void interrupt_test_apply_config(const struct interrupt_test_config *config) {
    if (config) {
        active_config = *config;
    } else {
        active_config.enabled = 1;
        active_config.verbosity = INTERRUPT_TEST_VERBOSITY_VERBOSE;
        active_config.suite_mask = INTERRUPT_TEST_SUITE_ALL;
        active_config.timeout_ms = 0;
    }

    test_flags = TEST_FLAG_CONTINUE_ON_FAIL;
    if (active_config.verbosity == INTERRUPT_TEST_VERBOSITY_VERBOSE) {
        test_flags |= TEST_FLAG_VERBOSE;
    } else {
        test_flags &= ~TEST_FLAG_VERBOSE;
    }

    refresh_timeout_cache();
}

/* Test case descriptor used for suite execution */
struct interrupt_test_case {
    test_function_t function;
    const char *name;
    int expected_vector;
};

#define TEST_CASE(fn, vec)      { (fn), #fn, (vec) }
#define TEST_CASE_NOEX(fn)      { (fn), #fn, -1 }

static int execute_test_suite(const char *suite_name,
                              const struct interrupt_test_case *cases,
                              size_t count);

/*
 * Initialize interrupt test framework
 */
void interrupt_test_init(const struct interrupt_test_config *config) {
    interrupt_test_apply_config(config);

    if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
        kprintln("INTERRUPT_TEST: Initializing test framework");
    }

    exception_set_mode(EXCEPTION_MODE_TEST);

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
    test_statistics.elapsed_ms = 0;
    test_statistics.timed_out = 0;

    // Install our exception handler for testing
    // Skip critical exceptions that should not be overridden
    for (int i = 0; i < 32; i++) {
        if (exception_is_critical((uint8_t)i)) {
            continue;
        }
        idt_install_exception_handler((uint8_t)i, test_exception_handler);
    }

    if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
        kprintln("INTERRUPT_TEST: Framework initialized");
    }
}

/*
 * Cleanup interrupt test framework
 */
void interrupt_test_cleanup(void) {
    if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
        kprintln("INTERRUPT_TEST: Cleaning up test framework");
    }

    // Remove our exception handlers
    for (int i = 0; i < 32; i++) {
        idt_install_exception_handler((uint8_t)i, NULL);
    }

    test_ctx.test_active = 0;

    exception_set_mode(EXCEPTION_MODE_NORMAL);

    if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
        kprintln("INTERRUPT_TEST: Framework cleanup complete");
    }
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
 * Update expected exception for current test
 */
void test_expect_exception(int vector) {
    if (!test_ctx.test_active) {
        test_ctx.expected_exception = vector;
        return;
    }

    test_ctx.expected_exception = vector;
    test_ctx.exception_occurred = 0;
    test_ctx.exception_vector = -1;
    test_ctx.resume_rip = 0;
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
 * Execute a suite of tests and report aggregated results
 */
static int execute_test_suite(const char *suite_name,
                              const struct interrupt_test_case *cases,
                              size_t count) {
    if (!cases || count == 0) {
        return 0;
    }

    if (suite_name && (test_flags & TEST_FLAG_VERBOSE)) {
        kprint("INTERRUPT_TEST: Running suite '");
        kprint(suite_name);
        kprintln("'");
    }

    int passed = 0;

    for (size_t i = 0; i < count; i++) {
        int result = safe_execute_test(cases[i].function,
                                       cases[i].name,
                                       cases[i].expected_vector);
        if (result == TEST_SUCCESS || result == TEST_EXCEPTION_CAUGHT) {
            passed++;
        } else if (!(test_flags & TEST_FLAG_CONTINUE_ON_FAIL)) {
            if (suite_name) {
                kprint("INTERRUPT_TEST: Aborting suite '");
                kprint(suite_name);
                kprintln("' due to failure");
            }
            break;
        }
    }

    if (suite_name && (test_flags & TEST_FLAG_VERBOSE)) {
        kprint("INTERRUPT_TEST: Suite '");
        kprint(suite_name);
        kprint("' - ");
        kprint_dec(passed);
        kprint(" / ");
        kprint_dec((int)count);
        kprintln(" tests passed");
    }

    return passed;
}

/*
 * Test memory allocation tracking for cleanup
 */
struct test_allocation_header {
    void *raw_ptr;
    size_t size;
};

static struct test_allocation_header *get_allocation_header(void *ptr) {
    if (!ptr) {
        return NULL;
    }

    uintptr_t addr = (uintptr_t)ptr;
    if (addr < sizeof(struct test_allocation_header)) {
        return NULL;
    }

    return (struct test_allocation_header*)(addr - sizeof(struct test_allocation_header));
}

/*
 * Allocate aligned test memory with optional zeroing
 */
void *allocate_test_memory(size_t size, int flags) {
    if (size == 0) {
        size = PAGE_SIZE_4KB;
    }

    size_t aligned_size = (size + PAGE_SIZE_4KB - 1) & ~(PAGE_SIZE_4KB - 1);
    size_t total_size = aligned_size + PAGE_SIZE_4KB + sizeof(struct test_allocation_header);

    uint8_t *raw = kmalloc(total_size);
    if (!raw) {
        kprintln("INTERRUPT_TEST: allocate_test_memory failed");
        return NULL;
    }

    uintptr_t base = (uintptr_t)(raw + sizeof(struct test_allocation_header));
    uintptr_t aligned_addr = (base + PAGE_SIZE_4KB - 1) & ~(PAGE_SIZE_4KB - 1);

    struct test_allocation_header *header =
        (struct test_allocation_header*)(aligned_addr - sizeof(struct test_allocation_header));
    header->raw_ptr = raw;
    header->size = aligned_size;

    if (flags & TEST_MEM_FLAG_ZERO) {
        uint8_t *buffer = (uint8_t*)aligned_addr;
        for (size_t i = 0; i < aligned_size; i++) {
            buffer[i] = 0;
        }
    }

    return (void*)aligned_addr;
}

/*
 * Free test memory allocation
 */
void free_test_memory(void *ptr) {
    if (!ptr) {
        return;
    }

    struct test_allocation_header *header = get_allocation_header(ptr);
    if (!header || !header->raw_ptr) {
        return;
    }

    kfree(header->raw_ptr);
}

/*
 * Map a single 4KB test page with specified flags
 */
int map_test_memory(uint64_t vaddr, uint64_t paddr, int flags) {
    if ((vaddr & (PAGE_SIZE_4KB - 1)) || (paddr & (PAGE_SIZE_4KB - 1))) {
        return -1;
    }
    return map_page_4kb(vaddr, paddr, (uint64_t)flags);
}

/*
 * Unmap a single 4KB test page
 */
int unmap_test_memory(uint64_t vaddr) {
    if (vaddr & (PAGE_SIZE_4KB - 1)) {
        return -1;
    }
    return unmap_page(vaddr);
}

/*
 * Test: Regular kernel memory access (no exception expected)
 */
__attribute__((noinline)) int test_kernel_memory_access(void) {
    uint8_t *buffer = allocate_test_memory(PAGE_SIZE_4KB, TEST_MEM_FLAG_ZERO);
    if (!buffer) {
        return TEST_FAILED;
    }

    for (size_t i = 0; i < 64; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    volatile uint32_t accumulator = 0;
    for (size_t i = 0; i < 64; i++) {
        accumulator += buffer[i];
    }
    (void)accumulator;

    free_test_memory(buffer);
    return TEST_SUCCESS;
}

/*
 * Test: Access unmapped memory page (expect page fault)
 */
__attribute__((noinline)) int test_unmapped_memory_access(void) {
    uint8_t *buffer = allocate_test_memory(PAGE_SIZE_4KB, TEST_MEM_FLAG_ZERO);
    if (!buffer) {
        return TEST_FAILED;
    }

    uint64_t phys_addr = mm_virt_to_phys((uint64_t)buffer);
    if (!phys_addr) {
        free_test_memory(buffer);
        return TEST_FAILED;
    }

    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");

    test_ctx.resume_rip = resume_addr;

    int unmapped = 0;
    int result = TEST_SUCCESS;

    if (unmap_test_memory((uint64_t)buffer) == 0) {
        unmapped = 1;

        __asm__ volatile (
            "mov %0, %%rsi\n\t"
            "movb (%%rsi), %%al\n\t"
            "1:\n\t"
            "nop\n\t"
            :
            : "r"(buffer)
            : "rsi", "rax", "memory");
    } else {
        result = TEST_FAILED;
    }

    test_clear_resume_point();

    if (unmapped) {
        if (map_test_memory((uint64_t)buffer, phys_addr, PAGE_KERNEL_RW) != 0) {
            result = TEST_FAILED;
        }
    }

    free_test_memory(buffer);
    return result;
}

/*
 * Test: Write to read-only mapped page (expect page fault)
 */
__attribute__((noinline)) int test_readonly_memory_write(void) {
    uint8_t *buffer = allocate_test_memory(PAGE_SIZE_4KB, TEST_MEM_FLAG_ZERO);
    if (!buffer) {
        return TEST_FAILED;
    }

    uint64_t phys_addr = mm_virt_to_phys((uint64_t)buffer);
    if (!phys_addr) {
        free_test_memory(buffer);
        return TEST_FAILED;
    }

    int ro_mapped = 0;
    int page_unmapped = 0;
    int result = TEST_SUCCESS;

    if (unmap_test_memory((uint64_t)buffer) == 0) {
        page_unmapped = 1;

        if (map_test_memory((uint64_t)buffer, phys_addr, PAGE_KERNEL_RO) == 0) {
            ro_mapped = 1;

            volatile uint64_t resume_addr;
            __asm__ volatile (
                "leaq 1f(%%rip), %0\n\t"
                : "=r"(resume_addr)
                :
                : "memory");

            test_ctx.resume_rip = resume_addr;

            __asm__ volatile (
                "movb $0xAB, (%0)\n\t"
                "1:\n\t"
                "nop\n\t"
                :
                : "r"(buffer)
                : "memory");

            test_clear_resume_point();
        } else {
            result = TEST_FAILED;
        }
    } else {
        result = TEST_FAILED;
    }

    if (ro_mapped) {
        if (unmap_test_memory((uint64_t)buffer) != 0 ||
            map_test_memory((uint64_t)buffer, phys_addr, PAGE_KERNEL_RW) != 0) {
            result = TEST_FAILED;
        }
    } else if (page_unmapped) {
        if (map_test_memory((uint64_t)buffer, phys_addr, PAGE_KERNEL_RW) != 0) {
            result = TEST_FAILED;
        }
    }

    free_test_memory(buffer);
    return result;
}

/*
 * Test: Execute code from dynamically allocated memory (no exception expected)
 */
__attribute__((noinline)) int test_executable_memory_access(void) {
    typedef int (*exec_test_func_t)(void);

    uint8_t *buffer = allocate_test_memory(PAGE_SIZE_4KB, TEST_MEM_FLAG_ZERO);
    if (!buffer) {
        return TEST_FAILED;
    }

    buffer[0] = 0xB8;  /* mov eax, 0x42 */
    buffer[1] = 0x42;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    buffer[4] = 0x00;
    buffer[5] = 0xC3;  /* ret */

    exec_test_func_t func = (exec_test_func_t)buffer;
    volatile int value = func();
    (void)value;

    free_test_memory(buffer);
    return TEST_SUCCESS;
}

/*
 * Test: Jump to invalid instruction pointer (expect page fault)
 */
__attribute__((noinline)) int test_invalid_instruction_pointer(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");

    test_ctx.resume_rip = resume_addr;

    __asm__ volatile (
        "movabs $0xDEADBEEF, %%rax\n\t"
        "call *%%rax\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rax", "memory");

    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Trigger general protection via explicit software interrupt
 */
__attribute__((noinline)) int test_privilege_violation(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");

    test_ctx.resume_rip = resume_addr;

    __asm__ volatile (
        "int $13\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "memory");

    test_clear_resume_point();
    return TEST_SUCCESS;
}

/*
 * Test: Load invalid segment selector (expect general protection fault)
 */
__attribute__((noinline)) int test_segment_violation(void) {
    volatile uint64_t resume_addr;
    __asm__ volatile (
        "leaq 1f(%%rip), %0\n\t"
        : "=r"(resume_addr)
        :
        : "memory");

    test_ctx.resume_rip = resume_addr;

    __asm__ volatile (
        "movw $0x0000, %%ax\n\t"
        "movw %%ax, %%fs\n\t"
        "1:\n\t"
        "nop\n\t"
        :
        :
        : "rax", "memory");

    test_clear_resume_point();
    return TEST_SUCCESS;
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
    static const struct interrupt_test_case basic_tests[] = {
        TEST_CASE(test_divide_by_zero, EXCEPTION_DIVIDE_ERROR),
        TEST_CASE(test_invalid_opcode, EXCEPTION_INVALID_OPCODE),
        TEST_CASE(test_breakpoint, EXCEPTION_BREAKPOINT),
    };

    size_t count = sizeof(basic_tests) / sizeof(basic_tests[0]);
    return execute_test_suite("Basic Exceptions", basic_tests, count);
}

/*
 * Run memory access tests
 */
int run_memory_access_tests(void) {
    static const struct interrupt_test_case memory_tests[] = {
        TEST_CASE(test_page_fault_read, EXCEPTION_PAGE_FAULT),
        TEST_CASE(test_page_fault_write, EXCEPTION_PAGE_FAULT),
        TEST_CASE(test_null_pointer_dereference, EXCEPTION_PAGE_FAULT),
        TEST_CASE(test_stack_overflow, EXCEPTION_PAGE_FAULT),
        TEST_CASE_NOEX(test_kernel_memory_access),
        TEST_CASE(test_unmapped_memory_access, EXCEPTION_PAGE_FAULT),
        TEST_CASE(test_readonly_memory_write, EXCEPTION_PAGE_FAULT),
        TEST_CASE_NOEX(test_executable_memory_access),
    };

    size_t count = sizeof(memory_tests) / sizeof(memory_tests[0]);
    return execute_test_suite("Memory Access", memory_tests, count);
}

/*
 * Run control flow tests
 */
int run_control_flow_tests(void) {
    static const struct interrupt_test_case control_tests[] = {
        TEST_CASE(test_general_protection_fault, EXCEPTION_GENERAL_PROTECTION),
        TEST_CASE(test_invalid_instruction_pointer, EXCEPTION_PAGE_FAULT),
        TEST_CASE(test_privilege_violation, EXCEPTION_GENERAL_PROTECTION),
        TEST_CASE(test_segment_violation, EXCEPTION_GENERAL_PROTECTION),
    };

    size_t count = sizeof(control_tests) / sizeof(control_tests[0]);
    return execute_test_suite("Control Flow", control_tests, count);
}

/*
 * Run all interrupt tests
 */
int run_all_interrupt_tests(const struct interrupt_test_config *config) {
    interrupt_test_apply_config(config);

    if (!active_config.enabled) {
        kprintln("INTERRUPT_TEST: Skipping interrupt tests (disabled)");
        return 0;
    }

    if (estimated_cycles_per_ms == 0) {
        estimated_cycles_per_ms = estimate_cycles_per_ms();
        if (estimated_cycles_per_ms == 0) {
            estimated_cycles_per_ms = 3000000ULL;
        }
    }

    if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
        kprintln("INTERRUPT_TEST: Starting interrupt test suites");
    }

    int total_passed = 0;
    int timed_out = 0;
    uint64_t start_cycles = read_tsc();
    uint64_t end_cycles = start_cycles;

    if (active_config.suite_mask & INTERRUPT_TEST_SUITE_BASIC) {
        total_passed += run_basic_exception_tests();
        end_cycles = read_tsc();
        if (test_timeout_cycles != 0 &&
            (end_cycles - start_cycles) > test_timeout_cycles) {
            timed_out = 1;
            if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
                kprintln("INTERRUPT_TEST: Timeout reached during basic exception tests");
            }
            goto finish_execution;
        }
    }
    if (active_config.suite_mask & INTERRUPT_TEST_SUITE_MEMORY) {
        total_passed += run_memory_access_tests();
        end_cycles = read_tsc();
        if (test_timeout_cycles != 0 &&
            (end_cycles - start_cycles) > test_timeout_cycles) {
            timed_out = 1;
            if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
                kprintln("INTERRUPT_TEST: Timeout reached during memory access tests");
            }
            goto finish_execution;
        }
    }
    if (active_config.suite_mask & INTERRUPT_TEST_SUITE_CONTROL) {
        total_passed += run_control_flow_tests();
        end_cycles = read_tsc();
        if (test_timeout_cycles != 0 &&
            (end_cycles - start_cycles) > test_timeout_cycles) {
            timed_out = 1;
            if (active_config.verbosity != INTERRUPT_TEST_VERBOSITY_QUIET) {
                kprintln("INTERRUPT_TEST: Timeout reached during control flow tests");
            }
            goto finish_execution;
        }
    }

finish_execution:
    if (test_flags & TEST_FLAG_VERBOSE) {
        kprint("INTERRUPT_TEST: Aggregate passed tests: ");
        kprint_dec(total_passed);
        kprintln("");
    }

    uint64_t elapsed_cycles = end_cycles - start_cycles;
    uint64_t elapsed_ms = cycles_to_ms(elapsed_cycles);
    if (elapsed_ms > 0xFFFFFFFFULL) {
        elapsed_ms = 0xFFFFFFFFULL;
    }
    test_statistics.elapsed_ms = (uint32_t)elapsed_ms;
    test_statistics.timed_out = timed_out;

    if (timed_out) {
        kprintln("INTERRUPT_TEST: Execution aborted due to timeout");
    }

    test_report_results();

    return total_passed;
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

    kprint("Elapsed (ms): ");
    kprint_dec(test_statistics.elapsed_ms);
    kprintln("");

    kprint("Timeout triggered: ");
    kprintln(test_statistics.timed_out ? "Yes" : "No");

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
