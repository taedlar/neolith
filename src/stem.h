#pragma once

/* command line arguments and trace settings */
#include "main.h"

/* stem states */
extern int g_proceeding_shutdown;
extern int comp_flag;
extern time_t boot_time;
extern int slow_shut_down_to_do;

/*  dynamic LPC memory allocations:
 *
 *  DXALLOC - allocation that never fails. Exits on failure.
 *  DMALLOC - generic allocation. Returns NULL on failure.
 *  DREALLOC - generic re-allocation. Returns NULL on failure.
 *  DCALLOC - generic cleared-allocation. Returns NULL on failure.
 *  FREE - free memory allocated by any of the above.
 */
extern char *reserved_area;
extern char *xalloc(size_t);

#include "malloc.h" /* selection of DMALLOC/DXALLOC/DREALLOC/DCALLOC/FREE */

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

/* interfaces to the LPMud virtual machine */
#include "applies.h"
#include "backend.h"
#include "simulate.h"
#include "error_context.h"

/* stem initialization */
extern int init_stem(
    int debug_level,
    unsigned long trace_flags,
    const char* config_file
);
