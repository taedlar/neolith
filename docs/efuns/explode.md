# explode
## NAME
explode() - break up a string

## SYNOPSIS
~~~cxx
string *explode (string str, string del);
~~~

## DESCRIPTION
explode() returns an array of strings, created when the string `str` is split into pieces as divided by the delimiter `del`.

> [!INFO]
> As a Neolith extension, calling explode() with empty string `""` as delimiter breaks the string into an array of individual **wide characters** (in multi-bytes encoding).
> When used with UTF-8 locale, it gives you an array of Unicode characters in each element.

## EXAMPLE
`explode(str," ")` will return as an array all of the words (separated by spaces) in the string `str`.

For breaking UTF-8 string into Unicode characters, `explode("小星星", "")` returns the array `({"小", "星", "星"})`.

## SEE ALSO
sscanf(3), extract(3), replace_string(3), strsrch(3)
