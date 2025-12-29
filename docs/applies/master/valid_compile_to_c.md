# valid_compile_to_c()
## NAME
**valid_compile_to_c** - controls whether or not an object may be compiled via LPC->C at runtime

## SYNOPSIS
~~~cxx
int valid_compile_to_c (string file);
~~~

## DESCRIPTION
When the driver is compiled with `LPC_TO_C` and `RUNTIME_LOADING` defined, `valid_compile_to_c()` will be called in the master object whenever a program loads whose name ends in `.C` instead of `.c`.
If it returns 1, the program compiles to C.
If it returns 0, it compiles as a normal LPC object.
The name is passed including the trailing `.C`.
