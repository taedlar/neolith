# catch()
## NAME
**catch** - catch an evaluation error

## SYNOPSIS
~~~cxx
mixed catch (mixed expr);
~~~

## DESCRIPTION
Evaluate **expr**.

If evaluation completes successfully, `catch()` returns `0`.

If a driver/runtime error occurs, `catch()` returns the caught value. For
driver-generated runtime errors this is typically a `*`-prefixed string.

The [throw()](throw.md) efun can also be used to return a custom value through
`catch()`. `throw(0)` is normalized to `"*Unspecified error"` so `catch()` does
not return the success sentinel `0` for a thrown error.

Calling [throw()](throw.md) without an active `catch()` raises the runtime error
`"*Throw with no catch."`.

`catch()` is not a normal function call; it is a compiler directive.

The `catch()` is somewhat costly, and should not be used just anywhere.
Rather, use it at places where an error would destroy consistency.

## SEE ALSO
[error()](error.md),
[throw()](throw.md),
[error_handler()](../applies/master/error_handler.md)
