#ifndef LIB_UNIT_TEST_H
#define LIB_UNIT_TEST_H

#include <stddef.h>
#include <stdint.h>

enum unit_test_status {
    UNIT_TEST_STATUS_PASS = 0,
    UNIT_TEST_STATUS_FAIL = 1,
    UNIT_TEST_STATUS_SKIP = 2,
    UNIT_TEST_STATUS_EXPECTED_FAIL = 3
};

struct unit_test_stats {
    uint32_t total_cases;
    uint32_t passed_cases;
    uint32_t failed_cases;
    uint32_t skipped_cases;
    uint32_t expected_failures;
    uint32_t unexpected_failures;
};

struct unit_test_case {
    const char *name;
    enum unit_test_status (*execute)(void *context);
    void *context;
};

struct unit_test_suite {
    const char *name;
    const struct unit_test_case *cases;
    size_t case_count;
};

struct unit_test_runner {
    const char *name;
    const char *current_case;
    struct unit_test_stats *stats;
};

void unit_test_runner_init(struct unit_test_runner *runner,
                           const char *name,
                           struct unit_test_stats *stats);
void unit_test_runner_begin_case(struct unit_test_runner *runner,
                                 const char *case_name);
void unit_test_runner_finish_case(struct unit_test_runner *runner,
                                  enum unit_test_status status);
void unit_test_runner_report(const struct unit_test_runner *runner);

const char *unit_test_status_string(enum unit_test_status status);

int unit_test_run_suite(struct unit_test_runner *runner,
                        const struct unit_test_suite *suite);

#endif /* LIB_UNIT_TEST_H */
