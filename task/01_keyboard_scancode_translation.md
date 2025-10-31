# Task 01: Implement PS/2 Keyboard Scancode Translation

**Priority: 1**  
**Status: Pending**  
**Estimated Time: 2-3 hours**

## Objective
Translate raw PS/2 keyboard scancodes (received via IRQ 1) into ASCII characters that can be used for input processing.

## Current State
- ✅ Keyboard IRQ handler exists in `drivers/irq.c` (`keyboard_irq_handler`)
- ✅ Scancodes are being received and logged
- ❌ No scancode-to-ASCII translation

## Requirements

### 1. Create Keyboard Driver Module
Create new files:
- `drivers/keyboard.c` - Main keyboard driver implementation
- `drivers/keyboard.h` - Keyboard driver API

### 2. Scancode Translation Requirements
- Support **PS/2 Scancode Set 1** (QEMU uses this)
- Handle **make codes** (key press) vs **break codes** (key release)
  - Break codes = make code + 0x80 (e.g., 'a' press = 0x1E, release = 0x9E)
- Track modifier key states for translation (Shift, Ctrl, Alt, Caps Lock)
- Map special keys to identifiable values (Enter, Backspace, Space, Tab, Esc)

### 3. Key Mapping Requirements

**Letters:** Map to ASCII 'a'-'z' (0x61-0x7A) or 'A'-'Z' (0x41-0x5A) with Shift
- Example mappings: 0x1E = 'a', 0x1F = 's', etc.

**Numbers:** Map to ASCII '0'-'9' (0x30-0x39)
- With Shift: map to symbols ('!', '@', '#', '$', etc.)
- Example: 0x02 = '1', 0x03 = '2', etc.

**Special Keys:** Map to control characters or special values
- Enter: 0x1C or 0x0A (newline)
- Backspace: 0x08
- Space: 0x39 → 0x20 (space character)
- Tab: 0x0F → 0x09

**Modifier Keys:** Track state, don't produce characters
- Left/Right Shift: 0x2A, 0x36 (track state)
- Ctrl: 0x1D (track state)
- Alt: 0x38 (track state)
- Caps Lock: 0x3A (toggle state)

### 4. State Management Requirements
- Maintain modifier key states (Shift pressed, Ctrl pressed, etc.)
- Track Caps Lock toggle state
- Handle modifier key press/release correctly

### 5. Integration Requirements
Modify `drivers/irq.c`:
- Remove current scancode logging code
- Call keyboard driver function from IRQ handler
- Pass raw scancode to driver for processing

### 6. API Requirements
Provide the following public functions:

- `char keyboard_getchar(void)` - Get next ASCII character from keyboard
  - Returns 0 if no character available
  - Returns ASCII character when available
  
- `int keyboard_has_input(void)` - Check if character is available (non-blocking)
  - Returns non-zero if character available, 0 otherwise
  
- Optional: `uint8_t keyboard_get_scancode(void)` - Get raw scancode (for debugging)

## Testing Requirements
1. Press letter keys and verify correct ASCII output
2. Test Shift modifier: Shift+'a' should produce 'A'
3. Test special keys: Enter, Backspace, Space, Tab produce correct values
4. Test Caps Lock toggle behavior
5. Test rapid key presses (no lost inputs)

## Success Criteria
- [ ] Typing 'a' on keyboard produces ASCII 'a' (or 'A' with Shift)
- [ ] All 26 letters work correctly (a-z)
- [ ] All 10 numbers work correctly (0-9)
- [ ] Shift modifier produces uppercase letters and symbols
- [ ] Special keys (Enter, Backspace, Space) produce correct values
- [ ] Modifier key states are tracked correctly

## Files to Create/Modify
- `drivers/keyboard.c` (NEW)
- `drivers/keyboard.h` (NEW)
- `drivers/irq.c` (MODIFY - integrate with keyboard driver)
- `meson.build` (MODIFY - add keyboard.c to sources)

## Technical Notes
- PS/2 Scancode Set 1 is standard for most emulators/virtual machines
- Break codes are always make code + 0x80
- Focus on US QWERTY layout initially
- Reference: https://wiki.osdev.org/PS/2_Keyboard for scancode tables
- Translation can be done via lookup table or switch statement
- Consider efficiency: lookup table is faster but uses more memory
