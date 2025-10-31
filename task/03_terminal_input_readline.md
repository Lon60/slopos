# Task 03: Implement Basic Terminal Input (Readline)

**Priority: 3**  
**Status: Pending**  
**Estimated Time: 2-3 hours**

## Objective
Create a readline-style input function that reads a complete line from keyboard, handles backspace/editing, and provides echo feedback.

## Current State
- ✅ (After Task 02) Keyboard input buffer working
- ✅ Serial output working (`kprint`, `kprintln`)
- ❌ No line input functionality
- ❌ No input editing (backspace)

## Requirements

### 1. Create Terminal/TTY Module
Create new files:
- `drivers/tty.c` - Terminal input/output functions
- `drivers/tty.h` - TTY API

Or integrate into existing keyboard driver if preferred.

### 2. Core Function: `tty_read_line()`

**Function Signature:**
- `size_t tty_read_line(char *buffer, size_t buffer_size)`
- Returns number of characters read (excluding null terminator)
- Returns 0 for empty line or error

**Functionality Requirements:**
1. Read characters one by one from keyboard buffer
2. Echo characters to console as typed (visual feedback)
3. Handle special keys appropriately
4. Enforce buffer size limits (prevent overflow)
5. Null-terminate result string

### 3. Special Key Handling

**Enter Key:**
- Finish line input
- Return line buffer
- Don't include Enter in buffer (or include as '\n', your choice)

**Backspace Key:**
- Delete last character from buffer
- Update visual cursor position
- Echo backspace sequence to erase character visually
- Options: `"\b \b"` or ANSI escape codes or other method

**Buffer Limits:**
- Prevent writing beyond `buffer_size - 1` (leave room for null terminator)
- Options: stop accepting input, ignore characters, or beep/alert user
- Don't crash or corrupt memory

**Control Characters:**
- Decide whether to echo control characters or not
- Backspace and Enter need special handling for echo

### 4. Echo Requirements
- Print each printable character as it's typed
- Handle backspace visually (erase character from display)
- Handle Enter (print newline)
- Use existing `kprint()` / `kprintln()` functions for output

### 5. Cursor Management (Simple Approach)
For initial implementation:
- Use simple backspace sequence (`\b \b`) to erase characters
- Don't need to track absolute cursor position initially
- Can enhance later with ANSI escape codes for advanced cursor control

## Testing Requirements
1. Type a complete line and verify echo
2. Test backspace: delete last character and verify visual erasure
3. Test Enter: finish input and return line
4. Test buffer overflow: type more than buffer_size characters
5. Test special characters: ensure they're handled correctly
6. Test empty line: just press Enter

## Success Criteria
- [ ] Can read complete line from keyboard
- [ ] Characters echo to console as typed
- [ ] Backspace deletes last character from buffer correctly
- [ ] Backspace visually erases character on screen
- [ ] Enter finishes input and returns line
- [ ] Buffer overflow is prevented (no memory corruption)
- [ ] Line is null-terminated
- [ ] Empty line (just Enter) returns 0 characters

## Files to Create/Modify
- `drivers/tty.c` (NEW)
- `drivers/tty.h` (NEW)
- `drivers/keyboard.c` (may need helper functions)
- `meson.build` (add tty.c to sources)

## Technical Notes
- Keep implementation simple: basic echo and backspace
- Can enhance later with command history, tab completion, etc.
- For serial console: echo to COM1 (serial port)
- For framebuffer console: echo to screen (can implement later)
- Line input should block until Enter is pressed
- Consider adding timeout or cancellation (Ctrl+C) if desired
