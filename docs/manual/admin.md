Neolith Administrator Guide
===========================

# Starting LPMud driver
The LPMud driver executable is the process to listen for incoming user connections.
The typical starting command is:
~~~sh
neolith -f neolith.conf
~~~

Traditionally, you would start the LPMud driver and let it run in the background.
Sometime people would wrap the starting command with a *shell script* and restart it if the driver crashed or shutdown by in-game administrator (e.g. archwizards).

You may use Neolith this way if you're just running a traditional LPMud.
If you are keen to add efuns or integrate LPMud with some other interesting stuff that involves *modifying* the driver, Neolith provides a ["console mode"](console-mode.md) for administrator to experiment with them:
~~~sh
neolith -f neolith.conf -C
~~~

For troubleshooting and debugging, see [trace.md](trace.md) for information about enabling trace flags.

# Command Line Options

Neolith accepts several command line options to control its behavior. All options can be viewed by running `neolith --help`.

## Available Options

| Option | Short | Argument | Description |
|--------|-------|----------|-------------|
| `-f` | | `config-file` | Specifies the file path of the configuration file. If not provided, defaults to `/etc/neolith.conf`. |
| `--console-mode` | `-c` | | Run the driver in console mode. See [console-mode.md](console-mode.md) for details. |
| | `-D` | `macro[=definition]` | Predefines global preprocessor macro for use in mudlib. Can be specified multiple times. |
| `--debug` | `-d` | `debug-level` | Specifies the runtime debug level (integer). Higher values produce more debug output. |
| `--epilog` | `-e` | `epilog-level` | Specifies the epilog level to be passed to the master object's `epilog()` apply. |
| `--pedantic` | `-p` | | Enable pedantic clean up on shutdown. Useful for testing memory leaks. |
| `--trace` | `-t` | `trace-flags` | Specifies an integer of trace flags to enable trace messages in debug log. See [trace.md](trace.md) for details. |

## Examples

Start with a specific configuration file:
~~~sh
neolith -f /path/to/neolith.conf
~~~

Start in console mode with tracing enabled:
~~~sh
neolith -f neolith.conf -c -t 0x40
~~~

Define preprocessor macros for mudlib:
~~~sh
neolith -f neolith.conf -D DEBUG_MODE -D MAX_USERS=100
~~~

Run with debug level 2 and epilog level 1:
~~~sh
neolith -f neolith.conf -d 2 -e 1
~~~

# neolith.conf

Before you can start running your own MUD, you need a configuration file to tell Neolith where is the mudlib along with other settings.
The source code of Neolith includes an example configuration in [src/neolith.conf](src/neolith.conf).

> :bulb: If you don't specify the `-f` option in neolith command line, it finds the configuration file in default location `/etc/neolith.conf`.

## Syntax

- For each line, leading whitespace characters are skipped. Then, if the line is empty or starts with the '`#`' character, the entire line is ignored.
- A setting consists of a name, followed by one or more whitespace character, then followed by the the literal setting value for the rest of the line.
  (This means it is possible to set empty string for a setting, if the setting name is followed by one or more whitespace characters)
- A setting name is case-insensitive (usually in camel case)
- A setting value is case-sensitive, with any trailing whitespace characters stripped for idiot-proof.


## Mandatory Settings

Below is a list of settings that are mandatory.
Name | Value |
--- | --- |
`MudlibDir` | Full-path of the mudlib directory in the host filesystem. |
`MasterFile` | The file path of the privileged master object. |
`Port` | The TCP port for which your MUD shall listen for new connections. |

## Optional Settings

Below is a list of optional settings.
Name | Value | Default |
--- | --- | --- |
`MudName` | Name of the MUD, which is made available to LPC by the pre-defined symbol `MUD_NAME`. | (empty string) |
`LogDir` | The full-path for `log_file()` to create log files. | use stderr (ideal for *read-only* mudlib) |
`DebugLogFile` | The filename of debug log file where the LPMud driver's log messages is appended to. | Use stderr |
`LogWithDate` | Prefix each log message with an ISO-8601 format date and time. | No |
`IncludeDir` | The search path of LPC #include. Multiple paths can be assigned by separate them with `:` character. | Not using |
`GlobalInclude` | An #include header that is automatically included by all LPC programs. | Not using |
`SaveBinaryDir` | The path for storing data file when using `#pragma save_binary`. | Ignores #pragma save_binary |
`SimulEfunFile` | The first LPC object to be loaded, and all its public functions are made available to any LPC program like efuns. | Not using |
`DefaultErrorMessage` | A default message shown to the interactive user when LPC runtime error occurs during processing of the commmand he or she has typed. | Not using |
`DefaultFailMessage` | A default message shown to the interactive user when he or she typed a command that is not recognized by any `add_action` | Not using |
`CleanUpDuration` | A duration in seconds that the LPMud driver's garbage collection routine waits before calling an unused object's `clean_up()` function | 600 |
`ResetDuration` | A duration in seconds between the `reset()` function is called in an object. | 1800 |
`MaxInheritDepth` | Maximum depth of inheritance of LPC objects. | 30 |
`MaxEvaluationCost` | Maximum cost of a LPC code evaluation | 1000000 |
`MaxArraySize` | Maximum size of a LPC array. | 15000 |
`MaxBufferSize` | Maximum size of a LPC buffer. | 4000000 |
`MaxMappingSize` | Maximum size of a LPC mapping. | 15000 |
`MaxStringLength` | Maximum length of a LPC string. | 200000 |
`StackSize` | Maxiumu size of LPC evaluation stack | 1000 |
`MaxLocalVariables` | Maximum number of local variables in a LPC function. | 25 |
`MaxCallDepth` | Maximum depth of LPC function calls before the LPMud driver should abort the evaluation. | 50 |
`ArgumentsInTrace` | Enable output of function call arguments in the dump trace message. | No |
`LocalVariablesInTrace` | Enable output of local variables in the dump trace message. | No |
