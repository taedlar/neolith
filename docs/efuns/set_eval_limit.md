# set_eval_limit()
## NAME
**set_eval_limit** - set the maximum evaluation cost

## SYNOPSIS
~~~cxx
void set_eval_limit( int );
~~~

## DESCRIPTION
set_eval_limit(), with a nonzero argument, sets the maximum
evaluation cost that is allowed for any one thread before a
runtime error occurs.  With a zero argument, it sets the
current evaluation counter to zero, and the maximum cost is
returned.  set_eval_limit(-1) returns the remaining
evaluation cost.

## SEE ALSO
[catch()](catch.md), [error()](error.md), [throw()](throw.md), [error_handler()](error_handler.md), [eval_cost()](eval_cost.md)
