# set_window_size()
## NAME
**set_window_size** - inform the mudlib of the user's terminal window size

## SYNOPSIS
~~~cxx
void set_window_size (int width, int height);
~~~

## DESCRIPTION
This apply is called on the interactive object when the client has responded to the TELNET_NAWS negotiation.
Neolith requests the client to provide this information when a connection is established.
If the client does not implement TELNET protocol, this will never be called.

## COMPATIBILITY
This apply is added by Neolith.

## SEE ALSO
[set_terminal_type()](set_terminal_type.md)

