# geteuid()
## NAME
**geteuid** - return the effective user id of an object or
function

## SYNOPSIS
~~~cxx
string geteuid( object|function );
~~~

## DESCRIPTION
If given an object argument, geteuid returns the effective
user id (euid) of the object.  If given an argument of type
**function**, it returns the euid of the object that created
that **function** variable.  If the object, at the time of the
function variable's construction, had no euid, the object's
uid is stored instead.

## SEE ALSO
[seteuid()](seteuid.md), [getuid()](getuid.md), [functionp()](functionp.md), [export_uid()](export_uid.md),
[previous_object()](previous_object.md), [this_object()](this_object.md), [valid_seteuid()](valid_seteuid.md)
