/*
 * Interrupt test configuration helpers
 * Provides compile-time defaults and runtime command line parsing
 */

#ifndef INTERRUPT_TEST_CONFIG_H
#define INTERRUPT_TEST_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#ifndef INTERRUPT_TESTS_DEFAULT_ENABLED
#define INTERRUPT_TESTS_DEFAULT_ENABLED 0
#endif

#ifndef INTERRUPT_TESTS_DEFAULT_TIMEOUT_MS
#define INTERRUPT_TESTS_DEFAULT_TIMEOUT_MS 0
#endif

#ifndef INTERRUPT_TESTS_DEFAULT_SUITE
#define INTERRUPT_TESTS_DEFAULT_SUITE "all"
#endif

#ifndef INTERRUPT_TESTS_DEFAULT_VERBOSITY
#define INTERRUPT_TESTS_DEFAULT_VERBOSITY "summary"
#endif

#ifndef INTERRUPT_TESTS_DEFAULT_SHUTDOWN
#define INTERRUPT_TESTS_DEFAULT_SHUTDOWN 0
#endif

/* Verbosity levels */
enum interrupt_test_verbosity {
    INTERRUPT_TEST_VERBOSITY_QUIET = 0,
    INTERRUPT_TEST_VERBOSITY_SUMMARY = 1,
    INTERRUPT_TEST_VERBOSITY_VERBOSE = 2,
};

/* Suite masks */
#define INTERRUPT_TEST_SUITE_BASIC      (1u << 0)
#define INTERRUPT_TEST_SUITE_MEMORY     (1u << 1)
#define INTERRUPT_TEST_SUITE_CONTROL    (1u << 2)
#define INTERRUPT_TEST_SUITE_SCHEDULER   (1u << 3)
#define INTERRUPT_TEST_SUITE_ALL        (INTERRUPT_TEST_SUITE_BASIC | \
                                         INTERRUPT_TEST_SUITE_MEMORY | \
                                         INTERRUPT_TEST_SUITE_CONTROL | \
                                         INTERRUPT_TEST_SUITE_SCHEDULER)

struct interrupt_test_config {
    int enabled;
    enum interrupt_test_verbosity verbosity;
    uint32_t suite_mask;
    uint32_t timeout_ms;
    int shutdown_on_complete;
    int stacktrace_demo;
};

void interrupt_test_config_init_defaults(struct interrupt_test_config *config);
void interrupt_test_config_parse_cmdline(struct interrupt_test_config *config,
                                         const char *cmdline);
const char *interrupt_test_verbosity_string(enum interrupt_test_verbosity verbosity);
const char *interrupt_test_suite_string(uint32_t suite_mask);

#endif /* INTERRUPT_TEST_CONFIG_H */
