# call_out_info()
## NAME
**call_out_info** - get pending call_out() information

## SYNOPSIS
~~~cxx
mixed *call_out_info( void );
~~~

## DESCRIPTION
Get information about all pending call outs. An array is
returned, where every item in the array consists 4 elements:
the object, the function, the delay to go, and the optional
argument.

## SEE ALSO
[call_out()](call_out.md), [remove_call_out()](remove_call_out.md)
