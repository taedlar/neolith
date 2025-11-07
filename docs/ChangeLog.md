# ChangeLog

## neolith-1.0.0-alpha
- created source code repository on github.
- clean up non-UTF8 comments and contents.
- changed build system to CMake.
- removed support for gettext for simplicity. (localization should be done in the mudlib).
- refactored trace logging to help driver hacking.
- re-organized directories and break the driver source code into one executable and several static libraries.
- converted documentations to markdown format.
- added googletest as unit-testing framework.
- added unit-tests (C++ code) for several efuns and base components.
- added lpmud driver architecture illustration using mermaid.
- provide pre-release versions with git tags.

## neolith-0.2
- imported from the code of stable driver used in ES2, which is MudOS v22pre5 with several crasher fixes and support for non-english multi-byte encoding.

## pre-neolith
Original LPMud and MudOS does not have official git repository becuase they are published before these services became mainstream.

The last MudOS released version **v22.2b14** can be found in [maldorne/mudos](https://github.com/maldorne/mudos).
Additional LPMud driver source code and history timeline can also be found in [maldorne/awesome-muds](https://github.com/maldorne/awesome-muds), which is an archive of some interesting stuff.
