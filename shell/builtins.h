#ifndef SHELL_BUILTINS_H
#define SHELL_BUILTINS_H

#include <stddef.h>

typedef int (*shell_builtin_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    shell_builtin_handler_t handler;
    const char *description;
} shell_builtin_t;

const shell_builtin_t *shell_builtin_lookup(const char *name);
const shell_builtin_t *shell_builtin_list(size_t *count);

#endif /* SHELL_BUILTINS_H */
