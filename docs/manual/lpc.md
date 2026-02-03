Neolith LPC Guide
=================
# For Experienced LPC Wizards

## Compatibility with MudOS
Neolith is a MudOS fork that retains almost every LPC language features and compatibility. A mudlib developed for MudOS should be able to running on Neolith with very little modification. If you are already familiar with LPC programming in vanilla LPMud or MudOS, you may simply skip this LPC Guide or just use the following links as
LPC reference manual.

## References
- [Efuns List](/docs/manual/efuns.md)
- Apply Functions
  - [All Objects](/docs/applies/all-objects.md)
  - [Interactive Objects](/docs/applies/interactive-objects.md)
  - [Master Object](/docs/applies/master-object.md)

# For Beginers
The LPC language is very similar to the C language.
If you have zero experience in the C language programming, we strongly suggest you find a textbook, website, or even some tutorial videos that walk you through the basics of C language and at least finish your first hello-world C program.
You may return to here once your C language lesson starts teaching you "pointers", because the programming in LPC language doesn't need them.

In below sections, this manual describes the synax, keywords, and data structures of LPC language.

## Basic Concepts
Like C programs, a LPC program consists one or more source files (`*.c`) and header files (`*.h`).
You also use preprocessor directives like `#include` in LPC programs to reference other source files or header files.

Unlike a C program using the `main()` function as program entry point, a LPC program has no such entry point.
Instead, LPC uses `create()` function to initialize a LPC object which is similar to C++'s constructor.
In the world of LPMud, we use the word "**load**" to mean compiling LPC program and calling its `create()` function.
Another difference is, when the `create()` function returns, the LPC object stays there until it is explicitly destroyed.

- When we run a C program, the `main()` function is called, and the program exits when `main()` function returns.
- When we **load** a LPC program, the `create()` function is called to initialize a LPC object. The LPC object stays there until it is explicitly destroyed.

By loading LPC objects (or cloning multiple copies of the same LPC object), the LPMud can populate its virtual world with all the things you saw in the MUD.

## Types

Types can be used in four places:
- Declaring type of global variables.
- Declaring type of functions.
- Declaring type of arguments to functions.
- Declaring type of local variables in functions.

Normally, the type information is completely ignored, and can be regarded purely as documentation.
The exception is that certain optimizations can use it, so the compiler is allowed to treat 'x + 0' as the same as 'x' if 'x' has been declared to be a int variable, but will not use this trick if 'x' is declared to be a string variable, since the results would then be different.
Also, when the basic type of a function is declared, then a more strict type checking will be enforced inside of that function.
#pragma strict_types forces functions to have return values, and hence strict type checking inside functions is unavoidable.
That means that the type of all arguments must be defined, and the variables can only be used to store values of the declared type.
The function call_other() is defined to return an unknown type, as the compiler has no way of knowing the return type.
If CAST_CALL_OTHERS is defined, this value must be casted, otherwise it is assumed to be of type 'mixed'.
Casting a type is done by putting the type name inside a pair of '(' and ')'.
Casting has no effect, except for pacifying the compiler.

An example when querying the short description of an object:

```
(string)call_other(ob, "short");
```
or,
```
(string)ob->short();
```

When a function is compiled with strict type testing, it can only call other functions that are already defined.
If they are not yet defined, prototypes must be used to allow the current function to call them.

An example of a prototype:

```
string func(int arg);
```

Note the ';' instead of a body to the function.  All arguments can be given by names, but do not have to have the same names as in the real definition. 
All types must of course be the same.  The name of the argument can also be omitted:

```
string func(int);
```

There are two kinds of types.  Basic types, and special types.
There can be at most one basic type, but any number of special types assigned to a variable/function.

The strict type checking is only used by the compiler, not at runtime.
So, it is actually possible to store a number in a string variable even when strict type checking is enabled.

Why use strict type checking?  It is really recommended, because the compiler will find many errors at compile time, which will save a lot of hard work.
It is in general much harder to trace an error occuring at run time. 

The basic types can be divided into groups. Those that are referenced by value, and those that are referenced by address.
The types **int**, **string**, and **float** are referenced by value.
The types **mapping**, **function**, **object**, and pointers (`<type> *`) are referenced by address.
If a value of this type is assigned to a variable or passed as argument, they will all point to the same actual data.
That means that if the value of an element in an array is changed, then it can modify all other variables pointing to the same array.
Changing the size of the array will always allocate a new one though.
The comparison operator, `==`, will compare the actual value for the group of value-referenced types above.
But for arrays, mappings, etc, it will simply check if it is the same array, mapping, etc.
That has the very important implication that the expression `({ 1 }) == ({ 1 })` will evaluate to false because the array construction operator-pair, `({ ... })` always generates a new array.

## Basic types

### `int`
In original LPMud and MudOS, an **int** type is a 32-bits integer number. In Neolith, an **int** type is defined to an integer number as wide as pointers.

LPC treats character literals (single-quoted character) as integers. For example:
~~~cxx
c = 'a'; // c is an integer
~~~

Neolith extends LPC to support wide character literals is in C++:
~~~cxx
c = L'酷';
~~~
The above statement gives you the integer value of a `wchar_t` represented by the wide character literal.

### `float`
In original LPMud and MudOS, a **float** type is equivalent to the C language float (32-bits). In Neolith, the **float** is equivalent to C language **double**.
 
Declare variables like this:
~~~cxx
float pi;
~~~
In general the same operations are supported for floats as are for integers.
Floating point constants may contain decimal points e.g.
~~~cxx
pi = 3.14159265;
~~~
Original LPC float type is based on the single precision floating point type
provided by C.  On most machines this will give about seven (7) digits
of precision (in base 10). Neolith has extended to use the double precision
floating point (64-bits).

### `double`
In Neolith, the name **double** is also a reserved word.

Like in C++, the optional `f` or `d` suffix for a floating point is also recognized by Neolith.
~~~cxx
double r; // equivalent to float r;
r = 0.3f;
~~~

Neolith also supports exponents, which is not in original LPC language:
~~~cxx
float r;
r = 3e+4d; // == 30000.0
~~~

### `string`
An unlimited string of characters (no '\0' allowed tho). 

You can take a substring from a variable by using the substring operation (`str[n1..n2]`).
Positive values are taken from the left and negative values from the right.
If a value is greater than the length of the string it will be treated as being equal to the length of the string.

If the two values are equal (`str[n1..n1]`) then the character at that position (n1) is returned.
If both values point to positions beyond the same end of the string the null string (`""`) is returned.
If the position pointed to by the first value is after the one pointed to by the second then the null string is also returned.

Examples:
~~~cxx
str = "abcdefg";
~~~
- str[0..0] == "a"
- str[0..-1] == "abcdefg"
- str[-4..-2] == "def"
- str[-7..6] == "abcdefg"
- str[3..2] == ""

Wide character strings are supported in Neolith as an extension to original LPC.
Unlinke in C, the `string` type in LPC is not a primitive character array, but a high level abstract data type more similar to modern C++'s `std::string`.

Neolith allows LPC program to assign a wide character string literal using C++'s `wchar_t` syntax:
~~~cxx
str = L"こんにちは";
~~~
The `L` prefix requires the lexial parser to **verify** if the literl string is a valid wide character string in current locale at compile time.
If the string contains any illegal multi-byte sequence, a compile time error is raised.

> [!IMPORTANT]
> Neolith always store a LPC string in multi-byte encoding internally (i.e. UTF-8). The `L` prefix only affects compile time validation.
>
> Neolith may work under some legacy multi-byte encoding (such as Big-5 Chinese).
> But these encoding may conflict with the backslash escape sequence like `\n` in string literals and causes many problems.
> It is strongly recommended to always set your locale to UTF-8 encoding to ensure best compatibility with LPC.

### `buffer`

- `buffer` is a cross between the LPC array type and the LPC string type.  
- `buffer` is intended as a way to conveniently manipulate binary data.  
- `buffer` is not zero-terminated (that is, it has an associated length).  A 
- `buffer` is an array of bytes that is implemented using one byte per element. `buf[i] = x` and `x = buf[i]` are allowed and do work.  `sizeof(buf)` works. `bufferp(buf)` is available.  `buf[i..j]` should work as well. `buff = read_buffer(file_name, ...)` (same args as read_bytes). also `int write_buffer(string file, int start, mixed source)`, `buf = buf1 + buf2`; `buf += buf1`, `buf = allocate_buffer(size)`.

The socket efuns have been modified to accept and return the `buffer` type for STREAM_BINARY (3) and DATAGRAM_BINARY (4) modes.

### `object`
A reference to an object.  

### `array`
Arrays are declared using a `*` following a basic type.
For example, declaring an array of numbers: `int *arr;`.
Use the type `mixed` if you want an array of arrays, or a mixed combination of types.

For example,
```
a = allocate(10);
a[0] = allocate(10);
a[1] = allocate(10);
```
Then you can reference array 0, element 0, via
``` 
a[0][0]
```
You can't declare an array of more than one dimension (using the type * notation, if you have type checking on), but you can have an array of more than one dimension.  If you have type checking on, you will probably have to declare them as type mixed.

This also works:
``` 
mixed a;
a = ({ ({ 1, 2, 3 }), ({ 1, 2, 3 }) });
```
In the above example, `a[0]` would be `({ 1, 2, 3 })`, and `a[0][2]` would be `3`.

Or this:
```
mixed a;  
a = ({ 0, 0, 0, 0 });  /* just to get the array to size 4 */
a[0] = ({ 1, 2, 3 });
a[1] = ({ 1, 2, 3 });
```

### `mapping`

MudOS 0.9 provides a datatype calling the `mapping`.
Mappings are the equivalent of associative arrays found in other languages (e.g. Perl).
An associative array is similar to a regular array except that associative arrays can be indexed by any type of data (string, object, int, array, etc) rather than just integers.
In addition, associative arrays are sparse arrays which means you can have a mapping which has a value for its 1,000,000th
element without having values for any other elements.
Two particularly effective uses for mappings are: 1) databases, and 2) a substitute for the aggregate type `struct` (as used in the C language) by representing each
field of the C struct as a key in the mapping.

A `mapping` is declared like this:
```
mapping x;
```
A `mapping` can be initialized in one of two ways:
```
x = ([key0 : value0, key1 : value1, ...]);
```
(note: `x = ([]);` can be used to create an empty mapping)

Note that a mapping *must* be initialized before you may assign any elements to it.
This restriction exists because of the way the gamedriver initializes all variables (regardless of type) to zero (0).
If you do not initialize the mapping, then you'll see an "Indexing on illegal type" error when you try to assign an element to the mapping.

New (key, value) pairs may be added to the map in the following way:
```
x[key] = value;
```
The above statement causes the driver to search the mapping named `x` for the specified key.
If the mapping contains that key, then the associated value (in the mapping) is replaced with the value on the right hand side of the assignment.
If the mapping does not already contain that key, then additional space is automatically allocated (dynamically) and the (key, value) pair is inserted into the mapping.

An element of a mapping may be referenced as follows:
```
write(x[key] + "\n");
```
An element of a mapping may be deleted as follows:
```
map_delete(x, key);
```
this deletion will cause the following expression to evaluate to true (1):
```
undefinedp(x[key])
```
so that you could write code such as this:
```
if (undefinedp(value = x["MudOS"])) {
    write("'MudOS' is not used as a key in the mapping 'x'\n");
} else {
    write("the value for the key 'MudOS' is " + value + "\n");
}
```
A list of the keys (indices) may be obtained using the `keys()` efun, for example:
```
mixed *idx;
map x;

x = ([ "x" : 3, "y" : 4]);
idx = keys(x);  /* idx == ({"x", "y"}) or ({"y", "x"}) */
```
Note that `keys()` will return the list of indices in an apparently random order (the order is a side effect of the implementation used to store the mapping -- in this case, an extensible hash table).

A list of the values in a mapping may be obtained using the `values()` efun, for example:
```
idx = values(x);
```
causes idx to be equal to `({3, 4})` or `({4, 3})`.  Note that `values()` will return the values in the same order as `keys()` returns the corresponding keys.

The (key, value) pairs in a mapping may be iterated over using the `each()` efun.  `each()` returns a null vector when the end of the mapping is reached.
`each()` returns the (key, value) pairs in the same order as `keys()` and `values()` do.  For example:
```
mixed *pair;

while ((pair = each(x)) != ({})) {
    write("key   = " + pair[0] + "\n");
    write("value = " + pair[1] + "\n");
}
```
Mappings can be two-dimensional (or n-dimensional for that matter) in the same sense that LPC arrays can be.
```
mapping x, y;

x = ([]);
y = ([]);

y["a"] = "c";
x["b"] = y;

And then x["b"]["a"]  == "c"
```
Mappings can also be composed using the `*` operator (composed in the mathematical sense of the word):
```
mapping r1, r2, a;

r1 = ([]);
r2 = ([]);

r1["driver"] = "mudlib";
r2["mudlib"] = "castle";
```
so:
```
a = r1 * r2 
```
defines a to be a map with: `a["driver"] == "castle";`

You may also add two mappings.  The sum of two mappings is defined as the union of the two mappings.
```
a = r1 + r2
```
defines a to be a map with `a["driver"] == "mudlib"` and `a["mudlib"] == "castle"`.

The `+=` operator is also supported.  Thus you could use:
```
a += ([key : value]);
```
as a substitute for:
```
a[key] = value;
```
However, the latter form (`a[key] = value`) is much more efficient since the former (in the present implementation) involves the creation of a new mapping while the latter does not.

The subtraction operator is not defined for mappings (use `map_delete()`).

The `sizeof()` efun may be used to determine how many (key, value) pairs are in the mapping.  For example,
```
write("The mapping 'x' contains " + sizeof(x) + " elements.\n");
```
the implementation:

MudOS's mappings are implemented using an extensible hash table. The size of the hash table is always a power of 2.
When a certain percentage of the hash table buckets become full, the size of the hash table is doubled in order to maintain the efficiency of accesses to the hash table.

> **Credits**:
> MudOS's mappings were originally implemented by Whiplash@TMI.
> Parts of the implementation were later rewritten by Truilkan@TMI (to use an extensible hash table rather than a binary tree).
> Parts of the data structure used to implement mappings are based on the hash.c module from the Perl programming language by Larry Wall.
> The Perl package is covered under the GNU Copyleft general public license.

### `function`

MudOS has a variable type named `function`.  Variables of this type may be used to point to a wide variety of functions.
You are probably already familiar with the idea of passing a function to certain efuns.
Take, for example, the `filter()` efun.  It takes an array, and returns an array containing the elements for which a certain function returns non-zero.
Traditionally, this was done by passing an object and a function name.
Now, it can also be done by passing an expression of type `function` which merely contains information about a function, which can be evaluated later.

Function pointers can be created and assigned to variables:
```
function f = (: local_func :);
```
Passed to other routines or efuns, just like normal values:
```
foo(f);  map_array( ({ 1, 2 }), f);
```
Or evaluated at a later time:
```
x = evaluate(f, "hi");
```
When the last line is run, the function that `f` points to is called, and `"hi"` is passed to it.
This will create the same effect as if you had done:
```
x = local_func("hi");
```
The advantage of using a function pointer is that if you later want to use a different function, you can just change the value of the variable.

Note that if `evaluate()` is passed a value that is not a function, it just returns the value.
So you can do something like:
```
void set_short(mixed x) { short = x; }
mixed query_short() { return evaluate(short); }
```
This way, simple objects can simply do `set_short("Whatever");`, while objects that want their shorts to change can do: `set_short( (: short_func :) );`.

The simplest function pointers are the ones shown above.
These simply point to a local function in the same object, and are made using `(: function_name :)`.
Arguments can also be included; for example:
```
string foo(string a, string b) {
   return "(" + a "," + b + ")";
}

void create() {
    function f = (: foo, "left" :);

    printf( "%s %s\n", evaluate(f), evaluate(f, "right") );
}
```
Will print:
```
(left,0) (left,right)
```
The second kind is the efun pointer, which is just `(: efun_name :)`.
This is very similar to the local function pointer.  For example, the `objects()` efun takes a optional function, and returns all objects for which the function is true, so:
```
objects( (: clonep :) )
```
will return an array of all the objects in the game which are clones.
Arguments can also be used:
```
void create() {
    int i;
    function f = (: write, "Hello, world!\n" :);

    for (i=0; i<3; i++) { evaluate(f); }
}
```
Will print:
```
Hello, world!
Hello, world!
Hello, world!
```
Note that simul_efuns work exactly like efuns with respect to function pointers.

The third type is the call_other function pointer, which is similar to the type of function pointer MudOS used to support.
The form is `(: object, function :)`.  If arguments are to be used, they should be added to an array along with the function name.
Here are some examples:
```
void create()
{
     string *ret;
     function f = (: this_player(), "query" :);    

     ret = map( ({ "name", "short", "long" }), f );     
     write(implode(ret, "\n"));
}
```
This would print the results of `this_player()->query("name")`, `this_player()->query("short")`, and `this_player()->query("long")`.
To make a function pointer that calls `query("short")` directly, use:
```
f = (: this_player(), ({ "query", "short" }) :)
```
For reference, here are some other ways of doing the same thing:
```
f = (: call_other, this_player(), "query", "short" :)  // a efun pointer using
                                                       // the call_other efun
f = (: this_player()->query("short") :) // an expression functional; see
                                        // below.
```

The fourth type is the expression function pointer.  It is made using `(: expression :)`.
Within an expression function pointer, the arguments to it can be refered to as `$1`, `$2`, `$3` ..., for example:
```
evaluate( (: $1 + $2 :), 3, 4)  // returns 7.
```
This can be very useful for using `sort_array`, for example:
```
top_ten = sort_array( player_list, 
	  (: $2->query_level() - $1->query_level :) )[0..9];
```

The fifth type is an anonymous function:
```
void create() {
    function f = function(int x) {
        int y;

        switch(x) {
        case 1: y = 3;
        case 2: y = 5;
        }
        return y - 2;
    };

    printf("%i %i %i\n", (*f)(1), (*f)(2), (*f)(3));
}
```
would print:
```
1 3 -2
```
Note that `(*f)(...)` is the same as `evaluate(f, ...)` and is retained for backwards compatibility.
Anything that is legal in a normal function is legal in an anonymous function.

When are things evaluated? The rule is that arguments included in the creation of efun, local function, and simul_efun function pointers are evaluated when the function pointer is made.
For expression and functional function pointers, nothing is evaluated until the function pointer is actually used:
```
(: destruct, this_player() :)  // When it is *evaluated*, it will destruct
                               // whoever "this_player()" was when it 
                               // was *made*
(: destruct(this_player()) :)  // destructs whoever is "this_player()"
                               // when the function is *evaluated*
```
For this reason, it is illegal to use a local variable in an expression pointer, since the local variable may no longer exist when the function pointer is evaluated.
However, there is a way around it:
```
(: destruct( $(this_player) ) :) // Same as the first example above
```
`$(whatever)` means "evaluate whatever, and hold it's value, inserting it when the function is evaluated".
It also can be used to make things more efficient:
```
map_array(listeners, (: tell_object($1, $(this_player()->query_name()) + " bows.\n") :) );
```
only does one `call_other`, instead of one for every message.
The string addition could also be done before hand:
```
map_array(listeners, (: tell_object($1, $(this_player()->query_name() + " bows.\n")) :) );
```
Notice, in this case we could also do:
```
map_array(listeners, (: tell_object, this_player()->query_name() + " bows.\n" :) );
```

### `void`
This type is only usable for functions.  It means that the function will not return any value.
The compiler will complain (when type checking is enabled) if a return value is used.
 
### `mixed`
This type is special, in that it is valid to use in any context.
Thus, if everything was declared `mixed`, then the compiler would never complain.
This is of course not the idea. It is really only supposed to be used when a variable really is going to contain different types of values.
This should be avoided if possible. It is not good coding practice to allow a function, for example, to return different types.
 
## Special Types
 
There are some special types, which can be given before the basic type.
These special types can also be combined.
When using special type T before an `inherit` statement, all symbols defined by inheritance will also get the special type T.
The only special case is `public`-defined symbols, which can not be redefined as private in a private inheritance statement.
 
### `varargs`
 A function of this type can be called with a variable number of arguments.
 Otherwise, the number of arguments is checked, and can generate an error.
 
### `private`
 Can be given for both functions and variables. Functions that are private in object A can not be called through `call_other()` in another object.
 They're also not accessable to any object that inherits A.
 
### `static`
 This special type behaves different for variables and functions.
 It is similar to private for functions, in that they cannot be called from other objects with `call_other()`.
 static variables will be neither saved nor restored when using `save_object()` or `restore_object()`.
 
### `public`
 A function defined as public will always be accessible from other objects, even if private inheritance is used.
 
### `nomask`
 All symbols defined as nomask cannot be redefined by inheritance.
 They can still be used and accessed as usual.  nomask also blocks functions from being shadowed with `shadow()`.

## LPC Substructures

### Indexing and Ranging

Since v20.25a6, MudOS provides a way of indexing or getting slices (which I will, following common use, call 'ranging') of strings/buffers/arrays/mappings
(for mappings only indexing is available) as well as means of changing the values of data via lvalues (i.e. 'assignable values') formed by indexing/ranging.

As an example, if we set str as `"abcdefg"`, `str[0]` will be `'a'`, `str[1]` `'b'` etc. Similarly, the nth element of an array arr is accessed via `arr[n-1]`, and the value corresponding to key x of `mapping m`, `m[x]`. The `<` token can be used to denote indexing from the right, i.e. `str[<x]` means `str[strlen(str) - x]` if `str` is a string.
More generally `arr[<x]` means `arr[sizeof(arr)-x]`. (Note that `sizeof(arr)` is the same as `strlen` if `arr` is a string).

Indexed values are reasonable lvalues, so one could do for e.g. `str[0] = 'g'` to change the 1st character of `str` to `g`.
Although it is possible to use `({ 1,2 })[1]` as a value (which is currently optimized in MudOS to compile to 2 directly), it is not possible to use it as an lvalue.
It is similarly not possible to use `([ "a" : "b" ])["c"]` as an lvalue (Even if we did support it, it would be useless, since there is no other reference to the affected mapping).
I will describe in more detail later what are the actually allowed lvalues.

Another method of obtaining a subpart of an LPC value is via ranging. An example of this is `str[1..2]`, where for str being `"abcdefg"`, gives `"bc"`.
In general `str[n1..n2]` returns a substring consisting of the `(n1+1)` to `(n2+1)`th characters of `str`.
If `n1` or `n2` is negative, and the driver is compiled with `OLD_RANGE_BEHAVIOR` defined, then it would take the negative indices to mean counting from the end.

Unlike indexing though, ranges with indexes which are out of bounds do not give an error.
Instead if a maximal subrange can be found within the range requested that lies within the bounds of the indexed value, it will be used.
So for e.g., without OLD_RANGE_BEHAVIOR, str[-1..2] is the same as str[0..2].
All other out of bounds ranges will return "" instead, which corresponds to the idea that there is no (hence there is one, namely the empty one) subrange within the range provided that is within bounds.
Similarly, for array elements, `arr[n1..n2]` represents the slice of the array with elements `(n1+1)` to `(n2+1)`, unless out of bounds occur.
OLD_RANGE_BEHAVIOR is only supported for buffers and strings. However, I suggest you not use it since it maybe confusing at times (i.e. in `str[x..y]` when x is not known at hand, it may lead to an unexpected result if x is negative).
One can however, also use `<` in ranging to mean counting from the end. So `str[<x..<y]` means `str[strlen(str)-x..strlen(str)-y]`.
```
/* Remark: If OLD_RANGE_BEHAVIOR is defined, then the priority of <
	   is higher than the priority of checking if it's negative.
           That is, if you do str[<x..y], it will mean the same
	   as str[strlen(str)-x..y], meaning therefore that it will
	   check only now if strlen(str)-x is negative and if so,
	   takes it to be from the end, leading you back to x again 
 */
```
Thus far, `str[<x..<y]`, `str[<x..y]`, `str[x..<y]`, `str[<x..]` (meaning the same as `str[<x..<1])` and `str[x..]` (same as `str[x..<1]`) are supported.
The same holds for arrays/buffers.

Perhaps this might seem strange at first, but ranges also are allowed to be lvalues!
The meaning of doing
```
str[x..y] = foo;  (1)
```
is basically 'at least' doing
```
str = str[0..x-1] + foo + str[y+1..];  (2)
```
Here x can range from `0..sizeof(str)` and y from -1 to `sizeof(str) - 1`.
The reason for these bounds is because, if I wanted to add foo to the front,
```
str = foo + str;
```
this is essentially the same as
```
str = str[0..-1] + foo + str;
```
since `str[0..-1]` is just "".
```
/* Remark: As far as range lvalues are concerned, negative indexes
	   do not translate into counting from the end even if 
	   OLD_RANGE_BEHAVIOR is defined. Perhaps this is confusing, 
	   but there is no good way of allowing for range lvalue 
           assignments which essentially result in the addition 
	   of foo to the front as above otherwise
 */
```
Hence, that's the same as doing
```	
str = str[0..0-1] + foo + str[0..];
```	
or, what's the same
```
str = str[0..0-1] + foo + str[-1+1..];
```
Now if you compare this to (1) and (2) you see finally that that conforms to the prescription there if we do `str[0..-1] = foo`!!
(Yes, those exclamation marks are not part of the code, and neither is this :))

Similarly, I will leave it to you to verify that 
```
str[sizeof(str)..sizeof(str)-1] = foo; (3)
```	
would lead to `str = str + foo`. Now, we can use `<` in range lvalues as well, so (3) could have been written as
```
str[<0..<1] = foo; (4)
```
or even
```
str[<0..] = foo; (5)
```
which is more compact and faster.
```
/* Remark: The code for str[<0..] = foo; is generated at compile time
	   to be identical to that for str[<0..<1] = foo; so neither
	   should be faster than the other (in principle) at runtime,
	   but (4) is faster than (3).
 */
```
Now we come to the part where I mentioned 'at least' above.
For strings, we know that when we do `x = "abc"; y = x;`, y has a new copy of the string "abc".
(This isn't always done immediately in the driver, but whenever y does not have a new copy, and a change is to be made to y, then y will make a new copy of itself, hence effectively, y has a new copy in that all simple direct changes to it such as `y[0] = 'g'` does not change x)

For buffers and arrays, however, when we do `y = x`, we don't get a new copy.
So what happens is if we change one, we could potentially change the other.
This is indeed true (as has always been) for assignments to indexed lvalues (i.e. lvalues of the form `y[0]`).
For range lvalues (i.e. `x[n1..n2]`), the rule is if the change of the lvalue will not affect it's size (determined by sizeof for e.g.), i.e. essentially if n1 and n2 are within x's bounds and the value on the right hand side has size `n2 - n1 + 1`, then indeed changing x affects y, otherwise it will not (i.e. if you do `x[0..-1] = ({ 2,3 })`.
This only applies to arrays/buffers, for strings it will never affect y if we assign to a range of x.

### More complex lvalues and applications

Since arrays/mappings can contain other arrays/mappings, it is possible in principle to index them twice or more.
So for e.g. if arr is `({ ({ 1,"foo",3 }),4 })` then `arr[0][1]` (read as the 2nd element of `arr[0]`, which is the 1st element of arr) is `"foo"`.
If we do, for e.g. `arr[0][2] = 5`, then arr will be `({ ({ 1, "foo", 5 }), 4 })`.
```
/* Remark: by the 'will be' or 'is' above, I mean technically: recursively
	   equal. (This is just if some people are confused)
 */
```
Similarly, `arr[0][1][1] = 'g'` changes arr to `({ ({ 1, "fgo", 3 }), 4 })`, and `arr[0][1][0..1] = "heh"` (note that the right hand side can have a different length, imagine this being like taking the 1st two characters out from `arr[0][1]`, which is currently `"foo"`, and putting `"heh"` in place, resulting in `"heho"`) gives arr as `({ ({ 1, "heho", 3 }), 4 })`.
You should now be able to generate more examples at your fancy.

Now I want to discuss some simple applications. Some of you may know that when we are doing 
```
sscanf("This is a test", "This %s %s test", x, y) (6)
```
that x and y are technically lvalues. This is since what the driver does is to parse the original string into the format you give, and then tries to assign the results (once all of them are parsed) to the lvalues that you give (here x and y).
So, now that we have more general lvalues, we may do
```
x = "simmetry";
arr = ({ 1, 2, 3, ({ "This is " }) });

sscanf("Written on a roadside: the char for 'y' has value 121\n",
       "Written on %sside: the char for 'y' has value %d\n",
       arr[3][0][<0..], x[1]);   
	                                                   (7)
```
will result in `arr` being `({ 1, 2, 3, ({ "This is a road" }) })` and `x` being `"symmetry"`.
(See how we have extended the string in `arr[3][0]` via `sscanf`?)
The driver essentially parses the string to gives the matches `"a road"` and 121, it then does the assignments to the lvalues, which is how we got them as above.

The `++`, `--`, `+=` and `-=` operators are supported for char lvalues, i.e. lvalues obtained by indexing strings.
Thus for e.g. to get an array consisting of 26 elements `"test.a"`, `"test.b"`, .., `"test.z"`, one might use a global var tmp as follows:
```
mixed tmp;

create(){
   string *arr;
   ...
   tmp = "test.`";
   arr = map_array(allocate(26), (: tmp[4]++ && tmp :));
   ...
}
							  (8)
```

### General syntax of valid lvalues

Finally, as a reference, I will just put here the grammar of valid lvalues accepted by the driver.

By a basic lvalue I mean a global or local variable name, or a parameter in a functional function pointer such as `$2`.

A basic lvalue is a valid lvalue, and so are indexed lvalues of basic or indexed lvalues.
(Notice that I did not say indexed lvalues of just basic lvalues to allow for `a[1][2]`).
I will generally call an lvalue obtained from a basic lvalue by indexing only indexed lvalues.

The following lvalues are also valid at compile time:
```
(<basic lvalue> <assignment token> <rhs>)[a_1][a_2]...[a_n] 
(<indexed lvalue> <assignment token> <rhs>)[a_1][a_2]...[a_n]
	                                                        (9)
/* Remark: n >= 1 here */
```
assignment token is one of +=, -=, *=, &=, /=, %=, ^=, |=, =, <<=, >>=.
	
However, because of the same reason that when we assign to a string, we obtain a new copy, `(x = "foo")[2] = 'a'` is invalidated at runtime.
(One way to think about this is, essentially, assignment leaves the rhs as a return value, so `x = "foo"` returns `"foo"`, the right hand side, which is not the same `"foo"` as the one in x. For arrays/buffers this is no problem because by assigning, we share the array/buffer)

Call the lvalues in (9) complex lvalues. Then the following is also a valid lvalue:
```
(<complex lvalue> <assignment token> <rhs>)[a_1][a_2]...[a_n]
	                                                         (10)
``` 
and if we now call the above lvalues also complex lvalues, it would still be consistent, i.e. `(((a[0] = b)[1] = c)[2] = d)[3]` is an okay lvalue (though I wouldn't suggest using it for clarity's sake :)).

Now, the last class of valid lvalues are range lvalues, which are denoted by ranging either a basic, indexed or complex lvalue:
```
<basic lvalue>[n1..n2]
<indexed lvalue>[n1..n2]
<complex lvalue>[n1..n2] 
	
plus other ranges such as `[<n1..<n2] etc.
            	                                                 (11)	
```
There is maximally only one 'range' you can take to obtain a valid lvalue, i.e. `arr[2..3][0..1]` is not a valid lvalue (note that a naive interpretation of this syntax is one equivalent to using `arr[2..3]` itself)


### Compile-time errors that occur and what they mean

Here I put some notes on compile-time errors for valid lvalues, hopefully to be useful for you.

- Can't do range lvalue of range lvalue

Diagnosis: You have done 'ranging' twice, e.g. something like x[2][0..<2][1..2] isn't a valid lvalue

- Can't do indexed lvalue of range lvalue.

Diagnosis: Something like x[0..<2][3] was done.

- Illegal lvalue, a possible lvalue is (x <assign> y)[a]

Diagnosis: Something like (x = foo)[2..3] or (x = foo) was taken to be an lvalue.

- Illegal to have (x[a..b] <assign> y) to be the beginning of an lvalue

Diagnosis: You did something as described, i.e. (x[1..6] = foo)[3] is not allowed.

- Illegal lvalue

Diagnosis: Oops, we are out of luck here :) Try looking at your lvalue more carefully, and see that it obeys the rules described in section 3 above.


### Coming attractions

Perhaps a pointer type will be introduced to allow passing by reference into functions. Mappings may be multivalued and multi-indexable.


> Author: Symmetry@Tmi-2, IdeaExchange  
> Last Updated : Tue Jan 10 11:02:40 EST 1995

## Constructs
### `inherit`

The syntax of LPC inherit statement:
```
inherit pathname;
```
where pathname is a full path delimited by quotes (e.g. `"/std/Object"`).

The `inherit` statement provides the inheritance capability (a concept from object-oriented programming) to LPC objects.
Inheritance lets an object inherit functions and variables from other objects.
Because the MudOSdriver internally stores global data and compiled code separately, many different objects can use inheritance to share the same piece of compiled code.
Each of these objects will have its own local copy of any global variables defined by the object.
Suppose that two object A and B inherit object C.  Recompiling object either of A or B will not cause C to be recompiled.
However, it will cause any global variables provided by C to lose whatever data they had (remember that A and B each have their own copy of the global variables provided by C.
Thus updating A will not effect the global variables of B (even those provided by C) and vice versa).

Suppose object A inherits object B.  Object A may define variables and functions having the same names as those defined by B.
If object A defines a function of the same name as one defined by B, then the definition provided by A overrides the definition provided by B.
If A wishes to access the definition provided by B, then it may do so.
For example suppose that object A defines its own function named `query_long` and yet wishes to call the `query_long` function provided by the `/std/Object.c` object.
Then A may refer to the `query_long` in `Object.c` as `Object::query_long()`.
If A defines a variable of the same name as a global variable defined in B, then the only way that A can access that variable is via functions provided by B.
If B defines a global variable that is not declared in A, then by default A may use that global variable as if the global variable were defined in A (assuming B does not choose to restrict access).
Note: if object B is recompiled, object A will continue to use the old version of object B until object A is also recompiled.

Multiple inheritance is allowed.  That is, an object may inherit more than one other object.
Suppose `special.c` inherits `weapon.c` and `armor.c` and that both `weapon.c` and `armor.c` each provide their own version of `query_long()`.
We may assume that `special.c` wants to sometimes act like a weapon and sometimes act like armor.  When `special.c` is to look like armor it can use `armor::query_long()` and when it is to look like a weapon it can use `weapon::query_long()`.

See the section [Special Types](#special-types) for more information on how inherited objects may hide data and function definitions from objects that inherit them.

### Function Prototypes

The LPC function prototype is very similar to that of ANSI C.
The function prototype allows for better type checking and can serve as a kind of 'forward' declaration.
```
return_type function_name(arg1_type arg1, arg2_type arg2, ...);
```
Also note that the arguments need not have names:
```
return_type function_name(arg1_type, arg2_type, ...);
```
	
### Functions

The LPC function is similar but not identical to that provided by C (it is most similar to that provided by ANSI C).
The syntax is as follows:
```
return_type function_name(arg1_type arg1, arg2_type arg2, ...)
{
  variable_declarations;
  ...;

  statements;
  ...;
  return var0;
}
```
Note that var0 must be of return_type.

If a function doesn't need to return a value, then it should be declared with a return_type of "void".  E.g.
```
void function_name(arg1_type arg1, ...)
{
	statements;
	...;
}
```
Invoke a function as follows:
```
function_name(arg1, arg2, arg3, ...);
```
You may invoke a function in another object as follows:
```
object->function_name(arg1, arg2, arg3, ...);
```
or:
```
call_other(object, function_name, arg1, arg2, ...);
```

### The `if` ... `else` statement:

LPC's if statement is identical to that provided by C.  Syntax is as follows:
```
    if (expression)
        statement;
```
Alternately:
```
    if (expression) {
        statements;
    }
```
Alternately:
```
    if (expression0) {
        statements;
    } else {
        statements1;
    }
```
Alternately:
```
    if (expression0) {
        statements0;
    } else if (expression1) {
        statements1;
    }
```
The number of else clauses is not explicitly limited.

Another favorite programming construct is the `?` `:` operator, which also operates identical to C.  The syntax is:
```
    expression0 ? expression1_if_true : expression2_if_false
```
In some cases, `?` `:` is an shorter way of expression constructs such as:
```
    if (expression0)
        var = expression1;
    else
        var = expression2;
```
which can be equivalently translated to:
```
    var = expression0 ? expression1 : expression;
```

### The `for` loop

The LPC for loop is also identical to that provided by C.  Syntax is as
follows:

~~~cxx
for (expression0; expression1; expression2) {
	statements;
	...;
}
~~~

Expression0 is evaluated once prior to the execution of the loop.  Expression1
is evaluated at the beginning of each iteration of the loop.  If expression1
evaluates to zero, then the loop terminates.  Expression2 is evaluated at
the end of each loop iteration.

A `break` in the body of the loop will terminate the loop. A `continue` will
continue the execution from the beginning of the loop (after evaluating
Expression2).

A typical usage of the for loop is to execute a body of code some
fixed number of times:

~~~cxx
int i;

for (i = 0; i < 10; i++) {
	write("i == " + i + "\n");
	write("10 - i == " + (10 - i) + "\n");
}
~~~

### The `foreach` loop

LPC offers a Python style `foreach` keyword to loop through string, array, and mappings. The syntax is:
~~~cxx
foreach (var in array) {
};

foreach (ch in string) {
};

foreach (key, value in mapping) {
};
~~~

### The `while` loop

LPC's while loop is identical to that provided by C.  Syntax is as follows:
```
while (expression)
	statement;	
```
where statement may be replaced by a block of statements delimited by matching curly brackets.  For example:
```
while (expression) {
	statement0;
	statement1;
}
```
The statements inside the body of the while loop will be executed repeatedly for as long as the test expression evaluates to non-zero.
If the test expression is zero just prior to the execution of the loop, then the body of the loop will not be executed.
A `break;` statement in the body of the loop will terminate the loop (skipping any statements in the loop that remain to be executed).
A `continue;` statement in the body of the loop will continue the execution from the beginning of the loop (skipping the remainder of the statements in the loop for the current iteration).
```
int test(int limit)
{
	total = 0;
	j = 0;
	while (j < limit) {
		if ((j % 2) != 0)
			continue;
		total += j;
		j++;
	}
	return total;
}
```
The results of this code fragment will be to sum all of the even numbers from 0 to to limit - 1. 

### The `switch` statement

The LPC switch statement is nearly identical to the C switch statement.
The only real difference is that the cases of the LPC switch may be strings as well as integers.
Syntax is as follows:
```
switch (expression) {
	case constant0 : statements0;
		break;
	case constant1 : statements1;
		break;
	default : statements2;
		break;
}
```
The switch is a replacement for the chained if else if else if else construct.
The above switch is equivalent to:
```
tmp = expression;
if (tmp == constant0) {
	statements0;
	...;
} else if (tmp == constant1) {
	statements1;
	...;
} else {
	statements2;
	...;
}
```
The main difference between the `switch` and the `if` statement is that if the `break;` statement is ommited from the end of a particular case, then the statements in the next `case` will be executed as well.

## Preprocessor

### The `#define` directive

The #define preprocessor command creates a macro that can be expanded later on in the file.
For example, if you have the line:
```
#define apples oranges
```
Then every time the word `apples` appears after that point, it will be treated as if it were `oranges`.

### The `#include` directive

A line of the form:
```
#include "filename"
```
will cause the compiler to pretend that the entire contents of 'filename' are actually contained in the file being compiled.
If you want to include a certain function in many different objects, use the `inherit` statement instead.
#including causes each object to have it's own copy of the code, while inheriting causes many objects to share code.

Alternatively,
```
#include <origin.h>
```
Note: the `#include "origin.h"` form looks for `origin.h` in the current directory.
The `#include <origin.h>` form looks for `origin.h` in one of the standard system include directories (on TMI these directories are `/include` and
`/local/include`).

For those that know C, the LPC `#include` directive is identical to C's `#include` statement.
For those that don't know C, the `#include` directive is a way to textually include one file into another.
Putting a directive `#include "origin.h"` in a file gives the same effect as if you had simply typed the contents of `origin.h` directly into the file at the point where you had the `#include` directive.
Included files are recompiled each time the object that include's them is recompiled.
If the included file contains variables or functions of the same name as variables in the file doing the including, then a duplicate-name error will occur at compile time (in the same way that the error would occur if you simply typed in `origin.h` rather than using `#include`).

### The `#pragma save_binary` directive
Using the `#pragma save_binary` directive tells the compiler to store the compiled LPC code (in binary) to a file. Every time a LPC file is about to be compiled, it checks if a previously saved binary file exists and up to date. If a saved binary file exists and up to date, the compiler loads the file in order to save the time for compiling.


