#pragma once

/* driver command line arguments and trace settings */
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* stem states */
extern int g_proceeding_shutdown;
extern int g_exit_code;
extern int comp_flag;
extern time_t boot_time;
extern int slow_shutdown_to_do;

extern int init_stem(
    int debug_level,
    unsigned long trace_flags,
    const char* config_file
);

#ifdef __cplusplus
}
#endif

/* legacy C message buffer formatting (to be replaced) */
#include "outbuf.h"

/* dynamic string allocations */
#include "stralloc.h"
#define string_copy(x,y) int_string_copy(x, NULL)
#define string_unlink(x,y) int_string_unlink(x)
#define new_string(x,y) int_new_string(x)
#define extend_string(x,sz) int_extend_string(x, sz)
#define alloc_cstring(x,y) int_alloc_cstring(x, NULL)

/* define this when included by something compiled before lib/lpc */
#ifndef NO_OPCODES
#include "efuns_opcode.h"
#endif

/* interfaces to the LPMud virtual machine */
#include "lpc/types.h"
#include "applies.h"
#include "backend.h"
#include "simulate.h"
#include "error_context.h"
