#include "fileio.h"

#include "../lib/memory.h"
#include "../mm/kernel_heap.h"

static file_descriptor_t file_descriptors[FILEIO_MAX_OPEN_FILES];
static int fileio_initialized = 0;

static void fileio_reset_descriptor(file_descriptor_t *desc) {
    if (!desc) {
        return;
    }
    desc->node = NULL;
    desc->position = 0;
    desc->flags = 0;
    desc->valid = 0;
}

void fileio_init(void) {
    for (int i = 0; i < FILEIO_MAX_OPEN_FILES; i++) {
        fileio_reset_descriptor(&file_descriptors[i]);
    }
    fileio_initialized = 1;
}

static void fileio_ensure_initialized(void) {
    if (!fileio_initialized) {
        fileio_init();
    }
}

static file_descriptor_t *fileio_get_descriptor(int fd) {
    if (fd < 0 || fd >= FILEIO_MAX_OPEN_FILES) {
        return NULL;
    }
    file_descriptor_t *desc = &file_descriptors[fd];
    if (!desc->valid) {
        return NULL;
    }
    return desc;
}

static int fileio_find_free_slot(void) {
    for (int i = 0; i < FILEIO_MAX_OPEN_FILES; i++) {
        if (!file_descriptors[i].valid) {
            return i;
        }
    }
    return -1;
}

static int fileio_ensure_capacity(ramfs_node_t *node, size_t required_size) {
    if (!node) {
        return -1;
    }

    if (required_size <= node->size) {
        if (node->size > 0 && !node->data) {
            void *new_data = kmalloc(node->size);
            if (!new_data) {
                return -1;
            }
            memset(new_data, 0, node->size);
            node->data = new_data;
        }
        return 0;
    }

    void *new_data = kmalloc(required_size);
    if (!new_data) {
        return -1;
    }

    if (node->size > 0 && node->data) {
        memcpy(new_data, node->data, node->size);
    } else if (node->size > 0) {
        memset(new_data, 0, node->size);
    }

    size_t gap = required_size - node->size;
    if (gap > 0) {
        memset((uint8_t *)new_data + node->size, 0, gap);
    }

    if (node->data) {
        kfree(node->data);
    }

    node->data = new_data;
    node->size = required_size;
    return 0;
}

int file_open(const char *path, uint32_t flags) {
    fileio_ensure_initialized();

    if (!path || !(flags & (FILE_OPEN_READ | FILE_OPEN_WRITE))) {
        return -1;
    }

    if ((flags & FILE_OPEN_APPEND) && !(flags & FILE_OPEN_WRITE)) {
        return -1;
    }

    ramfs_node_t *node = ramfs_find_node(path);
    if (!node) {
        if (flags & FILE_OPEN_CREAT) {
            node = ramfs_create_file(path, NULL, 0);
        }
        if (!node) {
            return -1;
        }
    }

    if (node->type != RAMFS_TYPE_FILE) {
        return -1;
    }

    int slot = fileio_find_free_slot();
    if (slot < 0) {
        return -1;
    }

    file_descriptor_t *desc = &file_descriptors[slot];
    desc->node = node;
    desc->flags = flags;
    desc->position = (flags & FILE_OPEN_APPEND) ? node->size : 0;
    desc->valid = 1;

    return slot;
}

ssize_t file_read(int fd, void *buffer, size_t count) {
    if (count == 0) {
        return 0;
    }

    if (!buffer) {
        return -1;
    }

    file_descriptor_t *desc = fileio_get_descriptor(fd);
    if (!desc || !(desc->flags & FILE_OPEN_READ)) {
        return -1;
    }

    ramfs_node_t *node = desc->node;
    if (!node || node->type != RAMFS_TYPE_FILE) {
        return -1;
    }

    if (desc->position >= node->size) {
        return 0;
    }

    size_t remaining = node->size - desc->position;
    size_t to_read = count < remaining ? count : remaining;

    if (to_read > 0) {
        if (!node->data) {
            return -1;
        }
        uint8_t *src = (uint8_t *)node->data + desc->position;
        memcpy(buffer, src, to_read);
        desc->position += to_read;
    }

    return (ssize_t)to_read;
}

ssize_t file_write(int fd, const void *buffer, size_t count) {
    if (count == 0) {
        return 0;
    }

    if (!buffer) {
        return -1;
    }

    file_descriptor_t *desc = fileio_get_descriptor(fd);
    if (!desc || !(desc->flags & FILE_OPEN_WRITE)) {
        return -1;
    }

    ramfs_node_t *node = desc->node;
    if (!node || node->type != RAMFS_TYPE_FILE) {
        return -1;
    }

    if (count > SIZE_MAX - desc->position) {
        return -1;
    }

    size_t required_size = desc->position + count;
    if (fileio_ensure_capacity(node, required_size) != 0) {
        return -1;
    }

    if (required_size > 0 && !node->data) {
        return -1;
    }

    memcpy((uint8_t *)node->data + desc->position, buffer, count);

    desc->position += count;
    if (desc->position > node->size) {
        node->size = desc->position;
    }

    return (ssize_t)count;
}

int file_close(int fd) {
    file_descriptor_t *desc = fileio_get_descriptor(fd);
    if (!desc) {
        return -1;
    }
    fileio_reset_descriptor(desc);
    return 0;
}

int file_seek(int fd, uint64_t offset, int whence) {
    file_descriptor_t *desc = fileio_get_descriptor(fd);
    if (!desc) {
        return -1;
    }

    ramfs_node_t *node = desc->node;
    if (!node || node->type != RAMFS_TYPE_FILE) {
        return -1;
    }

    size_t new_position = desc->position;
    if (offset > SIZE_MAX) {
        return -1;
    }
    size_t delta = (size_t)offset;

    switch (whence) {
        case SEEK_SET:
            if (delta > node->size) {
                return -1;
            }
            new_position = delta;
            break;
        case SEEK_CUR:
            if (delta > SIZE_MAX - desc->position) {
                return -1;
            }
            if (desc->position + delta > node->size) {
                return -1;
            }
            new_position = desc->position + delta;
            break;
        case SEEK_END:
            if (delta > node->size) {
                return -1;
            }
            new_position = node->size - delta;
            break;
        default:
            return -1;
    }

    desc->position = new_position;
    return 0;
}

size_t file_get_size(int fd) {
    file_descriptor_t *desc = fileio_get_descriptor(fd);
    if (!desc || !desc->node || desc->node->type != RAMFS_TYPE_FILE) {
        return (size_t)-1;
    }
    return desc->node->size;
}

int file_exists(const char *path) {
    if (!path) {
        return 0;
    }
    ramfs_node_t *node = ramfs_find_node(path);
    if (!node || node->type != RAMFS_TYPE_FILE) {
        return 0;
    }
    return 1;
}
