# replace_string()
## NAME
**replace_string** - replace all instances of a string within a string

## SYNOPSIS
~~~cxx
string replace_string (string str, string pattern, string replace);
string replace_string (string str, string pattern, string replace, int max);
string replace_string (string str, string pattern, string replace, int first, int last);
~~~

## DESCRIPTION
replace_string() returns **str** with all instances of **pattern** replaced with **replace**.
If **pattern** has zero length then **str** is returned unmodified.
If the resultant string would exceed the maximum string length then replace_string() returns an undefinedp(), non-stringp() value.

replace_string() can be used to remove characters from a string by specifying a pattern and a zero-length replace parameter.
For example, replace_string(" 1 2 3 ", " ", "") would return "123".
replace_string() executes faster this way then explode()/implode().

The 4-th and 5-th arguments are optional (to retain backward compatibility.)
The extra arguments have the following effect:

- 4 args<br/>
The 4th argument specifies the maximum number of replacements to make (the count starts at 1).
A value of 0 implies 'replace all', and thus, acts as replace_string() with 3 arguments would.
E.g., replace_string("xyxx", "x", "z", 2) would return "zyzx".

- 5 args<br/>
The 4th and 5th arguments specify the range of matches to replace between, with the following constraints:
- first < 1 : change all from the start.
- last == 0, or last > max_matches : change all to end
- first > last : return the unmodified array.
E.g., replace_string("xyxxy", "x", "z", 2, 3) returns "xyzzy".

## SEE ALSO
[sscanf()](sscanf.md),
[explode()](explode.md),
[strsrch()](strsrch.md)
