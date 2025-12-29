# time_expression()
## NAME
**time_expression** - return the amount of real time that an expression took

## SYNOPSIS
~~~cxx
int time_expression (mixed expr);
~~~

## DESCRIPTION
Evaluate **expr**.
The amount of real time that passed during the evaluation of **expr**, in microseconds, is returned.
The precision of the value is not necessarily 1 microsecond; in fact, it probably is much less precise.

## SEE ALSO
[rusage()](rusage.md), [function_profile()](function_profile.md), [time()](time.md)
