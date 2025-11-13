#pragma once

#include "lpc/types.h"

#define APPLY_CACHE_SIZE (1 << APPLY_CACHE_BITS)

/* for apply_master_ob */
#define MASTER_APPROVED(x) (((x)==(svalue_t *)-1) || ((x) && (((x)->type != T_NUMBER) || (x)->u.number))) 

extern int call_origin;

const char* origin_name (int);

extern svalue_t apply_ret_value;

#ifdef CACHE_STATS
extern unsigned int apply_low_call_others;
extern unsigned int apply_low_cache_hits;
extern unsigned int apply_low_slots_used;
extern unsigned int apply_low_collisions;
#endif
int apply_low(const char *fun, object_t *ob, int num_arg);
svalue_t *apply(const char *, object_t *, int, int);
svalue_t *safe_apply(const char *, object_t *, int, int);
svalue_t *apply_master_ob(const char *, int);
svalue_t *safe_apply_master_ob(const char *, int);

void clear_apply_cache(void);

char *function_exists(const char *, object_t *, int);
