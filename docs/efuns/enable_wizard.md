# enable_wizard()
## NAME
**enable_wizard** - give wizard priveleges to an object

## SYNOPSIS
~~~cxx
void enable_wizard( void );
~~~

## DESCRIPTION
Any interactive object that calls enable_wizard() will cause
wizardp() to return true if called on that object.
enable_wizard() gives three privileges to the interactive
object in question:

1.   ability to use restricted modes of ed when the
RESTRICTED_ED option is compiled into the driver.

2.   privilege of receiving descriptive runtime error
messages.

3.   privilege of using the [trace()](trace.md) and [traceprefix()](traceprefix.md)
efuns.

## SEE ALSO
[disable_wizard()](disable_wizard.md), [wizardp()](wizardp.md)
