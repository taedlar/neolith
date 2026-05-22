# m3 Mudlib

`m3_mudlib` is a minimal LPC mudlib used as both a hands-on example and the backing mudlib for Neolith's integration tests. It implements just enough of the [standard driver applies](../../docs/applies/) to boot the driver and support interactive or scripted sessions.

## Structure

| Path | Purpose |
|------|---------|
| `master.c` | Master object — implements driver applies (`valid_read`, `valid_write`, `connect`, etc.) |
| `user.c` | Interactive user object — handles player input and built-in commands |
| `simul_efun.c` | Simulated efuns available globally to all LPC code in the mudlib |
| `base/room.c` | Base room implementation |
| `room/start_room.c` | Starting room players are moved into on login |
| `room/observatory.c` | Additional example room |
| `api/unicode.c` | Unicode utility helpers |
| `apps/models.c` | Example application models |
| `config.h` | Compile-time mudlib configuration constants |

## Available Commands

Once connected (interactive or console mode), the following commands are available:

| Command | Description |
|---------|-------------|
| `say <message>` | Broadcast a message to the room |
| `help` | List available commands |
| `quit` | Disconnect |
| `shutdown` | Shut down the driver (useful in console/test mode) |
| `curlget <url>` | Fetch a URL asynchronously (requires `PACKAGE_CURL=ON`) |

## Running Manually

Run from the `examples/` directory, where `m3.conf` lives alongside `m3_mudlib/`.

### Interactive console

```bash
/path/to/neolith -f m3.conf -c
```

### Piped commands (non-interactive / scripted)

```bash
echo -e "say hello\nhelp\nshutdown" | /path/to/neolith -f m3.conf -c
```

### Networked (telnet)

```bash
/path/to/neolith -f m3.conf
telnet localhost <port>
```

The port is defined in `m3.conf` (`Port` setting).

## Configuration

`m3.conf` sits alongside `m3_mudlib/` in the `examples/` directory. Its `MudlibDir` is set to the relative path `m3_mudlib`, which Neolith resolves relative to the conf file's own location. This makes the entire `examples/` directory relocatable — no absolute paths required. (MudOS required `MudlibDir` to be an absolute path.)

To customize for local development, copy and edit it from `examples/`:

```bash
cp m3.conf m3.local.conf
# Edit m3.local.conf (port, log destination, etc.)
```

## Troubleshooting

**Driver exits immediately** — Check that `MudlibDir` in the config points to this directory and that `master.c` compiles without errors. Run with `-t` for trace output.

**"Failed to get console mode" warnings** — Expected on Windows when stdin/stdout are pipes. The driver handles this automatically.

**Commands not processed** — Ensure each command ends with a newline. On Linux use `echo -e`; on PowerShell use `` `n `` as the newline character.

## Further Reading

- [Console Mode](../../docs/manual/console-mode.md)
- [Developer Manual](../../docs/manual/dev.md)
- [Driver Applies Reference](../../docs/applies/)
