#pragma once

/* driver command line arguments and trace settings */
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* stem states */
extern bool g_proceeding_shutdown;
extern int g_exit_code;
extern int comp_flag;
extern time_t boot_time;
extern int slow_shutdown_to_do;

int init_stem(
    int debug_level,
    unsigned long trace_flags,
    const char* config_file
);

/* resource management */
void preload_objects(int eflag);
void look_for_objects_to_swap (void);
void start_timers(void);
#ifdef LAZY_RESETS
void try_reset(object_t *);
#endif

/* stem workflows */
int stem_startup(void);
void stem_run(void);
void stem_crash_handler(const char *msg);

void smart_log(const char *error_file, int line, const char *what, bool warning);

#ifdef __cplusplus
}
#endif

/* legacy C message buffer formatting (to be replaced) */
#include "outbuf.h"

/* dynamic string allocations */
#include "stralloc.h"

static inline malloc_str_t new_string(size_t size, const char *caller) {
  (void)caller; /* currently unused, but may be helpful for future debugging */
  return int_new_string(size);
}

static inline malloc_str_t extend_string(malloc_str_t s, size_t size) {
  return int_extend_string(to_malloc_str(s), size);
}

static inline malloc_str_t string_copy(const char *cstr, const char *caller) {
  (void)caller; /* currently unused, but may be helpful for future debugging */
  return int_string_copy(cstr, NULL);
}

static inline malloc_str_t string_unlink(malloc_str_t s, const char *caller) {
  (void)caller; /* currently unused, but may be helpful for future debugging */
  return int_string_unlink(to_malloc_str(s));
}

static inline char* alloc_cstring(const char* cstr, const char* caller) {
  (void)caller; /* currently unused, but may be helpful for future debugging */
  return int_alloc_cstring(cstr, NULL);
}

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
