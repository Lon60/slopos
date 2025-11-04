#include "string.h"

size_t strlen(const char *str) {
    if (!str) {
        return 0;
    }

    size_t length = 0;
    while (str[length] != '\0') {
        length++;
    }

    return length;
}

int strcmp(const char *lhs, const char *rhs) {
    if (lhs == rhs) {
        return 0;
    }

    if (!lhs) {
        return -1;
    }

    if (!rhs) {
        return 1;
    }

    while (*lhs && (*lhs == *rhs)) {
        lhs++;
        rhs++;
    }

    return (unsigned char)*lhs - (unsigned char)*rhs;
}

int strncmp(const char *lhs, const char *rhs, size_t n) {
    if (n == 0) {
        return 0;
    }

    if (!lhs) {
        return rhs ? -1 : 0;
    }

    if (!rhs) {
        return 1;
    }

    while (n > 0 && *lhs == *rhs) {
        if (*lhs == '\0') {
            return 0;
        }

        lhs++;
        rhs++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return (unsigned char)*lhs - (unsigned char)*rhs;
}

char *strcpy(char *dest, const char *src) {
    if (!dest || !src) {
        return dest;
    }

    char *out = dest;
    while ((*out++ = *src++) != '\0') {
        /* Copy including null terminator */
    }

    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    if (!dest || n == 0) {
        return dest;
    }

    size_t i = 0;
    for (; i < n && src && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}
