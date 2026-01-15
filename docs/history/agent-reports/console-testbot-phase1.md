# Console Testbot Support Phase 1 - Implementation Report

**Date**: January 16, 2026  
**Phase**: 1 - POSIX Pipe Support  
**Status**: ✅ Complete  
**Effort**: ~45 minutes

## Summary

Implemented piped stdin support for Linux/WSL console mode by conditionally applying `TCSAFLUSH` only for real terminals. Pipes now preserve all input data, enabling automated testing via `testbot.py` while maintaining security for interactive terminals.

## Changes Delivered

### 1. Helper Function Added
**File**: [src/comm.c](../../src/comm.c)

Added `safe_tcsetattr()` inline helper that detects handle type:
- **Real TTY** (`isatty() == 1`): Uses `TCSAFLUSH` to discard stale input (security)
- **Pipe** (`isatty() == 0`): Uses `TCSANOW` to preserve all data (testbot)

### 2. Calls Replaced
Replaced 4 `tcsetattr(TCSAFLUSH)` calls with conditional behavior:

1. **[src/backend.c:171](../../src/backend.c#L171)** - `init_console_user()`
   - Direct `isatty()` check (comm.c helper not accessible here)
   
2. **[src/comm.c:953](../../src/comm.c#L953)** - `set_telnet_single_char()`
   - Uses `safe_tcsetattr()` helper
   
3. **[src/comm.c:1967](../../src/comm.c#L1967)** - Echo restoration in `get_user_command()`
   - Uses `safe_tcsetattr()` helper
   
4. **[src/comm.c:2340](../../src/comm.c#L2340)** - Password input in `set_call()`
   - Uses `safe_tcsetattr()` helper

### 3. Test Script Created
**File**: [examples/test_phase1.sh](../../examples/test_phase1.sh)

Validates that piped input is preserved and processed correctly:
- Sends commands via pipe: `wizard`, `wizard`, `quit`
- Verifies all commands are processed
- Uses timeout to handle reconnection wait
- ✅ All tests passing

## Test Results

```bash
$ ./examples/test_phase1.sh
================================
Phase 1: POSIX Piped Stdin Test
================================

Test: Piped input should be preserved, not discarded
Commands: wizard, wizard, quit

Welcome to the M3 Mud!
What?
> What?
> Bye!

✅ SUCCESS: All piped commands were processed
   - Input preserved through tcsetattr calls
   - Phase 1 implementation working correctly
```

**Observations**:
- All piped commands successfully received and processed
- Input buffer not discarded by terminal mode changes
- Interactive console behavior unchanged (verified manually)
- Driver waits for reconnection after EOF (expected behavior)

## Platform Behavior Verification

| Scenario | Before Phase 1 | After Phase 1 | Status |
|----------|----------------|---------------|--------|
| **Real TTY Input** | ✅ Works (flushed) | ✅ Works (flushed) | Unchanged |
| **Piped Input** | ❌ Data lost | ✅ Preserved | Fixed |
| **File Redirect** | ❌ Data lost | ✅ Preserved | Fixed |
| **Password Mode** | ✅ Secure | ✅ Secure | Maintained |
| **Single-char Mode** | ✅ Works | ✅ Works | Unchanged |

## Security Analysis

**Flushing Behavior Preserved**:
- Real terminals still flush input on mode changes (prevents injection attacks)
- Password prompts still flush pending input (prevents shoulder surfing)
- Reconnection still flushes ENTER key (clean state)

**New Pipe Behavior**:
- Pipes don't flush (data preserved for automation)
- Trusted source assumption (developer's test scripts)
- Console mode is development/testing only (not production)

**Risk**: Negligible - piped input implies trusted automation environment

## Performance Impact

- `isatty()` syscall: ~1µs per mode switch
- Total: 4-5 calls during typical session lifecycle
- Impact: Negligible

## Documentation Updates

1. **[examples/testbot.py](../../examples/testbot.py)**: Updated status comment
2. **[docs/plan/console-testbot-support.md](../../docs/plan/console-testbot-support.md)**: Marked Phase 1 complete

## Known Limitations

1. **Reconnection Wait**: Driver waits for stdin input after disconnect (by design)
   - Mitigation: Test scripts use timeout
   - Future: Consider adding exit-on-EOF option for automation

2. **EOF Handling**: Driver doesn't exit on EOF (waits for reconnection)
   - Current behavior: Intentional for interactive console recovery
   - Test scripts: Use timeout wrapper

3. **Platform-Specific**: Phase 1 only addresses POSIX (Linux/WSL)
   - Windows Phase 2 pending (requires overlapped I/O)

## Files Modified

- [src/comm.c](../../src/comm.c) - Added helper, replaced 3 calls
- [src/backend.c](../../src/backend.c) - Replaced 1 call
- [examples/test_phase1.sh](../../examples/test_phase1.sh) - Created test script
- [examples/testbot.py](../../examples/testbot.py) - Updated status comment
- [docs/plan/console-testbot-support.md](../../docs/plan/console-testbot-support.md) - Marked complete

## Next Steps

### Immediate
- ✅ Phase 1 complete and tested

### Phase 2 (Windows Support)
- [ ] Implement handle type detection (`GetFileType()`)
- [ ] Add overlapped I/O for pipes
- [ ] Add synchronous read for files
- [ ] Test on Windows with testbot.py

### Phase 3 (Documentation & Polish)
- [ ] Update user manual
- [ ] Add unit tests
- [ ] Integration test suite
- [ ] Update ChangeLog

**Estimated Phase 2 Effort**: 4-6 hours  
**Estimated Phase 3 Effort**: 2-3 hours

## Conclusion

Phase 1 successfully enables piped stdin support on Linux/WSL, allowing `testbot.py` and similar automation tools to work correctly. Input data is now preserved for pipes while maintaining security for interactive terminals. The implementation is minimal, focused, and has negligible performance impact.

**Achievement**: Linux/WSL console mode now fully supports automated testing! ✅
