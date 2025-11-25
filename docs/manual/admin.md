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
If you are keen to add efuns or integrate LPMud with some other interesting stuff that involves *modifying* the driver, Neolith provides a "console mode" for administrator to experiment with them:
~~~sh
neolith -f neolith.conf -C
~~~
The `-C` option (or `--console-mode`) enables the LPMud driver to treat **standard input** as a "connection" from the console.
- After the master object finishes preloading, it receives a `connect()` apply with port number = 0
- The master object can then navigate the connection through regular login or character creation process:
  - Despite not using TELNET protocol, the `input_to()` and `get_char()` efuns are supported for console connection.
  - When the console connection is closed, e.g. typing "quit" or forced by another wizard, the standard input is *NOT* closed and allows initiating another console connection by pressing ENTER.
  - You can use Ctrl-C to **break** the LPMud driver process as like other processes reading data from standard input.

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
`LogDir` | The full-path for `log_file()` to create log files. | use MudlibDir |
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
