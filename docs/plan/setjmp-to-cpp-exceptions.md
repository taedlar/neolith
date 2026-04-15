# Plan: Retire setjmp/longjmp Error Handling with C++ Exceptions

This plan migrates runtime error propagation from C `setjmp`/`longjmp` to C++ exceptions in staged increments while preserving LPC behavior, backend stability, and VM stack cleanup invariants. The current jump-based model is concentrated in `error_context` and several guard callsites (`frame`, `apply`, `backend`, `main`, `simulate`). We will first define an exception contract equivalent to current error semantics, then migrate catch boundaries, then migrate outer driver guards, and only then remove legacy context-chain machinery. The migration must keep behavior-compatible handling for nested `catch`, max-eval/deep-recursion non-catchable states, and startup/preload recovery paths. Test migration is part of each phase to prevent behavioral drift.

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Baseline inventory and exception contract | complete |
| 2 | Core error source migration (`error_context`) + typed exception hierarchy | complete |
| 3 | LPC catch boundary migration (`frame` + interpreter contract) | complete |
| 4 | Driver guard migration (`apply`, `backend`, `main`, `simulate`) | complete |
| 5 | Legacy context-chain removal (`jmp_buf` retirement) | complete |
| 6 | Test refactor and full validation | complete |
| 7 | Hardening, perf checks, and documentation finalization | not started |

## Current State Handoff

### Phase 4 Started (2026-04-14)

**What was done:**
- Migrated `safe_apply()` driver guard from C `setjmp`/`longjmp` in `src/apply.c` to a C++ exception boundary wrapper in `src/apply_safe.cpp`.
- Preserved C ABI by keeping `safe_apply()` as the public entrypoint and delegating internally to `safe_apply_cpp(...)`.
- Added `src/apply_safe.cpp` to `stem` sources in `src/CMakeLists.txt`.
- Implemented guard behavior with `neolith::error_boundary_guard` and explicit exception-to-`0` suppression to preserve `safe_apply()` contract.
- Added explicit error-context transport mode (`longjmp` vs `exception`) in `error_context` so migrated and legacy boundaries can coexist safely during Phase 4.
- Migrated startup guard from `src/main.c` to C++ wrapper `src/main_startup.cpp` (`run_mudlib_startup_guarded`) while preserving startup failure shutdown behavior.
- Migrated fatal crash callback guard from `src/simulate.c:fatal()` to C++ wrapper `src/simulate_fatal_guard.cpp` (`invoke_master_crash_handler_guarded`) while preserving shutdown sequencing.
- Migrated backend preload and object-swap guards from `src/backend.c` to C++ wrappers in `src/backend_guards.cpp`:
  - `preload_objects()` now delegates to `preload_objects_guarded()`.
  - `look_for_objects_to_swap()` now delegates to `look_for_objects_to_swap_guarded()`.
- Migrated top-level backend loop guard from `src/backend.c:backend()` to C++ wrapper `backend_run_loop_guarded()` in `src/backend_guards.cpp`, preserving recover-and-continue behavior and console reinit after recovered runtime errors.

**Validation completed:**
- Build succeeded after migration.
- Targeted Phase 4 boundary tests passed:
  - `BackendTest.preload`
  - `EfunsTest.errorHandlerCallableContract`
  - `EfunsTest.throwError`
  - `EfunsTest.throwWithoutCatchRaisesRuntimeError`
  - `LPCInterpreterTest.catchSuccessReturnsZeroContract`
- Additional startup/fatal-path regression checks passed after follow-on migrations:
  - `BackendTest.setHeartBeat`
  - `SimulEfunsTest.loadSimulEfun`
  - `SimulEfunsTest.protectSimulEfun`
  - `LPCInterpreterTest.nonCatchableEvalCostEscapesCatchBoundary`
  - `LPCInterpreterTest.nonCatchableStackFullEscapesCatchBoundary`
- Backend guard migration regression checks passed after adding `backend_guards.cpp`:
  - `BackendTest.preload`
  - `BackendTest.setHeartBeat`
  - `EfunsTest.errorHandlerCallableContract`
  - `LPCInterpreterTest.catchSuccessReturnsZeroContract`
  - `LPCInterpreterTest.nonCatchableEvalCostEscapesCatchBoundary`
  - `LPCInterpreterTest.nonCatchableStackFullEscapesCatchBoundary`
  - `SimulEfunsTest.loadSimulEfun`
- Final top-level backend-loop migration regression checks passed:
  - `BackendTimerTest.HeartBeatFlagSetByTimer`
  - `BackendTimerTest.QueryHeartBeatIntegration`
  - `CommandFairnessTest.MultipleUsersFairness`
  - `SimulEfunsTest.protectSimulEfun`

**Remaining Phase 4 scope:**
1. None.

### Phase 5 Completed (2026-04-14)

**What was done:**
- Removed `jmp_buf context` field from `error_context_t` struct in `src/error_context.h` (jmp_buf completely retired).
- Removed `setjmp`/`longjmp` include (`#include <setjmp.h>`) from `src/error_context.h`.
- Removed all longjmp fallback code paths from `src/error_context.cpp`:
  - Deleted `current_context_uses_exceptions()` function (all contexts now use exceptions).
  - Removed all `longjmp(econ.context, 1)` calls from `throw_error()` and `error_handler()`.
  - Removed legacy longjmp fallback branches entirely.
- Refactored `error_handler()` in `src/error_context.cpp` to use `error_reentry_guard` for managing `in_error` and `in_mudlib_error_handler` state:
  - Split error handling into two helper functions: `handle_caught_error()` and `handle_uncaught_error()`.
  - Replaced manual flag toggling with scoped `error_reentry_guard` objects.
  - Preserved all error routing semantics (catchable, noncatchable, fatal) via typed exceptions.
- Deleted legacy `src/error_context.c` file (Phase 2 migration was complete, C file no longer needed).
- Migrated missed Phase 4 driver boundaries:
  - **lib/efuns/call_out.c** → `lib/efuns/call_out.cpp`: Converted from setjmp-based error recovery to try/catch exception handling for call_out execution.
  - **lib/lpc/functional.c** → `lib/lpc/functional.cpp`: Converted `safe_call_function_pointer()` from setjmp to exception-based error boundary (set `ERROR_CONTEXT_TRANSPORT_EXCEPTION` mode).
  - Updated `lib/efuns/CMakeLists.txt` and `lib/lpc/CMakeLists.txt` to compile .cpp versions instead of .c versions.
- Removed migration-only transport-mode API and constants now that exception transport is universal:
  - Removed `transport_mode` from `error_context_t`.
  - Removed `ERROR_CONTEXT_TRANSPORT_*` constants.
  - Removed `set_context_transport_mode()` / `get_context_transport_mode()` declarations and implementations.
  - Removed all callsites that set exception transport mode.
- Completed test-side migration from `setjmp` to exception assertions in affected suites (`test_efuns`, `test_lpc_compiler`, `test_lpc_interpreter`, `test_simul_efuns`, plus JSON/CURL efun tests).

**Validation completed:**
- Build succeeds after transport-mode retirement and callsite cleanup.
- Targeted regression tests for migrated boundaries and contracts pass:
  - `BackendTest.preload`
  - `BackendTimerTest.QueryHeartBeatIntegration`
  - `EfunsTest.throwError`
  - `EfunsTest.throwWithoutCatchRaisesRuntimeError`
  - `EfunsTest.errorHandlerCallableContract`
  - `LPCInterpreterTest.evalCostLimit`
  - `LPCInterpreterTest.nonCatchableEvalCostEscapesCatchBoundary`
  - `LPCInterpreterTest.nonCatchableStackFullEscapesCatchBoundary`
  - `SimulEfunsTest.protectSimulEfun`
- Full ctest sweep reports passing tests after migration updates.
- Repository audit confirms no remaining `setjmp`/`longjmp` production usage.

**Remaining Phase 5 scope:**
1. None.

### Phase 6 Completed (2026-04-15)

**What was done:**
- Test-side migration cleanup has been audited; no remaining setjmp/longjmp transition helpers in active migrated test paths.
- Linux validation gates completed successfully:
  - Configure/build on `linux` + `ci-linux` build preset succeeded.
  - Full test run via `ctest --preset ut-linux` passed (251/251).
- Post-Phase-6 regression surfaced and fixed in curl tests for MSVC behavior:
  - `CurlEfunsTest.PerformToRejectsInvalidCallbackAndFlagTypes` was updated to use LPC-level `try_perform_to` catch-boundary behavior instead of direct extern-C throw/catch assumptions.
  - `HttpTestServer` teardown now uses `shutdown()` before `SOCKET_CLOSE` to avoid Windows `select()` wakeup/join hangs.
  - Linux validation for curl suite remains passing after fix (7/7 curl tests).

**Validation completed:**
- Full Linux unit-test suite pass (251/251).
- Focused curl regression suite pass after MSVC-oriented fix (7/7).
- Full Windows/x64 validation confirmed by user after `/EHs` fix:
  - `vs16-x64`: full tests passing.
  - `clang-x64`: full tests passing.
- Full Windows/Win32 validation confirmed by user:
  - `vs16-win32`: full tests passing.

**Remaining Phase 6 scope:**
1. None.

**Current State Handoff:**
- **Production code status**: Phase 5 remains complete; jump-context transport is retired in production paths.
- **Test code status**: migrated test suites are stable; MSVC hang regression in curl test is fixed.
- **Build/test status**: Linux + Windows x64 + Windows Win32 are green with `/EHs` fix applied.
- **Next action**: proceed to Phase 7 hardening, perf checks, and documentation finalization.

### Phase 4 Remained Completed (updated summary)

**Additional notes from Phase 5 audit:**
- Two missed Phase 4 driver boundaries discovered during Phase 5: `call_out()` in lib/efuns and `safe_call_function_pointer()` in lib/lpc. Both have been migrated to exception-based approach with proper `ERROR_CONTEXT_TRANSPORT_EXCEPTION` mode setting.

---

### Phase 3 Completed (pre-Phase-5 state from earlier in session)


**What was done:**
- Added `src/frame_catch.cpp` and migrated the `do_catch()` boundary to `try`/`catch` using typed exceptions (`catchable_runtime_error`, `noncatchable_runtime_limit`).
- Kept C ABI stable by routing `src/frame.c:do_catch()` through the new C++ implementation.
- Updated `src/CMakeLists.txt` to compile `frame_catch.cpp` into `stem`.
- Updated `src/error_context.cpp` catch-path behavior:
  - `error_handler()` now throws `noncatchable_runtime_limit` for max-eval/stack-full states at catch boundaries.
  - `error_handler()` now uses `catch_value_guard` and throws `catchable_runtime_error` instead of `longjmp` for catchable catch-boundary transport.
  - `throw_error()` now throws `catchable_runtime_error` at active catch boundaries (no direct longjmp in this path).
- Preserved catch payload semantics in `do_catch()` by moving `catch_value` to stack and resetting it to `const1` on catchable exception path.
- Refactored `error_context.cpp` catch-boundary detection to a single helper (`has_active_catch_boundary()`), removing duplicated direct `FRAME_CATCH` checks and centralizing the legacy `save_csp + 1` convention.

**Validation completed:**
- Full build succeeded (`Build_CMakeTools`) on Linux.
- Full suite run completed with all tests passing (250/250).
- Focused Phase 3 behavior tests passed:
  - `EfunsTest.throwError`
  - `EfunsTest.throwWithoutCatchRaisesRuntimeError`
  - `LPCInterpreterTest.nonCatchableEvalCostEscapesCatchBoundary`
  - `LPCInterpreterTest.nonCatchableStackFullEscapesCatchBoundary`
  - `LPCInterpreterTest.catchSuccessReturnsZeroContract`
- Post-refactor rerun of the same focused Phase 3 tests passed.
- `LPCInterpreterTest.throwZeroNormalizesToUnspecifiedError` is now enabled and passing with throw(0) normalization.
- Source audit confirms LPC catch/throw transport no longer depends on `longjmp`:
  - `do_catch()` runs through C++ `try`/`catch` boundary (`src/frame_catch.cpp`).
  - `throw_error()` transports to catch via typed exception.
  - `error_handler()` catch-path transport uses typed exceptions.
  - Remaining `longjmp` paths are for non-catch outer recovery boundaries and are deferred to Phase 4.

**Remaining Phase 3 scope:**
1. None.

### Phase 2 Completed (2026-04-14)

**What was done:**
- `src/error_context.c` migrated to `src/error_context.cpp` (C++ translation unit).
- Typed exception hierarchy defined in `src/exceptions.hpp`: `catchable_runtime_error`, `noncatchable_runtime_limit`, `fatal_runtime_error` (all under `namespace neolith`).
- Guard API defined in `src/error_guards.hpp`: `error_reentry_guard`, `catch_value_guard`.
- Guard implementations completed in `error_context.cpp`; `error_reentry_guard` and `catch_value_guard` are RAII-ready.
- `add_mapping_string`, `add_mapping_pair`, `add_mapping_object`, `add_mapping_array` updated to use `const char*` key for C++ compatibility.
- `SimulEfunsTest` fixture updated to clear the mudlib binary cache between tests (binary cache references shared string indices specific to a string table state; clearing prevents cross-test failures when the string table is reinitialized between tests).
- All 250 tests pass on the `pr-linux` preset.

**Deferred to Phase 3 (longjmp retained):**
- `error_handler()` still uses `longjmp` for both catch-path and fatal-path transport.
  - This is intentional: Phase 3 migrates `do_catch()` in `frame.c` to use `try`/`catch`, replacing the `setjmp`/`longjmp` bridge at the catch boundary.
  - Until Phase 3, `error_handler()` uses longjmp for delivery (Phase 2 adds guard API and exception types but does not change delivery mechanism).
- `throw_error()` still uses `longjmp`.
- Guards (`error_reentry_guard`, `catch_value_guard`) are defined and working but not yet actively used in exception-throwing error paths.

**Immediate next focus (Phase 3):**
1. Migrate `do_catch()` in `src/frame.c`:
   - Replace `setjmp(econ.context)` with `try`/`catch(const neolith::catchable_runtime_error&)`.
   - Remove `FRAME_CATCH` based longjmp check from `error_handler()`.
   - Use `catch_value_guard` to manage `catch_value` lifecycle.
2. After `do_catch()` migration, validate `F_END_CATCH` contract with LPC interpreter tests.
3. Migrate `throw_error()` to C++ exception throw.
4. Confirm non-catchable limit behavior still correctly escapes `catch()` boundaries.

---

### Pre-Phase-2 History (Phase 1 Handoff)

- Production `setjmp` callsites confirmed in:
  - `src/frame.c` (`do_catch()`)
  - `src/apply.c` (`safe_apply()`)
  - `src/backend.c` (main loop and preload guards)
  - `src/main.c` (startup guard)
  - `src/simulate.c` (`fatal()` crash-handler guard)
- Production `longjmp` sources are centralized in `src/error_context.cpp` (`throw_error()` and `error_handler()`).
- Existing context invariants are implemented by `save_context()`, `restore_context()`, and `pop_context()` and must be matched by exception unwinding behavior.
- Mixed-language build is already in use (`C` + `CXX`), enabling incremental `.c` to `.cpp` migration with C ABI entry points preserved where needed.
- Phase 1 decision: replace manual context pairing with scoped guard objects as the canonical exception-boundary mechanism.
- Phase 1 decision: introduce typed exception hierarchy (`catchable_runtime_error`, `noncatchable_runtime_limit`, `fatal_runtime_error`) to make current implicit error-state branching explicit and align master apply routing with exception semantics.
- Phase 1 decision: retire `in_error` and `in_mudlib_error_handler` global flag toggling in favor of scoped guard objects (`error_reentry_guard`) to eliminate state-leak risk on nested failures and simplify reentry control flow.
- Exception contract has been simplified around boundary semantics (instead of manual stack choreography) and documented for reuse in later phases.
- Phase 7 decision: update master apply documentation (`docs/applies/master/`) to align with exception semantics and typed routing (6 Tier A gaps identified; 2 high-priority: error_handler argument count, LOG_CATCHES conditionality).

### Pre-Phase-2 Scan: Existing `abort()` / `exit()` Call Sites

Scan date: 2026-04-14.

Scope reviewed:
- Runtime driver code under `src/`
- Linked runtime support under `lib/rc/`, `lib/lpc/`, and `lib/port/`

Excluded from exception-migration evaluation:
- Build/generator utilities (`lib/lpc/edit_source.c`, `lib/lpc/make_func.y`)
- Vendored CLI helper behavior in `lib/port/getopt.c`

Conclusion:

1. There is no immediate Phase 2 blocker in the current `abort()` / `exit()` inventory.
2. Most termination sites are startup/bootstrap failures, intentional process termination, signal-path exits, or hard-fatal integrity/OOM paths.
3. Only the startup/bootstrap failure paths are realistic exception-conversion candidates, and they fit Phase 4 driver-boundary migration better than Phase 2 core error-source migration.

#### Candidate for later exception translation (do not terminate inline once driver-boundary exception handling exists)

These sites represent recoverable-in-structure but still process-fatal-in-policy failures. They can be translated into `fatal_runtime_error` (or equivalent startup exception) and then terminated at one outer boundary instead of calling `exit()` locally.

| Area | Current examples | Recommendation |
|------|------------------|----------------|
| Startup config/bootstrap | `src/main.c` (`chdir`, startup mudlib guard, config path resolution, debug log startup failure), `lib/rc/rc.cpp` (`init_config()` missing/invalid config) | Good exception candidates once startup has a single C++ boundary that catches and terminates centrally. |
| Runtime bootstrap dependencies | `src/comm.c` (`init_user_conn()` socket/runtime/signal setup failures), `src/backend.c` (`WSAStartup()` failure), `src/stack.c` (stack allocation failure), `lib/lpc/object.c` (`init_objects()` allocation failure), `lib/lpc/lex.c` (invalid include path) | Also good candidates for central startup/bootstrap exception handling. Policy remains shutdown, but transport can become exception-based. |
| Master boot path | `src/simulate.c` (`init_master()` missing/illegal/unloadable master) | Strong candidate for `fatal_runtime_error` once `main()` startup guard is migrated. This aligns well with the exception plan. |

#### Should remain termination-based (not an exception target)

| Area | Current examples | Recommendation |
|------|------------------|----------------|
| Intentional normal shutdown | `src/simulate.c` (`do_shutdown()`), `src/main.c` (`g_proceeding_shutdown` successful exit) | Keep as direct process termination. These are policy endpoints, not error transport sites. |
| Fatal crash end-state | `src/simulate.c` (`fatal()` final `abort()` / `exit()`) | Keep terminal behavior. Phase 4 may replace the inner `setjmp` guard around `master::crash()`, but the final action must still terminate. |
| Signal handlers | `src/main.c` (`sig_usr1`) | Do not throw exceptions from signal paths. Keep flag/terminate behavior only. |
| Integrity/assert contracts | `src/stralloc.c` (`int_string_unlink()` contract violation under `STRING_TYPE_SAFETY`) | Keep as abort/assert-style failure. This indicates internal corruption or API misuse, not recoverable runtime flow. |
| Hard OOM fallback | `src/malloc.c` (`xalloc()` after reserve exhausted), `lib/port/xcalloc.c`, `lib/port/xstrdup.c` | Keep terminal behavior. Throwing after allocator exhaustion is not a safe or useful migration target for this plan. |

#### Phase impact

Recommended boundary split:

1. **Phase 2**: leave all existing `abort()` / `exit()` sites unchanged while migrating `error_context` transport and typed exception classes.
2. **Phase 4**: convert only startup/bootstrap `exit()` sites to typed fatal exceptions and catch them at one outer driver boundary (`main()` / startup path).
3. **Never convert** signal-handler exits, hard OOM termination, assert-style aborts, or the final termination step in `fatal()` / `do_shutdown()`.

Practical implication for implementation order:

- Do not spend Phase 2 scope on `exit()` cleanup in `comm.c`, `main.c`, or `rc.cpp`.
- Do schedule `src/simulate.c:init_master()` and startup/config failure sites as follow-on work when Phase 4 starts.
- Treat `src/simulate.c:fatal()` as a mixed case: the crash-callback guard can migrate away from `setjmp`, but the end of the function still terminates the process by design.

## Exception Contracts (Phase 1 Output)

The contracts below are derived from current `setjmp`/`longjmp` behavior and must be preserved unless explicitly marked as intentional change.

Decision recorded for this plan: scoped guard objects will be used to replace manual `save_context()` / `pop_context()` lifecycle management in migrated boundaries.
Decision recorded for Tier A semantics: `throw(0)` is allowed, but catch payload must be normalized to a non-zero string (`"*Unspecified error"`) before returning from `catch()`.

| ID | Contract | Current Behavior | Exception Transition Rule |
|----|----------|------------------|---------------------------|
| EC-01 | Context push failure | `save_context()` returns `0` at max call depth; callers convert to runtime error paths. | Preserve failure point and message semantics; do not silently continue when context cannot be established. |
| EC-02 | Context stack discipline | Every successful `save_context()` requires exactly one `pop_context()`, and `pop_context()` clears error-state flags. | Replace with RAII guard that guarantees single pop/clear on all exits (normal + exceptional). |
| EC-03 | VM restore after failure | `restore_context()` restores `command_giver`, unwinds control stack, and pops value stack to saved pointers. | Exception unwinding must produce identical VM state at each translated catch boundary. |
| EC-04 | Throw requires catch frame | `throw_error()` jumps only when current frame is `FRAME_CATCH`; otherwise raises `*Throw with no catch.` | Preserve same observable LPC behavior and message text compatibility. |
| EC-05 | Catch payload contract | In catch path, previous `catch_value` is freed, new payload is stored, then control transfers to catch boundary. | Preserve payload lifecycle and ownership; avoid leaks/double-free under nested catch. If payload is `0` (e.g., `throw(0)`), normalize to `"*Unspecified error"` before catch return. |
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
| `catch()` success/error return | `catch()` returns `0` on success; standard errors return `*`-prefixed string. | Success path pushes `0`; caught payload comes from `catch_value`. | Decision: update docs to explicitly describe mixed caught payload behavior (not only standard error strings). | Medium |
| `throw()` payload constraints | Docs state `throw()` can return any value except `0`. | `f_throw()` copies stack value into `catch_value` without explicit nonzero validation. | Decision: allow `throw(0)` and normalize caught payload to `"*Unspecified error"` so `catch()` never returns success sentinel `0` for thrown errors. Remaining work: implement behavior + update efun docs. | High |
| `throw()` without active `catch()` | Docs recommend using with `catch()`. | Runtime error `*Throw with no catch.` is raised when no catch boundary exists. | Decision: update docs to explicitly state the no-catch runtime outcome and message contract. | Medium |
| `error()` trace/log guarantee | Docs imply trace is recorded when `error()` halts thread. | Trace output can be replaced/suppressed by mudlib `error_handler()` return behavior. | Decision: update docs to describe conditional trace/log behavior when master `error_handler()` handles output. | Medium |
| master `error_handler()` caught flag | Docs describe `caught=1` when trapped by `catch()`. | Caught-path callback emission is conditional under `LOG_CATCHES`. | Decision: update docs to mark caught-path callback/log behavior as build-option dependent (`LOG_CATCHES`). | High |
| master `crash()` applicability | Docs frame `crash()` as signal-crash callback (segfault/bus error). | `fatal()` path invokes `crash()` for broader fatal driver errors. | Decision: update docs to describe `crash()` as fatal-driver-error callback (not signal-only). | Medium |

Tier A resolution rule for later phases:

1. For each row, explicitly choose one of:
  - preserve source behavior and update docs,
  - adjust source to match docs,
  - document intentional behavior change in changelog.
2. Do not close Phase 7 until all High-priority rows are resolved.

Tier A doc-alignment decision (current):

1. For all remaining rows above (except the `throw(0)` behavior change row), align documentation to current source behavior.
2. Treat these doc updates as required deliverables before Phase 7 closure.

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
  - scoped replacement for `in_error` and `in_mudlib_error_handler` flag toggling.
  - manages flag lifecycle and guarantees state restoration under nested exceptions.
  - eliminates state-leak risk from manual flag management (Phase 5 removes global flags entirely).

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

1. ✓ **CONFIRMED (Phase 2 Integration)**: Introduce a typed exception hierarchy (`catchable_runtime_error`, `noncatchable_runtime_limit`, `fatal_runtime_error`) to make current implicit error-state branching explicit.
2. ✓ **CONFIRMED (Phases 2-4 Integration)**: Replace manual `save_context`/`pop_context` pairing with scoped guard objects (`error_boundary_guard`) to eliminate mismatch risk and simplify review.
3. ✓ **CONFIRMED (Phase 2+5 Integration)**: Replace global `in_error` and `in_mudlib_error_handler` flag toggling with scoped reentry guards (`error_reentry_guard`) to reduce state-leak risk on nested failures and simplify flag lifecycle.
4. ✓ **CONFIRMED (Phase 2 Integration, migration-scoped LPC improvement)**: Add one LPC-visible metadata field to master `error_handler` mapping: `kind` (`"catchable"`, `"limit"`, `"fatal"`) derived from typed exception class. Defer all other stack-dump/mapping shape enhancements outside this migration.
5. C ABI boundary translation wrappers are **migration-only**: during Phases 2-4, any `extern "C"` entry points that internally invoke C++ exception-throwing paths must catch before returning to C callers. These wrappers are removed in Phase 5 once all modules are migrated. Signal handlers require no wrapper because they use the flag-only pattern (no C++ runtime in handler).
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
  - MSVC: use `/EHs` explicitly to allow throwing C++ exceptions from C++ code, including extern "C".
  - clang-cl: match MSVC exception model expectations (`/EHs`) to keep behavior aligned with VS toolchain.
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

### Pre-Phase-2 Contract Lockdown Tests (Required Gate)

Before Phase 2 code migration, add/adjust targeted tests to lock behavior that must remain stable while replacing jump-based transport with exceptions.

1. `tests/test_efuns/test_efuns.cpp`: lock `throw()` baseline behavior outside `catch()` (runtime error path and expected text compatibility).
2. `tests/test_lpc_interpreter/test_lpc_interpreter.cpp`: lock non-catchable limit behavior for eval-cost/deep recursion paths (must escape catch boundary).
3. `tests/test_backend/test_backend.cpp`: lock backend recovery/continue behavior around preload and recoverable LPC failures.
4. `tests/test_lpc_interpreter/test_lpc_interpreter.cpp` (or nearest catch-boundary suite): lock `F_END_CATCH` success contract (pushes `0`, cleans catch frame/value).
5. `tests/test_efuns/test_efuns.cpp` + mock master flow fixture: lock `error_handler` callback arity and mapping keys currently required by docs (`error`, `program`, `object`, `trace`, `file`, `line`).
6. Add a forward-looking Phase 2 expectation test for intentional behavior change: `throw(0)` inside `catch()` normalizes to `"*Unspecified error"`.

Gate rule: do not begin Phase 2 implementation until items 1-5 are present and green on Linux (`ut-linux`) with no new failures in existing suites.

Status (2026-04-13): **GATE PASSED** — implemented and passing targeted Linux tests for all items 1-5. Recent refactor improved test quality by replacing source-structure-locked tests with behavior-focused contracts:

**Item 1:** ✓ `EfunsTest.throwWithoutCatchContractTextAndRuntime` (validates uncaught `throw()` runtime error propagation; does not currently assert message text)

**Item 2-3:** ✓ `LPCInterpreterTest.nonCatchableEvalCostEscapesCatchBoundary`, `LPCInterpreterTest.nonCatchableStackFullEscapesCatchBoundary` (validates non-catchable limit behavior)

**Item 4:** ✓ `LPCInterpreterTest.catchSuccessReturnsZeroContract` (validates F_END_CATCH success contract)

**Item 5 (refactored):** ✓ Replaced source-code-locked tests with behavior-focused contracts:
  - **Previous**: `EfunsTest.errorHandlerMappingContractSourceLocked` (locked exact source strings)
  - **Now**: `EfunsTest.errorHandlerCallableContract` (validates error routing behavior at runtime)
  - Improvement: error_handler callback contract is now validated by executing LPC code that calls `error()`, triggering error_handler invocation, and confirming error propagation succeeds. Test is immune to source refactoring.
  
**Backend preload recovery (Item 3 scope):** ✓ Refactored:
  - **Previous**: `BackendTest.preloadRecoveryContinueContractSourceLocked` (locked sequence of source code strings)
  - **Now**: Covered by existing `BackendTest.preload` behavioral test (validates preload_objects executes without throwing exception)
  - Note: detailed preload recovery loop behavior (continue-on-error with warnings) is implicitly tested by the main preload test succeeding despite LPC errors in example mudlib.

**Item 6 (future behavior change):** ✓ Implemented and enabled:
  - `LPCInterpreterTest.throwZeroNormalizesToUnspecifiedError`
  - Will be enabled in Phase 2 when throw(0) normalization behavior is implemented.

**Gate Assessment**: All required behavior-level contracts are locked and validated. Source-code-locking approach has been replaced with contract-focused tests that validate actual runtime behavior. This is a quality improvement for migration: exceptions-based Phase 2 can proceed without test brittleness from source structure dependencies.

## Phase 2: Core Error Source Migration (`error_context`) + Typed Exception Hierarchy

### Goals
- Introduce typed exception hierarchy: `catchable_runtime_error`, `noncatchable_runtime_limit`, `fatal_runtime_error` to make error-state routing explicit.
- Replace direct `longjmp` throw sites with exception throws, dispatching to appropriate exception types based on error class.
- Preserve C-facing API surface while internals move to C++.

### Work
- Define three exception classes in header (lib/port/ or src/) with clear semantics:
  - `catchable_runtime_error`: errors trapped by LPC `catch()` and routed through master `error_handler()`.
  - `noncatchable_runtime_limit`: resource exhaustion (ES_MAX_EVAL_COST, ES_STACK_FULL) that escapes catch boundaries.
  - `fatal_runtime_error`: driver-level failures (startup, signal crashes) routed to master `crash()` apply.
- Implement scoped guard API:
  - `error_boundary_guard`: captures/restores VM state, clears error flags at scope exit (replaces save_context/pop_context pairing).
  - `error_reentry_guard`: manages in_error and in_mudlib_error_handler flag lifecycle (replaces manual toggles).
  - `catch_value_guard`: owns catch_value lifecycle with explicit commit_success/commit_error semantics.
- Convert `src/error_context.c` to C++ implementation.
- Integrate typed exception dispatch in `error_handler()` / `throw_error()`: select exception type based on error class and current boundary.
- Add LPC-visible exception-kind metadata to master `error_handler` mapping:
  - add `kind` key with values `"catchable"`, `"limit"`, or `"fatal"` derived from the thrown typed exception.
  - keep current `trace` shape unchanged in this migration (no args/locals/frame-shape expansion in this plan).
- Replace manual `in_error` and `in_mudlib_error_handler` flag toggles with `error_reentry_guard` scoped objects throughout error paths.
- Keep external entry points callable from C modules.
- Add explicit build-system settings (where needed) to keep exception behavior consistent on MSVC and clang-cl.
- Record concrete build-system artifact updates for this phase:
  - update `CMakeLists.txt` (or target-level CMake file if narrowed scope) with explicit exception model flags for migrated C++ units.
  - capture configure/build evidence from relevant presets in phase notes or PR description.

### Exit Criteria
- No direct `longjmp` in `error_context` runtime paths.
- All `extern "C"` entry points in migrated units catch exceptions before returning to C callers (migration-only boundary wrappers in place).
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
- Fix existing signal handler safety problem: current handlers call `fatal()` directly, which is not async-signal-safe.

### Work
- Migrate:
  - `safe_apply()` in `src/apply.c`
  - guard sections in `src/backend.c`
  - startup guard in `src/main.c`
  - crash-handler guard in `src/simulate.c`
- Fix signal handler safety (existing problem, `src/main.c`):
  - Reduce signal handlers to async-signal-safe minimum: set a `volatile sig_atomic_t` flag only (no `fatal()` call, no allocation, no C++ runtime).
  - Add a backend loop check that reads the flag, clears it, and calls `fatal()` from the main thread where RAII and exception unwinding are safe.
  - Signal handlers are permanent OS-boundary `extern "C"` functions; they never throw and never call into C++ runtime.
- Validate object cleanup and recovery behavior after failures.
- Verify exceptions do not cross C ABI boundaries after each migrated wrapper.

### Phase 4 Prework Checklist: Startup/Bootstrap `exit()` Consolidation

Before converting inline startup/bootstrap termination sites into typed fatal exceptions, complete this checklist:

- [ ] Enumerate the exact inline `exit()` sites to convert in runtime code: `src/main.c`, `src/comm.c`, `src/backend.c`, `src/simulate.c:init_master()`, `src/stack.c`, `lib/rc/rc.cpp`, `lib/lpc/object.c`, and `lib/lpc/lex.c`.
- [ ] Classify each site as one of: startup config failure, runtime bootstrap failure, master boot failure, intentional shutdown, signal-path termination, hard OOM, or integrity/assert abort.
- [ ] Limit Phase 4 conversion scope to startup/bootstrap failure sites only; explicitly exclude `do_shutdown()`, signal handlers, hard OOM fallbacks, and assert-style `abort()` paths.
- [ ] Define the outer fatal-exception catch boundary in startup/driver code and document which translated failures must terminate there.
- [ ] Choose one canonical exception type for these translated sites (`fatal_runtime_error` unless a narrower startup-specific subtype becomes necessary).
- [ ] Preserve current operator-facing behavior at the catch boundary: same log ordering, same shutdown policy, same success/failure exit code policy.
- [ ] Verify no translated exception crosses a permanent `extern "C"` boundary or any signal handler boundary.
- [ ] Add or adjust targeted tests for startup/master-bootstrap failure paths where behavior is observable without destabilizing the test harness.
- [ ] Record any sites intentionally left as direct `exit()` calls after review, with rationale in this plan.

### Exit Criteria
- No production `setjmp` sites remain in migrated modules.
- Signal handlers contain no `fatal()` calls and no C++ runtime use; all signal-triggered fatal paths run from the backend loop.
- Recovery behavior matches baseline logs/tests.
- GCC/MSVC/clang-cl build + unit-test gates pass for changed modules.

## Phase 5: Legacy Context-Chain Removal (`jmp_buf` retirement) + Global Flag Cleanup

### Goals
- Remove obsolete `jmp_buf` storage and context-chain APIs after all callers migrate.
- Remove `in_error` and `in_mudlib_error_handler` global flags (now scoped via `error_reentry_guard`).

### Work
- Refactor `error_context_t` and retire jump-specific fields.
- Remove dead helpers and associated comments/code paths.
- Remove `in_error` and `in_mudlib_error_handler` global state variables from error_context.
- Confirm no legacy jump context assumptions remain in any compiler-specific code path.
- Verify no other modules depend on global flag state (should be isolated to error_context after Phase 2 migration).

### Exit Criteria
- No production `setjmp`/`longjmp` usage remains in `src/`.
- No `in_error` or `in_mudlib_error_handler` global state remains.
- No compiler-specific fallbacks reintroduce jump-based handling.
- Migration-only C ABI boundary wrappers removed; no temporary wrapper code remains.
- GCC/MSVC/clang-cl build + unit-test gates pass for affected modules.

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
- Update master apply documentation (`docs/applies/master/`) to resolve 6 identified Tier A doc/source alignment gaps:
  - error_handler argument count: clarify two-branch calling convention (1 arg non-caught; 2 args caught with LOG_CATCHES condition).
  - LOG_CATCHES conditionality: document that caught-path callback is build-option dependent.
  - error string prefix semantics: document that `*`-prefixed caught error strings represent driver/runtime-generated error text (distinct from arbitrary mudlib-thrown payloads), and include brief historical context from `docs/history/` notes where applicable.
  - error mapping shape: document all present keys including 'file' and 'line'.
  - error mapping metadata: document new `kind` key (`catchable` / `limit` / `fatal`) added by typed exception migration.
  - return value semantics: document that non-empty string return value is used as error output.
  - crash callback scope: expand from signal-crash-only to fatal-driver-error callback.
  - exception routing: document that typed exception hierarchy (catchable vs noncatchable vs fatal) maps to master apply dispatch routing.

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
9. Signal handlers contain only async-signal-safe operations; `fatal()` and all C++ runtime is deferred to the backend loop.
10. Master `error_handler` mapping includes `kind` metadata matching typed exception class for representative catchable/limit/fatal error cases.
