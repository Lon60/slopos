# Task 05: Implement Command Parser (Tokenization)

**Priority: 5**  
**Status: Pending**  
**Estimated Time: 2-3 hours**

## Objective
Parse command lines into tokens (command name + arguments) for execution.

## Current State
- ✅ (After Task 04) Shell loop reading lines
- ✅ Shell can receive command strings
- ❌ No command parsing
- ❌ No tokenization

## Requirements

### 1. Tokenization Function Requirements

**Function Signature:**
- `int shell_parse_line(const char *line, char **tokens, int max_tokens)`
- Returns number of tokens found
- Returns 0 for empty line or error

**Parsing Requirements:**
1. Skip leading whitespace (spaces, tabs)
2. Split line on whitespace characters (space, tab)
3. Handle consecutive whitespace (multiple spaces between tokens)
4. Null-terminate each token
5. Enforce maximum token limit (prevent buffer overflow)
6. Return token count

**Initial Version (Simple):**
- Space-separated tokens only
- No quoted string handling (can add later)
- No escape sequences
- No special characters (pipes, redirection) yet

### 2. Token Storage Options

**Option A: Static Buffer**
- Pre-allocated 2D array: `char tokens[MAX_TOKENS][MAX_TOKEN_LEN]`
- Copy tokens into buffer
- Safe, but uses more memory

**Option B: Pointer Array**
- Array of pointers: `char *tokens[MAX_TOKENS]`
- Point into original string (modify original by replacing spaces with nulls)
- Memory efficient, but modifies input string

Choose based on design preferences.

### 3. Parser Algorithm Requirements
- Start from beginning of line
- Skip leading whitespace
- For each token:
  - Find start of token (non-whitespace)
  - Find end of token (whitespace or end of string)
  - Extract token (copy or mark with pointer)
  - Skip trailing whitespace
- Stop when max_tokens reached or end of string

### 4. Command Execution Integration
Update `shell_execute_command()` to:
1. Call parser function
2. Check token count (0 = empty, skip)
3. First token is command name
4. Remaining tokens are arguments
5. Pass tokens to command dispatch (next task)

### 5. Edge Cases to Handle
- Empty line (all whitespace)
- Line with only spaces/tabs
- Multiple consecutive spaces between tokens
- Leading/trailing whitespace
- Maximum tokens reached before end of line
- Very long tokens (may need truncation)

## Testing Requirements
1. Parse simple command: `"echo hello"` → 2 tokens: ["echo", "hello"]
2. Parse multi-argument: `"echo hello world"` → 3 tokens
3. Parse with extra spaces: `"echo   hello   world"` → 3 tokens (not 5)
4. Parse empty line: `""` → 0 tokens
5. Parse line with only spaces: `"   "` → 0 tokens
6. Parse max tokens: verify limit enforcement

## Success Criteria
- [ ] Can split "echo hello" into 2 tokens: ["echo", "hello"]
- [ ] Handles multiple arguments correctly
- [ ] Handles extra/consecutive whitespace correctly
- [ ] Returns 0 tokens for empty line
- [ ] First token is always command name
- [ ] Remaining tokens are arguments in order
- [ ] Maximum token limit enforced (no overflow)

## Files to Modify
- `shell/shell.c` (ADD parser function)
- `shell/shell.h` (ADD parser declaration)

## Technical Notes
- Keep parser simple: space/tab-separated tokens only
- Quoted strings can be added later if needed
- No need for complex shell features (pipes, redirection, wildcards) yet
- Parser should be robust (handle all edge cases)
- Consider memory safety (bounds checking, null termination)
- Token limit prevents buffer overflow attacks
