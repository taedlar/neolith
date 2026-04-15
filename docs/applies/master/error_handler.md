# error_handler()
## NAME
**error_handler** - function in master object to handle errors

## SYNOPSIS
~~~cxx
void error_handler(mapping error);
void error_handler(mapping error, int caught);
~~~

## DESCRIPTION
This function allows the mudlib to handle driver/runtime errors instead of
using default driver-side trace output.

The driver always calls `error_handler(error)` for uncaught runtime errors.

For errors raised inside [catch()](../../efuns/catch.md), the two-argument
form `error_handler(error, 1)` is only used when the driver is built with
`LOG_CATCHES`. Without `LOG_CATCHES`, caught errors do not invoke this apply.

The **error** mapping contains:

```c
([
    "error"   : string,     // error text
    "program" : string,     // current program (when available)
    "object"  : object,     // current object (when available)
    "file"    : string,     // source file for current pc
    "line"    : int,        // the line number
    "trace"   : mapping*    // traceback frames
])
```

`program` and `object` may be absent when no current program/object exists
at the point the error is reported.

Each traceback frame is a mapping containing:

```c
([
    "function"  : string,   // the function name
    "program"   : string,   // the program
    "object"    : object,   // the object
    "file"      : string,   // the file to which the line number refers
    "line"      : int       // the line number
])
```

If this apply returns a non-empty string, that string is used as the emitted
error output. Otherwise, the driver falls back to its default debug/trace output.

## ERROR TEXT CONVENTION
Historically, driver-generated runtime error strings returned through
[catch()](../../efuns/catch.md) are `*`-prefixed (for example,
`*Bad argument ...`). This prefix identifies driver/runtime-origin error text.

Mudlib-thrown payloads are not restricted to this format, so caught values are
not guaranteed to be `*`-prefixed unless the payload came from driver/runtime
error generation.

## SEE ALSO
[catch()](../../efuns/catch.md),
[error()](../../efuns/error.md),
[throw()](../../efuns/throw.md),
[log_error()](log_error.md)

## AUTHOR
Beek
