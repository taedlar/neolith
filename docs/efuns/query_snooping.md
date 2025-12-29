# query_snooping()
## NAME
**query_snooping** - return the object than an object is
snooping

## SYNOPSIS
~~~cxx
object query_snooping( object ob );
~~~

## DESCRIPTION
If `ob' (an interactive object) is snooping another
interactive object, the snooped object is returned.
Otherwise, 0 is returned.

## SEE ALSO
[snoop()](snoop.md), [query_snoop()](query_snoop.md)
