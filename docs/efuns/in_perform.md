# in_perform()
## NAME
**in_perform** - report whether the current object has an active HTTP transfer

## SYNOPSIS
~~~cxx
int in_perform(void);
~~~

## DESCRIPTION
`in_perform()` returns non-zero when the current object has a `perform_to()` request
still in flight, and `0` otherwise.

This efun has no side effects. It is intended for mudlib code that wants to prevent
duplicate submissions or to expose transfer state in higher-level request wrappers.

## RETURN VALUE
Returns `1` while the current object's transfer is active and `0` once the object is
idle.

## SEE ALSO
[perform_using()](perform_using.md), [perform_to()](perform_to.md)