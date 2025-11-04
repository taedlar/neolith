# strsrch
## NAME
strsrch() - search for substrings in a string

## SYNOPSIS
~~~cxx
int strsrch (string str, string substr | int char, int flag);
~~~

## DESCRIPTION
strsrch() searches for the first occurance of the string *substr* in the string *str*.
The last occurance of *substr* can be found by passing `-1` as the 3rd argument (which is optional).
If the second argument is an integer, that character is found (à la C's strchr()/strrchr().)
The empty string or null value cannot be searched for.

> [!TIP]
> As a neolith extension, the second argument is treated as a wide character instead of ASCII character.
> It is converted to a UTF-8 string before search.
> If found, the offset returned from `strsrch` is in bytes to allow using it in range operators.

## RETURN VALUE
The integer offset of the first (last) match is returned.
`-1` is returned if there was no match, or an error occurred (bad args, etc).

## SEE ALSO
[explode()](explode.md),
[sscanf()](sscanf.md),
[replace_string()](replace_string.md),
[regexp()](regexp.md)
