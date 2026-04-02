# CURL Integration Plan

## Overview
Implement non-blocking CURL REST API integration with async worker threads, per-object handle management, and callback dispatch on completion. Total estimated scope: ~2500 LOC across foundation, efuns, and callback dispatch.

**All phases complete.** Ready to close or archive.

## Project Status

### Phase 1: Foundation ✅ COMPLETE
- [x] Add `PACKAGE_CURL` CMake option (toggles compilation)
- [x] Create [lib/curl/](lib/curl/) library structure  
- [x] Integrate `libcurl` dependency in CMakeLists.txt
- [x] Build and compile successfully
- **Status**: All foundation work complete; library compiles cleanly

### Phase 2: Efun Registration & Stubs ✅ COMPLETE
- [x] Create [lib/curl/func_spec.c.in](lib/curl/func_spec.c.in) with 3 efuns (perform_using, perform_to, in_perform)
- [x] Register in [lib/lpc/func_spec.c.in](lib/lpc/func_spec.c.in) 
- [x] Update CMake to generate [build/lib/curl/func_spec.i](build/lib/curl/func_spec.i) from template
- [x] Ensure efun prototypes compile into LPC object signatures
- [x] Implement stub versions in [lib/curl/curl_efuns.cpp](lib/curl/curl_efuns.cpp)
- **Status**: Efuns register in LPC, callable but return empty/0; all traces green

### Phase 3: Async Runtime Integration ✅ COMPLETE
Implemented full async worker thread pool, per-object handle management, and callback dispatch.
- [x] Per-object handle pool (array-based, reusable slots, generation tracking)
- [x] Async task/result queues for main↔worker communication
- [x] Worker thread driving `curl_multi_perform` and `curl_multi_poll`
- [x] Completion key integration with `async_runtime_wait()` via `CURL_COMPLETION_KEY` in [src/comm.c](src/comm.c)
- [x] `drain_curl_completions()` dispatches callbacks via `safe_apply` and `safe_call_function_pointer`
- [x] `init_curl_subsystem()` / `deinit_curl_subsystem()` wired into comm init/deinit
- [x] `close_curl_handles()` cleans up per-object state on object destruction
- **Status**: All implementation complete; build and tests pass

## Current State Handoff

**Completed as of [2026-04-01]:**
- All three phases fully implemented and passing
- libcurl integrated as an optional build dependency (`PACKAGE_CURL`)
- Three efuns fully operational from LPC: `perform_using()`, `perform_to()`, `in_perform()`
- Non-blocking async worker drives `curl_multi_perform`; completions drain on the main loop via `CURL_COMPLETION_KEY`
- Callback dispatch uses `safe_apply` / `safe_call_function_pointer` with generation-based stale-completion guards
- Object destruction correctly cancels in-flight transfers and cleans up handles

## Lessons Learned
- CMake target-based dependencies (e.g., `PkgConfig::libcurl`) simplify platform compatibility vs manual dependency linking
- Async library patterns in Neolith are well-designed for this use case (task queue → worker → result queue → completion key)
- The gap between Phase 2 stubs and Phase 3 implementation is substantial due to LPC callback safety requirements; recommend pairing next implementation with dedicated LPC apply/callback design review
