Neolith Administrator Guide
===========================

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
| Name | Value |
| --- | --- |
| MudlibDir | Full-path of the mudlib directory in the host filesystem. |
| IncludeDir | The search path of LPC #include. Multiple paths can be assigned by separate them with `:` character. |
| SaveBinaryDir | The path for storing data file when using `#pragma save_binary`. |
| MasterFile | The file path of the privileged master object. |
| Port | The TCP port for which your MUD shall listen for new connections. |

## Optional Settings

Below is a list of optional settings.
| Name | Value | Default |
| --- | --- | --- |
| MudName | Name of the MUD, which is made available to LPC by the pre-defined symbol `MUD_NAME`. | (empty string) |
| LogDir | The full-path for `log_file()` to create log files. | use MudlibDir |
| DebugLogFile | The filename of debug log file where the LPMud driver's log messages is appended to. | NULL |
| LogWithDate | Prefix each log message with an ISO-8601 format date and time. | No |
| GlobalInclude | An #include header that is automatically included by all LPC programs. | Not using |
| SimulEfunFile | The first LPC object to be loaded, and all its public functions are made available to any LPC program like efuns. | Not using |
| DefaultErrorMessage | A default message shown to the interactive user when LPC runtime error occurs during processing of the commmand he or she has typed. | Not using |
| DefaultFailMessage | A default message shown to the interactive user when he or she typed a command that is not recognized by any `add_action` | Not using |
| CleanUpDuration | A duration in seconds that the LPMud driver's garbage collection routine waits before calling an unused object's `clean_up()` function | 600 |
| ResetDuration | A duration in seconds between the `reset()` function is called in an object. | 1800 |
| MaxInheritDepth | Maximum depth of inheritance of LPC objects. | 30 |
| MaxEvaluationCost | Maximum cost of a LPC code evaluation | 1000000 |
| MaxArraySize | Maximum size of a LPC array. | 15000 |
| MaxBufferSize | Maximum size of a LPC buffer. | 4000000 |
| MaxMappingSize | Maximum size of a LPC mapping. | 15000 |
| MaxStringLength | Maximum length of a LPC string. | 200000 |
| StackSize | Maxiumu size of LPC evaluation stack | 1000 |
| MaxLocalVariables | Maximum number of local variables in a LPC function. | 25 |
| MaxCallDepth | Maximum depth of LPC function calls before the LPMud driver should abort the evaluation. | 50 |
| ArgumentsInTrace | Enable output of function call arguments in the dump trace message. | No |
| LocalVariablesInTrace | Enable output of local variables in the dump trace message. | No |



