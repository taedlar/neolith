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

### Exit Criteria
- Migration deemed stable, documented, and ready for plan close.
- Portability sign-off completed for GCC/MSVC/clang-cl.

## Verification Matrix

1. Nested catch/throw behavior remains correct.
2. Max eval and deep recursion remain non-catchable where expected.
3. Backend continues after recoverable LPC errors.
4. Startup/preload failures produce expected shutdown behavior.
5. Crash handler path still degrades safely under secondary failures.
6. `src/` grep confirms no remaining production `setjmp`/`longjmp` runtime usage.
7. GCC/MSVC/clang-cl all pass required build and unit-test presets.
8. No exception escapes through `extern "C"` interfaces.
