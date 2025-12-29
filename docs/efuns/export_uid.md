# export_uid()
## NAME
**export_uid** - set the uid of another object

## SYNOPSIS
~~~cxx
int export_uid( object ob );
~~~

## DESCRIPTION
Set the uid of **ob** to the effective uid of this_object().
It is only possible when **ob** has an effective uid of 0.

## SEE ALSO
[this_object()](this_object.md), [seteuid()](seteuid.md), [getuid()](getuid.md), [geteuid()](geteuid.md),
[previous_object()](previous_object.md), [valid_seteuid()](valid_seteuid.md)
