/*
 * Boot logging interface
 * Provides early boot logging with adjustable verbosity so that
 * subsystems can emit debug traces without spamming normal boots.
 */

#ifndef BOOT_LOG_H
#define BOOT_LOG_H

#include <stdint.h>

enum boot_log_level {
    BOOT_LOG_LEVEL_ERROR = 0,
    BOOT_LOG_LEVEL_INFO  = 1,
    BOOT_LOG_LEVEL_DEBUG = 2,
};

void boot_log_set_level(enum boot_log_level level);
enum boot_log_level boot_log_get_level(void);
int boot_log_is_enabled(enum boot_log_level level);

void boot_log_attach_serial(void);

void boot_log_line(enum boot_log_level level, const char *text);
void boot_log_raw(enum boot_log_level level, const char *text);
void boot_log_error(const char *text);
void boot_log_info(const char *text);
void boot_log_debug(const char *text);
void boot_log_newline(void);

/* Convenience helpers so callers can gate arbitrary statements. */
#define BOOT_LOG_BLOCK(level, code) \
    do { \
        if (boot_log_is_enabled(level)) { \
            code; \
        } \
    } while (0)

#endif /* BOOT_LOG_H */
