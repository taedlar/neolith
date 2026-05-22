# Neolith Examples

This directory contains example MUD server applications, a minimal example mudlib, and Python integration testbots for the Neolith LPMud driver.

## Example Types

### [apps/](apps/README.md) — MUD Applications

Single-file LPC programs passed directly to the driver. Use these to quickly try out driver features or prototype new functionality without setting up a full mudlib.

```bash
/path/to/neolith -c apps/hello_world.c
```

### [m3_mudlib/](m3_mudlib/README.md) — Example Mudlib

A minimal LPC mudlib that boots the driver and supports interactive or scripted sessions. It is also the backing mudlib for Neolith's integration tests.

```bash
/path/to/neolith -f m3.conf -c
```

### [m3_testbots/](m3_testbots/README.md) — Integration Testbots

Python scripts that run the driver as a subprocess in console mode and validate end-to-end behavior by sending commands through piped stdin/stdout.

```bash
hatch run smoke_test
```

Requires a one-time hatch setup — see [m3_testbots/README.md](m3_testbots/README.md) for details.

## Configuration

`m3.conf` sits alongside `m3_mudlib/` in this directory. Its `MudlibDir` is the relative path `m3_mudlib`, which Neolith resolves relative to the conf file's own location. This keeps the whole `examples/` tree relocatable — no absolute paths needed. (MudOS required `MudlibDir` to be absolute.)

Copy to `m3.local.conf` for local customization:

```bash
cp m3.conf m3.local.conf
```

> [!NOTE]
> The `m3.conf` disables telnet port by default to accelerate Neolith unit-testing and allows multiple instances of MUD applications.
> You can enable the telnet port by uncommenting the `Port` line.

## Further Reading

- [Console Mode](../docs/manual/console-mode.md)
- [Developer Manual](../docs/manual/dev.md)
