# Plan: Extend from_json() to Accept Buffer Input

This plan adds buffer input support to `from_json()` so callers can parse JSON payloads larger than max string length. The efun signature will become `from_json(string|buffer)`, with implementation updates in `lib/efuns/json.cpp` to parse from either LPC string bytes or LPC buffer bytes. Existing JSON-to-LPC conversion behavior remains unchanged. We will add unit tests for valid and invalid buffer inputs and update user-facing docs and changelog entries accordingly. The goal is a backward-compatible enhancement: existing `from_json(string)` callers keep working with no semantic changes.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Efun signature update | complete |
| 2 | Implementation update | complete |
| 3 | Unit tests | complete |
| 4 | Docs and changelog | complete |

## Current State Handoff

- `from_json` signature now accepts `string | buffer` in `lib/lpc/func_spec.c.in`.
- `lib/efuns/json.cpp` now parses from either string bytes or buffer bytes and frees the input via `free_svalue()`.
- Added `fromJsonBuffer` and `fromJsonInvalidBufferError` unit tests in `tests/test_efuns/test_json.cpp`.
- Updated `docs/efuns/from_json.md` and `docs/ChangeLog.md` for the new input type support.

## Verification

1. `from_json("{\"a\":1}")` still returns a mapping with key `"a"` and value `1`.
2. `from_json(buffer)` parses valid JSON bytes and returns expected LPC values.
3. `from_json(buffer)` raises runtime error on invalid JSON bytes.
4. `tests/test_efuns/test_json.cpp` passes for JSON-enabled builds.
