---
name: trace
description: "Add opt_trace() calls for debugging and profiling. Use -t option to enable specific trace tiers and levels."
---
# Trace Skill Instructions
The `opt_trace()` macro provides printf-style debug logging with dynamic log levels and categories. The usage pattern is:
```c
opt_trace(tier, "format string", ...);
```
  
**tier** is a bitflag defined in `debug.h` that specifies the trace category (e.g., `TT_COMPILE`, `TT_MEMORY`) of the log message and verbose level. The verbose level is an integer from 0 (least verbose) to 7 (most verbose) that specifies the importance of the log message. In the `opt_trace()` call, the tier is specified with the `TT_` category bits combined with the verbose level in the lowest 3 bits. For example, `TT_COMPILE|1` means a message in the compile category with verbose level 1.

The trace tier is compared against the trace flags specified at runtime via the `-t` option. If the tier matches the enabled flags and the message's verbose level is less than or equal to the enabled level, the message will be printed to the console.

## Running with Tracing Enabled
The `opt_trace()` logging can be enabled with the `-t` option when running neolith. For example:
```bash
# Enables trace tier 020 at verbose level 1
./neolith -f neolith.conf -t 021
```
The trace flags are specified as an octal number, where the bits represent the categories and levels to enable. The lowest 3 bits specify the log level (0-7), while the higher bits specify the tiers. In this example, tier 020 corresponds to category `TT_COMPILE`, and level 1 means only messages with level 0 or 1 will be printed.

The trace tier -1 enables all categories and levels, while 0 disables all tracing.

## Trace Levels
- Level 0: reserved for tracing particular events at development stage. Change to 1 or higher before committing code with `opt_trace()`.
- Level 1: important check-points for the tier, such as completion of a major phase (e.g., end of compilation, start of execution)
- Level 2: significant events within the tier, such as decision points, state changes, or important function calls
- Level 3: detailed information about the flow of execution, such as entering/exiting functions, I/O operations, or memory allocations
- Level 4: verbose information about variable values, intermediate results, or less critical events
- Level 5 to Level 7: increasingly verbose and detailed information, useful for in-depth debugging but may produce a large volume of logs

