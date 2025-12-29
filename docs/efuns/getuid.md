# getuid()
## NAME
**getuid** - return the user id (uid) of an object

## SYNOPSIS
~~~cxx
string getuid( object ob );
~~~

## DESCRIPTION
Returns the user id of an object.  The uid of an object is
determined at object creation by the creator_file()
function.

## SEE ALSO
[seteuid()](seteuid.md), [geteuid()](geteuid.md), [export_uid()](export_uid.md), [this_object()](this_object.md),
[previous_object()](previous_object.md), [creator_file()](creator_file.md), [valid_seteuid()](valid_seteuid.md)
