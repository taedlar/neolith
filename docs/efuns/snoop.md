# snoop()
## NAME
**snoop** - snoop an interactive user

## SYNOPSIS
~~~cxx
varargs object snoop( object snooper, object snoopee );
~~~

## DESCRIPTION
When both arguments are used, begins snooping of `snoopee'
by `snooper'.  If the second argument is omitted, turns off
all snooping by `snoopee'.  Security for snoop() is normally
controlled by a simul_efun.  snoop() returns `snoopee' if
successful in the two-argument case, and `snooper' if it was
successful in the single-argument case.  A return of 0
indicates failure.

## SEE ALSO
[query_snoop()](query_snoop.md), [query_snooping()](query_snooping.md)
