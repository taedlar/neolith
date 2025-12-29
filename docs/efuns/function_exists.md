# function_exists()
## NAME
**function_exists** - find the file containing a given
function in an object

## SYNOPSIS
~~~cxx
string function_exists( string str, object ob );
~~~

## DESCRIPTION
Return the file name of the object that defines the function
`str' in object `ob'. The returned value can be other than
`file_name(ob)' if the function is defined by an inherited
object.

0 is returned if the function was not defined.

Note that function_exists() does not check shadows.

## SEE ALSO
[call_other()](call_other.md), [call_out()](call_out.md), [functionp()](functionp.md), [valid_shadow()](valid_shadow.md)
