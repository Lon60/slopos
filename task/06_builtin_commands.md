# Task 06: Create Built-in Commands

**Priority: 6**  
**Status: Pending**  
**Estimated Time: 2-3 hours**

## Objective
Implement basic built-in shell commands that work without a filesystem.

## Current State
- ✅ (After Task 05) Command parser working (tokens available)
- ✅ Shell can dispatch to commands
- ❌ No command implementations

## Requirements

### 1. Create Built-in Commands Module
Create new files:
- `shell/builtins.c` - Built-in command implementations
- `shell/builtins.h` - Built-in command declarations

### 2. Command Structure Requirements

**Command Entry Structure:**
- Command name (string)
- Handler function pointer: `int (*handler)(int argc, char **argv)`
- Description string (for help)
- Optional: minimum/maximum argument count

**Command Registration:**
- Array or table of command entries
- Null-terminated list or count-based
- Easy to iterate for lookup

### 3. Command Handler Signature
- `int builtin_<name>(int argc, char **argv)`
- Returns: 0 on success, non-zero on error
- `argc`: Argument count (including command name)
- `argv`: Argument vector (argv[0] = command name, argv[1..N] = arguments)

### 4. Required Commands

#### `help` - List available commands
- Lists all registered built-in commands
- Shows command name and description
- No arguments needed
- Always succeeds

#### `echo` - Print arguments
- Prints all arguments separated by spaces
- Prints newline after arguments
- No arguments = print empty line
- Always succeeds

#### `clear` - Clear screen
- Clears terminal screen
- Options: ANSI escape sequence (`\x1B[2J\x1B[H`) or other method
- No arguments needed
- Always succeeds

#### `halt` - Shutdown kernel
- Shuts down the kernel
- Calls kernel shutdown function
- No arguments needed
- Doesn't return (kernel stops)

#### `info` - Show kernel information
- Displays kernel/system statistics
- Information to include:
  - Memory statistics (total/free pages)
  - Task statistics (active tasks, context switches)
  - Optional: uptime, scheduler stats
- No arguments needed
- Always succeeds

### 5. Command Dispatch Requirements
Update `shell_execute_command()`:
1. Parse command line into tokens
2. First token is command name
3. Look up command in built-in command table
4. If found: call handler with argc/argv
5. If not found: print "Command not found" error message
6. Show help hint on unknown command

### 6. String Utilities Needed
May need to implement if not available:
- `strcmp(const char *s1, const char *s2)` - String comparison
- `strlen(const char *s)` - String length
- `strncpy(char *dest, const char *src, size_t n)` - Bounded string copy

### 7. Error Handling
- Commands should return appropriate exit codes
- Unknown commands show clear error message
- Invalid arguments handled gracefully (error message or usage)
- Commands should not crash shell on errors

## Testing Requirements
1. Run `help` - should list all commands with descriptions
2. Run `echo hello world` - should print "hello world"
3. Run `echo` - should print empty line
4. Run `clear` - should clear screen
5. Run `info` - should show kernel statistics
6. Run `halt` - should shutdown kernel
7. Run `nonexistent` - should show "Command not found" error
8. Run `echo` with many arguments - should print all

## Success Criteria
- [ ] `help` command lists all available commands
- [ ] `echo` prints arguments correctly (single and multiple)
- [ ] `clear` clears the terminal screen
- [ ] `info` displays kernel information
- [ ] `halt` shuts down the kernel
- [ ] Unknown commands show appropriate error message
- [ ] Commands handle invalid arguments gracefully

## Files to Create/Modify
- `shell/builtins.c` (NEW)
- `shell/builtins.h` (NEW)
- `shell/shell.c` (MODIFY - add command dispatch)
- `meson.build` (MODIFY - add builtins.c)
- `lib/string.c` (NEW - if string functions needed)

## Technical Notes
- Commands receive argc/argv like standard C main()
- Return 0 = success, non-zero = error (Unix convention)
- Command lookup: linear search is fine initially (can optimize later)
- String comparison: case-sensitive matching
- Can add more commands later (date, uptime, etc.)
- Consider command aliases in the future
