#pragma once

/* configurations and packages initialized in main() */
#include "main.h"

extern char *xalloc(int);

/* dynamic LPC memory allocations */
#include "malloc.h"

#define ALLOCATE(type, tag, desc) ((type *)DXALLOC(sizeof(type), tag, desc))
#define CALLOCATE(num, type, tag, desc) ((type *)DXALLOC(sizeof(type[1]) * (num), tag, desc))
#define RESIZE(ptr, num, type, tag, desc) ((type *)DREALLOC((void *)ptr, sizeof(type) * (num), tag, desc))

/* dynamic string allocations */
#include "stralloc.h"

#define string_copy(x,y) int_string_copy(x)
#define string_unlink(x,y) int_string_unlink(x)
#define new_string(x,y) int_new_string(x)
#define alloc_cstring(x,y) int_alloc_cstring(x)

/* LPC types and the LPMud virtual machine */
#include "lpc/types.h"

extern svalue_t const0;
extern svalue_t const1;
extern svalue_t const0u;

extern object_t *master_ob;

/* interfaces to the LPMud virtual machine */
#include "applies.h"
#include "backend.h"
