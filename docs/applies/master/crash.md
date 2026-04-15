# crash()
## NAME
**crash** - function in master that is called in the event the driver crashes

## SYNOPSIS
~~~cxx
void crash (string crash_message, object command_giver, object current_object);
~~~

## DESCRIPTION
The driver calls `crash()` from the fatal-error shutdown path, not only for
signal-origin crashes.

This apply is invoked as:

```c
crash(crash_message, command_giver, current_object)
```

`command_giver` and `current_object` may be `undefined` when not available.

This hook lets the mudlib persist critical state (for example, save players and
other important data) and write diagnostics before process termination.

If `master::crash()` itself errors, the driver logs that failure and proceeds
with immediate shutdown.

## SEE ALSO
[slow_shutdown()](slow_shutdown.md),
[shutdown()](../../efuns/shutdown.md)
