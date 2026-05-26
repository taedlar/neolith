# query_verb()
## NAME
**query_verb** - return the name of the command currently being executed.

## SYNOPSIS
~~~cxx
string query_verb( void );
~~~

## DESCRIPTION
Give the name of the current command, or 0 if not executing from a command.
This function is useful when several commands (verbs) may cause the same function to execute and it is necessary to determine which verb it was that invoked the function.

When called from `process_input(string input)`, this efun is also available.
In that context, it returns the first token (the command verb) parsed from
`input` after skipping leading whitespace.

If `process_input()` was triggered by an escaped command (for example a
leading `!` in input-to flows), `query_verb()` is computed from the string
passed to `process_input()` (that is, after escape-prefix handling).

If no command token is present, it returns 0.

## SEE ALSO
[add_action()](add_action.md)
