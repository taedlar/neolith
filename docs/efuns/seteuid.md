# seteuid()
## NAME
**seteuid** - set the effective user id (euid) of an object

## SYNOPSIS
~~~cxx
int seteuid( string str );
~~~

## DESCRIPTION
Set effective uid to `str'.  valid_seteuid() in master.c
controls which values the euid of an object may be set to.

When this value is 0, then the current object's uid can be
changed by export_uid(), and only then.

But, when the value is 0, no objects can be loaded or cloned
by this object.

## SEE ALSO
[export_uid()](export_uid.md), [getuid()](getuid.md), [geteuid()](geteuid.md), [this_object()](this_object.md),
[valid_seteuid()](valid_seteuid.md)
