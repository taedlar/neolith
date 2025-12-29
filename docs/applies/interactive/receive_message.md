# receive_message()
## NAME
**receive_message** - provides the interface used by the message efun

## SYNOPSIS
~~~cxx
void receive_message (string class, string message);
~~~

## DESCRIPTION
The [message()](../../efuns/message.md) efun calls this method in the player object.
The **class** parameter is typically used to indicate the class (say, tell, emote, combat, room description, etc) of the message.
The `receive_message()` apply together with the [message()](../../efuns/message.md) efun can provide a good mechanism for interfacing with a "smart" client.

## SEE ALSO
[catch_tell()](catch_tell.md),
[message()](../../efuns/message.md),
[receive()](../../efuns/receive.md),
[receive_snoop()](receive_snoop.md)
