# __INIT()
## NAME
**__INIT** - obsolete apply

## SYNOPSIS
~~~cxx
void __INIT (void);
~~~

## DESCRIPTION
This function used to be called in objects right before [create()](create.md).
Global variable initialization is now handled by another function that cannot be interfered with, so this is no longer called.
