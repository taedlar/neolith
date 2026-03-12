# Documentation File Organization

Documentation files should be named using lowercase letters and dashes (`-`) as word separators. Avoid underscores and camelCase.
Prefix the filenames with the library name, feature or subsystem name for easy identification (e.g., `async-dns-integration.md`).

## Design & Planning ([docs/plan/](plan/))

- Create design docs here before implementation starts. Move to manual/internals when implementation begins.
- When extending an existing feature, update the original design doc instead of creating a new one.
- Don't duplicate implementation details here; focus on high-level design, rationale, alternatives considered, and final decisions.

## High-level Design Documentation ([docs/manual/](manual/))

- [admin.md](manual/admin.md): Admin guide for server operators - configuration options, logging, performance tuning.
- [lpc.md](manual/lpc.md): LPC language reference - syntax, semantics, standard libraries.
- [efuns.md](manual/efuns.md): Comprehensive efun reference manual, categorized by functionality.
- [dev.md](manual/dev.md): Developer workflow and build system, testing patterns and git workflow.
- [unit-tests.md](manual/unit-tests.md): Guidelines for writing and organizing unit tests using GoogleTest.
- [console-mode.md](manual/console-mode.md): Using the interactive console mode for debugging and live interaction.
- [internals.md](manual/internals.md): Driver architecture overview. Links to [docs/internals/](internals/) for deep dives into specific subsystems.
- [trace.md](manual/trace.md): Debugging and tracing guide - how to enable and interpret trace logs.
- When extending an existing feature, update the original design doc instead of creating a new one.
- Keep these documents updated as the codebase evolves. Focus on high-level architecture and terminology for current code.
- High-level concepts includes features that are visible to mudlib developers (efuns, applies, object model, compiler behavior).
- When starting new features, create design docs in [docs/plan/](plan/) first, then move to manual when implementation starts. Keep implementation status updated. Link implementation details back to design docs.

## Implementation Details ([docs/internals/](internals/))

- Keep these documents focused on design decisions, technical specifications and internal architecture such as C APIs and data structures.
- Update as implementation details change. Link back to high-level design docs in [docs/manual/internals.md](manual/internals.md).
- [lpc-types.md](internals/lpc-types.md): Complete LPC type system reference - lpc_type_t vs svalue_type_t, encoding schemes, compatibility checking, common pitfalls
- [lpc-program.md](internals/lpc-program.md): Complete LPC compiler memory block system, binary save/load format, pointer serialization, inheritance resolution
- [int64-design.md](internals/int64-design.md): Platform-agnostic 64-bit integer implementation - runtime types, bytecode encoding, binary compatibility
- [async-library.md](internals/async-library.md): Async library design - queues, workers, runtime integration

When working on compiler features, consult these documents for:
- **Type system rules**: lpc_type_t vs svalue_type_t domains, masking NAME_TYPE_MOD, array/class detection
- **Integer handling**: svalue_u.number is int64_t, use PRId64 for formatting, F_LONG opcode for large literals
- Memory block allocations and their data types
- Binary file format and version validation
- Function/variable/class indexing schemes
- Pointer conversion during serialization
- Switch table patching mechanics
