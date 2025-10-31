# Task 02: Create Keyboard Input Buffer/Queue

**Priority: 2**  
**Status: Pending**  
**Estimated Time: 1-2 hours**

## Objective
Create a thread-safe input buffer to queue keyboard events so they can be read asynchronously by the shell or other tasks.

## Current State
- ✅ (After Task 01) Keyboard can translate scancodes to ASCII characters
- ❌ No buffering mechanism for characters
- ❌ Characters generated during interrupt may be lost if not immediately read

## Requirements

### 1. Buffer Design Requirements
- Implement a **ring buffer** or **FIFO queue** for characters
- Buffer size: **256-512 characters** (allows typing ahead)
- Must be thread-safe (IRQ context writes, task context reads)

### 2. Thread Safety Requirements
Since keyboard IRQ runs in interrupt context and consumer runs in task context:
- Use atomic operations for buffer updates
- Options: disable interrupts (`cli`/`sti`) during critical sections, or use atomic primitives
- Ensure no race conditions between producer (IRQ) and consumer (task)

### 3. Buffer Operations Required

**Push Operation** (called from IRQ handler):
- Add character to buffer
- Handle buffer full condition (drop oldest character or ignore new one)
- Update write index/head atomically
- Update count atomically

**Pop Operation** (called from consumer task):
- Remove character from buffer
- Handle buffer empty condition (return 0 or special value)
- Update read index/tail atomically
- Update count atomically
- Return character value

### 4. Buffer State Tracking
Track at minimum:
- Write position (head)
- Read position (tail)
- Current item count
- Buffer capacity

### 5. Integration Requirements
Update keyboard driver API:
- `keyboard_handle_scancode()` should push translated characters to buffer
- `keyboard_getchar()` should pop characters from buffer
- `keyboard_has_input()` should check buffer count > 0

## Testing Requirements
1. Type rapidly and verify all characters are captured in buffer
2. Fill buffer to capacity and verify overflow handling
3. Read characters from buffer while typing (concurrent access)
4. Verify no characters are lost during rapid typing
5. Test buffer empty condition (no input available)

## Success Criteria
- [ ] Characters typed are stored in buffer
- [ ] Multiple characters can be queued (up to buffer size)
- [ ] Buffer doesn't lose data during rapid typing
- [ ] `keyboard_getchar()` retrieves characters in FIFO order
- [ ] Buffer handles overflow gracefully (no crashes, predictable behavior)
- [ ] Thread-safe operation (no corruption during concurrent access)

## Files to Modify
- `drivers/keyboard.c` (ADD buffer implementation)
- `drivers/keyboard.h` (ADD buffer-related declarations if needed externally)

## Technical Notes
- Ring buffer is simplest design (no dynamic allocation, O(1) operations)
- Overflow strategy: dropping oldest is simpler than blocking, but either is acceptable
- Lock-free design is possible (single producer = IRQ, single consumer = shell task)
- Consider using `volatile` for shared buffer state
- Buffer can be statically allocated (no heap needed)
