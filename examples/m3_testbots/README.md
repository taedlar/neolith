# m3 Testbots

`m3_testbots` contains Python testbot scripts that run the Neolith driver as a subprocess in [console mode](../../docs/manual/console-mode.md) and validate its behavior through piped stdin/stdout. These are integration tests focused on end-to-end interaction with the [m3_mudlib](../m3_mudlib/README.md).

## How It Works

1. The testbot launches the Neolith driver with `pexpect.PopenSpawn`.
2. Commands are sent line-by-line via stdin with interactive pattern matching.
3. The driver processes commands until stdin closes (EOF).
4. On EOF the driver shuts down automatically.
5. The testbot validates a clean exit (exit code 0) and checks expected output.

## Requirements

- Python 3.8+
- [hatch](https://hatch.pypa.io/) (manages the virtual environment and dependencies)
- A built Neolith driver binary

Dependencies are declared in `pyproject.toml` and installed automatically by `hatch`.

## Running the Tests

### One-time hatch setup

Register this project with hatch and enable project mode so `hatch run` works from any directory:

```bash
hatch config set projects.m3_testbots /path/to/neolith/examples/m3_testbots
hatch config set mode project
```

With this configuration hatch resolves the project by name rather than by CWD, so `hatch run smoke_test` works from anywhere. Combined with Neolith resolving `MudlibDir` relative to the conf file's location, the entire test setup requires no absolute paths.

### Running

```bash
hatch run smoke_test
```

Each testbot script controls its own driver launch arguments (config file, console mode flag, etc.). Extra arguments passed on the command line are forwarded to the driver and are typically used for driver-level flags such as debug level, epilog level, or trace flags:

```bash
hatch run smoke_test -d1 -t011     # debug level 1 and trace flags 011 (octal)
hatch run smoke_test -e1           # epilog level 1
```

## Scripts

| Script | Description |
|--------|-------------|
| `src/smoke_test.py` | Sends a basic set of commands (`say`, `help`, `shutdown`) and validates a clean exit. Use as a template for new testbots. |

## Example Output

```
✓ Using driver: ../../out/build/linux/src/RelWithDebInfo/neolith
✓ Using config: ../m3.conf

============================================================
CONSOLE MODE AUTOMATED TEST
============================================================
Platform: posix

Input commands:
  1. say Hello from Python test!
  2. help
  3. shutdown

Driver started. Sending commands...
------------------------------------------------------------
...
✅ TEST PASSED - Driver exited successfully
```

## Writing New Testbots

Copy `src/smoke_test.py` as a starting point. Declare the new entry point in `pyproject.toml` under `[tool.hatch.envs.default.scripts]` so it can be invoked with `hatch run <name>`.

## Further Reading

- [Console Mode](../../docs/manual/console-mode.md)
- [Testbot Support](../../docs/manual/testbot.md)
- [m3_mudlib](../m3_mudlib/README.md)
