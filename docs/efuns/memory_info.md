# memory_info()
## NAME
**memory_info** - obtain info on object/overall memory usage

## SYNOPSIS
~~~cxx
varargs int memory_info( object ob );
~~~

## DESCRIPTION
If optional argument `ob' is given, memory_info() returns
the approximate amount of memory that `ob' is using.  If no
argument is given, memory_info() returns the approximate
amount of memory that the entire mud is using.  Note that
the amount of memory the mud is using does not necessarily
correspond to the amount of memory actually allocated by the
mud from the system.

## SEE ALSO
[debug_info()](debug_info.md), [malloc_status()](malloc_status.md), [mud_status()](mud_status.md)
