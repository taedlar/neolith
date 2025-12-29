# valid_asm()
## NAME
**valid_asm** - controls whether or not a LPC->C compiled object can use asm { }

## SYNOPSIS
~~~cxx
int valid_asm (string file);
~~~

## DESCRIPTION
When the driver is compiled with `LPC_TO_C`, `valid_asm()` is called whenever the `asm { }` structure is found in code.
If it returns 0, the compilation will terminate with an error.

The `asm { }` structure is used as follows:

```c
asm {
  <C code>
}
```

It causes the code between the braces to be literally inserted into the compiled file.

## SEE ALSO
[valid_compile_to_c()](valid_compile_to_c.md)
