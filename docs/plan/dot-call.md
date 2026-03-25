# Dot-Call Plan

## Status
- Stage 1 (Syntax + Lowering Design): complete
- Stage 2 (Compiler Implementation): in progress
- Stage 3 (Validation + Tests): not started
- Stage 4 (Docs + Rollout Guidance): not started

## Current State Handoff
- Agreed direction: dot-call is syntax sugar only.
- Proposed semantic: `receiver.method(a, b, ...)` lowers to `efun::method(receiver, a, b, ...)`.
- Dispatch policy: dot-call resolves to efun only (never local function, never simul_efun).
- Compatibility rule: do not change existing efun contracts in this feature.
- Error rule: if injected receiver makes arguments invalid for the target efun, compilation must fail in efun validation.
- Chaining support target: allow `expr.method1(...).method2(...)` via normal expression composition.
- Stage 2 has started with lexer/grammar/compiler lowering work.
- Initial implementation target: tokenize bare `.` distinctly, add postfix dot-call parse rule, and lower through `validate_efun_call()` by prepending receiver as argument 1.
- Implemented in current step:
  - lexer returns `L_DOT` for bare `.` while preserving `..` and `...`
  - grammar accepts `expr4 . identifier(expr_list)`
  - dot-call currently lowers through existing efun validation by prepending receiver as argument 1
- Validation status:
  - `lpc` target builds successfully after parser regeneration
  - no new opcode introduced in this slice, so `driver_id` remains unchanged

## Problem Statement
Mudlibs may provide simul_efuns that shadow driver efuns (for example `to_json()` / `from_json()`).
Standalone calls are therefore context-dependent:
- `to_json(v)` may call mudlib simul_efun.
- `efun::to_json(v)` forces driver efun.

Goal: introduce concise syntax that makes driver efun intent explicit and avoids simul_efun ambiguity, without changing runtime efun behavior.

## Scope
In scope:
- Add dot token support for expression call syntax.
- Add postfix dot-call grammar for efun calls.
- Lower dot-call to efun invocation with receiver prepended as argument 1.
- Keep existing `->` and existing call resolution unchanged.
- Support chaining of dot-calls.

Out of scope:
- Changing any efun signature or default-argument behavior.
- Adding class/object method dispatch semantics.
- Replacing simul_efun or local function call rules.
- Property/member access with `.` without call parentheses.

## Design Summary
### 1) Syntax
- New form: `expr4 '.' identifier '(' expr_list ')'`
- Examples:
  - `m.to_json()` -> `efun::to_json(m)`
  - `payload.from_json()` -> `efun::from_json(payload)`
  - `m.to_json().from_json()` -> `efun::from_json(efun::to_json(m))`

### 2) Resolution Rules
- Dot-call looks up efun by name only.
- If no efun exists with that identifier: compiler error.
- If efun exists but arity/types are invalid after receiver injection: compiler error from efun validation.

### 3) Chaining
- Chaining is supported by treating each dot-call as a normal expression node.
- No special runtime support expected; each call result is the next receiver expression.

### 4) Error Model
- This feature should fail at compile time when invalid.
- Practically, most failures occur during semantic/efun validation phase (not raw parse phase), which is acceptable for this compiler architecture.

## Rationale
### A) Ambiguity Reduction
- `call()` remains mudlib-sensitive (local -> simul -> efun).
- Dot-call provides explicit efun route without verbose `efun::`.

### B) AI and Tooling
- Dot-call makes receiver and dispatch intent explicit in one compact form.
- This improves generated code determinism in codebases where simul_efun names overlap efuns.
- AI can still use standalone calls when mudlib-specific simul behavior is intended.

### C) Backward Compatibility
- No behavior changes for existing code paths.
- Existing source syntax is unaffected.
- Existing efun contracts and runtime semantics remain unchanged.

## Implementation Plan
### Stage 1: Syntax + Lowering Design (complete)
1. Define the feature boundary
- Dot-call is syntax sugar only.
- Dot-call targets efuns only.
- Dot-call prepends the receiver as argument 1.

2. Define dispatch semantics
- `receiver.method(args...)` lowers to `efun::method(receiver, args...)`.
- Dot-call never resolves to local functions or simul_efuns.

3. Define compatibility rules
- Existing efun contracts remain unchanged.
- If the target efun cannot accept the injected receiver, compilation must fail.

4. Define composition behavior
- Dot-call result is an ordinary expression.
- Chaining is allowed through normal expression composition.

### Stage 2: Compiler Implementation (in progress)
1. Lexer update
- Teach lexer to return a dedicated dot token for `.` while preserving existing `..` and `...` behavior.

2. Grammar update
- Add dot token declaration.
- Add postfix dot-call production in function call grammar.

3. AST lowering
- Build call node as efun invocation.
- Prepend receiver expression node to argument list before efun validation.
- Reuse existing `validate_efun_call()` path.

4. Diagnostics
- If identifier has no efun symbol, emit clear error (for example: `Unknown efun in dot-call: <name>`).
- Preserve existing efun type/arity diagnostics for invalid receiver injection.

5. Saved binary compatibility
- If implementation introduces a new opcode or changes compiled program layout/encoding, bump `driver_id` in `lib/lpc/program/binaries.c` so saved binaries are invalidated and recompiled.

### Stage 3: Validation + Tests (not started)
1. Positive parse/compile cases
- `x.to_json()`
- `x.to_json(indent)`
- `x.to_json().from_json()`

2. Negative cases
- Unknown method name in dot-call.
- Efun exists but does not accept injected receiver.
- Non-call dot forms (if out of scope) should error.

3. Compatibility checks
- `to_json(x)` still follows existing resolution.
- `efun::to_json(x)` unchanged.
- `x->foo()` unchanged.

4. Regression run
- Run unit tests and relevant compiler/interpreter test subsets.

5. Binary compatibility check
- Verify whether the implementation required a new opcode or binary format change.
- If yes, confirm `driver_id` was updated and saved binaries are treated as out-of-date.

### Stage 4: Docs + Rollout Guidance (not started)
1. Add user-facing syntax section (manual) with explicit lowering rule.
2. Add examples showing when to use:
- standalone call (mudlib-aware behavior),
- `efun::` call (explicit efun),
- dot-call (explicit efun with receiver sugar).
3. Note that not all efuns are suitable for dot-call; validation enforces this.

## Acceptance Criteria
- Dot-call compiles and lowers to efun call with receiver-first semantics.
- Chained dot-calls compile and evaluate in left-to-right expression order.
- Invalid dot-call usage is rejected at compile time with actionable diagnostics.
- No regressions in existing LPC syntax (`->`, standalone calls, ranges).
- If new opcodes or binary layout changes are introduced, saved LPC binaries are invalidated by a `driver_id` bump.
