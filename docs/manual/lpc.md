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
## Constructs
## Preprocessor
