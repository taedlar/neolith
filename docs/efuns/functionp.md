# functionp()
## NAME
**functionp** - determine whether or not a given variable is a
function pointer, and if so what kind

## SYNOPSIS
~~~cxx
int functionp( mixed arg );
~~~

## DESCRIPTION
Return nonzero if `arg' is a function pointer and zero (0)
otherwise.  Function pointers are variables of type
**function** as indicated in the documentation for the type
**function**, for example:

f = (: obj, func :);

The return value indicates the type of function pointer
using the values given in the driver include file
"include/function.h".

function pointer type      value ---------------------
----- call_other            FP_CALL_OTHER lfun
FP_LOCAL efun             FP_EFUN simul_efun
FP_SIMUL functional            FP_FUNCTIONAL

In addition, the following values will be added in some
cases:  (arguments provided)      FP_HAS_ARGUMENTS (creator
has been dested) FP_OWNER_DESTED (not rebindable)
FP_NOT_BINDABLE

The last set of values are bit values and can be tested with
bit operations.  The value FP_MASK is provided for ignoring
the bit values and testing the basic type of the function
pointer.

Examples:

To test if a function variable is an efun pointer:

if ((functionp(f) & FP_MASK) == FP_EFUN) ...

to test if it has args:

if (functionp(f) & FP_HAS_ARGUMENTS) ...

## SEE ALSO
[mapp()](mapp.md), [stringp()](stringp.md), [pointerp()](pointerp.md), [objectp()](objectp.md), [intp()](intp.md),
[bufferp()](bufferp.md), [floatp()](floatp.md), [nullp()](nullp.md), [undefinedp()](undefinedp.md), [errorp()](errorp.md),
[bind()](bind.md), lpc/types/function
