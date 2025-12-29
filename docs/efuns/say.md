# say()
## NAME
**say** - send a message to all users in the same environment

## SYNOPSIS
~~~cxx
varargs void say( string str, object obj );
~~~

## DESCRIPTION
Sends a message to the environment of the originator, all
items in the same environment, and all items inside of the
originator.  The originator is this_player(), unless
this_player() == 0, in which case, the originator is
this_object().

The second argument is optional.  If second argument `obj'
is specified, the message is sent to all except `obj'.  If
`obj' is not an object, but an array of objects, all those
objects are excluded from receiving the message.

## SEE ALSO
[message()](message.md), [write()](write.md), [shout()](shout.md), [tell_object()](tell_object.md), [tell_room()](tell_room.md)
