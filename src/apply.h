#pragma once

#include "lpc/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APPLY_CACHE_SIZE (1 << APPLY_CACHE_BITS)

/* for apply_master_ob */
#define MASTER_APPROVED(x) (((x)==(svalue_t *)-1) || ((x) && (((x)->type != T_NUMBER) || (x)->u.number))) 

extern int call_origin;

const char* origin_name (int);

void push_undefined(void);
void pop_stack(void);

/*
 * Stack-slot apply wrappers.
 * Caller must pair *_SLOT_CALL() with APPLY_SLOT_FINISH_CALL() when done
 * reading the returned pointer.
 */
#define APPLY_SLOT_CALL(fun, ob, num_arg, where) \
  (push_undefined(), apply_call((fun), (ob), (num_arg), (where), 1))
#define APPLY_SLOT_SAFE_CALL(fun, ob, num_arg, where) \
  (push_undefined(), safe_apply_call((fun), (ob), (num_arg), (where), 1))
#define APPLY_SLOT_MASTER_CALL(fun, num_arg) \
  (push_undefined(), apply_master_ob((fun), (num_arg), 1))
#define APPLY_SLOT_SAFE_MASTER_CALL(fun, num_arg) \
  (push_undefined(), safe_apply_master_ob((fun), (num_arg), 1))
#define APPLY_SLOT_FINISH_CALL() pop_stack()

/*
 * Compatibility wrappers.
 * These preserve existing call sites that expect non-slot behavior and
 * do not manage explicit stack placeholders.
 */
#define APPLY_CALL(fun, ob, num_arg, where) \
  apply_call((fun), (ob), (num_arg), (where), 0)
#define APPLY_SAFE_CALL(fun, ob, num_arg, where) \
  safe_apply_call((fun), (ob), (num_arg), (where), 0)
#define APPLY_MASTER_CALL(fun, num_arg) \
  apply_master_ob((fun), (num_arg), 0)
#define APPLY_SAFE_MASTER_CALL(fun, num_arg) \
  safe_apply_master_ob((fun), (num_arg), 0)

#ifdef CACHE_STATS
extern unsigned int apply_low_call_others;
extern unsigned int apply_low_cache_hits;
extern unsigned int apply_low_slots_used;
extern unsigned int apply_low_collisions;
#endif
int apply_low(const char *fun, object_t *ob, int num_arg);
svalue_t *apply_call(const char *, object_t *, int, int, int);
svalue_t *safe_apply_call(const char *, object_t *, int, int, int);
svalue_t *apply_master_ob(const char *, int, int);
svalue_t *safe_apply_master_ob(const char *, int, int);

void clear_apply_cache(void);

program_t *find_function (program_t * prog, shared_str_t name, int *index, int *fio, int *vio);

shared_str_t function_exists(const char *, object_t *, int);

#ifdef __cplusplus
}
#endif
