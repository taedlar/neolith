# previous_object()
## NAME
**previous_object** - returns the object(s) that called the
current function

## SYNOPSIS
~~~cxx
mixed previous_object( int | void );
~~~

## DESCRIPTION
Returns an object pointer to the object, if any, that called
current function.  Note that local function calls do not set
previous_object() to the current object, but leave it
unchanged.  If passed a positive integer, it goes back the
given number of previous objects in the calling chain.
previous_object(0) is the same as previous_object(),
previous_object(1) is the previous object's
previous_object(), etc.  previous_object(-1) returns an
array containing all of the previous objects.

## SEE ALSO
[call_other()](call_other.md), [call_out()](call_out.md), [origin()](origin.md)
