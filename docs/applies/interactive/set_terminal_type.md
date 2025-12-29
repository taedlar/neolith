# set_terminal_type()
## NAME
**set_terminal_type** - inform the mudlib of the user's terminal type

## SYNOPSIS
~~~cxx
void set_terminal_type (string term);
~~~

## DESCRIPTION
This apply is called on the interactive object with **term** set to the terminal type for the user, as reported by telnet negotiation.
If the user's client never responds (it's not telnet, for example) this will never be called.

## COMPATIBILITY
In MudOS, this apply function is called `terminal_type`. Neolith changes the name to `set_terminal_type`.

## SEE ALSO
[set_window_size()](set_window_size.md)

