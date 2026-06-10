# input_prompt()
## NAME
**input_prompt** - called when the driver wants a prompt while input_to() or get_char() is active

## SYNOPSIS
~~~cxx
void input_prompt (string | function func, int flags, mixed ... args);
~~~

## DESCRIPTION
If `input_prompt()` is present in the player object, the driver will call it whenever a prompt would normally be printed while the object is waiting for `input_to()` or `get_char()` input.

The driver passes the callback function and any carryover arguments that were supplied to `input_to()` or `get_char()`.