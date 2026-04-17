# ChangeLog

## neolith-1.0.0-alpha (unreleased)

### Unreleased

### 1.0.0-alpha.9 — 2026-04-18

#### Changes since 1.0.0-alpha.8

#### Features
- feat: add optional `PACKAGE_JSON` support with `to_json()` and `from_json()` efuns
- feat: extend `from_json()` to accept `buffer` input for large JSON payloads
- feat: add optional `PACKAGE_CURL` support with `perform_using()`, `perform_to()`, and `in_perform()` efuns
- feat: add `c_str()` efun to make C-string truncation an explicit LPC contract
- feat: harden counted-string comparison and JSON/CURL text-option boundaries to use byte-span semantics and explicit embedded-NUL rejection/conversion rules

#### Documentation
- doc: add efun docs for `to_json()` and `from_json()`
- doc: add efun docs and unit tests for CURL efuns
- doc: add guarded `curlget` PACKAGE_CURL example to the sample mudlib
- doc: add `c_str()` efun docs and unit tests

### 1.0.0-alpha.8 — 2026-03-25

#### Changes since 1.0.0-alpha.7

##### Features
- f6be500 feat: add PACKAGE_SOCKETS cmake option (#309)
- dcdd4ce feat: shared resolver admission control (#307)
- b8ea5fc feat: implement c-ares backed shared resolver with tests (#306)
- 2951ac1 feat: add dns lookup cache for shared resolver (#305)
- cf69a0f feat: add resolver worker pool to prevent head-of-line (#304)
- 77d1999 feat: implement shared resolver for no c-ares case (#303)
- 6b1f998 feat: import or fetch c-ares (#300)
- eefcfc3 feat: async reverse dns resolver (#298)
- 53bf590 feat: align socket efuns with lib/async (#296)
- 67a018a feat: more socket efun behavior tests (#295)
- 9313e38 feat: echo/single-char handling on windows console mode (#294)
- 2bf4ea6 feat: add socket efun unit-tests (#293)
- 5e20cf5 feat: add cmake variable FETCH_CURL_FROM_SOURCE (#291)
- 85659df feat: add cmake variable FETCH_OPENSSL_FROM_SOURCE (#290)
- bbb9ac2 feat: archive agent reports to avoid RAG noise (#287)

##### Bug Fixes
- 2faae00 fix: resolved design decisions of shared resolver (#302)
- 60c457a fix: socket_connect() DNS option (#301)

##### Documentation
- b518e94 doc: shared resolver and socket efuns (#308)

##### Other
- 307e9c0 ai: add prompt for release (#310)
- a4f9cca agent: condense instructions (#288)

### 1.0.0-alpha.7 — 2026-03-09

#### Changes since 1.0.0-alpha.6
- cd6ac70 feat: add portable crypt based on openssl (#286)
- 169ca2c doc: guideline about optional packages (#285)
- fd51ecc chore: detect optional dependencies (#284)
- 13c88f7 fix: null guard all_users in process_io to prevent segfault in network mode (#283)
- 4cfce93 chore: fix include setting of grammar.h (#282)
- 8a88199 clean: control stack handling (#281)
- e436bc9 clean: separate command handling from comm.c (#280)
- efa71a8 fix: telnet echo handling (#279)
- 901a745 doc: update build info (#278)
- e9b97e3 fix: call_function to access current_object variables (#277)
- 4ae5a70 doc: update m3_mudlib (#276)
- fc66990 chore: linux-build error (#275)
- 756ce6f fix: socket async read event (#274)
- c7760f4 feat: add clang-cl build (#273)
- 470ed2d doc: update comm refactor plan (#272)
- b2f9675 chore: move in_edit() efun to ed.c (#271)
- c95bb57 feat: unify input_to, get_char and add_action efun carryover arguments (#270)
- d017f46 doc: add sentence enhance plan (#269)
- 5dbda1f doc: comm refactor plan (#268)
- 6e479fc clean: split outbuf.c from comm.c (#267)
- 3bb12f3 clean: lpc interpreter source code (#266)
- 3f54f16 fix: refactor sync with c++ primitives (#265)
- 04c74c9 chore: remove librt timer (#264)
- bd2f28a fix: refactor timer with c++11 primitives (#263)
- c388ce8 clean: f_localtime (#262)
- 9c81bcf feat: foreach extracts unicode chars in strings (#261)

### 1.0.0-alpha.6 — 2026-02-03

#### Changes since 1.0.0-alpha.5
- e1b21f6 fix: lpc warnings (Ted Zhang)
- 99d564a fix: sprintf warnings (Ted Zhang)
- 1c7dc1c fix: windows compile warnings (Ted Zhang)
- 8ecb713 fix: file utils warnings (Ted Zhang)
- dabbe3e fix: various warnings (Ted Zhang)
- 896c84f fix: misc. warnings (Ted Zhang)
- 58c736e fix: warnings in compiler.c (taedlar)
- c284ac9 fix: misc int type warnings (Ted Zhang)
- bbea0b8 clean: update set_eval_limit docs (Ted Zhang)
- 80b8eb7 fix: console worker unit tests (Ted Zhang)
- 1518362 clean: console type detection on windows (Ted Zhang)
- 0e99d28 fix: posix console type (Ted Zhang)
- d3979d5 fix: leaked shared strings (Ted Zhang)
- 0fafe10 fix: destruct objects in tear down (Ted Zhang)
- 0e313ee fix: free permanent identifiers (Ted Zhang)
- 8428eea fix: updating vital objects (Ted Zhang)
- 32eb865 fix: free ip entry string (Ted Zhang)
- 31de21f clean: fix misc warnings (Ted Zhang)
- ec67b2b clean: fix windows build warnings (Ted Zhang)
- 8691abe fix: accept worker for windows iocp integration (Ted Zhang)
- 16727e7 feat: add timer flags (Ted Zhang)
- 89c1b34 feat: integrate async_worker with port_event_t (Ted Zhang)
- fb84ba3 fix: windows line mode setting (Ted Zhang)
- d8ac4e8 feat: integrate console mode with lib/async (Ted Zhang)
- 96ef28b feat: add library async to unify io reactor and worker threads (Ted Zhang)
- caa079f doc: add design for lib/async (Ted Zhang)
- a31965b feat: use pexpect for testbot (Ted Zhang)
- 6f6f8d7 feat: added console testbot (Ted Zhang)
- 47617f4 clean: simplify lpc compiler init (Ted Zhang)

### Security & Fairness
- **Fixed command processing fairness exploit**: Implemented turn-based token system to prevent single users from monopolizing backend cycles through command buffering. Each connected user now processes exactly one buffered command per backend cycle, ensuring fair round-robin scheduling. See [docs/internals/user-command-turn.md](internals/user-command-turn.md) for design details.
  - Added `HAS_CMD_TURN` flag to `interactive_t->iflags`
  - Backend loop grants turns to all connected users each cycle
  - Command processing loop uses actual connected user count instead of historical peak
  - Zero timeout when unprocessed commands remain (improves responsiveness)
  - `command()` efun unaffected (LPC-initiated commands bypass turn system)

### Development & Testing
- created source code repository on github.
- clean up non-UTF8 comments and contents.
- changed build system to CMake.
- removed support for gettext for simplicity. (localization should be done in the mudlib).
- refactored trace logging to help driver hacking.
- re-organized directories and break the driver source code into one executable and several static libraries.
- converted documentations to markdown format.
- added googletest as unit-testing framework.
- added unit-tests (C++ code) for several efuns and base components.
- added unit-tests for command fairness system.
- added lpmud driver architecture illustration using mermaid.
- provide pre-release versions with git tags.
- **Console Mode Piped Stdin Support**: Enabled automated testing via piped stdin on both Linux/WSL and Windows platforms.
  - **Linux/WSL (Phase 1)**: Added `safe_tcsetattr()` helper using `isatty()` detection to conditionally preserve input data for non-TTY handles. Real terminals still flush input on mode changes for security; pipes preserve all data for testbot automation.
  - **Windows (Phase 2)**: Implemented handle type detection with `GetFileType()` to distinguish real consoles from pipes/files. Uses synchronous `ReadFile()` for pipes instead of overlapped I/O, mirroring POSIX simplicity.
  - **EOF Handling**: Pipes/files trigger clean shutdown instead of console reconnection loop when input closes.
  - **Reactor Improvements**: Fixed IOCP timeout handling to properly wait on socket I/O completions. Fixed `io_reactor_wakeup()` to signal both event handle and IOCP for reliable timer interrupts.
  - **Testing**: Added shutdown command to example mudlib. Updated `testbot.py` to exit cleanly on pipe closure. All 37 io_reactor unit tests passing.
  - See [docs/manual/console-testbot-support.md](manual/console-testbot-support.md) for design overview.

## neolith-0.2
- imported from the code of stable driver used in ES2, which is MudOS v22pre5 with several crasher fixes and support for non-english multi-byte encoding.

## pre-neolith
Original LPMud and MudOS does not have official git repository becuase they are published before these services became mainstream.

The last MudOS released version **v22.2b14** can be found in [maldorne/mudos](https://github.com/maldorne/mudos).
Additional LPMud driver source code and history timeline can also be found in [maldorne/awesome-muds](https://github.com/maldorne/awesome-muds), which is an archive of some interesting stuff.
