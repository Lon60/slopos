# Task 07: Implement Ramdisk Filesystem (In-Memory)

**Priority: 7**  
**Status: Pending**  
**Estimated Time: 3-4 hours**

## Objective
Create a simple in-memory filesystem (ramdisk) to store files without needing disk I/O.

## Current State
- ✅ Memory allocator available (kernel heap via `kmalloc`/`kfree`)
- ✅ Shell working with built-in commands
- ❌ No filesystem
- ❌ No way to store/read files

## Requirements

### 1. Create Ramdisk Module
Create new files:
- `fs/ramfs.c` - Ramdisk filesystem implementation
- `fs/ramfs.h` - Ramdisk API

### 2. File System Node Structure Requirements

**Node Structure Must Include:**
- File/directory name (string, fixed max length or dynamic)
- Node type (FILE or DIRECTORY)
- File size (for files only, 0 for directories)
- File data pointer (for files, NULL for directories)
- Parent directory pointer
- Children list (for directories - linked list or array)
- Sibling pointers (for linking nodes in same directory)

**Node Type Constants:**
- `RAMFS_TYPE_FILE` - Regular file
- `RAMFS_TYPE_DIRECTORY` - Directory

### 3. Root Directory Requirements
- Create root directory node at initialization
- Root name: "/"
- Root is a directory (no data, only children)
- Root has no parent (parent = NULL)
- Maintain root pointer for filesystem operations

### 4. Core Operations Required

#### Initialize Filesystem
- `int ramfs_init(void)` - Initialize ramdisk
  - Create root directory
  - Return 0 on success, -1 on failure

#### Create File
- `ramfs_node_t *ramfs_create_file(const char *path, const void *data, size_t size)`
  - Parse path string
  - Find or create parent directory
  - Create file node
  - Allocate data buffer if size > 0
  - Copy data if provided
  - Link into parent's children list
  - Return node pointer or NULL on error

#### Create Directory
- `ramfs_node_t *ramfs_create_directory(const char *path)`
  - Similar to create_file, but type = DIRECTORY
  - No data buffer needed
  - Initialize children list to empty
  - Return node pointer or NULL on error

#### Read File
- `int ramfs_read_file(const char *path, void *buffer, size_t buffer_size, size_t *bytes_read)`
  - Find file node by path
  - Verify it's a file (not directory)
  - Copy data to buffer (up to buffer_size or file size)
  - Set bytes_read parameter
  - Return 0 on success, -1 on error

#### Write/Update File
- `int ramfs_write_file(const char *path, const void *data, size_t size)`
  - Find file node (or create if doesn't exist)
  - Reallocate data buffer if size increased
  - Copy new data
  - Update file size
  - Return 0 on success, -1 on error

#### List Directory
- `int ramfs_list_directory(const char *path, ramfs_node_t **entries, int *count)`
  - Find directory node by path
  - Walk children linked list
  - Return array of entry pointers (allocate or use static buffer)
  - Set count parameter
  - Return 0 on success, -1 on error

#### Find Node
- `ramfs_node_t *ramfs_find_node(const char *path)`
  - Parse path string
  - Traverse directory tree from root
  - Return node pointer or NULL if not found

### 5. Path Parsing Requirements
- Support absolute paths starting with "/"
- Split path into components (e.g., "/path/to/file" → ["path", "to", "file"])
- Traverse from root to target node
- Handle path components correctly
- Optional: Support "." (current) and ".." (parent) for convenience

### 6. Memory Management Requirements
- Use kernel heap (`kmalloc`/`kfree`) for all allocations
- Files store data in heap-allocated buffers
- Directories store linked lists of child nodes
- Handle allocation failures gracefully
- No persistence needed (all lost on reboot)

### 7. Optional: Sample Files
- Create sample files/directories at initialization for testing
- Examples: `/etc/readme.txt`, `/tmp` directory
- Helps verify filesystem works correctly

## Testing Requirements
1. Initialize ramdisk and verify root exists
2. Create file in root: `/test.txt` with content "hello"
3. Read file back and verify content matches
4. Create directory: `/mydir`
5. List root directory: should show both file and directory
6. Create file in subdirectory: `/mydir/file.txt`
7. Verify nested path traversal works

## Success Criteria
- [ ] Can create files in root directory
- [ ] Can read files back correctly (content matches)
- [ ] Can create subdirectories
- [ ] Can create files in subdirectories
- [ ] Can list directory contents (files and directories)
- [ ] Path parsing works correctly (absolute paths)
- [ ] Filesystem handles nested directory structures

## Files to Create/Modify
- `fs/ramfs.c` (NEW)
- `fs/ramfs.h` (NEW)
- `boot/early_init.c` (MODIFY - initialize ramdisk)
- `meson.build` (MODIFY - add fs/ sources)

## Technical Notes
- Start simple: basic tree structure with files and directories
- Use linked lists for directory children (simple to implement)
- All data stored in kernel heap (no disk I/O)
- No persistence: all data lost on reboot (expected for ramdisk)
- Can enhance later: file permissions, timestamps, symlinks, etc.
- Consider: flat structure initially vs nested from start (recommend nested)
