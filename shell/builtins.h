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

int builtin_help(int argc, char **argv);
int builtin_echo(int argc, char **argv);
int builtin_clear(int argc, char **argv);
int builtin_halt(int argc, char **argv);
int builtin_info(int argc, char **argv);
int builtin_ls(int argc, char **argv);
int builtin_cat(int argc, char **argv);
int builtin_write(int argc, char **argv);
int builtin_mkdir(int argc, char **argv);
int builtin_rm(int argc, char **argv);

#endif /* SHELL_BUILTINS_H */
