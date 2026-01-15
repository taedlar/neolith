# ChangeLog

## neolith-1.0.0-alpha (unreleased)

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
- **Console Mode Piped Stdin Support (Linux/WSL)**: Enabled automated testing via piped stdin by conditionally preserving input data for non-TTY handles. Real terminals still flush input on mode changes for security; pipes preserve all data for testbot automation. Added `safe_tcsetattr()` helper using `isatty()` detection. See [docs/plan/console-testbot-support.md](plan/console-testbot-support.md) for design.

## neolith-0.2
- imported from the code of stable driver used in ES2, which is MudOS v22pre5 with several crasher fixes and support for non-english multi-byte encoding.

## pre-neolith
Original LPMud and MudOS does not have official git repository becuase they are published before these services became mainstream.

The last MudOS released version **v22.2b14** can be found in [maldorne/mudos](https://github.com/maldorne/mudos).
Additional LPMud driver source code and history timeline can also be found in [maldorne/awesome-muds](https://github.com/maldorne/awesome-muds), which is an archive of some interesting stuff.
