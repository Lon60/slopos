/*
 * Interrupt test configuration helpers
 * Parses compile-time defaults and runtime kernel command line options
 */

#include "interrupt_test_config.h"

#define TOKEN_BUFFER_SIZE 128

static size_t local_strlen(const char *str) {
    size_t len = 0;
    if (!str) {
        return 0;
    }
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static int char_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char to_lower_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static int string_equals_ci(const char *lhs, const char *rhs) {
    if (!lhs || !rhs) {
        return 0;
    }
    while (*lhs && *rhs) {
        if (to_lower_char(*lhs) != to_lower_char(*rhs)) {
            return 0;
        }
        lhs++;
        rhs++;
    }
    return *lhs == '\0' && *rhs == '\0';
}

static int string_equals_n_ci(const char *lhs, const char *rhs, size_t count) {
    if (!lhs || !rhs) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        char lc = lhs[i];
        char rc = rhs[i];
        if (lc == '\0' || rc == '\0') {
            return 0;
        }
        if (to_lower_char(lc) != to_lower_char(rc)) {
            return 0;
        }
    }
    return 1;
}

static enum interrupt_test_verbosity verbosity_from_string(const char *value) {
    if (!value) {
        return INTERRUPT_TEST_VERBOSITY_SUMMARY;
    }
    if (string_equals_ci(value, "quiet")) {
        return INTERRUPT_TEST_VERBOSITY_QUIET;
    }
    if (string_equals_ci(value, "verbose")) {
        return INTERRUPT_TEST_VERBOSITY_VERBOSE;
    }
    return INTERRUPT_TEST_VERBOSITY_SUMMARY;
}

static uint32_t suite_from_string(const char *value) {
    if (!value) {
        return INTERRUPT_TEST_SUITE_ALL;
    }
    if (string_equals_ci(value, "none") || string_equals_ci(value, "off")) {
        return 0;
    }
    if (string_equals_ci(value, "all")) {
        return INTERRUPT_TEST_SUITE_ALL;
    }
    if (string_equals_ci(value, "basic")) {
        return INTERRUPT_TEST_SUITE_BASIC;
    }
    if (string_equals_ci(value, "memory")) {
        return INTERRUPT_TEST_SUITE_MEMORY;
    }
    if (string_equals_ci(value, "control")) {
        return INTERRUPT_TEST_SUITE_CONTROL;
    }
    if (string_equals_ci(value, "basic+memory") ||
        string_equals_ci(value, "memory+basic")) {
        return INTERRUPT_TEST_SUITE_BASIC | INTERRUPT_TEST_SUITE_MEMORY;
    }
    if (string_equals_ci(value, "basic+control") ||
        string_equals_ci(value, "control+basic")) {
        return INTERRUPT_TEST_SUITE_BASIC | INTERRUPT_TEST_SUITE_CONTROL;
    }
    if (string_equals_ci(value, "memory+control") ||
        string_equals_ci(value, "control+memory")) {
        return INTERRUPT_TEST_SUITE_MEMORY | INTERRUPT_TEST_SUITE_CONTROL;
    }
    return INTERRUPT_TEST_SUITE_ALL;
}

static uint32_t parse_u32(const char *value, uint32_t fallback) {
    if (!value) {
        return fallback;
    }

    uint64_t result = 0;
    size_t index = 0;

    while (value[index] != '\0') {
        char c = value[index];
        if (c >= '0' && c <= '9') {
            result = (result * 10) + (uint64_t)(c - '0');
            if (result > 0xFFFFFFFFull) {
                return 0xFFFFFFFFu;
            }
            index++;
            continue;
        }

        /* Allow optional "ms" suffix */
        if ((c == 'm' || c == 'M') &&
            (value[index + 1] == 's' || value[index + 1] == 'S')) {
            index += 2;
            break;
        }

        /* Unexpected character - abort parsing */
        return fallback;
    }

    (void)index;
    return (uint32_t)result;
}

static int parse_on_off_flag(const char *value, int current) {
    if (!value) {
        return current;
    }

    if (string_equals_ci(value, "on") ||
        string_equals_ci(value, "true") ||
        string_equals_ci(value, "yes") ||
        string_equals_ci(value, "enabled") ||
        string_equals_ci(value, "1")) {
        return 1;
    }

    if (string_equals_ci(value, "off") ||
        string_equals_ci(value, "false") ||
        string_equals_ci(value, "no") ||
        string_equals_ci(value, "disabled") ||
        string_equals_ci(value, "0")) {
        return 0;
    }

    return current;
}

static void apply_enable_token(struct interrupt_test_config *config,
                               const char *value) {
    if (!config || !value) {
        return;
    }

    if (string_equals_ci(value, "on") || string_equals_ci(value, "true") ||
        string_equals_ci(value, "enabled")) {
        config->enabled = 1;
        return;
    }

    if (string_equals_ci(value, "off") || string_equals_ci(value, "false") ||
        string_equals_ci(value, "disabled")) {
        config->enabled = 0;
        config->shutdown_on_complete = 0;
        return;
    }

    /* Interpret suite names as implicit enable */
    uint32_t suite = suite_from_string(value);
    if (suite != 0) {
        config->enabled = 1;
        config->suite_mask = suite;
    } else {
        config->enabled = 0;
        config->suite_mask = 0;
        config->shutdown_on_complete = 0;
    }
}

static void process_token(struct interrupt_test_config *config,
                          const char *token) {
    if (!config || !token) {
        return;
    }

    /* Accept both itests.* and interrupt_tests.* prefixes */
    const char *value = NULL;

    if (string_equals_n_ci(token, "itests=", 7)) {
        value = token + 7;
        apply_enable_token(config, value);
        return;
    }

    if (string_equals_n_ci(token, "interrupt_tests=", 16)) {
        value = token + 16;
        apply_enable_token(config, value);
        return;
    }

    if (string_equals_n_ci(token, "itests.suite=", 13)) {
        value = token + 13;
        uint32_t suite = suite_from_string(value);
        config->suite_mask = suite;
        if (suite != 0) {
            config->enabled = 1;
        }
        return;
    }

    if (string_equals_n_ci(token, "interrupt_tests.suite=", 22)) {
        value = token + 22;
        uint32_t suite = suite_from_string(value);
        config->suite_mask = suite;
        if (suite != 0) {
            config->enabled = 1;
        }
        return;
    }

    if (string_equals_n_ci(token, "itests.verbosity=", 17)) {
        value = token + 17;
        config->verbosity = verbosity_from_string(value);
        return;
    }

    if (string_equals_n_ci(token, "interrupt_tests.verbosity=", 26)) {
        value = token + 26;
        config->verbosity = verbosity_from_string(value);
        return;
    }

    if (string_equals_n_ci(token, "itests.timeout=", 15)) {
        value = token + 15;
        config->timeout_ms = parse_u32(value, config->timeout_ms);
        return;
    }

    if (string_equals_n_ci(token, "interrupt_tests.timeout=", 24)) {
        value = token + 24;
        config->timeout_ms = parse_u32(value, config->timeout_ms);
        return;
    }

    if (string_equals_n_ci(token, "itests.shutdown=", 16)) {
        value = token + 16;
        config->shutdown_on_complete = parse_on_off_flag(value, config->shutdown_on_complete);
        return;
    }

    if (string_equals_n_ci(token, "interrupt_tests.shutdown=", 25)) {
        value = token + 25;
        config->shutdown_on_complete = parse_on_off_flag(value, config->shutdown_on_complete);
        return;
    }

    if (string_equals_n_ci(token, "itests.stacktrace_demo=", 23)) {
        value = token + 23;
        config->stacktrace_demo = parse_on_off_flag(value, config->stacktrace_demo);
        return;
    }

    if (string_equals_n_ci(token, "interrupt_tests.stacktrace_demo=", 32)) {
        value = token + 32;
        config->stacktrace_demo = parse_on_off_flag(value, config->stacktrace_demo);
        return;
    }
}

void interrupt_test_config_init_defaults(struct interrupt_test_config *config) {
    if (!config) {
        return;
    }

    config->enabled = INTERRUPT_TESTS_DEFAULT_ENABLED ? 1 : 0;
    config->timeout_ms = (uint32_t)INTERRUPT_TESTS_DEFAULT_TIMEOUT_MS;
    config->verbosity = verbosity_from_string(INTERRUPT_TESTS_DEFAULT_VERBOSITY);
    config->suite_mask = suite_from_string(INTERRUPT_TESTS_DEFAULT_SUITE);
    config->shutdown_on_complete = INTERRUPT_TESTS_DEFAULT_SHUTDOWN ? 1 : 0;
    config->stacktrace_demo = 0;
}

void interrupt_test_config_parse_cmdline(struct interrupt_test_config *config,
                                         const char *cmdline) {
    if (!config || !cmdline) {
        return;
    }

    size_t len = local_strlen(cmdline);
    if (len == 0) {
        return;
    }

    const char *cursor = cmdline;
    while (*cursor != '\0') {
        while (*cursor != '\0' && char_is_space(*cursor)) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char token[TOKEN_BUFFER_SIZE];
        size_t index = 0;

        while (*cursor != '\0' && !char_is_space(*cursor)) {
            if (index < (TOKEN_BUFFER_SIZE - 1)) {
                token[index++] = *cursor;
            }
            cursor++;
        }
        token[index] = '\0';

        if (index > 0) {
            process_token(config, token);
        }
    }
}

const char *interrupt_test_verbosity_string(enum interrupt_test_verbosity verbosity) {
    switch (verbosity) {
        case INTERRUPT_TEST_VERBOSITY_QUIET:
            return "quiet";
        case INTERRUPT_TEST_VERBOSITY_VERBOSE:
            return "verbose";
        case INTERRUPT_TEST_VERBOSITY_SUMMARY:
        default:
            return "summary";
    }
}

const char *interrupt_test_suite_string(uint32_t suite_mask) {
    if (suite_mask == 0) {
        return "none";
    }
    if (suite_mask == INTERRUPT_TEST_SUITE_ALL) {
        return "all";
    }
    if (suite_mask == INTERRUPT_TEST_SUITE_BASIC) {
        return "basic";
    }
    if (suite_mask == INTERRUPT_TEST_SUITE_MEMORY) {
        return "memory";
    }
    if (suite_mask == INTERRUPT_TEST_SUITE_CONTROL) {
        return "control";
    }
    if (suite_mask == (INTERRUPT_TEST_SUITE_BASIC | INTERRUPT_TEST_SUITE_MEMORY)) {
        return "basic+memory";
    }
    if (suite_mask == (INTERRUPT_TEST_SUITE_BASIC | INTERRUPT_TEST_SUITE_CONTROL)) {
        return "basic+control";
    }
    if (suite_mask == (INTERRUPT_TEST_SUITE_MEMORY | INTERRUPT_TEST_SUITE_CONTROL)) {
        return "memory+control";
    }
    return "custom";
}
