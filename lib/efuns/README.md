Efuns library
=====
An **Efun** is a function available for LPC code to call in the mudlib. These functions are written in native C code instead of LPC, therefore you'll not find their LPC code in the mudlib.

Some of the LPMud documentations said these are "external" functions, but they are actually LPMud driver's internal functions with exposed LPC interfaces.
I think it more appropriate to call them "extension" functions.

## Extending Efuns
The LPMud Driver is designed to allow C programmers to extend more efuns for the mudlib.
There are many reasons when you'll prefer implement a function in C rather in LPC: speed, memory efficiency, external dependencies, or extending the LPC language.

> [!IMPORTANT]
> Because Efuns are written in C, a bug in the C code could result in serious software error such as **segmentation fault** and bring down the LPMud server process (and lost unsaved data for all connecting users).
> If you are not an experienced C programmer, it is recommended to try simul_efun first.

## Generating efuns table
The `edit_source` program is used internally to produce several tables (in C data structure) that contains the definitions of efuns interface and LPC language.

The following source files contains the specifications of LPC efuns:
- `options.h`
- `func_spec.c` (This is a LPC source file, not C)

The `func_spec.c` is first processed with C preprocessor (`gcc -E` requires a compatible file extension) to produce the intermediate file `func_spec.i`.

The `edit_source` is invoked to read `func_spec.i` and generate the efuns table (`efuns_*.h`) that are included in the efuns library.
