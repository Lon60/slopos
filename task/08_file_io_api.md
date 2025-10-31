# Task 08: Create File I/O API

**Priority: 8**  
**Status: Pending**  
**Estimated Time: 2-3 hours**

## Objective
Create a file I/O API (open, read, write, close) that abstracts the ramdisk filesystem.

## Current State
- ✅ (After Task 07) Ramdisk filesystem working (nodes, create, read, write)
- ❌ No file descriptor system
- ❌ No unified file I/O API

## Requirements

### 1. Create File I/O Module
Create new files:
- `fs/fileio.c` - File I/O implementation
- `fs/fileio.h` - File I/O API

### 2. File Descriptor System Requirements

**File Descriptor Table:**
- Array of file descriptor structures
- Maximum open files: 32 (adjustable)
- Track which slots are in use

**File Descriptor Structure Must Include:**
- Pointer to ramfs node
- Current read/write position (offset)
- Open flags (read, write, etc.)
- Validity flag (slot in use)

### 3. Open Flags Required
Define constants for file opening:
- `FILE_OPEN_READ` - Open for reading
- `FILE_OPEN_WRITE` - Open for writing
- `FILE_OPEN_CREAT` - Create file if doesn't exist
- `FILE_OPEN_APPEND` - Append mode (seek to end)

Flags can be combined with bitwise OR.

### 4. Core API Functions Required

#### `file_open()` - Open file
- `int file_open(const char *path, uint32_t flags)`
- Find file node by path (or create if FILE_OPEN_CREAT)
- Find free file descriptor slot
- Initialize descriptor: node pointer, position (0 or end if append), flags
- Mark slot as valid
- Return file descriptor (0-based index) or -1 on error

#### `file_read()` - Read from file
- `ssize_t file_read(int fd, void *buffer, size_t count)`
- Validate file descriptor (bounds check, valid flag)
- Check node type (must be file, not directory)
- Calculate bytes to read (min of count and remaining bytes)
- Copy data from node's data buffer at current position
- Update position offset
- Return bytes read (0 at EOF) or -1 on error

#### `file_write()` - Write to file
- `ssize_t file_write(int fd, const void *buffer, size_t count)`
- Validate file descriptor and write flag
- Check node type (must be file)
- Expand file if needed (position + count > current size)
  - Reallocate data buffer
  - Update file size
- Copy data to node's data buffer at current position
- Update position offset
- Return bytes written or -1 on error

#### `file_close()` - Close file
- `int file_close(int fd)`
- Validate file descriptor
- Clear descriptor slot (set valid = 0, clear pointers)
- Return 0 on success, -1 on error

### 5. Optional Helper Functions

#### `file_seek()` - Seek in file
- `int file_seek(int fd, uint64_t offset, int whence)`
- Whence: SEEK_SET (0), SEEK_CUR (1), SEEK_END (2)
- Update file position
- Validate new position (0 <= pos <= file_size)

#### `file_get_size()` - Get file size
- `size_t file_get_size(int fd)`
- Return file size from node

#### `file_exists()` - Check if file exists
- `int file_exists(const char *path)`
- Return 1 if exists, 0 if not

#### `file_unlink()` - Delete file
- `int file_unlink(const char *path)`
- Find node, remove from parent, free data, free node

### 6. Position Management Requirements
- Each file descriptor maintains independent position
- Multiple opens of same file = separate positions
- Position starts at 0 (or file end if append mode)
- Position advances on read/write
- Position must not exceed file size (for reads)
- Position can exceed file size (for writes, expands file)

### 7. Error Handling Requirements
- Invalid file descriptor: return -1
- File not found: return -1
- Not opened for read/write: return -1
- Directory instead of file: return -1
- No free file descriptors: return -1

## Testing Requirements
1. Open file for reading: verify returns valid fd
2. Read from file: verify data matches
3. Open file for writing: verify returns valid fd
4. Write to file: verify data written
5. Read back written data: verify matches
6. Close file: verify slot freed
7. Test error cases: invalid fd, wrong flags, etc.
8. Test multiple opens: same file, different positions
9. Test file expansion: write beyond current size

## Success Criteria
- [ ] Can open file and receive valid file descriptor
- [ ] Can read from file descriptor (correct data, correct length)
- [ ] Can write to file descriptor (data stored, size updated)
- [ ] Can close file descriptor (slot freed)
- [ ] Position tracking works correctly (independent per fd)
- [ ] File expansion on write works (grows file as needed)
- [ ] Error cases handled correctly (invalid fd, wrong mode, etc.)

## Files to Create/Modify
- `fs/fileio.c` (NEW)
- `fs/fileio.h` (NEW)
- `meson.build` (MODIFY - add fileio.c)

## Technical Notes
- Simple file descriptor system: static array (no dynamic allocation)
- No multi-process support yet (single kernel task, shared FD table)
- Position is per-file-descriptor (multiple opens = separate positions)
- File expansion uses `krealloc` or manual reallocation
- Can add buffering later if needed for performance
- Consider: file locking (not needed initially, single task)
