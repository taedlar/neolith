# CURL Integration Plan

## Overview
Implement non-blocking CURL REST API integration with async worker threads, per-object handle management, and callback dispatch on completion. Total estimated scope: ~2500 LOC across foundation, efuns, and callback dispatch.

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

### Phase 3: Async Runtime Integration ⏸️ NOT STARTED
Implement full async worker thread pool, per-object handle management, and callback dispatch.
- Per-object handle pool (array-based, reusable slots, generation tracking)
- Async task/result queues for main↔worker communication
- Worker thread driving curl_multi_perform and curl_multi_poll
- Completion key integration with async_runtime_wait() from main comm polling loop
- **Blockers**: Requires careful understanding of safe_apply patterns for LPC callbacks during async completion drain

## Current State Handoff

**Completed as of [2025-01-16]:**
- libcurl is integrated as an optional build dependency (CMake option `PACKAGE_CURL`)
- Three efuns are registered and callable from LPC: `perform_using()`, `perform_to()`, `in_perform()`
- Stubs are in place in [lib/curl/curl_efuns.cpp](lib/curl/curl_efuns.cpp)
- Build passes with all tests green

**Next phase requires:**
1. Deep refamiliarization with [docs/internals/async-library.md](docs/internals/async-library.md) for worker/queue/runtime patterns
2. Review [src/apply.c](src/apply.c) and existing callback patterns (heart_beat, reset, init) to design safe callback dispatch during async drain
3. Implementation of Phase 3: worker thread management, per-object handle pool lifecycle, and callback queueing on completion

## Lessons Learned
- CMake target-based dependencies (e.g., `PkgConfig::libcurl`) simplify platform compatibility vs manual dependency linking
- Async library patterns in Neolith are well-designed for this use case (task queue → worker → result queue → completion key)
- The gap between Phase 2 stubs and Phase 3 implementation is substantial due to LPC callback safety requirements; recommend pairing next implementation with dedicated LPC apply/callback design review
