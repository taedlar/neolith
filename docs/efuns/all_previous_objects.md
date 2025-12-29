# all_previous_objects()
## NAME
**all_previous_objects** - returns an array of objects that
called the current function

## SYNOPSIS
~~~cxx
object *all_previous_objects();
~~~

## DESCRIPTION
Returns an array of objects that called current function.
Note that local function calls do not set previous_object()
to the current object, but leave it unchanged.

## SEE ALSO
[call_other()](call_other.md), [call_out()](call_out.md), [origin()](origin.md), [previous_object()](previous_object.md)
