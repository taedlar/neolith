Tracing Neolith LPMud Driver
============================

Troubleshooting a LPMud Driver in your local test is one thing, but troubleshooting a LPMud Driver **while users are connected** is another.
It is not likely you can attach a debugger to the process and set breakpoints. Also, there are cases where bug or crash issues will not reproduce
without user interaction with the MUD. You may need the old-fashioned tracing techniques to find out what goes wrong.

## Command Line Option

Tracing is enabled via the `--trace` (or `-t`) command line option. The argument is an integer representing trace flags that control which trace messages are enabled.

### Usage

~~~sh
neolith -f neolith.conf -t <trace-flags>
~~~

The trace flags argument can be specified in decimal, hexadecimal (prefix `0x`), or octal (prefix `0`) notation:

~~~sh
neolith -f neolith.conf -t 040       # Octal: simul efun tracing
neolith -f neolith.conf -t 0x20      # Hex: simul efun tracing (equivalent)
neolith -f neolith.conf -t 32        # Decimal: simul efun tracing (equivalent)
~~~

## Trace Flag Format

Neolith uses a bitmask system for trace flags:
- **Lowest 3 bits (0-7)**: Verbose level
- **Higher bits**: Individual trace tier enable flags

Conventionally, trace flags are represented as **octal integers** so the last digit represents the verbose level.

### Verbose Levels

| Level | Description |
|-------|-------------|
| 0 | Basic tracing (if tier is enabled) |
| 1 | Additional detail |
| 2 | More detail |
| 3 | Maximum verbosity |

> **NOTE**: Verbose level is shared by all enabled trace tiers.

## Example Output

Here's sample output when enabling simul efun tracing (`-t 040`):

```
2022-10-05 16:00:35     {}      ----- loading simul efuns -----
2022-10-05 16:00:35     simul_efun loaded successfully.
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   atoi: runtime_index=0
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   chinese_number: runtime_index=1
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   chinese_period: runtime_index=2
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   to_chinese: runtime_index=3
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   break_chinese_string: runtime_index=4
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   currency_string: runtime_index=5
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   cat: runtime_index=6
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   log_file: runtime_index=7
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   assure_file: runtime_index=8
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   base_name: runtime_index=9
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   gender_self: runtime_index=10
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   gender_pronoun: runtime_index=11
2022-10-05 16:00:35     ["TRACE","simul_efun.c",217,"find_or_add_simul_efun"]   getoid: runtime_index=12
```

## Trace Tiers

Trace tiers are specific subsystems of the driver that can be individually enabled for tracing. Each tier is represented by a bit in the trace flags.

### Tier 010 (Octal): TT_EVAL

**Description**: Enables trace messages about LPC code evaluation.

**Flag Values**:
- `010`: Basic evaluation tracing
- `011`: Switch statement internals
- `013`: Per-instruction program counter change

**Use Cases**: Debugging LPC execution flow, understanding control flow issues, tracking evaluation costs.

### Tier 020 (Octal): TT_COMPILE

**Description**: Enables trace messages about compiling LPC programs.

**Flag Values**:
- `020`: Basic compilation tracing
- `021`: Detailed info of compiled LPC program, save binary operations
- `023`: Header includes and file dependencies

**Use Cases**: Debugging compilation errors, understanding include dependencies, tracking program loading.

### Tier 040 (Octal): TT_SIMUL_EFUN

**Description**: Enables trace messages about Simul Efuns.

**Flag Values**:
- `040`: Simul efun registration and basic calls
- `042`: Number of arguments when calling simul efun

**Use Cases**: Debugging simul efun loading, verifying function registration, tracking simul efun usage.

### Tier 0100 (Octal): TT_BACKEND

**Description**: Enables trace messages about LPMud driver backend activities (heart beats, call outs, reset, garbage collection).

**Flag Values**:
- `0100`: Call_out scheduling and execution
- `0101`: Heart_beat timer and summary statistics
- `0102`: SIGALRM signals, individual heart_beat executions

**Use Cases**: Debugging timing issues, tracking object lifecycle, monitoring background tasks.

## Combining Trace Flags

Multiple trace tiers can be enabled simultaneously by combining their octal values:

~~~sh
# Enable both compilation and simul efun tracing with verbose level 1
neolith -f neolith.conf -t 061  # 020 + 040 + 1

# Enable all tiers with maximum verbosity
neolith -f neolith.conf -t 0173  # 010 + 020 + 040 + 0100 + 3
~~~

## Configuration File Alternative

Instead of using the command line, you can also enable tracing via configuration file settings:

| Setting | Description |
|---------|-------------|
| `ArgumentsInTrace` | Enable output of function call arguments in the dump trace message. Equivalent to `DUMP_WITH_ARGS` (0x0001). |
| `LocalVariablesInTrace` | Enable output of local variables in the dump trace message. Equivalent to `DUMP_WITH_LOCALVARS` (0x0002). |

These settings affect the stack trace output produced when errors occur or when `dump_trace()` is called.

## Debugging with GDB

A typical usage for trace logging is to debug crashing bugs. You can run Neolith under the `gdb` debugger:

~~~sh
gdb /path/to/neolith
(gdb) run -t 0173 -f /path/to/neolith.conf
~~~

The trace flag `0173` enables all trace tiers with verbose level 3, producing maximum diagnostic output.

### GDB Workflow

1. **Start the driver**: The driver starts under debugger control with full tracing enabled. You'll see extensive trace logs during startup and preloading.

2. **Connect and test**: Connect to the MUD from another terminal and perform your tests.

3. **Handle crashes**: If your MUD crashes, the `neolith` process breaks in the debugger, allowing you to examine the stack trace and variable states.

4. **Interrupt execution**: If your LPC code gets stuck and you cannot escape with regular mudlib commands, press Ctrl-C in the `gdb` session to interrupt the `neolith` process.

5. **Force shutdown**: Use the debugger's `kill` command to terminate the `neolith` process and disconnect all MUD connections.

### Useful GDB Commands

~~~sh
(gdb) bt                    # Print backtrace
(gdb) info locals          # Show local variables
(gdb) print variable_name  # Examine a specific variable
(gdb) continue             # Resume execution after Ctrl-C
(gdb) quit                 # Exit debugger (terminates process)
~~~

## Best Practices

1. **Start with specific tiers**: Enable only the trace tiers relevant to your issue to reduce log noise.

2. **Increase verbosity gradually**: Start with verbose level 0, increase only if you need more detail.

3. **Use log files**: Configure `DebugLogFile` and `LogWithDate` in your config file for timestamped, persistent logs.

4. **Combine with console mode**: Use `-c` together with `-t` for interactive debugging without network connections.

5. **Production tracing**: Use minimal tracing in production (if any) to avoid performance impact and excessive log sizes.

