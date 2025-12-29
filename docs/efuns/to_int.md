# to_int()
## NAME
**to_int** - convert a float or buffer to an int

## SYNOPSIS
~~~cxx
int to_int( float | buffer x );
~~~

## DESCRIPTION
If `x' is a float, the to_int() call returns the number of
type `int' that is equivalent to `x' (with any decimal
stripped off).  If `x' is a buffer, the call returns the
integer (in network-byte-order) that is embedded in the
buffer.

## SEE ALSO
[to_float()](to_float.md), [read_buffer()](read_buffer.md), [sprintf()](sprintf.md), [sscanf()](sscanf.md)
