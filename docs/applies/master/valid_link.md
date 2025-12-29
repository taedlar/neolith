# valid_link()
## NAME
**valid_link** - controls the use of link()

## SYNOPSIS
~~~cxx
int valid_link (string from, string to);
~~~

## DESCRIPTION
The driver calls `valid_link(from, to)` in the master object from inside the [link()](../../efuns/link.md) efun.
If `valid_link()` returns 0, then the [link()](../../efuns/link.md) will fail.
If `valid_link()` returns 1, then the [link()](../../efuns/link.md) will succeed if `rename()` would succeed if called with the same arguments.

## SEE ALSO
[link()](../../efuns/link.md)
