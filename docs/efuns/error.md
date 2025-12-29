# error()
## NAME
**error** - generate a run-time error

## SYNOPSIS
~~~cxx
void error( string err );
~~~

## DESCRIPTION
A run-time error `err' will be generated when error() is
called.  Execution of the current thread will halt, and the
trace will be recorded to the debug log.

## SEE ALSO
[catch()](catch.md), [throw()](throw.md), [error_handler()](error_handler.md)
