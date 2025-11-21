# userp
## NAME
userp() - determine if a given object was once interactive

## SYNOPSIS
~~~cxx
int userp (object ob);
~~~

## DESCRIPTION
Returns `1` if the *ob* was once interactive.

Returns `2` if the *ob* was once interactive that connects from the console mode. This is a Neolith extension.

## SEE ALSO
[interactive()](interactive.md),
[users()](users.md),
[living()](living.md)
