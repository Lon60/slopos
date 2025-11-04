#ifndef LIB_STACKTRACE_H
#define LIB_STACKTRACE_H

#include <stdint.h>

struct stacktrace_entry {
    uint64_t frame_pointer;
    uint64_t return_address;
};

int stacktrace_capture(struct stacktrace_entry *entries, int max_entries);
int stacktrace_capture_from(uint64_t rbp,
                            struct stacktrace_entry *entries,
                            int max_entries);
void stacktrace_dump(int max_frames);
void stacktrace_dump_from(uint64_t rbp, int max_frames);

#endif /* LIB_STACKTRACE_H */
