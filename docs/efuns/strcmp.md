# strcmp()
## NAME
**strcmp** - determines the lexical relationship between two
strings.

## SYNOPSIS
~~~cxx
int strcmp( string one, string two );
~~~

## DESCRIPTION
This implementatin of strcmp() is identical to the one found
in C libraries.  If string one lexically precedes string
two, then strcmp() returns a number less than 0.  If the two
strings have the same value, strcmp() returns 0.  If string
two lexically precedes string one, then strcmp() returns a
number greater than 0.  This efunction is particularly
useful in the compare functions needed by [sort_array()](sort_array.md).

`strcmp()` intentionally keeps C-string semantics. If either argument contains
an embedded null byte, comparison stops at the first `\0`, just as it would in
the C library. Use [c_str()](c_str.md) when you want to make that truncation
explicit before passing LPC strings across a C-string boundary.

## SEE ALSO
[c_str()](c_str.md), [sort_array()](sort_array.md)
