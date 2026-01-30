# eval_cost()
## NAME
**eval_cost** - returns the evaluation cost remaining

## SYNOPSIS
~~~cxx
int eval_cost()
~~~

## DESCRIPTION
eval_cost() returns the number of instructions that can be executed before the driver decides it is in an loop.

> [!NOTE]
> This is an alias of `set_eval_limit(-1)`.

## SEE ALSO
[catch()](catch.md), [error()](error.md), [throw()](throw.md), [error_handler()](error_handler.md),
[set_eval_limit()](set_eval_limit.md), [reset_eval_cost()](reset_eval_cost.md)
