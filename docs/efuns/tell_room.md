# tell_room()
## SYNOPSIS
~~~cxx
void tell_room( mixed ob, string str, object *exclude );
~~~

## DESCRIPTION
Send a message `str' to object all objects in the room `ob'.
`ob' can also be the filename of the room (string).  If
`exclude' is specified, all objects in the exclude array
will not receive the message.

## SEE ALSO
[message()](message.md), [write()](write.md), [shout()](shout.md), [say()](say.md), [tell_object()](tell_object.md)
