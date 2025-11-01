#ifndef FS_FILEIO_H
#define FS_FILEIO_H

#include <stddef.h>
#include <stdint.h>

#include "ramfs.h"

#ifndef __FILEIO_SSIZE_T_DEFINED
typedef long ssize_t;
#define __FILEIO_SSIZE_T_DEFINED
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#define FILE_OPEN_READ   (1u << 0)
#define FILE_OPEN_WRITE  (1u << 1)
#define FILE_OPEN_CREAT  (1u << 2)
#define FILE_OPEN_APPEND (1u << 3)

#define FILEIO_MAX_OPEN_FILES 32

typedef struct file_descriptor {
    ramfs_node_t *node;
    size_t position;
    uint32_t flags;
    int valid;
} file_descriptor_t;

void fileio_init(void);
int file_open(const char *path, uint32_t flags);
ssize_t file_read(int fd, void *buffer, size_t count);
ssize_t file_write(int fd, const void *buffer, size_t count);
int file_close(int fd);
int file_seek(int fd, uint64_t offset, int whence);
size_t file_get_size(int fd);
int file_exists(const char *path);

#endif /* FS_FILEIO_H */
