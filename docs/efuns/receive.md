# receive()
## NAME
**receive** - displays a message to the current object

## SYNOPSIS
~~~cxx
int receive( string message );
~~~

## DESCRIPTION
This efun is an interface to the add_message() function in
the driver.  Its purpose is to display a message to the
current object.  It returns 1 if the current object is
interactive, 0 otherwise.  Often, receive() is called from
within [catch_tell()](catch_tell.md) or [receive_message()](receive_message.md).

## SEE ALSO
[catch_tell()](catch_tell.md), [receive_message()](receive_message.md), [message()](message.md)
