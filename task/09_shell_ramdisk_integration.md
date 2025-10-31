# Task 09: Integrate Shell with Ramdisk (ls, cat, etc.)

**Priority: 9**  
**Status: Pending**  
**Estimated Time: 2-3 hours**

## Objective
Add filesystem-aware shell commands that work with the ramdisk.

## Current State
- ✅ (After Task 08) File I/O API working (open, read, write, close)
- ✅ Shell and command system working (parser, built-in dispatch)
- ✅ Ramdisk filesystem working
- ❌ No filesystem commands in shell

## Requirements

### 1. Filesystem Commands to Implement

#### `ls` - List directory contents
**Function:** `int builtin_ls(int argc, char **argv)`

**Requirements:**
- Accept optional path argument (default: "/" if not provided)
- List all files and directories in specified path
- Display directories with marker (e.g., brackets: `[dirname]`)
- Display files with name and size (e.g., `filename (123 bytes)`)
- Handle errors: print "cannot access" message if path doesn't exist
- Return 0 on success, 1 on error

**Output Format:**
- One entry per line
- Directories marked clearly
- Files show size information

#### `cat` - Display file contents
**Function:** `int builtin_cat(int argc, char **argv)`

**Requirements:**
- Require file path argument
- Open file for reading using file I/O API
- Read entire file contents
- Display contents to console
- Handle errors: "missing file operand" or "No such file or directory"
- Print newline after file if file doesn't end with one
- Return 0 on success, 1 on error

#### `write` - Write text to file
**Function:** `int builtin_write(int argc, char **argv)`

**Requirements:**
- Require file path and text arguments
- Open file for writing (with CREATE flag if needed)
- Write text to file
- Close file
- Handle errors: usage message, open failures
- Return 0 on success, 1 on error

**Note:** Simple version: single text argument. Can enhance later to support multiple arguments or quoted strings.

#### `mkdir` - Create directory
**Function:** `int builtin_mkdir(int argc, char **argv)`

**Requirements:**
- Require directory path argument
- Create directory using ramfs API
- Handle errors: "missing operand" or "cannot create directory"
- Return 0 on success, 1 on error

#### `rm` - Delete file
**Function:** `int builtin_rm(int argc, char **argv)`

**Requirements:**
- Require file path argument
- Delete file using file_unlink or ramfs API
- Handle errors: "missing operand" or "cannot remove"
- Return 0 on success, 1 on error

**Note:** Simple version: files only. Can enhance later for recursive directory deletion.

### 2. Command Registration Requirements
- Add new commands to builtin command table
- Include command name, handler function, description
- Update help command to show new commands

### 3. Error Message Requirements
Commands should follow Unix conventions:
- Clear, descriptive error messages
- Format: `command: error message`
- Example: `ls: cannot access '/badpath': No such file or directory`
- Usage messages for missing arguments

### 4. Integration Requirements
- Use file I/O API (`file_open`, `file_read`, `file_write`, `file_close`)
- Use ramfs API (`ramfs_list_directory`, `ramfs_create_directory`, etc.)
- Commands work with absolute paths (starting with "/")
- Commands handle nested directory structures

### 5. Memory Management
- Allocate buffers as needed (for reading files)
- Free allocated memory after use
- Handle allocation failures gracefully

### 6. String Utilities
May need additional string functions:
- `strlen()` - String length
- `strcmp()` - String comparison (already needed)
- Optional: `strncpy()` for safe copying

## Testing Requirements
1. Create file: `write test.txt "Hello world"`
2. Read file: `cat test.txt` (should display "Hello world")
3. List directory: `ls` (should show test.txt with size)
4. Create directory: `mkdir mydir`
5. List again: `ls` (should show both test.txt and [mydir])
6. Create nested file: `write mydir/file.txt "Nested"`
7. List subdirectory: `ls mydir` (should show file.txt)
8. Delete file: `rm test.txt`
9. List again: `ls` (file should be gone)
10. Test error cases: `cat nonexistent`, `ls badpath`

## Success Criteria
- [ ] `ls` lists directory contents correctly
- [ ] `ls` shows files with size and directories with markers
- [ ] `cat` displays file contents correctly
- [ ] `write` creates and writes files correctly
- [ ] `mkdir` creates directories correctly
- [ ] `rm` deletes files correctly
- [ ] Commands handle nested paths correctly
- [ ] Error messages are clear and helpful
- [ ] Commands return appropriate exit codes

## Files to Modify
- `shell/builtins.c` (ADD filesystem commands)
- `shell/builtins.h` (ADD command declarations)

## Technical Notes
- Start with simple versions (single argument, basic functionality)
- Can enhance later: multiple files, recursive operations, flags
- Error messages should match Unix shell conventions
- Consider adding `pwd` command if implementing working directory later
- File paths are always absolute (start with "/") for now

## Example Shell Session Flow
1. `ls` - Show empty root or initial files
2. `write test.txt "Hello"` - Create file
3. `cat test.txt` - Display "Hello"
4. `ls` - Show test.txt with size
5. `mkdir mydir` - Create directory
6. `write mydir/file.txt "Nested"` - Create nested file
7. `ls mydir` - Show nested file
8. `rm test.txt` - Delete file
9. `ls` - Verify file removed
