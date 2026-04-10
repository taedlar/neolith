# Plan: Retire setjmp/longjmp Error Handling with C++ Exceptions

This plan migrates runtime error propagation from C `setjmp`/`longjmp` to C++ exceptions in staged increments while preserving LPC behavior, backend stability, and VM stack cleanup invariants. The current jump-based model is concentrated in `error_context` and several guard callsites (`frame`, `apply`, `backend`, `main`, `simulate`). We will first define an exception contract equivalent to current error semantics, then migrate catch boundaries, then migrate outer driver guards, and only then remove legacy context-chain machinery. The migration must keep behavior-compatible handling for nested `catch`, max-eval/deep-recursion non-catchable states, and startup/preload recovery paths. Test migration is part of each phase to prevent behavioral drift.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Baseline inventory and exception contract | in progress |
| 2 | Core error source migration (`error_context`) | not started |
| 3 | LPC catch boundary migration (`frame` + interpreter contract) | not started |
| 4 | Driver guard migration (`apply`, `backend`, `main`, `simulate`) | not started |
| 5 | Legacy context-chain removal (`jmp_buf` retirement) | not started |
| 6 | Test refactor and full validation | not started |
| 7 | Hardening, perf checks, and documentation finalization | not started |

## Current State Handoff

- Production `setjmp` callsites confirmed in:
  - `src/frame.c` (`do_catch()`)
  - `src/apply.c` (`safe_apply()`)
  - `src/backend.c` (main loop and preload guards)
  - `src/main.c` (startup guard)
  - `src/simulate.c` (`fatal()` crash-handler guard)
- Production `longjmp` sources are centralized in `src/error_context.c` (`throw_error()` and `error_handler()`).
- Existing context invariants are implemented by `save_context()`, `restore_context()`, and `pop_context()` and must be matched by exception unwinding behavior.
- Mixed-language build is already in use (`C` + `CXX`), enabling incremental `.c` to `.cpp` migration with C ABI entry points preserved where needed.
- Phase 1 decision: replace manual context pairing with scoped guard objects as the canonical exception-boundary mechanism.
- Exception contract has been simplified around boundary semantics (instead of manual stack choreography) and documented for reuse in later phases.
- Immediate next implementation focus: materialize `error_boundary_guard` in catch and safe-apply boundaries first (`frame` then `apply`).

## Exception Contracts (Phase 1 Output)

The contracts below are derived from current `setjmp`/`longjmp` behavior and must be preserved unless explicitly marked as intentional change.

Decision recorded for this plan: scoped guard objects will be used to replace manual `save_context()` / `pop_context()` lifecycle management in migrated boundaries.

| ID | Contract | Current Behavior | Exception Transition Rule |
|----|----------|------------------|---------------------------|
| EC-01 | Context push failure | `save_context()` returns `0` at max call depth; callers convert to runtime error paths. | Preserve failure point and message semantics; do not silently continue when context cannot be established. |
| EC-02 | Context stack discipline | Every successful `save_context()` requires exactly one `pop_context()`, and `pop_context()` clears error-state flags. | Replace with RAII guard that guarantees single pop/clear on all exits (normal + exceptional). |
| EC-03 | VM restore after failure | `restore_context()` restores `command_giver`, unwinds control stack, and pops value stack to saved pointers. | Exception unwinding must produce identical VM state at each translated catch boundary. |
| EC-04 | Throw requires catch frame | `throw_error()` jumps only when current frame is `FRAME_CATCH`; otherwise raises `*Throw with no catch.` | Preserve same observable LPC behavior and message text compatibility. |
| EC-05 | Catch payload contract | In catch path, previous `catch_value` is freed, new error string is stored, then control transfers to catch boundary. | Preserve payload lifecycle and ownership; avoid leaks/double-free under nested catch. |
| EC-06 | Non-catchable resource limits | `do_catch()` rethrows for `ES_MAX_EVAL_COST` and `ES_STACK_FULL` with canonical messages. | Preserve non-catchable classification; these must not become catchable by accident. |
| EC-07 | Recursive error guard | `in_error` detects reentrant error generation and escalates directly via context/fatal fallback. | Keep recursion guard semantics; never recurse indefinitely while handling error reporting. |
| EC-08 | Mudlib error handler invocation | `error_handler()` calls master `error_handler`, with catch/non-catch flag behavior and fallback trace logging if missing. | Preserve invocation order and fallback logging contract for operability. |
| EC-09 | Heart-beat shutdown on runtime error | If `current_heart_beat` is active during error, driver disables heart-beat and logs this action. | Preserve this safety action in exception path. |
| EC-10 | Safe apply suppression | `safe_apply()` returns `0` on error and restores VM state instead of propagating hard failure. | Preserve return-value contract for all existing callers. |
| EC-11 | Backend recovery boundaries | Backend loop and object-sweep/preload guards recover from LPC errors and continue where designed. | Preserve continue-vs-abort policy per boundary; do not widen shutdown conditions unintentionally. |
| EC-12 | Startup and fatal boundaries | Startup error exits with failure; fatal crash path attempts master crash callback, then exits/aborts by config. | Preserve startup/fatal terminal behavior and ordering guarantees. |
| EC-13 | F_END_CATCH success contract | On successful catch block completion, `F_END_CATCH` frees `catch_value`, pops catch frame, pushes `0`. | Preserve stack/result semantics exactly to avoid LPC behavior regression. |

### Exception Contract Gaps (Tier A LPC-visible Outcomes)

| Area | Documentation Contract | Source Behavior | Gap | Priority |
|------|-------------------------|-----------------|-----|----------|
| `catch()` success/error return | `catch()` returns `0` on success; standard errors return `*`-prefixed string. | Success path pushes `0`; caught payload comes from `catch_value`. | Mostly aligned, but docs under-specify non-string throw payload behavior. | Medium |
| `throw()` payload constraints | Docs state `throw()` can return any value except `0`. | `f_throw()` copies stack value into `catch_value` without explicit nonzero validation. | Ambiguity on `throw(0)` semantics; can collide with success sentinel `0`. | High |
| `throw()` without active `catch()` | Docs recommend using with `catch()`. | Runtime error `*Throw with no catch.` is raised when no catch boundary exists. | Concrete outcome exists in source but is not explicitly documented. | Medium |
| `error()` trace/log guarantee | Docs imply trace is recorded when `error()` halts thread. | Trace output can be replaced/suppressed by mudlib `error_handler()` return behavior. | Documentation implies stronger guarantee than implementation. | Medium |
| master `error_handler()` caught flag | Docs describe `caught=1` when trapped by `catch()`. | Caught-path callback emission is conditional under `LOG_CATCHES`. | Docs present unconditional behavior, source is build-option dependent. | High |
| master `crash()` applicability | Docs frame `crash()` as signal-crash callback (segfault/bus error). | `fatal()` path invokes `crash()` for broader fatal driver errors. | Documentation narrower than actual trigger conditions. | Medium |

Tier A resolution rule for later phases:

1. For each row, explicitly choose one of:
  - preserve source behavior and update docs,
  - adjust source to match docs,
  - document intentional behavior change in changelog.
2. Do not close Phase 7 until all High-priority rows are resolved.

### Simplified Contract with Scoped Guards

With scoped guards selected as the migration strategy, the contract shifts from manual stack choreography to boundary semantics:

1. Replace EC-01 + EC-02 + EC-03 with one boundary-unwind contract:
  - Entering a protected boundary creates a guard that captures VM restoration state.
  - Leaving the boundary (normal return or exception) restores VM state exactly once.
  - Error-state clearing is guard-owned and cannot be skipped.
2. Keep EC-04, EC-05, EC-06, and EC-13 as explicit LPC-language behavior contracts.
3. Keep EC-07, EC-08, and EC-09 as runtime safety and operability contracts.
4. Keep EC-10, EC-11, and EC-12 as driver boundary outcome contracts (continue, suppress, or terminate behavior).

This reduces the contract surface from 13 low-level mixed concerns to 4 grouped contract classes:

- Boundary Unwind Contract: deterministic VM restoration and state cleanup via guard lifetime.
- LPC Semantics Contract: catch/throw payload and non-catchable-limit behavior.
- Runtime Safety Contract: reentry protection, mudlib error-handler behavior, heart-beat safety.
- Driver Outcome Contract: safe_apply suppression, backend recovery policy, startup/fatal terminal policy.

Guard-based design also removes one recurring failure mode from the contract set:
- manual pop mismatch (missing or duplicate `pop_context()`), currently an implicit correctness requirement.

### Scoped Guard API Sketch (Phase 1 Design Draft)

Proposed initial guard types and responsibilities:

1. `error_boundary_guard`
  - captures boundary snapshot on construction (`command_giver`, `sp`, `csp`, and boundary metadata).
  - restores VM state on exception path.
  - clears error-state flags at scope exit.
  - guarantees single exit cleanup (replaces manual `save_context()`/`pop_context()` pairing).
2. `catch_value_guard`
  - owns `catch_value` lifecycle transitions in catch paths.
  - ensures previous value is released before replacement.
  - provides explicit `commit_success()`/`commit_error()` semantics to mirror current behavior.
3. `error_reentry_guard`
  - scoped replacement for `in_error` and `in_mudlib_error_handler` toggling.
  - guarantees flag restoration under nested exceptions.

Proposed usage shape (conceptual):

```cpp
error_boundary_guard boundary{boundary_kind::catch_block};
try {
  // protected LPC execution path
}
catch (const catchable_runtime_error &e) {
  boundary.restore();
  // preserve catch payload behavior
}
```

Initial rollout order for guard adoption:

1. `src/frame.c` `do_catch()` boundary first (highest semantic leverage for catch contract).
2. `src/apply.c` `safe_apply()` (small boundary, clear return-value contract).
3. `src/backend.c` guard blocks (continue semantics under runtime faults).
4. `src/main.c` startup guard and `src/simulate.c` fatal crash callback guard.
5. `src/error_context.c` internal reentry state via `error_reentry_guard`.

Phase 1 acceptance notes for guard API draft:

- Guard destructors must be `noexcept` and must not throw.
- Exceptions must not cross `extern "C"` boundaries; boundary wrappers translate before crossing.
- Guard behavior must be validated with parity tests for EC-04/05/06/10/11/12/13.

## Transition Improvement Opportunities

1. Introduce a typed exception hierarchy (`catchable_runtime_error`, `noncatchable_runtime_limit`, `fatal_runtime_error`) to make current implicit error-state branching explicit.
2. Replace manual `save_context`/`pop_context` pairing with scoped guard objects to eliminate mismatch risk and simplify review.
3. Replace global `in_error` and `in_mudlib_error_handler` flag toggling with scoped reentry guards to reduce state-leak risk on nested failures.
4. Preserve current LPC-visible error strings, but add optional structured internal fields (error code, source phase, boundary id) for better diagnostics.
5. Centralize C ABI boundary translation wrappers so exceptions are always converted at one layer and never cross `extern "C"` boundaries.
6. Add phase-specific parity tests for the contracts above, especially non-catchable limit behavior and backend continue semantics.

## Portability Review: GCC / MSVC / clang-cl

### Key Portability Risks
- Exception model mismatch on Windows toolchains can produce divergent catch behavior unless compiler flags are explicit and consistent.
- Throwing across C ABI boundaries is undefined as a contract; migrated code must catch before crossing `extern "C"` interfaces.
- Incremental `.c` to `.cpp` migration can accidentally route exceptions through non-migrated modules if boundaries are not staged.
- Compiler warning surfaces differ (`/W4` vs `-Wall -Wextra -Wpedantic`), so warning-clean status in one toolchain does not imply parity.

### Required Portability Decisions
1. Define canonical exception policy per toolchain:
  - GCC: C++ exceptions enabled; avoid relying on implementation-defined propagation through legacy C units.
  - MSVC: use `/EHsc` explicitly for migrated C++ translation units.
  - clang-cl: match MSVC exception model expectations (`/EHsc`) to keep behavior aligned with VS toolchain.
2. Enforce boundary rule: no exception may escape a C ABI function (`extern "C"` entry points must translate to driver error state or rethrow only within C++ domain).
3. Add per-phase multi-toolchain build/test gates so migration cannot proceed based on Linux-only validation.

### Toolchain Validation Matrix (Required)
1. GCC/Linux: `ci-linux` + `ut-linux`.
2. MSVC/x64: `ci-vs16-x64` + `ut-vs16-x64`.
3. clang-cl/x64: `ci-clang-x64` + `ut-clang-x64`.

Any phase that changes exception flow or ABI-adjacent code must pass all three rows before status moves to `complete`.
These rows are CI matrix requirements split by host OS/toolchain lanes (not expected to run on a single local machine).

## Phase Details

## Phase 1: Baseline Inventory and Exception Contract

### Goals
- Lock exact behavior contracts currently encoded by jump handling.
- Define one canonical runtime exception type and metadata needed by catch semantics.

### Work
- Document semantic mapping of:
  - `error()` and `throw_error()` behavior
  - `catch_value` lifecycle
  - non-catchable states (`ES_MAX_EVAL_COST`, `ES_STACK_FULL`)
- Decide temporary compatibility strategy during transition.
- Lock compiler portability policy for migrated units:
  - required exception flags for MSVC/clang-cl
  - boundary handling at `extern "C"` interfaces
  - preset-based CI gates used as phase exit checks

### Exit Criteria
- Written exception contract approved and mapped to current error states.
- Toolchain exception policy approved for GCC/MSVC/clang-cl.

## Phase 2: Core Error Source Migration (`error_context`)

### Goals
- Replace direct `longjmp` throw sites with exception throws.
- Preserve C-facing API surface while internals move to C++.

### Work
- Convert `src/error_context.c` to C++ implementation.
- Introduce runtime exception type and throw paths in `error_handler()` / `throw_error()`.
- Keep external entry points callable from C modules.
- Add explicit build-system settings (where needed) to keep exception behavior consistent on MSVC and clang-cl.
- Record concrete build-system artifact updates for this phase:
  - update `CMakeLists.txt` (or target-level CMake file if narrowed scope) with explicit exception model flags for migrated C++ units.
  - capture configure/build evidence from relevant presets in phase notes or PR description.

### Exit Criteria
- No direct `longjmp` in `error_context` runtime paths.
- Equivalent error text and state behavior under tests.
- Build and unit tests pass on GCC/MSVC/clang-cl presets listed in this plan.
- Build-system change location is documented (file + rationale), and CI evidence is attached for all required toolchain rows.

## Phase 3: LPC Catch Boundary Migration (`frame` + interpreter contract)

### Goals
- Re-implement `do_catch()` semantics using `try`/`catch`.
- Preserve `F_CATCH`/`F_END_CATCH` contract and `catch_value` behavior.

### Work
- Convert `src/frame.c` boundary handling.
- Verify `src/interpret.c` catch-related control flow remains behavior-compatible.
- Add/adjust tests for nested catch and throw propagation.
- Verify catch behavior parity across GCC/MSVC/clang-cl (same observed LPC semantics and driver logs for equivalent scenarios).

### Exit Criteria
- Catch-path tests pass with exception unwinding.
- Non-catchable states still escape as before.
- Cross-toolchain parity confirmed for catch-path regression cases.

## Phase 4: Driver Guard Migration (`apply`, `backend`, `main`, `simulate`)

### Goals
- Replace remaining guard `setjmp` blocks with `try`/`catch` wrappers.
- Keep backend loop resilience and startup/preload failure behavior unchanged.

### Work
- Migrate:
  - `safe_apply()` in `src/apply.c`
  - guard sections in `src/backend.c`
  - startup guard in `src/main.c`
  - crash-handler guard in `src/simulate.c`
- Validate object cleanup and recovery behavior after failures.
- Verify exceptions do not cross C ABI boundaries after each migrated wrapper.

### Exit Criteria
- No production `setjmp` sites remain in migrated modules.
- Recovery behavior matches baseline logs/tests.
- GCC/MSVC/clang-cl build + unit-test gates pass for changed modules.

## Phase 5: Legacy Context-Chain Removal (`jmp_buf` retirement)

### Goals
- Remove obsolete `jmp_buf` storage and context-chain APIs after all callers migrate.

### Work
- Refactor `error_context_t` and retire jump-specific fields.
- Remove dead helpers and associated comments/code paths.
- Confirm no legacy jump context assumptions remain in any compiler-specific code path.

### Exit Criteria
- No production `setjmp`/`longjmp` usage remains in `src/`.
- No compiler-specific fallbacks reintroduce jump-based handling.

## Phase 6: Test Refactor and Full Validation

### Goals
- Replace test-side `setjmp(econ.context)` patterns with exception assertions.

### Work
- Update affected test modules:
  - `tests/test_lpc_interpreter`
  - `tests/test_lpc_compiler`
  - `tests/test_efuns` (`json`, `curl`, core)
  - `tests/test_simul_efuns`
- Run targeted suites per migration phase, then full preset.
- Ensure test assertions remain compiler-agnostic (no toolchain-specific expected strings unless intentionally gated).

### Exit Criteria
- Full unit test preset passes on Linux target.
- All required toolchain rows in the validation matrix pass.

## Phase 7: Hardening, Perf Checks, and Documentation Finalization

### Goals
- Ensure no regressions in runtime stability or expected performance characteristics.

### Work
- Compare baseline vs migrated behavior in hot paths (normal execution) and cold throw paths.
- Update docs for error model and migration outcomes.
- Add changelog entry if behavior or developer contract changes.
- Capture any compiler-specific caveats discovered during rollout in permanent docs.
- Finalize Tier A LPC-visible contract and publish it in manual documentation (`docs/manual/`) as the post-migration behavior contract.

### Exit Criteria
- Migration deemed stable, documented, and ready for plan close.
- Portability sign-off completed for GCC/MSVC/clang-cl.
- Final LPC-visible contract has been promoted from plan notes into manual docs.

## Verification Matrix

1. Nested catch/throw behavior remains correct.
2. Max eval and deep recursion remain non-catchable where expected.
3. Backend continues after recoverable LPC errors.
4. Startup/preload failures produce expected shutdown behavior.
5. Crash handler path still degrades safely under secondary failures.
6. `src/` grep confirms no remaining production `setjmp`/`longjmp` runtime usage.
7. GCC/MSVC/clang-cl all pass required build and unit-test presets.
8. No exception escapes through `extern "C"` interfaces.
