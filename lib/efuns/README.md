efuns library
=====

## Generating efuns table
The `edit_source` program is used internally to produce several tables (in C data structure) that contains the definitions of efuns interface and LPC language.

The following source files contains the specifications of LPC efuns:
- `options.h`
- `func_spec.c` (This is a LPC source file, not C)

The `func_spec.c` is first processed with C preprocessor (`gcc -E` requires a compatible file extension) to produce the intermediate file `func_spec.i`.

The `edit_source` is invoked to read `func_spec.i` and generate the efuns table (`efuns_*.h`) that are included in the efuns library.
