# living()
## NAME
**living** - detects whether or not a given object is "living"

## SYNOPSIS
~~~cxx
int living( object ob );
~~~

## DESCRIPTION
Return true if `ob' is a living object (that is, if
"enable_commands()" has been called by `ob').

## SEE ALSO
[interactive()](interactive.md), [enable_commands()](enable_commands.md)
