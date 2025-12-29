# nullp()
## NAME
**nullp** - determine whether or not a given variable is null.

## SYNOPSIS
~~~cxx
int nullp (mixed arg);
~~~

## DESCRIPTION
Return `1` if **arg** is null. **arg** will be null in the following cases:

1. it has not yet been initialized.
2. it points to a destructed object.
3.  it is a function (formal) parameter that corresponds to a missing actual argument.

## SEE ALSO
[mapp()](mapp.md), [stringp()](stringp.md), [pointerp()](pointerp.md), [objectp()](objectp.md), [intp()](intp.md), [bufferp()](bufferp.md), [floatp()](floatp.md), [functionp()](functionp.md), [undefinedp()](undefinedp.md), [errorp()](errorp.md)
