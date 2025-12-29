# remove_action()
## NAME
**remove_action** - unbind a command verb from a local function

## SYNOPSIS
~~~cxx
int remove_action( string fun, string cmd );
~~~

## DESCRIPTION
[remove_action()](remove_action.md) unbinds a verb cmd from an object function
fun. Basically, remove_action() is the complement to
[add_action()](add_action.md) and [add_verb()](add_verb.md). When a verb is no longer
required, it can be unbound with remove_action().

## RETURN VALUE
remove_action() returns:

1 on success.

0 on failure.

## SEE ALSO
[add_action()](add_action.md), [query_verb()](query_verb.md), [init()](init.md)
