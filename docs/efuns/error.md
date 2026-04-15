# error()
## NAME
**error** - generate a run-time error

## SYNOPSIS
~~~cxx
void error( string err );
~~~

## DESCRIPTION
Calling `error(err)` raises a driver/runtime error and stops normal execution
of the current evaluation path.

The driver routes uncaught errors through master
[error_handler()](../applies/master/error_handler.md). If that apply returns a
non-empty string, that string is emitted as error output. Otherwise the driver
falls back to default debug/trace output.

When [catch()](catch.md) traps the error, `catch()` receives the caught payload
according to its normal contract.

## SEE ALSO
[catch()](catch.md),
[throw()](throw.md),
[error_handler()](../applies/master/error_handler.md)
