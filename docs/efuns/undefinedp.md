# undefinedp()
## NAME
**undefinedp** - determine whether or not a given variable is undefined.

## SYNOPSIS
~~~cxx
int undefinedp (mixed arg);
~~~

## DESCRIPTION
Return `1` if **arg** is undefined.
**arg** will be undefined in the following cases:

1. it is a variable set equal to the return value of a call_other to a non-existent method (e.g. arg = call_other(obj, "???")).
2. it is a variable set equal to the return value of an access of an element in a mapping that doesn't exist (e.g. arg = map[not_there]).

## SEE ALSO
[mapp()](mapp.md), [stringp()](stringp.md), [pointerp()](pointerp.md), [objectp()](objectp.md), [intp()](intp.md), [bufferp()](bufferp.md), [floatp()](floatp.md), [functionp()](functionp.md), [nullp()](nullp.md), [errorp()](errorp.md)
