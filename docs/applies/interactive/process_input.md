# process_input()
## NAME
**process_input** - provide a hook for the driver to send a copy of what the player is typing to the player object

## SYNOPSIS
~~~cxx
string process_input (string input);
~~~

## DESCRIPTION
If `process_input()` is present in the player object, then the driver parser will send it a copy of each line the player types.
`process_input()` makes it easy to implement features like command history and protection against wizcuffs.
`process_input()` can also modify the player input before it is parsed.
If `process_input()` returns a non-zero string, that string is used by the driver instead of what the player typed.

## AUTHOR
Truilkan@TMI
