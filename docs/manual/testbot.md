# Testbot

## Overview

Neolith supports automated integration testing via piped stdin/stdout on Linux and Windows. When the driver detects that stdin is a pipe rather than an interactive terminal, it switches to a pipe-friendly I/O mode: commands are read from stdin as-is, and the driver shuts down cleanly when stdin reaches EOF.

This makes it straightforward to script interactions against a real running driver without a telnet client or special test harness.

## Driver Behavior in Pipe Mode

| Condition | Behavior |
|-----------|----------|
| stdin is a real terminal | Interactive console UX; input flushed on mode changes for security |
| stdin is a pipe or file | All input data preserved; clean shutdown (`exit 0`) on EOF |

The switch is automatic — no special flag is needed beyond `-c` (console mode).

## Piped commands without a testbot

For quick one-off checks you can pipe commands directly:

```bash
# Linux
echo -e "say test\nshutdown" | /path/to/neolith -f your.conf -c

# Windows (PowerShell)
"say test`nshutdown" | neolith.exe -f your.conf -c
```

Simple piping works well for fire-and-forget sequences, but it provides no way to wait for a specific response before sending the next command, or to assert on particular output. For that, use `pexpect`.

## Interactive testing with pexpect

[pexpect](https://pexpect.readthedocs.io/) gives you full control over the interaction: send a command, wait for a specific pattern in the output, then send the next command. Use `pexpect.PopenSpawn` to launch the driver as a subprocess (works on both Linux and Windows):

```python
import pexpect
from pexpect.popen_spawn import PopenSpawn

child = PopenSpawn(
    ["/path/to/neolith", "-f", "your.conf", "-c"],
    timeout=10, encoding="utf-8"
)

child.sendline("say hello")
child.expect("You say: hello")   # blocks until the pattern appears

child.sendline("shutdown")
child.expect(pexpect.EOF)
child.wait()
assert child.exitstatus == 0
```

Each `expect()` call blocks until the pattern appears in stdout or a timeout fires, making the test sensitive to actual driver output rather than timing. A `pexpect.TIMEOUT` exception is raised when a pattern is not seen within the timeout, which surfaces as a test failure.

## Using hatch to manage testbots

[hatch](https://hatch.pypa.io/) is the recommended way to manage a testbot project. It handles the Python virtual environment and dependencies, and with project mode enabled it resolves the project by name so `hatch run` works from any working directory.

### One-time setup

```bash
hatch config set projects.<your-project> /path/to/your/testbots
hatch config set mode project
```

### Declaring scripts

In your `pyproject.toml`, declare each testbot as a script under `[tool.hatch.envs.default.scripts]`:

```toml
[tool.hatch.envs.default.scripts]
my_test = "python src/my_test.py {args}"
```

### Running

```bash
hatch run my_test
```

Extra arguments are forwarded to the driver and are useful for tuning debug, epilog, or trace levels:

```bash
hatch run my_test -d 1 -t 011  # debug level 1 and trace flags 011 (octal)
hatch run my_test -e 1         # epilog level 1
```

## Related Documentation

- [Console Mode](console-mode.md) — console mode flags and behavior
- [m3_testbots](../../examples/m3_testbots/README.md) — reference testbot implementation using the m3 mudlib

