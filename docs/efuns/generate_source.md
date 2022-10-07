# generate_source
## NAME
          generate_source() - generates the C code corresponding to a
          give object

## SYNOPSIS
          void generate_source( string file, void | string out_file );

## DESCRIPTION
          generate_source() calls the LPC->C compiler to generate the
          source code for a given object.  If no output file is
          specified, the output is saved in the SAVE_BINARIES
          directory.

## SEE ALSO
          valid_asm(4), valid_compile_to_c(4)
