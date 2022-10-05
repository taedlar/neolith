Tracing Neolith LPMud Driver
============================

Troubleshooting a LPMud Driver in your local test is one thing, while troubleshooting a LPMud Driver while there are users connected is another.
It is not likely you can attach a debugger to the process and set breakpoints. Also, there are also cases that bug or crash issues will not reproduce
without user interaction with the MUD. You may need thoe old-fashioned tracing techniques to find out what goes wrong.

# No-break Tracing

To trace Neolith without breakpoints, you can use the command line argument `--trace` or `-t` to set trace flags. For example:
```
$ neolith -f neolith.conf -t 040
```
The trace above enables additional trace messages about simul efuns that shows you the state of simul efun object:
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

The trace flags can enable aditional log messages to help you figure out what Neolith is doing, and provides various information that are helpful to debugging.
Neolith uses the lowest 3 bits of trace flag to represent verbose level, while other bits are bit-masks to enable individual trace tiers.
Conventionally a trace flag is represented as an octal integer so we use the last digit to represent verbose level.

> NOTE: verbose level is shared by all trace tiers, for simplicity.

# Trace Tiers

## Tier 010: TT_EVAL

Enables trace messages about LPC code evaluation.

## Tier 020: TT_COMPILE

Enables trace messages about compiling LPC programs.

## Tier 040: TT_SIMUL_EFUN

Enables trace messages about Simul Efuns.
