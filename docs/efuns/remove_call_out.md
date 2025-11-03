# remove_call_out
## NAME
remove_call_out() - remove a pending call_out

## SYNOPSIS
~~~cxx
int remove_call_out( string fun | void );
~~~

## DESCRIPTION
Remove next pending call out for function `fun' in the current object.
The return value is the time remaining before the callback is to be called.
The returned value is -1 if there were no call out pending to this function.

When called without argument, remove all call out from `this_object()`.

## SEE ALSO
[call_out()](call_out.md),
[call_out_info()](call_out_info.md).
