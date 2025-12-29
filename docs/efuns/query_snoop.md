# query_snoop()
## NAME
**query_snoop** - return the snooper of an interactive object

## SYNOPSIS
~~~cxx
object query_snoop( object ob );
~~~

## DESCRIPTION
If `ob' (an interactive object) is being snooped by another
interactive object, the snooping object is returned.
Otherwise, 0 is returned.

## SEE ALSO
[snoop()](snoop.md), [query_snooping()](query_snooping.md)
