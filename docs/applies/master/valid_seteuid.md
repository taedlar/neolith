# valid_seteuid()
## NAME
**valid_seteuid** - secures the use of seteuid()

## SYNOPSIS
~~~cxx
int valid_seteuid (object obj, string euid);
~~~

## DESCRIPTION
The driver calls `valid_seteuid(ob, euid)` in the master object from inside the [seteuid()](../../efuns/seteuid.md) efun.
If `valid_seteuid()` returns 0, then the [seteuid()](../../efuns/seteuid.md) call will fail.
If `valid_seteuid()` returns 1, then the [seteuid()](../../efuns/seteuid.md) will succeed.

## SEE ALSO
[seteuid()](../../efuns/seteuid.md),
[geteuid()](../../efuns/geteuid.md),
[getuid()](../../efuns/getuid.md),
[export_uid()](../../efuns/export_uid.md)
