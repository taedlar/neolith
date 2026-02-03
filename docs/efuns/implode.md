# implode()
## NAME
**implode** - concatenate strings

## SYNOPSIS
~~~cxx
string implode (mixed *arr, string delimiter);
string implode (mixed *arr, function f);
~~~

## DESCRIPTION
Concatenate all strings found in array `arr`, with the string `delimiter` between each element.
Only strings are used from the array.
Elements that are not strings are ignored.

With the second form, the function `f` is called with an accumelated string and each element in `arr` to compose the imploded string.

## SEE ALSO
[explode()](explode.md), [sprintf()](sprintf.md)
