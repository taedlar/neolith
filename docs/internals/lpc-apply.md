# LPC Apply Wrappers And Call Contract

This document defines the stack and return-value contract for driver-side apply and function-pointer wrapper calls.

## Scope

- Apply wrappers in [src/apply.h](../../src/apply.h) and [src/apply.cpp](../../src/apply.cpp)
- Function-pointer wrappers in [lib/lpc/functional.h](../../lib/lpc/functional.h) and [lib/lpc/functional.cpp](../../lib/lpc/functional.cpp)
- Efun consumer contract example in [lib/lpc/operator.c](../../lib/lpc/operator.c)

## Wrapper Families

### Apply wrappers

- `APPLY_CALL(fun, ob, num_arg, where)`
- `APPLY_SAFE_CALL(fun, ob, num_arg, where)`
- `APPLY_MASTER_CALL(fun, num_arg)`
- `APPLY_SAFE_MASTER_CALL(fun, num_arg)`

Slot variants:

- `APPLY_SLOT_CALL(...)`
- `APPLY_SLOT_SAFE_CALL(...)`
- `APPLY_SLOT_MASTER_CALL(...)`
- `APPLY_SLOT_SAFE_MASTER_CALL(...)`
- `APPLY_SLOT_FINISH_CALL()`

### Function-pointer wrappers

- `CALL_FUNCTION_POINTER_CALL(funp, num_arg)`
- `SAFE_CALL_FUNCTION_POINTER_CALL(funp, num_arg)`

Slot variants:

- `CALL_FUNCTION_POINTER_SLOT_CALL(funp, num_arg)`
- `SAFE_CALL_FUNCTION_POINTER_SLOT_CALL(funp, num_arg)`
- `CALL_FUNCTION_POINTER_SLOT_FINISH()`

## Non-slot Contract

### Apply calls

For `apply_call()` and `apply_master_ob()`:

- Input: caller has pushed `num_arg` values.
- Success return:
- `apply_call()` / `apply_master_ob()` return non-null sentinel (`&const1`).
- Stack result: returned LPC value is consumed by wrapper before return (non-slot behavior).
- Failure return:
- `apply_call()` returns `0`.
- `apply_master_ob()` returns `0` when function missing, `(svalue_t *)-1` when master object missing.

Safe variants (`safe_apply_call()`, `safe_apply_master_ob()`) mirror the same return contract but catch `driver_runtime_error` and return failure values instead of propagating.

### Function-pointer calls

For `call_function_pointer()`:

- Input: caller has pushed `num_arg` values.
- Success return: non-null sentinel (`&const1`).
- Runtime errors propagate.

For `safe_call_function_pointer()`:

- Mirrors return contract of `call_function_pointer()`.
- Catches `driver_runtime_error` and returns `0`.

## Slot Contract

Slot wrappers always push an undefined placeholder before dispatch.

Think of the slot as a temporary return cell owned by the wrapper pair:

- `*_SLOT_CALL(...)` creates/fills the cell.
- Caller may inspect the returned pointer while the cell exists.
- `*_SLOT_FINISH()` destroys the cell.

### Caller obligations

- Treat the returned pointer as valid only until the matching finish macro.
- Always pair slot calls with the corresponding finish macro:
- `APPLY_SLOT_*` must pair with `APPLY_SLOT_FINISH_CALL()`.
- `*_FUNCTION_POINTER_SLOT_*` must pair with `CALL_FUNCTION_POINTER_SLOT_FINISH()`.

### Success path

- Slot call returns a pointer to the slot value on stack.
- Exactly one slot value remains on stack until finish is called.
- That slot contains the call result.
- Finish pops that slot value.

### Failure path

Failure includes function-not-found, missing master object, null/destructed apply object, and safe-wrapper caught errors.

- Slot call returns failure (`0` or `(svalue_t *)-1` for missing master in master wrappers).
- Exactly one placeholder slot still remains on stack.
- The placeholder remains undefined/zero-like and is not a call result.
- Finish must still be called to pop that placeholder.

Safe wrappers normalize early/error paths to preserve this invariant.

## Stack Shape Notes

Slot wrappers conceptually operate on this transition:

- Entry (caller): `[args..., slot]`
- Dispatch input: `[slot, args...]`
- Post-success (before finish): `[slot(result)]`
- Post-failure (before finish): `[slot(undefined)]`

The finish macro removes the final slot in both success and failure paths.

### Illustration: APPLY_SLOT_CALL Success

Example call site pattern:

- caller pushes two arguments: `push_number(11); push_number(22);`
- caller invokes `APPLY_SLOT_CALL("capture", ob, 2, ORIGIN_DRIVER)`

Stack timeline (left = older stack cells, right = top of stack):

1. Before slot wrapper macro:

	`[... , 11, 22]`

2. After `APPLY_SLOT_CALL` macro pushes placeholder:

	`[... , 11, 22, U]`  where `U` is undefined placeholder

3. Internal rotate before dispatch (`apply_call` with slot):

	`[... , U, 11, 22]`

4. After apply returns result (for example `R = ({11,22})`), before finish:

	`[... , R]`

	Why this matters: in this driver, `R` lives in the slot cell on the eval stack. In legacy LPMud/MudOS code paths, apply return handling was often not modeled as a dedicated stack cell owned by a call/finish pair. That non-slot style is fragile under re-entrant activity (nested applies/errors) because later calls can overwrite or invalidate transient return storage. The slot contract avoids that class of bug by making the return value explicit stack state until `APPLY_SLOT_FINISH_CALL()`.

5. After `APPLY_SLOT_FINISH_CALL()`:

	`[...]`

Net effect: one temporary slot cell exists between slot call and finish; after finish, stack depth is back to pre-call level, meaning the call arguments have been consumed (popped) by the apply execution path.

### Illustration: APPLY_SLOT_SAFE_CALL Failure

Example failure path:

- caller pushes one argument: `push_number(7);`
- caller invokes `APPLY_SLOT_SAFE_CALL("does_not_exist", ob, 1, ORIGIN_DRIVER)`

Stack timeline:

1. Before slot wrapper macro:

	`[... , 7]`

2. After macro pushes placeholder:

	`[... , 7, U]`

3. Safe wrapper failure cleanup normalizes to placeholder-only slot state:

	`[... , U]`

4. After `APPLY_SLOT_FINISH_CALL()`:

	`[...]`

Net effect: even on failure (including early-return safe paths), call arguments are consumed (popped) and exactly one placeholder slot remains until finish is called.

## Efun Consumer Contract Example

`f_evaluate()` in [lib/lpc/operator.c](../../lib/lpc/operator.c) consumes function-pointer slot wrappers and must leave the evaluated expression result on top of stack (efun contract).

Efun top-of-stack rule for expression operators:

- On return, the evaluated expression value must be at stack top.
- Temporary wrapper artifacts (for example, slot placeholders) must not remain.
- The function pointer/function object used as input is consumed by the efun evaluation.

Current implementation stores the evaluated value back into the expression slot before calling `CALL_FUNCTION_POINTER_SLOT_FINISH()`, so the result remains at top-of-stack after slot cleanup.

In `f_evaluate()`, this is why result assignment targets the expression slot (`arg`) rather than the temporary slot (`sp`):

- assign to expression slot first
- finish slot wrapper second
- resulting top-of-stack is the evaluated value

## Practical Rules

- Use slot wrappers when caller needs direct access to returned stack value before cleanup.
- Use non-slot wrappers for sentinel-style success/failure checks where no transient slot value is needed.
- In safe wrappers, do not assume error paths preserve call inputs automatically; the wrapper is responsible for restoring stack contract.
