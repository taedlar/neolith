# Neolith Examples

This directory contains example mudlibs and testing tools for the Neolith LPMud driver.

## Contents

### m3_mudlib/
A minimal example mudlib demonstrating core LPC functionality:
- **master.c**: Master object implementing driver applies
- **user.c**: Interactive user object with basic commands
- **simul_efun.c**: Simulated efuns (mudlib-level functions)

**Available Commands**:
- `say <message>` - Say something
- `help` - Show available commands
- `quit` - Disconnect from MUD
- `shutdown` - Shutdown the driver (console mode)

### testbot.py
Example of an automated **testing robot** using console mode.
The python script runs Neolith with `popen` and enables console mode to send command and receive outputs via piped stdin/stdout of the LPMud server process.

**Purpose**: Validate cross-platform piped stdin support for automated testing.

**Platform Support**:
- ✅ Linux/WSL
- ✅ Windows
- Requires `pexpect` python module for interaction with the mudlib. It can be installed via `pip install pexpect`.

**Usage**:
- Create a python virtual environment and install `pexpect`.
- Activate the virtual environment
```bash
cd examples
python testbot.py
```

**How It Works**:
1. Starts the Neolith driver in console mode (`-c` flag)
2. Pipes test commands via stdin
3. Driver processes commands until pipe closes (EOF)
4. Driver automatically shuts down on pipe EOF
5. Test validates clean exit (exit code 0)

**Example Output**:
```
✓ Using driver: ../out/build/vs16-x64/src/RelWithDebInfo/neolith.exe
✓ Using config: m3.conf

============================================================
CONSOLE MODE AUTOMATED TEST
============================================================
Platform: nt

Input commands:
  1. say Hello from Python test!
  2. help
  3. shutdown

Driver started. Sending commands...
------------------------------------------------------------
...
✅ TEST PASSED - Driver exited successfully
```

## Configuration Files

### m3.conf
Example configuration for the m3_mudlib:
- Minimal configuration for testing
- Uses relative paths for convenience
- Logs to stderr by default

**Customization**:
Copy `m3.conf` to `m3.local.conf` for local testing:
```bash
cp m3.conf m3.local.conf
# Edit m3.local.conf to customize paths, ports, etc.
```

## Manual Testing

### Interactive Console Mode

**Linux/WSL**:
```bash
../out/build/linux/src/RelWithDebInfo/neolith -f m3.conf -c
```

**Windows**:
```powershell
..\out\build\vs16-x64\src\RelWithDebInfo\neolith.exe -f m3.conf -c
```

### Piped Commands

**Linux/WSL**:
```bash
echo -e "say test\nhelp\nshutdown" | ../out/build/linux/src/RelWithDebInfo/neolith -f m3.conf -c
```

**Windows PowerShell**:
```powershell
"say test`nhelp`nshutdown" | ..\out\build\vs16-x64\src\RelWithDebInfo\neolith.exe -f m3.conf -c
```

### File Redirect

**Linux/WSL**:
```bash
echo -e "say test\nhelp\nshutdown" > commands.txt
../out/build/linux/src/RelWithDebInfo/neolith -f m3.conf -c < commands.txt
```

**Windows PowerShell**:
```powershell
"say test`nhelp`nshutdown" | Out-File commands.txt
Get-Content commands.txt | ..\out\build\vs16-x64\src\RelWithDebInfo\neolith.exe -f m3.conf -c
```

## Expected Behavior

### Real Console (Interactive)
- Driver accepts keyboard input
- Reconnection supported after disconnect
- Unicode input works correctly
- Ctrl+C interrupts driver

### Piped/Redirected Input (Automated)
- Driver processes all commands from pipe/file
- Automatic shutdown on EOF (pipe closure)
- No reconnection prompt
- Clean exit with code 0

## Troubleshooting

### "Failed to get console mode" warnings
**Expected on Windows with piped stdin**. These are informational messages indicating stdin/stdout are not real console handles. The driver detects this and uses appropriate I/O methods.

### Driver exits immediately
- Check `MudlibDir` path in config file
- Verify master.c compiles successfully
- Check debug log for compilation errors

### Commands not processed
- Ensure commands end with newline
- Windows: Use backtick-n (`` `n ``) for newlines in PowerShell
- Linux: Use `\n` in echo with `-e` flag

### Test timeout
If testbot.py times out, the driver might not be exiting cleanly. Check:
- Shutdown command is included in test commands
- Driver logs don't show errors preventing shutdown
- No infinite loops in LPC code

## Further Reading

- [Console Mode Documentation](../docs/manual/console-mode.md)
- [Console Testbot Support](../docs/manual/console-testbot-support.md)
- [Developer Manual](../docs/manual/dev.md)
