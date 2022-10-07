# error
## NAME
          error - generate a run-time error

## SYNOPSIS
          void error( string err );

## DESCRIPTION
          A run-time error `err' will be generated when error() is
          called.  Execution of the current thread will halt, and the
          trace will be recorded to the debug log.

## SEE ALSO
          catch(3), throw(3), error_handler(4)
