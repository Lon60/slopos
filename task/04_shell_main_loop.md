# Task 04: Create Shell Main Loop (REPL)

**Priority: 4**  
**Status: Pending**  
**Estimated Time: 1-2 hours**

## Objective
Create the main shell REPL (Read-Eval-Print Loop) that reads commands and executes them.

## Current State
- ✅ (After Task 03) Can read lines from keyboard using `tty_read_line()`
- ✅ Task system working (can create tasks)
- ✅ Scheduler working (cooperative multitasking)
- ❌ No shell loop

## Requirements

### 1. Create Shell Module
Create new files:
- `shell/shell.c` - Main shell implementation
- `shell/shell.h` - Shell API

### 2. Shell Main Function Requirements

**Function Signature:**
- `void shell_main(void *arg)` - Entry point for shell task
- Receives task argument (may be unused)

**REPL Loop Requirements:**
1. Print welcome message on startup (optional)
2. Display prompt (e.g., `$ `)
3. Read line from keyboard using `tty_read_line()`
4. Handle empty lines (skip or re-prompt)
5. Execute command (placeholder for now)
6. Repeat indefinitely (infinite loop)

### 3. Command Execution Stub
Create placeholder function:
- `void shell_execute_command(const char *line)`
- For now: just print command or placeholder message
- Will be enhanced in Task 05 (parser) and Task 06 (commands)

### 4. Shell Task Integration
Modify `boot/early_init.c` in `kernel_main()`:

**Required Steps:**
1. After scheduler initialization completes
2. Create shell task using `task_create()`:
   - Name: "shell"
   - Entry point: `shell_main`
   - Priority: Medium (e.g., 5)
   - Flags: Kernel mode (0x02)
3. Schedule shell task using `schedule_task()`
4. Create idle task using `create_idle_task()`
5. Start scheduler using `start_scheduler()`

**Important:** Shell should run as separate kernel task, not in `kernel_main()` loop.

### 5. Integration Points
- Shell runs as kernel task (uses task system)
- Reads from keyboard input buffer (via `tty_read_line()`)
- Outputs to serial console (via `kprint()` / `kprintln()`)
- Will integrate with command parser in Task 05

## Testing Requirements
1. Boot kernel and verify shell task is created
2. Verify shell prompt appears (`$ `)
3. Type text and verify echo works
4. Press Enter and verify line is processed (even if command fails)
5. Verify shell continues running (infinite loop)
6. Verify empty lines are handled gracefully

## Success Criteria
- [ ] Shell task created successfully at boot
- [ ] Shell prompt appears (`$ `)
- [ ] Can read input lines from keyboard
- [ ] Commands are processed (echoed for now)
- [ ] Shell runs continuously (infinite loop, doesn't exit)
- [ ] Empty lines handled correctly (no crash, re-prompt)

## Files to Create/Modify
- `shell/shell.c` (NEW)
- `shell/shell.h` (NEW)
- `boot/early_init.c` (MODIFY - create and start shell task)
- `meson.build` (MODIFY - add shell/ sources)

## Technical Notes
- Shell should run as separate task (not in kernel_main idle loop)
- REPL pattern: Read → Execute → Print → Loop
- Shell will block on `tty_read_line()` waiting for input
- Priority should allow other tasks to run if needed
- Keep REPL simple initially; enhance with parsing/commands later
