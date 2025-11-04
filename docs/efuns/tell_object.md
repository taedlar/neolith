# tell_object
## NAME
tell_object() - send a message to an object

## SYNOPSIS
~~~cxx
void tell_object (object ob, string str);
~~~

## DESCRIPTION
Send a message **str** to object **ob**.
If it is an interactive object (a player), then the message will go to him, otherwise it will go to the local function [catch_tell()](../applies/interactive/catch_tell.md).

## SEE ALSO
[message()](message.md),
[write()](write.md),
[shout()](shout.md),
[say()](say.md),
[tell_room()](tell_room.md)
